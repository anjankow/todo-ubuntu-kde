#include "appcontroller.h"

#include "core/palette.h"
#include "core/storage.h"
#include "core/taskinput.h"
#include "core/todostore.h"

#include <KLocalizedString>

#include <QClipboard>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QGuiApplication>

namespace
{
QString stateName(TaskState state)
{
    switch (state) {
    case TaskState::Postponed:
        return QStringLiteral("postponed");
    case TaskState::Completed:
        return QStringLiteral("completed");
    case TaskState::Cancelled:
        return QStringLiteral("cancelled");
    case TaskState::Active:
        break;
    }
    return QStringLiteral("active");
}

PostponePreset toCore(AppController::Preset preset)
{
    switch (preset) {
    case AppController::ThreeDays:
        return PostponePreset::ThreeDays;
    case AppController::OneWeek:
        return PostponePreset::OneWeek;
    case AppController::OneMonth:
        return PostponePreset::OneMonth;
    case AppController::Tomorrow:
        break;
    }
    return PostponePreset::Tomorrow;
}
}

AppController::AppController(const QString &configFile, Clock clock, QObject *parent)
    : QObject(parent)
    , m_settings(configFile)
    , m_clock(clock ? std::move(clock) : [] {
        return QDateTime::currentDateTime();
    })
    , m_store(new TodoStore(this))
{
    // In tab order.
    for (TaskState state : {TaskState::Active, TaskState::Postponed, TaskState::Completed, TaskState::Cancelled}) {
        auto *model = new TaskListModel(m_store, state, this);
        m_models.append(model);
        connect(model, &TaskListModel::checkedChanged, this, &AppController::actionsChanged);
    }
    m_storageDir = m_settings.storageDir();
    connect(m_store, &TodoStore::changed, this, &AppController::onStoreChanged);
    loadFromDisk();
}

TaskListModel *AppController::model(TaskState state) const
{
    for (TaskListModel *model : m_models) {
        if (model->state() == state) {
            return model;
        }
    }
    return m_models.first();
}

void AppController::setCurrentTab(int tab)
{
    if (tab >= 0 && tab < m_models.size() && tab != m_currentTab) {
        m_currentTab = tab;
        Q_EMIT currentTabChanged();
        Q_EMIT actionsChanged();
    }
}

TaskListModel *AppController::currentModel() const
{
    return m_models.at(m_currentTab);
}

QVariantList AppController::labels() const
{
    QVariantList result;
    for (const Label &label : m_store->labels()) {
        result.append(QVariantMap {{QStringLiteral("id"), label.id}, {QStringLiteral("name"), label.name},
            {QStringLiteral("color"), labelPalette().at(label.color).name()}, {QStringLiteral("colorIndex"), label.color},
            {QStringLiteral("filtering"), m_filter.contains(label.id)}});
    }
    return result;
}

QStringList AppController::palette() const
{
    QStringList colors;
    for (const QColor &color : labelPalette()) {
        colors.append(color.name());
    }
    return colors;
}

QString AppController::storagePath() const
{
    return Storage::filePath(m_storageDir);
}

void AppController::loadFromDisk()
{
    QString error;
    m_loading = true;
    const bool ok = Storage::load(*m_store, storagePath(), &error);
    m_loading = false;
    if (!ok) {
        // Keep the unreadable file aside so that saving does not destroy it.
        const QString backup = storagePath() + QStringLiteral(".unreadable-") + m_clock().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        QFile::rename(storagePath(), backup);
        m_store->setContent({}, {});
        setError(i18n("Could not read %1: %2\nThe file was kept as %3.", storagePath(), error, backup));
    }
    // Postponed tasks whose time has come return to the list on startup.
    m_store->releaseDuePostponed(m_clock());
}

void AppController::save()
{
    QString error;
    if (!Storage::save(*m_store, storagePath(), &error)) {
        setError(i18n("Could not save %1: %2", storagePath(), error));
    }
}

void AppController::onStoreChanged()
{
    if (!m_loading) {
        save();
    }
    QSet<QString> known;
    for (const Label &label : m_store->labels()) {
        known.insert(label.id);
    }
    m_filter.intersect(known); // a deleted label no longer filters
    applyFilter();
    Q_EMIT actionsChanged();
}

void AppController::applyFilter()
{
    for (TaskListModel *model : std::as_const(m_models)) {
        model->setLabelFilter(m_filter);
    }
    Q_EMIT labelsChanged();
}

AppController::FolderResult AppController::changeStorageDir(const QString &dir, FolderMode mode)
{
    const QString oldPath = storagePath();
    const QString newPath = Storage::filePath(dir);
    if (dir.isEmpty() || !QDir(dir).exists()) {
        setError(i18n("The folder %1 does not exist.", dir));
        return FolderError;
    }
    if (QFileInfo(oldPath).absoluteFilePath() == QFileInfo(newPath).absoluteFilePath()) {
        return FolderChanged;
    }
    if (QFile::exists(newPath)) {
        if (mode == AskOnConflict) {
            return FolderConflict;
        }
        if (mode == LoadExisting) {
            // The previous file is left where it was; nothing is overwritten.
            m_storageDir = dir;
            m_settings.setStorageDir(dir);
            loadFromDisk();
            Q_EMIT storageDirChanged();
            return FolderChanged;
        }
    }
    // Move: write to the new place first, remove the old file only after.
    QString error;
    if (!Storage::save(*m_store, newPath, &error)) {
        setError(i18n("Could not save %1: %2", newPath, error));
        return FolderError;
    }
    m_storageDir = dir;
    m_settings.setStorageDir(dir);
    QFile::remove(oldPath);
    Q_EMIT storageDirChanged();
    return FolderChanged;
}

QString AppController::urlToPath(const QUrl &url) const
{
    return url.isLocalFile() ? url.toLocalFile() : url.toString();
}

void AppController::setError(const QString &message)
{
    m_error = message;
    Q_EMIT errorChanged();
}

void AppController::clearError()
{
    setError({});
}

bool AppController::canComplete() const
{
    return currentModel()->checkedCount() > 0 && m_currentTab != CompletedTab;
}

bool AppController::canCancel() const
{
    return currentModel()->checkedCount() > 0 && m_currentTab != CancelledTab;
}

bool AppController::canPostpone() const
{
    return currentModel()->checkedCount() > 0 && (m_currentTab == ActiveTab || m_currentTab == PostponedTab);
}

bool AppController::canRestore() const
{
    return currentModel()->checkedCount() > 0 && m_currentTab != ActiveTab;
}

bool AppController::canDelete() const
{
    return currentModel()->checkedCount() > 0;
}

bool AppController::addTask(const QString &text)
{
    QString title;
    QString description;
    parseTaskInput(text, &title, &description);
    return !m_store->addTask(title, description, true).isEmpty();
}

QVariantMap AppController::task(const QString &id) const
{
    const Task *task = m_store->task(id);
    if (!task) {
        return {};
    }
    return {{QStringLiteral("id"), task->id}, {QStringLiteral("title"), task->title}, {QStringLiteral("description"), task->description},
        {QStringLiteral("state"), stateName(task->state)}, {QStringLiteral("labelIds"), task->labelIds}};
}

bool AppController::updateTask(const QString &id, const QString &title, const QString &description, const QStringList &labelIds)
{
    if (!m_store->updateTask(id, title, description)) {
        return false;
    }
    return m_store->setTaskLabels(id, labelIds);
}

void AppController::complete(const QStringList &ids)
{
    m_store->complete(ids);
}

void AppController::cancel(const QStringList &ids)
{
    m_store->cancel(ids);
}

void AppController::postpone(const QStringList &ids, Preset preset)
{
    m_store->postpone(ids, ::postponeUntil(toCore(preset), m_clock()));
}

void AppController::postponeUntil(const QStringList &ids, const QDateTime &until)
{
    m_store->postpone(ids, until);
}

QDateTime AppController::suggestedPostponeTime() const
{
    return m_clock().addDays(1);
}

void AppController::completeChecked()
{
    if (canComplete()) {
        m_store->complete(currentModel()->checkedIds());
    }
}

void AppController::cancelChecked()
{
    if (canCancel()) {
        m_store->cancel(currentModel()->checkedIds());
    }
}

void AppController::restoreChecked()
{
    if (canRestore()) {
        m_store->restore(currentModel()->checkedIds());
    }
}

void AppController::postponeChecked(Preset preset)
{
    if (canPostpone()) {
        postpone(currentModel()->checkedIds(), preset);
    }
}

void AppController::postponeCheckedUntil(const QDateTime &until)
{
    if (canPostpone()) {
        m_store->postpone(currentModel()->checkedIds(), until);
    }
}

void AppController::deleteChecked()
{
    const QStringList ids = currentModel()->checkedIds();
    if (ids.size() > 1) {
        Q_EMIT deleteConfirmationRequested(ids.size()); // several at once need a confirmation
    } else {
        m_store->remove(ids);
    }
}

void AppController::confirmDelete()
{
    m_store->remove(currentModel()->checkedIds());
}

void AppController::cancelSelected()
{
    if (m_currentTab != CancelledTab) {
        m_store->cancel(currentModel()->selectedIds());
    }
}

bool AppController::duplicateSelected()
{
    return !m_store->duplicate(currentModel()->selectedIds()).isEmpty();
}

bool AppController::copySelected()
{
    const QStringList ids = currentModel()->selectedIds();
    if (ids.isEmpty()) {
        return false;
    }
    QStringList titles;
    for (const QString &id : ids) {
        titles.append(m_store->task(id)->title);
    }
    m_copiedIds = ids;
    m_copiedText = titles.join(QLatin1Char('\n'));
    QGuiApplication::clipboard()->setText(m_copiedText);
    return true;
}

bool AppController::pasteTasks()
{
    // Something else copied since then is ordinary text for the input.
    if (m_copiedIds.isEmpty() || QGuiApplication::clipboard()->text() != m_copiedText) {
        return false;
    }
    return !m_store->duplicate(m_copiedIds).isEmpty();
}

bool AppController::addLabel(const QString &name, int colorIndex)
{
    return !m_store->addLabel(name, colorIndex).isEmpty();
}

bool AppController::updateLabel(const QString &id, const QString &name, int colorIndex)
{
    return m_store->updateLabel(id, name, colorIndex);
}

bool AppController::removeLabel(const QString &id)
{
    return m_store->removeLabel(id);
}

void AppController::labelClicked(const QString &labelId)
{
    if (!m_store->label(labelId)) {
        return;
    }
    const QStringList ticked = currentModel()->checkedIds();
    if (!ticked.isEmpty()) {
        m_store->toggleLabel(ticked, labelId);
        return;
    }
    if (m_filter.contains(labelId)) {
        m_filter.remove(labelId);
    } else {
        m_filter.insert(labelId);
    }
    applyFilter();
}
