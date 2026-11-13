#pragma once

#include "core/task.h"

#include <QAbstractListModel>
#include <QSet>

class TodoStore;

// The tasks of one state, as shown by one tab. Besides the task data it
// keeps what the user marked in that tab: the ticked tasks (checkboxes) and
// the highlighted task.
class TaskListModel : public QAbstractListModel
{
    Q_OBJECT
    Q_PROPERTY(int count READ count NOTIFY countChanged)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY countChanged) // ignoring the label filter
    Q_PROPERTY(int checkedCount READ checkedCount NOTIFY checkedChanged)
    Q_PROPERTY(QString currentId READ currentId WRITE setCurrentId NOTIFY currentIdChanged)
public:
    enum Roles { IdRole = Qt::UserRole + 1, TitleRole, DescriptionRole, LabelColorsRole, PostponedTextRole, CheckedRole, ReturnedRole };

    TaskListModel(TodoStore *store, TaskState state, QObject *parent = nullptr);

    int rowCount(const QModelIndex &parent = {}) const override;
    QVariant data(const QModelIndex &index, int role) const override;
    QHash<int, QByteArray> roleNames() const override;

    TaskState state() const { return m_state; }
    int count() const { return m_tasks.size(); }
    int totalCount() const;
    int checkedCount() const { return m_checked.size(); }
    void setLabelFilter(const QSet<QString> &labelIds);

    QString currentId() const { return m_currentId; }
    void setCurrentId(const QString &id); // empty = no highlight

    Q_INVOKABLE QStringList visibleIds() const;
    Q_INVOKABLE QStringList checkedIds() const;
    // The ticked tasks; when none is ticked, the highlighted task.
    Q_INVOKABLE QStringList selectedIds() const;
    Q_INVOKABLE int indexOfId(const QString &id) const;
    Q_INVOKABLE void setChecked(const QString &id, bool checked);
    Q_INVOKABLE void checkAll();
    // Space: ticks the highlighted task, or unticks it when it is ticked.
    Q_INVOKABLE void toggleCurrentChecked();
    // Shift+Up/Down: moves the highlight and ticks both the task it leaves
    // and the one it reaches (it never unticks).
    Q_INVOKABLE void extendSelection(int delta);
    Q_INVOKABLE void clearChecked();

    // Moves the highlight up (-1) or down (+1); without a highlight it
    // starts at the first or last task.
    Q_INVOKABLE void moveHighlight(int delta);
    // Reordering (only the active list can be reordered).
    Q_INVOKABLE bool moveTask(int from, int to);
    Q_INVOKABLE bool moveCurrent(int delta);

Q_SIGNALS:
    void countChanged();
    void checkedChanged();
    void currentIdChanged();

private:
    void reload();

    TodoStore *m_store;
    TaskState m_state;
    QVector<Task> m_tasks;
    QSet<QString> m_filter;
    QSet<QString> m_checked;
    QString m_currentId;
    bool m_moving = false;
};
