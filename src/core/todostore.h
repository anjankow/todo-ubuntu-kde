#pragma once

#include "task.h"

#include <QObject>
#include <QSet>
#include <QVector>

// In-memory model of all tasks and labels. The order of tasks() is the
// user-defined order shown in the lists.
class TodoStore : public QObject
{
    Q_OBJECT
public:
    explicit TodoStore(QObject *parent = nullptr);

    const QVector<Task> &tasks() const { return m_tasks; }
    const QVector<Label> &labels() const { return m_labels; }
    const Task *task(const QString &id) const;
    const Label *label(const QString &id) const;

    // Tasks in `state`, in list order. A non-empty filter keeps only tasks
    // carrying at least one of the given labels.
    QVector<Task> tasksIn(TaskState state, const QSet<QString> &labelFilter = {}) const;

    // Returns the new task id, or an empty string if the title is blank.
    // The task goes to the end of the list, or to its start with atTop.
    QString addTask(const QString &title, const QString &description = {}, bool atTop = false);
    bool updateTask(const QString &id, const QString &title, const QString &description);
    bool setTaskLabels(const QString &id, const QStringList &labelIds);
    // Adds the label to the given tasks; if all of them already carry it,
    // removes it from them instead.
    bool toggleLabel(const QStringList &ids, const QString &labelId);

    void complete(const QStringList &ids);
    void cancel(const QStringList &ids);
    void postpone(const QStringList &ids, const QDateTime &until);
    void restore(const QStringList &ids); // back to the active list
    void remove(const QStringList &ids); // permanent

    // Appends a copy (title, description, labels) of each task to the end
    // of the list as an active task. Returns the ids of the copies.
    QStringList duplicate(const QStringList &ids);

    // Moves postponed tasks whose time has come back to the active list.
    int releaseDuePostponed(const QDateTime &now);

    // Reorders the given tasks among themselves: the slots they occupy in
    // the full list are refilled in the order given.
    bool reorder(const QStringList &idsInNewOrder);

    // Returns the new label id, or an empty string if the name is blank or
    // already used, or the color is not one of the 12 palette colors.
    QString addLabel(const QString &name, int color);
    bool updateLabel(const QString &id, const QString &name, int color);
    bool removeLabel(const QString &id); // also unassigns it from all tasks

    // Replaces all content (used when loading a file).
    void setContent(const QVector<Task> &tasks, const QVector<Label> &labels);

Q_SIGNALS:
    void changed();

private:
    int indexOf(const QString &id) const;
    void setState(const QStringList &ids, TaskState state, const QDateTime &until = {});
    bool labelNameAllowed(const QString &name, const QString &exceptId) const;

    QVector<Task> m_tasks;
    QVector<Label> m_labels;
};
