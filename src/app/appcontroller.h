#pragma once

#include "core/postpone.h"
#include "settings.h"
#include "tasklistmodel.h"

#include <QDateTime>
#include <QObject>
#include <QSet>
#include <QUrl>
#include <QVariant>

#include <functional>

class TodoStore;

// Everything the user interface does goes through this object: it owns the
// todo list, loads and saves the file, and offers the lists and actions to
// the QML side.
class AppController : public QObject
{
    Q_OBJECT
    Q_PROPERTY(TaskListModel *activeModel READ activeModel CONSTANT)
    Q_PROPERTY(TaskListModel *postponedModel READ postponedModel CONSTANT)
    Q_PROPERTY(TaskListModel *completedModel READ completedModel CONSTANT)
    Q_PROPERTY(TaskListModel *cancelledModel READ cancelledModel CONSTANT)
    Q_PROPERTY(int currentTab READ currentTab WRITE setCurrentTab NOTIFY currentTabChanged)
    Q_PROPERTY(TaskListModel *currentModel READ currentModel NOTIFY currentTabChanged)
    Q_PROPERTY(QVariantList labels READ labels NOTIFY labelsChanged)
    Q_PROPERTY(QStringList palette READ palette CONSTANT)
    Q_PROPERTY(QString storageDir READ storageDir NOTIFY storageDirChanged)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY errorChanged) // empty = none
    Q_PROPERTY(bool canComplete READ canComplete NOTIFY actionsChanged)
    Q_PROPERTY(bool canCancel READ canCancel NOTIFY actionsChanged)
    Q_PROPERTY(bool canPostpone READ canPostpone NOTIFY actionsChanged)
    Q_PROPERTY(bool canRestore READ canRestore NOTIFY actionsChanged)
    Q_PROPERTY(bool canDelete READ canDelete NOTIFY actionsChanged)
public:
    enum Tab { ActiveTab, PostponedTab, CompletedTab, CancelledTab };
    Q_ENUM(Tab)
    enum Preset { Tomorrow, ThreeDays, OneWeek, OneMonth };
    Q_ENUM(Preset)
    enum FolderMode { AskOnConflict, LoadExisting, Overwrite };
    Q_ENUM(FolderMode)
    enum FolderResult { FolderChanged, FolderConflict, FolderError };
    Q_ENUM(FolderResult)

    using Clock = std::function<QDateTime()>;
    explicit AppController(const QString &configFile = {}, Clock clock = {}, QObject *parent = nullptr);

    TodoStore *store() const { return m_store; }
    TaskListModel *model(TaskState state) const;
    TaskListModel *activeModel() const { return model(TaskState::Active); }
    TaskListModel *postponedModel() const { return model(TaskState::Postponed); }
    TaskListModel *completedModel() const { return model(TaskState::Completed); }
    TaskListModel *cancelledModel() const { return model(TaskState::Cancelled); }
    int currentTab() const { return m_currentTab; }
    void setCurrentTab(int tab);
    TaskListModel *currentModel() const;

    // One entry per label: id, name, color (#rrggbb), colorIndex, filtering.
    QVariantList labels() const;
    QStringList palette() const;
    QSet<QString> labelFilter() const { return m_filter; }

    QString storageDir() const { return m_storageDir; }
    QString storagePath() const;
    // Moves the todo file to another folder. If that folder already has a
    // todo file, nothing happens with AskOnConflict (FolderConflict is
    // returned); LoadExisting switches to that file, Overwrite replaces it.
    Q_INVOKABLE AppController::FolderResult changeStorageDir(const QString &dir, AppController::FolderMode mode = AskOnConflict);
    Q_INVOKABLE QString urlToPath(const QUrl &url) const;

    QString errorMessage() const { return m_error; }
    Q_INVOKABLE void clearError();

    bool canComplete() const;
    bool canCancel() const;
    bool canPostpone() const;
    bool canRestore() const;
    bool canDelete() const;

    // --- tasks
    // Creates a task from the text of the new-task input (first line:
    // title, further lines: description) on top of the list.
    Q_INVOKABLE bool addTask(const QString &text);
    Q_INVOKABLE QVariantMap task(const QString &id) const;
    Q_INVOKABLE bool updateTask(const QString &id, const QString &title, const QString &description, const QStringList &labelIds);
    Q_INVOKABLE void complete(const QStringList &ids);
    Q_INVOKABLE void cancel(const QStringList &ids);
    Q_INVOKABLE void postpone(const QStringList &ids, AppController::Preset preset);
    Q_INVOKABLE void postponeUntil(const QStringList &ids, const QDateTime &until);
    Q_INVOKABLE QDateTime suggestedPostponeTime() const;

    // --- toolbar: act on the ticked tasks of the current tab
    Q_INVOKABLE void completeChecked();
    Q_INVOKABLE void cancelChecked();
    Q_INVOKABLE void restoreChecked();
    Q_INVOKABLE void postponeChecked(AppController::Preset preset);
    Q_INVOKABLE void postponeCheckedUntil(const QDateTime &until);
    // Deletes one ticked task at once; for several it only asks for the
    // confirmation (deleteConfirmationRequested), given with confirmDelete().
    Q_INVOKABLE void deleteChecked();
    Q_INVOKABLE void confirmDelete();

    // --- keyboard: act on the ticked tasks, or else the highlighted one
    Q_INVOKABLE void cancelSelected();
    Q_INVOKABLE bool duplicateSelected();
    Q_INVOKABLE bool copySelected();
    // Duplicates the copied tasks if they are still what the clipboard holds.
    Q_INVOKABLE bool pasteTasks();

    // --- labels
    Q_INVOKABLE bool addLabel(const QString &name, int colorIndex);
    Q_INVOKABLE bool updateLabel(const QString &id, const QString &name, int colorIndex);
    Q_INVOKABLE bool removeLabel(const QString &id);
    // A click on a label of the filter bar: with tasks ticked it toggles
    // the label on them, otherwise it toggles filtering by that label.
    Q_INVOKABLE void labelClicked(const QString &labelId);

Q_SIGNALS:
    void currentTabChanged();
    void labelsChanged();
    void storageDirChanged();
    void actionsChanged();
    void deleteConfirmationRequested(int count);
    void errorChanged();

private:
    void loadFromDisk();
    void save();
    void onStoreChanged();
    void applyFilter();
    void setError(const QString &message);

    Settings m_settings;
    Clock m_clock;
    TodoStore *m_store;
    QString m_storageDir;
    bool m_loading = false;
    int m_currentTab = ActiveTab;
    QVector<TaskListModel *> m_models;
    QSet<QString> m_filter;
    QStringList m_copiedIds;
    QString m_copiedText;
    QString m_error;
};
