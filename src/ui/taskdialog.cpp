#include "taskdialog.h"

#include "core/palette.h"
#include "core/todostore.h"

#include <KLocalizedString>

#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QPlainTextEdit>
#include <QPushButton>

TaskDialog::TaskDialog(TodoStore *store, const QString &taskId, Hooks hooks, QWidget *parent)
    : QDialog(parent)
    , m_store(store)
    , m_taskId(taskId)
    , m_hooks(std::move(hooks))
    , m_title(new QLineEdit(this))
    , m_description(new QPlainTextEdit(this))
    , m_labels(new QListWidget(this))
    , m_complete(new QPushButton(QIcon::fromTheme(QStringLiteral("task-complete")), i18nc("@action:button", "Complete"), this))
    , m_cancelTask(new QPushButton(QIcon::fromTheme(QStringLiteral("dialog-cancel")), i18nc("@action:button", "Cancel task"), this))
    , m_postpone(new QPushButton(QIcon::fromTheme(QStringLiteral("chronometer")), i18nc("@action:button", "Postpone"), this))
{
    setObjectName(QStringLiteral("taskDialog"));
    setWindowTitle(i18nc("@title:window", "Task"));

    auto *layout = new QFormLayout(this);
    layout->addRow(i18n("Title:"), m_title);
    layout->addRow(i18n("Description:"), m_description);
    layout->addRow(i18n("Labels:"), m_labels);
    auto *stateRow = new QHBoxLayout;
    stateRow->addWidget(m_complete);
    stateRow->addWidget(m_postpone);
    stateRow->addWidget(m_cancelTask);
    stateRow->addStretch();
    layout->addRow(stateRow);
    for (QPushButton *button : {m_complete, m_postpone, m_cancelTask}) {
        button->setAutoDefault(false); // Enter in the title must not change the state
    }

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
            postpone(postponeUntil(kind, m_hooks.now()));
        });
    }
    postponeMenu->addSeparator();
    m_postponeCustom = postponeMenu->addAction(i18nc("@action postpone", "Custom…"));
    connect(m_postponeCustom, &QAction::triggered, this, [this] {
        postpone(m_hooks.askPostponeTime());
    });
    m_postpone->setMenu(postponeMenu);

    connect(m_complete, &QPushButton::clicked, this, [this] {
        const QString id = m_taskId;
        if (applyEdits()) {
            m_store->complete({id});
            QDialog::accept();
        }
    });
    connect(m_cancelTask, &QPushButton::clicked, this, [this] {
        const QString id = m_taskId;
        if (applyEdits()) {
            m_store->cancel({id});
            QDialog::accept();
        }
    });

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    layout->addRow(buttons);
    connect(buttons, &QDialogButtonBox::accepted, this, &TaskDialog::accept);
    connect(buttons, &QDialogButtonBox::rejected, this, &TaskDialog::reject);

    const Task *task = store->task(taskId);
    if (task) {
        m_title->setText(task->title);
        m_description->setPlainText(task->description);
        m_complete->setEnabled(task->state != TaskState::Completed);
        m_cancelTask->setEnabled(task->state != TaskState::Cancelled);
        m_postpone->setEnabled(task->state == TaskState::Active || task->state == TaskState::Postponed);
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

bool TaskDialog::applyEdits()
{
    if (m_title->text().trimmed().isEmpty()) {
        m_title->setFocus();
        return false; // a task needs a title
    }
    QStringList labelIds;
    for (int i = 0; i < m_labels->count(); ++i) {
        if (m_labels->item(i)->checkState() == Qt::Checked) {
            labelIds.append(m_labels->item(i)->data(Qt::UserRole).toString());
        }
    }
    // Copy the id: applying the edit refreshes the views.
    const QString id = m_taskId;
    if (!m_store->task(id)) {
        return false;
    }
    m_store->updateTask(id, m_title->text(), m_description->toPlainText());
    m_store->setTaskLabels(id, labelIds);
    return true;
}

void TaskDialog::postpone(const QDateTime &until)
{
    const QString id = m_taskId;
    if (until.isValid() && applyEdits()) {
        m_store->postpone({id}, until);
        QDialog::accept();
    }
}

void TaskDialog::accept()
{
    if (applyEdits()) {
        QDialog::accept();
    }
}
