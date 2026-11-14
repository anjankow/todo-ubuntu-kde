#include "appcontroller.h"

#include "core/palette.h"
#include "core/storage.h"
#include "core/taskinput.h"
#include "core/todostore.h"

#include <KLocalizedString>

#include <QDir>
#include <QFile>
#include <QFileInfo>

namespace
{
struct ConfigurableShortcut {
    const char *action;
    const char *defaultKeys;
};
// In the order shown in the Keyboard shortcuts dialog.
const ConfigurableShortcut configurable[] = {
    {"complete", "K"},
    {"cancel", "Del"},
    {"postpone", "P"},
    {"restore", "R"},
};

QString shortcutName(const QString &action)
{
    if (action == QLatin1String("complete")) {
        return i18nc("@action", "Complete");
    }
    if (action == QLatin1String("cancel")) {
        return i18nc("@action", "Cancel task");
    }
    if (action == QLatin1String("postpone")) {
        return i18nc("@action", "Postpone");
    }
    return i18nc("@action", "Restore");
}

QString defaultKeysOf(const QString &action)
{
    for (const ConfigurableShortcut &c : configurable) {
        if (action == QLatin1String(c.action)) {
            return QString::fromLatin1(c.defaultKeys);
        }
    }
    return {};
}

bool isConfigurable(const QString &action)
{
    for (const ConfigurableShortcut &c : configurable) {
        if (action == QLatin1String(c.action)) {
            return true;
        }
    }
    return false;
}

// A key with modifiers. Qt's modifier and key enums are different types; they
// are meant to be combined as plain ints.
QKeySequence combo(int modifiers, int key)
{
    return QKeySequence(modifiers | key);
}

// Keys with a fixed meaning: they cannot be given to an action.
struct FixedShortcut {
    QKeySequence keys;
    QString meaning;
};
const QVector<FixedShortcut> &fixedShortcuts()
{
    static const QVector<FixedShortcut> list = {
        {QKeySequence(Qt::Key_Up), i18n("move the highlight")},
        {QKeySequence(Qt::Key_Down), i18n("move the highlight")},
        {combo(Qt::SHIFT, Qt::Key_Up), i18n("tick the tasks the highlight passes")},
        {combo(Qt::SHIFT, Qt::Key_Down), i18n("tick the tasks the highlight passes")},
        {combo(Qt::CTRL, Qt::Key_Up), i18n("move the highlighted task")},
        {combo(Qt::CTRL, Qt::Key_Down), i18n("move the highlighted task")},
        {QKeySequence(Qt::Key_Return), i18n("open the task card")},
        {QKeySequence(Qt::Key_Enter), i18n("open the task card")},
        {combo(Qt::SHIFT, Qt::Key_Return), i18n("focus the new-task input")},
        {combo(Qt::SHIFT, Qt::Key_Enter), i18n("focus the new-task input")},
        {QKeySequence(Qt::Key_Escape), i18n("leave the input and remove the highlight")},
        {QKeySequence(Qt::Key_Space), i18n("tick the highlighted task")},
        {combo(Qt::CTRL, Qt::Key_A), i18n("tick all tasks")},
        {combo(Qt::CTRL, Qt::Key_D), i18n("duplicate the selected tasks")},
        {combo(Qt::SHIFT, Qt::Key_Delete), i18n("delete the selected tasks")},
        {combo(Qt::CTRL, Qt::Key_PageUp), i18n("previous tab")},
        {combo(Qt::CTRL, Qt::Key_PageDown), i18n("next tab")},
        {combo(Qt::CTRL, Qt::Key_Q), i18n("quit")},
    };
    return list;
}

QKeySequence parseKeys(const QString &text)
{
    return QKeySequence(text, QKeySequence::PortableText);
}

// One real key or combination. (Text Qt cannot read parses as one "unknown"
// key whose text is empty.)
bool isSingleKey(const QKeySequence &keys)
{
    return keys.count() == 1 && !keys.toString(QKeySequence::PortableText).isEmpty();
}

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
        connect(model, &TaskListModel::currentIdChanged, this, &AppController::actionsChanged);
    }
    m_storageDir = m_settings.storageDir();
    m_releaseTimer.setSingleShot(true);
    connect(&m_releaseTimer, &QTimer::timeout, this, &AppController::releaseDue);
    connect(m_store, &TodoStore::changed, this, &AppController::onStoreChanged);
    loadFromDisk();
    scheduleRelease();
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

QVariantList AppController::topTasks() const
{
    QVariantList result;
    const QVector<Task> tasks = m_store->tasksIn(TaskState::Active); // not narrowed by the label filter
    for (int i = 0; i < tasks.size() && i < topTaskCount; ++i) {
        result.append(QVariantMap {{QStringLiteral("id"), tasks.at(i).id}, {QStringLiteral("title"), tasks.at(i).title}});
    }
    return result;
}

void AppController::setKeepRunningInTray(bool keep)
{
    if (keep != m_keepRunningInTray) {
        m_keepRunningInTray = keep;
        Q_EMIT keepRunningInTrayChanged();
    }
}

QStringList AppController::palette() const
{
    QStringList colors;
    for (const QColor &color : labelPalette()) {
        colors.append(color.name());
    }
    return colors;
}

QVariantList AppController::configurableShortcuts() const
{
    QVariantList result;
    for (const ConfigurableShortcut &c : configurable) {
        const QString action = QString::fromLatin1(c.action);
        const QString keys = shortcutKeys(action);
        result.append(QVariantMap {{QStringLiteral("action"), action}, {QStringLiteral("name"), shortcutName(action)}, {QStringLiteral("keys"), keys},
            {QStringLiteral("defaultKeys"), defaultKeysOf(action)}, {QStringLiteral("isDefault"), keys == defaultKeysOf(action)}});
    }
    return result;
}

QString AppController::shortcutKeys(const QString &action) const
{
    const QMap<QString, QString> &saved = m_store->shortcuts(); // from the todo file
    const QString stored = saved.contains(action) ? saved.value(action) : defaultKeysOf(action);
    // What is stored is normalised: a hand-edited or broken entry reads as no shortcut.
    const QKeySequence keys = parseKeys(stored);
    return isSingleKey(keys) ? keys.toString(QKeySequence::PortableText) : QString();
}

QString AppController::keyText(int key, int modifiers) const
{
    switch (key) {
    case Qt::Key_Shift:
    case Qt::Key_Control:
    case Qt::Key_Alt:
    case Qt::Key_AltGr:
    case Qt::Key_Meta:
    case Qt::Key_Super_L:
    case Qt::Key_Super_R:
    case Qt::Key_CapsLock:
    case Qt::Key_NumLock:
    case Qt::Key_ScrollLock:
    case Qt::Key_unknown:
    case 0:
        return {};
    default:
        break;
    }
    const Qt::KeyboardModifiers wanted = Qt::ShiftModifier | Qt::ControlModifier | Qt::AltModifier | Qt::MetaModifier;
    const int mods = static_cast<int>(static_cast<unsigned>(Qt::KeyboardModifiers(modifiers) & wanted));
    return QKeySequence(key | mods).toString(QKeySequence::PortableText);
}

QString AppController::actionForKey(int key, int modifiers) const
{
    const QString text = keyText(key, modifiers);
    if (text.isEmpty()) {
        return {};
    }
    for (const ConfigurableShortcut &c : configurable) {
        const QString action = QString::fromLatin1(c.action);
        if (shortcutKeys(action) == text) {
            return action;
        }
    }
    return {};
}

QString AppController::conflictOf(const QString &action, const QKeySequence &keys) const
{
    for (const FixedShortcut &fixed : fixedShortcuts()) {
        if (fixed.keys == keys) {
            return i18n("%1 is already used to %2.", keys.toString(QKeySequence::NativeText), fixed.meaning);
        }
    }
    for (const ConfigurableShortcut &c : configurable) {
        const QString other = QString::fromLatin1(c.action);
        if (other != action && parseKeys(shortcutKeys(other)) == keys) {
            return i18n("%1 is already the shortcut of \"%2\".", keys.toString(QKeySequence::NativeText), shortcutName(other));
        }
    }
    return {};
}

QString AppController::setShortcut(const QString &action, const QString &keys)
{
    if (!isConfigurable(action)) {
        return i18n("Unknown action.");
    }
    QString normalised;
    if (!keys.isEmpty()) {
        const QKeySequence parsed = parseKeys(keys);
        if (!isSingleKey(parsed)) {
            return i18n("%1 is not a single key or key combination.", keys);
        }
        const QString conflict = conflictOf(action, parsed);
        if (!conflict.isEmpty()) {
            return conflict;
        }
        normalised = parsed.toString(QKeySequence::PortableText);
    }
    // All four are written to the todo file, whichever one changed.
    QMap<QString, QString> all;
    for (const ConfigurableShortcut &entry : configurable) {
        const QString name = QString::fromLatin1(entry.action);
        all.insert(name, name == action ? normalised : shortcutKeys(name));
    }
    m_store->setShortcuts(all); // saves the todo file
    Q_EMIT shortcutsChanged();
    return {};
}

QString AppController::resetShortcut(const QString &action)
{
    return setShortcut(action, defaultKeysOf(action));
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
        m_store->setContent({}, {}, {});
        setError(i18n("Could not read %1: %2\nThe file was kept as %3.", storagePath(), error, backup));
    }
    // Postponed tasks whose time has come return to the list on startup.
    m_store->releaseDuePostponed(m_clock());
    // The shortcuts come with the file: another folder's file has its own.
    Q_EMIT shortcutsChanged();
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
    Q_EMIT topTasksChanged();
    scheduleRelease(); // a task may have been postponed
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

bool AppController::hasSelection() const
{
    return !currentModel()->selectedIds().isEmpty();
}

bool AppController::canComplete() const
{
    return hasSelection() && m_currentTab != CompletedTab;
}

bool AppController::canCancel() const
{
    return hasSelection() && m_currentTab != CancelledTab;
}

bool AppController::canPostpone() const
{
    return hasSelection() && (m_currentTab == ActiveTab || m_currentTab == PostponedTab);
}

bool AppController::canRestore() const
{
    return hasSelection() && m_currentTab != ActiveTab;
}

bool AppController::canDelete() const
{
    return hasSelection();
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

void AppController::dismissReturned(const QString &id)
{
    m_store->dismissReturned(id);
}

QDateTime AppController::nextReleaseAt() const
{
    return m_store->nextPostponedTime();
}

void AppController::releaseDue()
{
    if (m_store->releaseDuePostponed(m_clock()) == 0) {
        scheduleRelease(); // nothing was due yet (an early or capped wake-up)
    }
}

void AppController::scheduleRelease()
{
    const QDateTime next = m_store->nextPostponedTime();
    if (!next.isValid()) {
        m_releaseTimer.stop();
        return;
    }
    const qint64 wait = qBound<qint64>(0, m_clock().msecsTo(next), maxReleaseWaitMs);
    m_releaseTimer.start(int(wait));
}

QDateTime AppController::suggestedPostponeTime() const
{
    return m_clock().addDays(1);
}

void AppController::completeSelected()
{
    if (canComplete()) {
        m_store->complete(currentModel()->selectedIds());
    }
}

void AppController::cancelSelected()
{
    if (canCancel()) {
        m_store->cancel(currentModel()->selectedIds());
    }
}

void AppController::restoreSelected()
{
    if (canRestore()) {
        m_store->restore(currentModel()->selectedIds());
    }
}

void AppController::postponeSelected(Preset preset)
{
    if (canPostpone()) {
        postpone(currentModel()->selectedIds(), preset);
    }
}

void AppController::postponeSelectedUntil(const QDateTime &until)
{
    if (canPostpone()) {
        m_store->postpone(currentModel()->selectedIds(), until);
    }
}

void AppController::deleteSelected()
{
    const QStringList ids = currentModel()->selectedIds();
    if (ids.size() > 1) {
        Q_EMIT deleteConfirmationRequested(ids.size()); // several at once need a confirmation
    } else {
        m_store->remove(ids);
    }
}

void AppController::confirmDelete()
{
    m_store->remove(currentModel()->selectedIds());
}

bool AppController::duplicateSelected()
{
    return !m_store->duplicate(currentModel()->selectedIds()).isEmpty();
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
