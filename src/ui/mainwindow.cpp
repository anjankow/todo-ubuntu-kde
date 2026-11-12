#include "mainwindow.h"

#include "core/palette.h"
#include "core/storage.h"
#include "core/todostore.h"
#include "labelsdialog.h"
#include "newtaskedit.h"
#include "taskdialog.h"
#include "tasklistview.h"

#include <KHelpMenu>
#include <KLocalizedString>
#include <KMessageBox>
#include <KStandardGuiItem>

#include <QAction>
#include <QDateTimeEdit>
#include <QDialogButtonBox>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QLabel>
#include <QMenu>
#include <QMenuBar>
#include <QTabWidget>
#include <QToolBar>
#include <QToolButton>
#include <QVBoxLayout>

namespace
{
QDateTime askPostponeTimeDialog(QWidget *parent, const QDateTime &now)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(i18nc("@title:window", "Postpone until"));
    auto *layout = new QVBoxLayout(&dialog);
    auto *edit = new QDateTimeEdit(now.addDays(1), &dialog);
    edit->setCalendarPopup(true);
    edit->setMinimumDateTime(now);
    layout->addWidget(edit);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, &dialog);
    layout->addWidget(buttons);
    QObject::connect(buttons, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(buttons, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    return dialog.exec() == QDialog::Accepted ? edit->dateTime() : QDateTime();
}
}

MainWindow::MainWindow(const QString &configFile, Hooks hooks, QWidget *parent)
    : KMainWindow(parent)
    , m_settings(configFile)
    , m_hooks(std::move(hooks))
    , m_store(new TodoStore(this))
{
    if (!m_hooks.now) {
        m_hooks.now = [] {
            return QDateTime::currentDateTime();
        };
    }
    if (!m_hooks.confirmDelete) {
        m_hooks.confirmDelete = [this](int count) {
            return KMessageBox::warningContinueCancel(this, i18np("Permanently delete %1 task?", "Permanently delete %1 tasks?", count),
                       i18nc("@title:window", "Delete tasks"), KStandardGuiItem::del())
                == KMessageBox::Continue;
        };
    }
    if (!m_hooks.resolveFolderConflict) {
        m_hooks.resolveFolderConflict = [this] {
            const int answer = KMessageBox::questionTwoActionsCancel(this, i18n("The selected folder already contains a todo file."),
                i18nc("@title:window", "Todo file exists"), KGuiItem(i18nc("@action:button", "Load it")), KStandardGuiItem::overwrite());
            if (answer == KMessageBox::PrimaryAction) {
                return FolderConflict::LoadExisting;
            }
            return answer == KMessageBox::SecondaryAction ? FolderConflict::Overwrite : FolderConflict::Abort;
        };
    }
    if (!m_hooks.askPostponeTime) {
        m_hooks.askPostponeTime = [this] {
            return askPostponeTimeDialog(this, m_hooks.now());
        };
    }
    if (!m_hooks.askFolder) {
        m_hooks.askFolder = [this] {
            return QFileDialog::getExistingDirectory(this, i18nc("@title:window", "Folder for the todo file"), m_storageDir);
        };
    }
    if (!m_hooks.reportError) {
        m_hooks.reportError = [this](const QString &message) {
            KMessageBox::error(this, message);
        };
    }

    auto *central = new QWidget(this);
    auto *layout = new QVBoxLayout(central);

    m_filterBar = new QWidget(central);
    m_filterLayout = new QHBoxLayout(m_filterBar);
    m_filterLayout->setContentsMargins(0, 0, 0, 0);
    layout->addWidget(m_filterBar);

    m_tabs = new QTabWidget(central);
    layout->addWidget(m_tabs);

    const QList<TaskState> states = {TaskState::Active, TaskState::Postponed, TaskState::Completed, TaskState::Cancelled};
    for (TaskState state : states) {
        auto *view = new TaskListView(m_store, state, m_tabs);
        m_views.insert(state, view);
        connect(view, &TaskListView::taskActivated, this, &MainWindow::openTaskCard);
        connect(view, &TaskListView::checkedChanged, this, &MainWindow::updateActions);
        if (state == TaskState::Active) {
            auto *page = new QWidget(m_tabs);
            auto *pageLayout = new QVBoxLayout(page);
            pageLayout->setContentsMargins(0, 0, 0, 0);
            pageLayout->addWidget(view);
            m_newTask = new NewTaskEdit(page);
            pageLayout->addWidget(m_newTask);
            m_tabs->addTab(page, QString());
        } else {
            m_tabs->addTab(view, QString());
        }
    }
    setCentralWidget(central);

    connect(m_newTask, &NewTaskEdit::taskSubmitted, this, [this](const QString &title, const QString &description) {
        m_store->addTask(title, description);
        view(TaskState::Active)->scrollToBottom();
        m_newTask->setFocus();
    });
    connect(m_tabs, &QTabWidget::currentChanged, this, &MainWindow::updateActions);

    setupActions();

    m_storageDir = m_settings.storageDir();
    connect(m_store, &TodoStore::changed, this, &MainWindow::onStoreChanged);
    loadFromDisk();
    resize(520, 640);
}

void MainWindow::setupActions()
{
    m_complete = new QAction(QIcon::fromTheme(QStringLiteral("task-complete")), i18nc("@action", "Complete"), this);
    m_cancel = new QAction(QIcon::fromTheme(QStringLiteral("dialog-cancel")), i18nc("@action", "Cancel task"), this);
    m_postpone = new QAction(QIcon::fromTheme(QStringLiteral("chronometer")), i18nc("@action", "Postpone"), this);
    m_restore = new QAction(QIcon::fromTheme(QStringLiteral("edit-undo")), i18nc("@action", "Restore"), this);
    m_delete = new QAction(QIcon::fromTheme(QStringLiteral("edit-delete")), i18nc("@action", "Delete"), this);

    auto *postponeMenu = new QMenu(this);
    const QList<QPair<PostponePreset, QString>> presets = {
        {PostponePreset::Tomorrow, i18nc("@action postpone", "Until tomorrow")},
        {PostponePreset::ThreeDays, i18nc("@action postpone", "3 days")},
        {PostponePreset::OneWeek, i18nc("@action postpone", "1 week")},
        {PostponePreset::OneMonth, i18nc("@action postpone", "1 month")},
    };
    for (const auto &preset : presets) {
        QAction *action = postponeMenu->addAction(preset.second);
        m_postponePresets.insert(preset.first, action);
        connect(action, &QAction::triggered, this, [this, kind = preset.first] {
            postponeChecked(postponeUntil(kind, m_hooks.now()));
        });
    }
    postponeMenu->addSeparator();
    m_postponeCustom = postponeMenu->addAction(i18nc("@action postpone", "Custom…"));
    connect(m_postponeCustom, &QAction::triggered, this, [this] {
        postponeChecked(m_hooks.askPostponeTime());
    });
    m_postpone->setMenu(postponeMenu);

    connect(m_complete, &QAction::triggered, this, [this] {
        m_store->complete(currentView()->checkedIds());
    });
    connect(m_cancel, &QAction::triggered, this, [this] {
        m_store->cancel(currentView()->checkedIds());
    });
    connect(m_restore, &QAction::triggered, this, [this] {
        m_store->restore(currentView()->checkedIds());
    });
    connect(m_delete, &QAction::triggered, this, &MainWindow::deleteChecked);

    auto *toolbar = new QToolBar(i18nc("@title:menu", "Main Toolbar"), this);
    toolbar->setObjectName(QStringLiteral("mainToolBar"));
    toolbar->setToolButtonStyle(Qt::ToolButtonTextBesideIcon);
    toolbar->addActions({m_complete, m_cancel, m_postpone, m_restore, m_delete});
    if (auto *button = qobject_cast<QToolButton *>(toolbar->widgetForAction(m_postpone))) {
        button->setPopupMode(QToolButton::InstantPopup);
    }
    addToolBar(toolbar);

    QMenu *fileMenu = menuBar()->addMenu(i18nc("@title:menu", "&File"));
    fileMenu->addAction(QIcon::fromTheme(QStringLiteral("tag")), i18nc("@action", "Labels…"), this, &MainWindow::showLabelsDialog);
    fileMenu->addAction(QIcon::fromTheme(QStringLiteral("folder")), i18nc("@action", "Storage folder…"), this, &MainWindow::chooseStorageDir);
    fileMenu->addSeparator();
    fileMenu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), i18nc("@action", "Quit"), this, &QWidget::close, QKeySequence::Quit);
    auto *helpMenu = new KHelpMenu(this);
    menuBar()->addMenu(helpMenu->menu());
}

TaskListView *MainWindow::currentView() const
{
    const QList<TaskListView *> views = m_views.values();
    for (TaskListView *view : views) {
        QWidget *page = m_tabs->currentWidget();
        if (page == view || page->isAncestorOf(view)) {
            return view;
        }
    }
    return m_views.value(TaskState::Active);
}

QString MainWindow::storagePath() const
{
    return Storage::filePath(m_storageDir);
}

void MainWindow::loadFromDisk()
{
    QString error;
    m_loading = true;
    const bool ok = Storage::load(*m_store, storagePath(), &error);
    m_loading = false;
    if (!ok) {
        // Keep the unreadable file aside so that saving does not destroy it.
        const QString backup = storagePath() + QStringLiteral(".unreadable-") + m_hooks.now().toString(QStringLiteral("yyyyMMdd-HHmmss"));
        QFile::rename(storagePath(), backup);
        m_hooks.reportError(i18n("Could not read %1: %2\nThe file was kept as %3.", storagePath(), error, backup));
        m_store->setContent({}, {});
    }
    // Postponed tasks whose time has come return to the list on startup.
    m_store->releaseDuePostponed(m_hooks.now());
}

void MainWindow::save()
{
    QString error;
    if (!Storage::save(*m_store, storagePath(), &error)) {
        m_hooks.reportError(i18n("Could not save %1: %2", storagePath(), error));
    }
}

void MainWindow::onStoreChanged()
{
    if (!m_loading) {
        save();
    }
    rebuildFilterBar();
    const QList<TaskListView *> views = m_views.values();
    for (TaskListView *view : views) {
        view->setLabelFilter(m_filter);
    }
    updateTabTitles();
    updateActions();
}

void MainWindow::rebuildFilterBar()
{
    qDeleteAll(m_filterBar->findChildren<QWidget *>(QString(), Qt::FindDirectChildrenOnly));
    while (QLayoutItem *item = m_filterLayout->takeAt(0)) {
        delete item;
    }
    m_filterButtons.clear();

    QSet<QString> known;
    if (!m_store->labels().isEmpty()) {
        m_filterLayout->addWidget(new QLabel(i18n("Filter:"), m_filterBar));
    }
    for (const Label &label : m_store->labels()) {
        known.insert(label.id);
        auto *button = new QToolButton(m_filterBar);
        button->setText(label.name);
        button->setCheckable(true);
        button->setChecked(m_filter.contains(label.id));
        const QColor color = labelPalette().at(label.color);
        button->setStyleSheet(QStringLiteral("QToolButton { border: 2px solid %1; border-radius: 9px; padding: 1px 8px; }"
                                             "QToolButton:checked { background: %1; color: %2; }")
                                  .arg(color.name(), color.lightness() > 150 ? QStringLiteral("black") : QStringLiteral("white")));
        connect(button, &QToolButton::toggled, this, [this, id = label.id](bool on) {
            if (on) {
                m_filter.insert(id);
            } else {
                m_filter.remove(id);
            }
            const QList<TaskListView *> views = m_views.values();
            for (TaskListView *view : views) {
                view->setLabelFilter(m_filter);
            }
        });
        m_filterLayout->addWidget(button);
        m_filterButtons.insert(label.id, button);
    }
    m_filterLayout->addStretch();
    m_filter.intersect(known);
    m_filterBar->setVisible(!m_store->labels().isEmpty());
}

QToolButton *MainWindow::filterButton(const QString &labelId) const
{
    return m_filterButtons.value(labelId);
}

void MainWindow::updateTabTitles()
{
    const QMap<TaskState, QString> names = {
        {TaskState::Active, i18nc("@title:tab", "Tasks")},
        {TaskState::Postponed, i18nc("@title:tab", "Postponed")},
        {TaskState::Completed, i18nc("@title:tab", "Completed")},
        {TaskState::Cancelled, i18nc("@title:tab", "Cancelled")},
    };
    const QList<TaskState> states = {TaskState::Active, TaskState::Postponed, TaskState::Completed, TaskState::Cancelled};
    for (int i = 0; i < states.size(); ++i) {
        const int count = m_store->tasksIn(states.at(i)).size();
        m_tabs->setTabText(i, i18nc("tab name (task count)", "%1 (%2)", names.value(states.at(i)), count));
    }
}

void MainWindow::updateActions()
{
    const TaskListView *view = currentView();
    const TaskState state = view->state();
    const bool any = !view->checkedIds().isEmpty();
    m_complete->setEnabled(any && state != TaskState::Completed);
    m_cancel->setEnabled(any && state != TaskState::Cancelled);
    m_postpone->setEnabled(any && (state == TaskState::Active || state == TaskState::Postponed));
    m_restore->setEnabled(any && state != TaskState::Active);
    m_delete->setEnabled(any);
}

void MainWindow::postponeChecked(const QDateTime &until)
{
    if (until.isValid()) {
        m_store->postpone(currentView()->checkedIds(), until);
    }
}

void MainWindow::deleteChecked()
{
    const QStringList ids = currentView()->checkedIds();
    if (ids.isEmpty()) {
        return;
    }
    // Deleting several tasks at once needs a confirmation.
    if (ids.size() > 1 && !m_hooks.confirmDelete(ids.size())) {
        return;
    }
    m_store->remove(ids);
}

TaskDialog *MainWindow::openTaskCard(const QString &taskId)
{
    if (!m_store->task(taskId)) {
        return nullptr;
    }
    auto *dialog = new TaskDialog(m_store, taskId, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
    return dialog;
}

void MainWindow::showLabelsDialog()
{
    auto *dialog = new LabelsDialog(m_store, this);
    dialog->setAttribute(Qt::WA_DeleteOnClose);
    dialog->open();
}

void MainWindow::chooseStorageDir()
{
    const QString dir = m_hooks.askFolder();
    if (!dir.isEmpty()) {
        changeStorageDir(dir);
    }
}

bool MainWindow::changeStorageDir(const QString &dir)
{
    const QString oldPath = storagePath();
    const QString newPath = Storage::filePath(dir);
    if (!QDir(dir).exists()) {
        m_hooks.reportError(i18n("The folder %1 does not exist.", dir));
        return false;
    }
    if (QFileInfo(oldPath).absoluteFilePath() == QFileInfo(newPath).absoluteFilePath()) {
        return true;
    }

    bool loadExisting = false;
    if (QFile::exists(newPath)) {
        const FolderConflict choice = m_hooks.resolveFolderConflict();
        if (choice == FolderConflict::Abort) {
            return false;
        }
        loadExisting = choice == FolderConflict::LoadExisting;
    }

    if (loadExisting) {
        // The previous file is left where it was; nothing is overwritten.
        m_storageDir = dir;
        m_settings.setStorageDir(dir);
        loadFromDisk();
        return true;
    }

    // Move: write to the new place first, remove the old file only after.
    QString error;
    if (!Storage::save(*m_store, newPath, &error)) {
        m_hooks.reportError(i18n("Could not save %1: %2", newPath, error));
        return false;
    }
    m_storageDir = dir;
    m_settings.setStorageDir(dir);
    QFile::remove(oldPath);
    return true;
}
