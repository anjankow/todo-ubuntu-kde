#include "taskdialog.h"

#include "core/palette.h"
#include "core/todostore.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QPlainTextEdit>

TaskDialog::TaskDialog(TodoStore *store, const QString &taskId, QWidget *parent)
    : QDialog(parent)
    , m_store(store)
    , m_taskId(taskId)
    , m_title(new QLineEdit(this))
    , m_description(new QPlainTextEdit(this))
    , m_labels(new QListWidget(this))
{
    setObjectName(QStringLiteral("taskDialog"));
    setWindowTitle(i18nc("@title:window", "Task"));

    auto *layout = new QFormLayout(this);
    layout->addRow(i18n("Title:"), m_title);
    layout->addRow(i18n("Description:"), m_description);
    layout->addRow(i18n("Labels:"), m_labels);
    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &TaskDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &TaskDialog::reject);

    const Task *task = store->task(taskId);
    if (task) {
        m_title->setText(task->title);
        m_description->setPlainText(task->description);
    }
    for (const Label &label : store->labels()) {
        auto *item = new QListWidgetItem(label.name, m_labels);
        QPixmap swatch(12, 12);
        swatch.fill(labelPalette().at(label.color));
        item->setIcon(QIcon(swatch));
        item->setData(Qt::UserRole, label.id);
        item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsUserCheckable);
        item->setCheckState(task && task->labelIds.contains(label.id) ? Qt::Checked : Qt::Unchecked);
    }
    m_labels->setVisible(m_labels->count() > 0);
    if (QWidget *labelCaption = layout->labelForField(m_labels)) {
        labelCaption->setVisible(m_labels->count() > 0);
    }
}

void TaskDialog::accept()
{
    if (m_title->text().trimmed().isEmpty()) {
        m_title->setFocus();
        return; // a task needs a title
    }
    QStringList labelIds;
    for (int i = 0; i < m_labels->count(); ++i) {
        if (m_labels->item(i)->checkState() == Qt::Checked) {
            labelIds.append(m_labels->item(i)->data(Qt::UserRole).toString());
        }
    }
    // Copy the id: applying the edit refreshes the views.
    const QString id = m_taskId;
    if (m_store->task(id)) {
        m_store->updateTask(id, m_title->text(), m_description->toPlainText());
        m_store->setTaskLabels(id, labelIds);
    }
    QDialog::accept();
}
