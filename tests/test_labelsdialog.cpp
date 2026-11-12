#include "core/palette.h"
#include "core/todostore.h"
#include "ui/labelsdialog.h"

#include <QAbstractButton>
#include <QButtonGroup>
#include <QLineEdit>
#include <QListWidget>
#include <QPushButton>
#include <QTest>

class TestLabelsDialog : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void offersTwelveColors()
    {
        TodoStore store;
        LabelsDialog dialog(&store);
        QCOMPARE(dialog.colorButtons()->buttons().size(), 12);
        QCOMPARE(labelPalette().size(), 12);
    }

    void addLabelWithNameAndColor()
    {
        TodoStore store;
        LabelsDialog dialog(&store);
        dialog.nameEdit()->setText("Work");
        dialog.colorButtons()->button(4)->click();
        dialog.addButton()->click();

        QCOMPARE(store.labels().size(), 1);
        QCOMPARE(store.labels().at(0).name, QString("Work"));
        QCOMPARE(store.labels().at(0).color, 4);
        QCOMPARE(dialog.labelList()->count(), 1);
        QCOMPARE(dialog.labelList()->item(0)->text(), QString("Work"));
    }

    void emptyNameIsNotAdded()
    {
        TodoStore store;
        LabelsDialog dialog(&store);
        dialog.nameEdit()->setText("  ");
        dialog.addButton()->click();
        QVERIFY(store.labels().isEmpty());
    }

    void editAndDeleteLabel()
    {
        TodoStore store;
        const QString id = store.addLabel("Work", 1);
        LabelsDialog dialog(&store);
        QVERIFY(!dialog.saveButton()->isEnabled());

        dialog.labelList()->setCurrentRow(0);
        QCOMPARE(dialog.nameEdit()->text(), QString("Work"));
        QCOMPARE(dialog.colorButtons()->checkedId(), 1);

        dialog.nameEdit()->setText("Job");
        dialog.colorButtons()->button(9)->click();
        dialog.saveButton()->click();
        QCOMPARE(store.label(id)->name, QString("Job"));
        QCOMPARE(store.label(id)->color, 9);

        dialog.labelList()->setCurrentRow(0);
        dialog.deleteButton()->click();
        QVERIFY(store.labels().isEmpty());
        QCOMPARE(dialog.labelList()->count(), 0);
    }
};

QTEST_MAIN(TestLabelsDialog)
#include "test_labelsdialog.moc"
