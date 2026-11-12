#include "tasklistview.h"

#include "core/palette.h"
#include "core/todostore.h"

#include <KLocalizedString>

#include <QDropEvent>
#include <QKeyEvent>
#include <QLocale>
#include <QMouseEvent>
#include <QPainter>

TaskListView::TaskListView(TodoStore *store, TaskState state, QWidget *parent)
    : QListWidget(parent)
    , m_store(store)
    , m_state(state)
{
    setSelectionMode(QAbstractItemView::SingleSelection);
    setIconSize(QSize(40, 12));
    if (state == TaskState::Active) {
        setDragDropMode(QAbstractItemView::InternalMove);
        setDefaultDropAction(Qt::MoveAction);
    }

    connect(this, &QListWidget::itemChanged, this, [this](QListWidgetItem *item) {
        if (m_refreshing) {
            return;
        }
        const QString id = item->data(Qt::UserRole).toString();
        if (item->checkState() == Qt::Checked) {
            m_checked.insert(id);
        } else {
            m_checked.remove(id);
        }
        Q_EMIT checkedChanged();
    });
    connect(this, &QListWidget::itemClicked, this, [this](QListWidgetItem *item) {
        if (!checkRect(row(item)).contains(m_pressPos)) {
            Q_EMIT taskActivated(item->data(Qt::UserRole).toString());
        }
    });
    refresh();
}

void TaskListView::setLabelFilter(const QSet<QString> &labelIds)
{
    m_filter = labelIds;
    refresh();
}

QIcon TaskListView::labelIcon(const Task &task) const
{
    if (task.labelIds.isEmpty()) {
        return {};
    }
    const int dot = 10;
    QPixmap pixmap(iconSize());
    pixmap.fill(Qt::transparent);
    QPainter painter(&pixmap);
    painter.setRenderHint(QPainter::Antialiasing);
    painter.setPen(Qt::NoPen);
    int x = 0;
    for (const QString &labelId : task.labelIds) {
        const Label *label = m_store->label(labelId);
        if (!label || x + dot > pixmap.width()) {
            continue;
        }
        painter.setBrush(labelPalette().at(label->color));
        painter.drawEllipse(x, 1, dot, dot);
        x += dot + 3;
    }
    return QIcon(pixmap);
}

void TaskListView::refresh()
{
    const QString currentId = currentRow() >= 0 ? idAt(currentRow()) : QString();
    m_refreshing = true;
    clear();
    QSet<QString> stillChecked;
    const QVector<Task> tasks = m_store->tasksIn(m_state, m_filter);
    for (const Task &task : tasks) {
        QString text = task.title;
        if (m_state == TaskState::Postponed) {
            text = i18nc("task title, date", "%1  (until %2)", task.title, QLocale().toString(task.postponedUntil, QLocale::ShortFormat));
        }
        auto *item = new QListWidgetItem(text, this);
        item->setData(Qt::UserRole, task.id);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable | Qt::ItemIsUserCheckable | Qt::ItemIsDragEnabled);
        const bool checked = m_checked.contains(task.id);
        item->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        if (checked) {
            stillChecked.insert(task.id);
        }
        item->setIcon(labelIcon(task));
        if (task.id == currentId) {
            setCurrentItem(item);
        }
    }
    m_refreshing = false;
    if (stillChecked != m_checked) {
        m_checked = stillChecked;
        Q_EMIT checkedChanged();
    }
}

QString TaskListView::idAt(int row) const
{
    const QListWidgetItem *it = item(row);
    return it ? it->data(Qt::UserRole).toString() : QString();
}

QStringList TaskListView::visibleIds() const
{
    QStringList ids;
    for (int i = 0; i < count(); ++i) {
        ids.append(idAt(i));
    }
    return ids;
}

QStringList TaskListView::checkedIds() const
{
    QStringList ids;
    for (int i = 0; i < count(); ++i) {
        if (item(i)->checkState() == Qt::Checked) {
            ids.append(idAt(i));
        }
    }
    return ids;
}

void TaskListView::setChecked(const QString &id, bool checked)
{
    for (int i = 0; i < count(); ++i) {
        if (idAt(i) == id) {
            item(i)->setCheckState(checked ? Qt::Checked : Qt::Unchecked);
        }
    }
}

void TaskListView::clearChecked()
{
    for (int i = 0; i < count(); ++i) {
        item(i)->setCheckState(Qt::Unchecked);
    }
}

QRect TaskListView::checkRect(int row) const
{
    QStyleOptionViewItem option = viewOptions();
    option.rect = visualRect(model()->index(row, 0));
    option.features |= QStyleOptionViewItem::HasCheckIndicator;
    return style()->subElementRect(QStyle::SE_ItemViewItemCheckIndicator, &option, this);
}

bool TaskListView::moveCurrent(int delta)
{
    const int from = currentRow();
    const int to = from + delta;
    if (m_state != TaskState::Active || from < 0 || to < 0 || to >= count()) {
        return false;
    }
    QStringList ids = visibleIds();
    ids.move(from, to);
    return m_store->reorder(ids); // the refresh keeps the moved task current
}

void TaskListView::applyVisualOrder()
{
    m_store->reorder(visibleIds());
}

void TaskListView::mousePressEvent(QMouseEvent *event)
{
    m_pressPos = event->pos();
    QListWidget::mousePressEvent(event);
}

void TaskListView::keyPressEvent(QKeyEvent *event)
{
    if (event->modifiers() & Qt::ControlModifier) {
        if (event->key() == Qt::Key_Up || event->key() == Qt::Key_Down) {
            moveCurrent(event->key() == Qt::Key_Up ? -1 : 1);
            event->accept();
            return;
        }
    }
    if ((event->key() == Qt::Key_Return || event->key() == Qt::Key_Enter) && currentItem()) {
        Q_EMIT taskActivated(idAt(currentRow()));
        event->accept();
        return;
    }
    QListWidget::keyPressEvent(event);
}

void TaskListView::dropEvent(QDropEvent *event)
{
    QListWidget::dropEvent(event);
    applyVisualOrder();
}
