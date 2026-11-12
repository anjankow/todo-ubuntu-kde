#include "labelsdialog.h"

#include "core/palette.h"
#include "core/todostore.h"

#include <KLocalizedString>

#include <QButtonGroup>
#include <QDialogButtonBox>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QToolButton>
#include <QVBoxLayout>

LabelsDialog::LabelsDialog(TodoStore *store, QWidget *parent)
    : QDialog(parent)
    , m_store(store)
    , m_list(new QListWidget(this))
    , m_name(new QLineEdit(this))
    , m_colors(new QButtonGroup(this))
    , m_add(new QPushButton(i18nc("@action:button", "Add"), this))
    , m_save(new QPushButton(i18nc("@action:button", "Save changes"), this))
    , m_delete(new QPushButton(i18nc("@action:button", "Delete"), this))
{
    setObjectName(QStringLiteral("labelsDialog"));
    setWindowTitle(i18nc("@title:window", "Labels"));
    m_name->setPlaceholderText(i18n("Label name"));

    auto *layout = new QVBoxLayout(this);
    layout->addWidget(m_list);
    layout->addWidget(m_name);

    auto *grid = new QGridLayout;
    const QVector<QColor> &palette = labelPalette();
    for (int i = 0; i < palette.size(); ++i) {
        auto *button = new QToolButton(this);
        button->setCheckable(true);
        button->setFixedSize(28, 28);
        button->setStyleSheet(QStringLiteral("QToolButton { background: %1; border: 2px solid transparent; border-radius: 4px; }"
                                             "QToolButton:checked { border-color: palette(text); }")
                                  .arg(palette.at(i).name()));
        m_colors->addButton(button, i);
        grid->addWidget(button, i / 6, i % 6);
    }
    m_colors->setExclusive(true);
    m_colors->button(0)->setChecked(true);
    layout->addLayout(grid);

    auto *row = new QHBoxLayout;
    row->addWidget(m_add);
    row->addWidget(m_save);
    row->addWidget(m_delete);
    layout->addLayout(row);

    auto *buttons = new QDialogButtonBox(QDialogButtonBox::Close, this);
    layout->addWidget(buttons);
    connect(buttons, &QDialogButtonBox::rejected, this, &QDialog::reject);

    connect(m_list, &QListWidget::currentRowChanged, this, &LabelsDialog::showSelected);
    connect(m_add, &QPushButton::clicked, this, [this] {
        const QString id = m_store->addLabel(m_name->text(), m_colors->checkedId());
        if (!id.isEmpty()) {
            m_name->clear();
        }
    });
    connect(m_save, &QPushButton::clicked, this, [this] {
        m_store->updateLabel(selectedId(), m_name->text(), m_colors->checkedId());
    });
    connect(m_delete, &QPushButton::clicked, this, [this] {
        m_store->removeLabel(selectedId());
    });
    connect(m_store, &TodoStore::changed, this, &LabelsDialog::refresh);
    refresh();
}

QString LabelsDialog::selectedId() const
{
    const QListWidgetItem *item = m_list->currentItem();
    return item ? item->data(Qt::UserRole).toString() : QString();
}

void LabelsDialog::refresh()
{
    const QString selected = selectedId();
    m_list->clear();
    for (const Label &label : m_store->labels()) {
        auto *item = new QListWidgetItem(label.name, m_list);
        QPixmap swatch(12, 12);
        swatch.fill(labelPalette().at(label.color));
        item->setIcon(QIcon(swatch));
        item->setData(Qt::UserRole, label.id);
        if (label.id == selected) {
            m_list->setCurrentItem(item);
        }
    }
    showSelected();
}

void LabelsDialog::showSelected()
{
    const Label *label = m_store->label(selectedId());
    m_save->setEnabled(label != nullptr);
    m_delete->setEnabled(label != nullptr);
    if (label) {
        m_name->setText(label->name);
        m_colors->button(label->color)->setChecked(true);
    }
}
