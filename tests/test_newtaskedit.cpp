#include "ui/newtaskedit.h"

#include <QSignalSpy>
#include <QTest>

class TestNewTaskEdit : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void enterCreatesTask()
    {
        NewTaskEdit edit;
        QSignalSpy spy(&edit, &NewTaskEdit::taskSubmitted);
        QTest::keyClicks(&edit, "Buy milk");
        QCOMPARE(spy.count(), 0); // typing alone does not finalize
        QTest::keyClick(&edit, Qt::Key_Return);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("Buy milk"));
        QVERIFY(spy.at(0).at(1).toString().isEmpty());
        QVERIFY(edit.toPlainText().isEmpty()); // ready for the next task
    }

    void singleLetterIsATask()
    {
        NewTaskEdit edit;
        QSignalSpy spy(&edit, &NewTaskEdit::taskSubmitted);
        QTest::keyClicks(&edit, "x");
        QTest::keyClick(&edit, Qt::Key_Return);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("x"));
    }

    void shiftEnterStartsDescription()
    {
        NewTaskEdit edit;
        QSignalSpy spy(&edit, &NewTaskEdit::taskSubmitted);
        QTest::keyClicks(&edit, "Call mum");
        QTest::keyClick(&edit, Qt::Key_Return, Qt::ShiftModifier);
        QCOMPARE(spy.count(), 0);
        QTest::keyClicks(&edit, "about Sunday");
        QTest::keyClick(&edit, Qt::Key_Return, Qt::ShiftModifier);
        QTest::keyClicks(&edit, "and the cake");
        QTest::keyClick(&edit, Qt::Key_Return);
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toString(), QString("Call mum"));
        QCOMPARE(spy.at(0).at(1).toString(), QString("about Sunday\nand the cake"));
    }

    void enterOnEmptyInputCreatesNothing()
    {
        NewTaskEdit edit;
        QSignalSpy spy(&edit, &NewTaskEdit::taskSubmitted);
        QTest::keyClick(&edit, Qt::Key_Return);
        QTest::keyClicks(&edit, "   ");
        QTest::keyClick(&edit, Qt::Key_Return);
        QCOMPARE(spy.count(), 0);
        QVERIFY(edit.toPlainText().isEmpty());
    }

    void clearedInputIsDroppedOnFocusLoss()
    {
        NewTaskEdit edit;
        QSignalSpy spy(&edit, &NewTaskEdit::taskSubmitted);
        QTest::keyClicks(&edit, "ab");
        QTest::keyClick(&edit, Qt::Key_Backspace);
        QTest::keyClick(&edit, Qt::Key_Backspace);
        QTest::keyClick(&edit, Qt::Key_Return, Qt::ShiftModifier); // only blank lines left
        QFocusEvent out(QEvent::FocusOut);
        QApplication::sendEvent(&edit, &out);
        QCOMPARE(spy.count(), 0);
        QVERIFY(edit.toPlainText().isEmpty());
    }

    void unfinishedTextStaysAsDraftOnFocusLoss()
    {
        NewTaskEdit edit;
        QSignalSpy spy(&edit, &NewTaskEdit::taskSubmitted);
        QTest::keyClicks(&edit, "half typed");
        QFocusEvent out(QEvent::FocusOut);
        QApplication::sendEvent(&edit, &out);
        QCOMPARE(spy.count(), 0);
        QCOMPARE(edit.toPlainText(), QString("half typed"));
        QTest::keyClick(&edit, Qt::Key_Return);
        QCOMPARE(spy.count(), 1);
    }

    void parse()
    {
        QString title;
        QString description;
        NewTaskEdit::parse("\n  Title \n\n desc \n", &title, &description);
        QCOMPARE(title, QString("Title"));
        QCOMPARE(description, QString("desc"));
        NewTaskEdit::parse(" \n ", &title, &description);
        QVERIFY(title.isEmpty());
        QVERIFY(description.isEmpty());
    }
};

QTEST_MAIN(TestNewTaskEdit)
#include "test_newtaskedit.moc"
