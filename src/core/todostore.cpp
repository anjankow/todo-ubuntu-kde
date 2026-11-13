#include "todostore.h"

#include "palette.h"

#include <QUuid>

#include <algorithm>

namespace
{
QString newId()
{
    return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
}

TodoStore::TodoStore(QObject *parent)
    : QObject(parent)
{
}

int TodoStore::indexOf(const QString &id) const
{
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

const Task *TodoStore::task(const QString &id) const
{
    const int i = indexOf(id);
    return i < 0 ? nullptr : &m_tasks.at(i);
}

const Label *TodoStore::label(const QString &id) const
{
    for (const Label &l : m_labels) {
        if (l.id == id) {
            return &l;
        }
    }
    return nullptr;
}

QVector<Task> TodoStore::tasksIn(TaskState state, const QSet<QString> &labelFilter) const
{
    QVector<Task> result;
    for (const Task &t : m_tasks) {
        if (t.state != state) {
            continue;
        }
        if (!labelFilter.isEmpty()) {
            const bool match = std::any_of(t.labelIds.cbegin(), t.labelIds.cend(), [&](const QString &id) {
                return labelFilter.contains(id);
            });
            if (!match) {
                continue;
            }
        }
        result.append(t);
    }
    return result;
}

QString TodoStore::addTask(const QString &title, const QString &description, bool atTop)
{
    const QString cleanTitle = title.trimmed();
    if (cleanTitle.isEmpty()) {
        return {};
    }
    Task t;
    t.id = newId();
    t.title = cleanTitle;
    t.description = description.trimmed();
    if (atTop) {
        m_tasks.prepend(t);
    } else {
        m_tasks.append(t);
    }
    Q_EMIT changed();
    return t.id;
}

bool TodoStore::updateTask(const QString &id, const QString &title, const QString &description)
{
    const int i = indexOf(id);
    const QString cleanTitle = title.trimmed();
    if (i < 0 || cleanTitle.isEmpty()) {
        return false;
    }
    m_tasks[i].title = cleanTitle;
    m_tasks[i].description = description.trimmed();
    Q_EMIT changed();
    return true;
}

bool TodoStore::setTaskLabels(const QString &id, const QStringList &labelIds)
{
    const int i = indexOf(id);
    if (i < 0) {
        return false;
    }
    QStringList valid;
    for (const QString &labelId : labelIds) {
        if (label(labelId) && !valid.contains(labelId)) {
            valid.append(labelId);
        }
    }
    m_tasks[i].labelIds = valid;
    Q_EMIT changed();
    return true;
}

bool TodoStore::toggleLabel(const QStringList &ids, const QString &labelId)
{
    if (!label(labelId)) {
        return false;
    }
    QVector<int> rows;
    bool allHaveIt = true;
    for (const QString &id : ids) {
        const int i = indexOf(id);
        if (i >= 0 && !rows.contains(i)) {
            rows.append(i);
            allHaveIt = allHaveIt && m_tasks.at(i).labelIds.contains(labelId);
        }
    }
    if (rows.isEmpty()) {
        return false;
    }
    for (int i : std::as_const(rows)) {
        if (allHaveIt) {
            m_tasks[i].labelIds.removeAll(labelId);
        } else if (!m_tasks.at(i).labelIds.contains(labelId)) {
            m_tasks[i].labelIds.append(labelId);
        }
    }
    Q_EMIT changed();
    return true;
}

void TodoStore::setState(const QStringList &ids, TaskState state, const QDateTime &until)
{
    bool any = false;
    for (const QString &id : ids) {
        const int i = indexOf(id);
        if (i < 0) {
            continue;
        }
        m_tasks[i].state = state;
        m_tasks[i].postponedUntil = state == TaskState::Postponed ? until : QDateTime();
        m_tasks[i].returned = false;
        any = true;
    }
    if (any) {
        Q_EMIT changed();
    }
}

void TodoStore::complete(const QStringList &ids)
{
    setState(ids, TaskState::Completed);
}

void TodoStore::cancel(const QStringList &ids)
{
    setState(ids, TaskState::Cancelled);
}

void TodoStore::postpone(const QStringList &ids, const QDateTime &until)
{
    if (until.isValid()) {
        setState(ids, TaskState::Postponed, until);
    }
}

void TodoStore::restore(const QStringList &ids)
{
    setState(ids, TaskState::Active);
}

void TodoStore::remove(const QStringList &ids)
{
    const int before = m_tasks.size();
    m_tasks.erase(std::remove_if(m_tasks.begin(), m_tasks.end(),
                      [&](const Task &t) {
                          return ids.contains(t.id);
                      }),
        m_tasks.end());
    if (m_tasks.size() != before) {
        Q_EMIT changed();
    }
}

QStringList TodoStore::duplicate(const QStringList &ids)
{
    QVector<Task> copies;
    for (const Task &t : std::as_const(m_tasks)) { // in list order
        if (ids.contains(t.id)) {
            Task copy;
            copy.id = newId();
            copy.title = t.title;
            copy.description = t.description;
            copy.labelIds = t.labelIds;
            copies.append(copy);
        }
    }
    QStringList newIds;
    for (const Task &copy : std::as_const(copies)) {
        m_tasks.append(copy);
        newIds.append(copy.id);
    }
    if (!newIds.isEmpty()) {
        Q_EMIT changed();
    }
    return newIds;
}

int TodoStore::releaseDuePostponed(const QDateTime &now)
{
    QStringList due;
    for (const Task &t : std::as_const(m_tasks)) {
        if (t.state == TaskState::Postponed && (!t.postponedUntil.isValid() || t.postponedUntil <= now)) {
            due.append(t.id);
        }
    }
    if (due.isEmpty()) {
        return 0;
    }
    for (Task &t : m_tasks) {
        if (due.contains(t.id)) {
            t.state = TaskState::Active;
            t.postponedUntil = QDateTime();
            t.returned = true;
        }
    }
    Q_EMIT changed();
    return due.size();
}

QDateTime TodoStore::nextPostponedTime() const
{
    QDateTime next;
    for (const Task &t : m_tasks) {
        if (t.state == TaskState::Postponed && t.postponedUntil.isValid() && (!next.isValid() || t.postponedUntil < next)) {
            next = t.postponedUntil;
        }
    }
    return next;
}

bool TodoStore::dismissReturned(const QString &id)
{
    const int i = indexOf(id);
    if (i < 0 || !m_tasks.at(i).returned) {
        return false;
    }
    m_tasks[i].returned = false;
    Q_EMIT changed();
    return true;
}

bool TodoStore::reorder(const QStringList &idsInNewOrder)
{
    QVector<int> positions;
    QVector<Task> moved;
    for (const QString &id : idsInNewOrder) {
        const int i = indexOf(id);
        if (i < 0 || positions.contains(i)) {
            return false;
        }
        positions.append(i);
        moved.append(m_tasks.at(i));
    }
    std::sort(positions.begin(), positions.end());
    for (int k = 0; k < positions.size(); ++k) {
        m_tasks[positions.at(k)] = moved.at(k);
    }
    Q_EMIT changed();
    return true;
}

bool TodoStore::labelNameAllowed(const QString &name, const QString &exceptId) const
{
    if (name.isEmpty()) {
        return false;
    }
    return std::none_of(m_labels.cbegin(), m_labels.cend(), [&](const Label &l) {
        return l.id != exceptId && l.name.compare(name, Qt::CaseInsensitive) == 0;
    });
}

QString TodoStore::addLabel(const QString &name, int color)
{
    const QString cleanName = name.trimmed();
    if (!labelNameAllowed(cleanName, {}) || color < 0 || color >= labelPalette().size()) {
        return {};
    }
    Label l;
    l.id = newId();
    l.name = cleanName;
    l.color = color;
    m_labels.append(l);
    Q_EMIT changed();
    return l.id;
}

bool TodoStore::updateLabel(const QString &id, const QString &name, int color)
{
    const QString cleanName = name.trimmed();
    if (!labelNameAllowed(cleanName, id) || color < 0 || color >= labelPalette().size()) {
        return false;
    }
    for (Label &l : m_labels) {
        if (l.id == id) {
            l.name = cleanName;
            l.color = color;
            Q_EMIT changed();
            return true;
        }
    }
    return false;
}

bool TodoStore::removeLabel(const QString &id)
{
    const int before = m_labels.size();
    m_labels.erase(std::remove_if(m_labels.begin(), m_labels.end(),
                       [&](const Label &l) {
                           return l.id == id;
                       }),
        m_labels.end());
    if (m_labels.size() == before) {
        return false;
    }
    for (Task &t : m_tasks) {
        t.labelIds.removeAll(id);
    }
    Q_EMIT changed();
    return true;
}

void TodoStore::setContent(const QVector<Task> &tasks, const QVector<Label> &labels)
{
    m_tasks = tasks;
    m_labels = labels;
    Q_EMIT changed();
}
