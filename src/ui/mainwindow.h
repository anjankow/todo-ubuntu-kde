#pragma once

#include "core/postpone.h"
#include "core/task.h"
#include "settings.h"

#include <KMainWindow>

#include <QDateTime>
#include <QMap>
#include <QSet>

#include <functional>

class NewTaskEdit;
class QAction;
class QHBoxLayout;
class QMenu;
class QTabWidget;
class QToolButton;
class TaskDialog;
class TaskListView;
class TodoStore;

class MainWindow : public KMainWindow
{
    Q_OBJECT
public:
    enum class FolderConflict { LoadExisting, Overwrite, Abort };

    // Points where the window asks the user or the system something.
    // Unset hooks use the real dialogs / clock; tests replace them.
    struct Hooks {
        std::function<QDateTime()> now;
        std::function<bool(int taskCount)> confirmDelete;
        std::function<FolderConflict()> resolveFolderConflict;
        std::function<QDateTime()> askPostponeTime; // invalid = cancelled
        std::function<QString()> askFolder; // empty = cancelled
        std::function<void(const QString &)> reportError;
    };

    explicit MainWindow(const QString &configFile = {}, Hooks hooks = {}, QWidget *parent = nullptr);

    TodoStore *store() const { return m_store; }
    QTabWidget *tabs() const { return m_tabs; }
    TaskListView *view(TaskState state) const { return m_views.value(state); }
    TaskListView *currentView() const;
    NewTaskEdit *newTaskEdit() const { return m_newTask; }

    QAction *completeAction() const { return m_complete; }
    QAction *cancelAction() const { return m_cancel; }
    QAction *postponeAction() const { return m_postpone; }
    QAction *restoreAction() const { return m_restore; }
    QAction *deleteAction() const { return m_delete; }
    QAction *labelsDialogAction() const { return m_labelsDialog; }
    QAction *storageFolderAction() const { return m_storageFolder; }
    QAction *postponeAction(PostponePreset preset) const { return m_postponePresets.value(preset); }
    QAction *postponeCustomAction() const { return m_postponeCustom; }

    QString storagePath() const;
    bool changeStorageDir(const QString &dir);

    QSet<QString> labelFilter() const { return m_filter; }
    // The label's button in the filter bar: filters the lists, or, while
    // tasks are ticked, toggles the label on them.
    QToolButton *filterButton(const QString &labelId) const;

    TaskDialog *openTaskCard(const QString &taskId);

private:
    void setupActions();
    void loadFromDisk();
    void save();
    void onStoreChanged();
    void rebuildFilterBar();
    void updateActions();
    void updateTabTitles();
    void postponeChecked(const QDateTime &until);
    void deleteChecked();
    void chooseStorageDir();
    void showLabelsDialog();

    Settings m_settings;
    Hooks m_hooks;
    TodoStore *m_store;
    QString m_storageDir;
    bool m_loading = false;

    QTabWidget *m_tabs = nullptr;
    QMap<TaskState, TaskListView *> m_views;
    NewTaskEdit *m_newTask = nullptr;
    QHBoxLayout *m_filterLayout = nullptr;
    QWidget *m_filterBar = nullptr;
    QMap<QString, QToolButton *> m_filterButtons;
    QSet<QString> m_filter;

    QAction *m_complete = nullptr;
    QAction *m_cancel = nullptr;
    QAction *m_postpone = nullptr;
    QAction *m_restore = nullptr;
    QAction *m_delete = nullptr;
    QAction *m_labelsDialog = nullptr;
    QAction *m_storageFolder = nullptr;
    QAction *m_postponeCustom = nullptr;
    QMap<PostponePreset, QAction *> m_postponePresets;
};
