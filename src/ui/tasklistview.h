#pragma once

#include "core/task.h"

#include <QListWidget>
#include <QSet>

class TodoStore;

// List of the tasks in one state. Shows titles only. Each row has a
// checkbox used to select tasks for batch actions; clicking the title
// asks for the task card.
class TaskListView : public QListWidget
{
    Q_OBJECT
public:
    TaskListView(TodoStore *store, TaskState state, QWidget *parent = nullptr);

    TaskState state() const { return m_state; }
    void setLabelFilter(const QSet<QString> &labelIds);
    void refresh();

    QStringList visibleIds() const;
    QStringList checkedIds() const;
    void setChecked(const QString &id, bool checked);
    void clearChecked();
    void checkAll();

    // Area of the row's checkbox, in viewport coordinates.
    QRect checkRect(int row) const;

    // Moves the current task up (-1) or down (+1) in the list.
    bool moveCurrent(int delta);
    // Stores the order currently shown by the rows (after a drag & drop).
    void applyVisualOrder();

Q_SIGNALS:
    void taskActivated(const QString &id);
    void checkedChanged();

protected:
    void mousePressEvent(QMouseEvent *event) override;
    void keyPressEvent(QKeyEvent *event) override;
    void dropEvent(QDropEvent *event) override;

private:
    QString idAt(int row) const;
    QIcon labelIcon(const Task &task) const;

    TodoStore *m_store;
    TaskState m_state;
    QSet<QString> m_filter;
    QSet<QString> m_checked;
    QPoint m_pressPos;
    bool m_refreshing = false;
};
