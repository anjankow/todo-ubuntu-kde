#include "tasklistmodel.h"

#include "core/palette.h"
#include "core/todostore.h"

#include <QLocale>

TaskListModel::TaskListModel(TodoStore *store, TaskState state, QObject *parent)
    : QAbstractListModel(parent)
    , m_store(store)
    , m_state(state)
{
    connect(store, &TodoStore::changed, this, [this] {
        if (!m_moving) {
            reload();
        }
    });
    reload();
}

int TaskListModel::rowCount(const QModelIndex &parent) const
{
    return parent.isValid() ? 0 : m_tasks.size();
}

QVariant TaskListModel::data(const QModelIndex &index, int role) const
{
    if (!index.isValid() || index.row() < 0 || index.row() >= m_tasks.size()) {
        return {};
    }
    const Task &task = m_tasks.at(index.row());
    switch (role) {
    case IdRole:
        return task.id;
    case Qt::DisplayRole:
    case TitleRole:
        return task.title;
    case DescriptionRole:
        return task.description;
    case LabelColorsRole: {
        QStringList colors;
        for (const QString &labelId : task.labelIds) {
            if (const Label *label = m_store->label(labelId)) {
                colors.append(labelPalette().at(label->color).name());
            }
        }
        return colors;
    }
    case PostponedTextRole:
        return task.state == TaskState::Postponed ? QLocale().toString(task.postponedUntil, QLocale::ShortFormat) : QString();
    case CheckedRole:
        return m_checked.contains(task.id);
    case ReturnedRole:
        return task.returned;
    default:
        return {};
    }
}

QHash<int, QByteArray> TaskListModel::roleNames() const
{
    return {{IdRole, "taskId"}, {TitleRole, "title"}, {DescriptionRole, "description"}, {LabelColorsRole, "labelColors"}, {PostponedTextRole, "postponedText"},
        {CheckedRole, "checked"}, {ReturnedRole, "returned"}};
}

int TaskListModel::totalCount() const
{
    return m_store->tasksIn(m_state).size();
}

void TaskListModel::setLabelFilter(const QSet<QString> &labelIds)
{
    if (m_filter != labelIds) {
        m_filter = labelIds;
        reload();
    }
}

void TaskListModel::reload()
{
    beginResetModel();
    m_tasks = m_store->tasksIn(m_state, m_filter);
    endResetModel();

    QSet<QString> present;
    for (const Task &task : std::as_const(m_tasks)) {
        present.insert(task.id);
    }
    const int checkedBefore = m_checked.size();
    m_checked.intersect(present);
    if (m_checked.size() != checkedBefore) {
        Q_EMIT checkedChanged();
    }
    if (!m_currentId.isEmpty() && !present.contains(m_currentId)) {
        m_currentId.clear();
        Q_EMIT currentIdChanged();
    }
    Q_EMIT countChanged();
}

void TaskListModel::setCurrentId(const QString &id)
{
    const QString newId = indexOfId(id) >= 0 ? id : QString();
    if (newId != m_currentId) {
        m_currentId = newId;
        Q_EMIT currentIdChanged();
    }
}

QStringList TaskListModel::visibleIds() const
{
    QStringList ids;
    for (const Task &task : m_tasks) {
        ids.append(task.id);
    }
    return ids;
}

QStringList TaskListModel::checkedIds() const
{
    QStringList ids;
    for (const Task &task : m_tasks) {
        if (m_checked.contains(task.id)) {
            ids.append(task.id);
        }
    }
    return ids;
}

QStringList TaskListModel::selectedIds() const
{
    QStringList ids = checkedIds();
    if (ids.isEmpty() && !m_currentId.isEmpty()) {
        ids.append(m_currentId);
    }
    return ids;
}

int TaskListModel::indexOfId(const QString &id) const
{
    for (int i = 0; i < m_tasks.size(); ++i) {
        if (m_tasks.at(i).id == id) {
            return i;
        }
    }
    return -1;
}

void TaskListModel::setChecked(const QString &id, bool checked)
{
    const int row = indexOfId(id);
    if (row < 0 || m_checked.contains(id) == checked) {
        return;
    }
    if (checked) {
        m_checked.insert(id);
    } else {
        m_checked.remove(id);
    }
    Q_EMIT dataChanged(index(row), index(row), {CheckedRole});
    Q_EMIT checkedChanged();
}

void TaskListModel::checkAll()
{
    if (m_checked.size() == m_tasks.size()) {
        return;
    }
    for (const Task &task : std::as_const(m_tasks)) {
        m_checked.insert(task.id);
    }
    Q_EMIT dataChanged(index(0), index(m_tasks.size() - 1), {CheckedRole});
    Q_EMIT checkedChanged();
}

void TaskListModel::toggleCurrentChecked()
{
    if (!m_currentId.isEmpty()) {
        setChecked(m_currentId, !m_checked.contains(m_currentId));
    }
}

void TaskListModel::extendSelection(int delta)
{
    if (!m_currentId.isEmpty()) {
        setChecked(m_currentId, true);
    }
    moveHighlight(delta);
    if (!m_currentId.isEmpty()) {
        setChecked(m_currentId, true);
    }
}

void TaskListModel::clearChecked()
{
    if (m_checked.isEmpty()) {
        return;
    }
    m_checked.clear();
    if (!m_tasks.isEmpty()) {
        Q_EMIT dataChanged(index(0), index(m_tasks.size() - 1), {CheckedRole});
    }
    Q_EMIT checkedChanged();
}

void TaskListModel::moveHighlight(int delta)
{
    if (m_tasks.isEmpty() || delta == 0) {
        return;
    }
    int row = indexOfId(m_currentId);
    if (row < 0) {
        row = delta > 0 ? 0 : m_tasks.size() - 1;
    } else {
        row = qBound(0, row + delta, m_tasks.size() - 1);
    }
    setCurrentId(m_tasks.at(row).id);
}

bool TaskListModel::moveTask(int from, int to)
{
    if (m_state != TaskState::Active || from == to || from < 0 || to < 0 || from >= m_tasks.size() || to >= m_tasks.size()) {
        return false;
    }
    // Moved in place (no reset), so that a drag in progress keeps its row.
    beginMoveRows({}, from, from, {}, to > from ? to + 1 : to);
    m_tasks.move(from, to);
    endMoveRows();
    m_moving = true;
    const bool ok = m_store->reorder(visibleIds());
    m_moving = false;
    return ok;
}

bool TaskListModel::moveCurrent(int delta)
{
    const int from = indexOfId(m_currentId);
    return from >= 0 && moveTask(from, from + delta);
}
