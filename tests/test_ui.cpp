#include "testenv.h"

#include "app/application.h"

#include <KLocalizedString>

#include <QClipboard>
#include <QQmlApplicationEngine>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTest>

namespace
{
QQuickItem *findItem(QQuickItem *parent, const QString &name)
{
    if (parent->objectName() == name) {
        return parent;
    }
    const QList<QQuickItem *> children = parent->childItems();
    for (QQuickItem *child : children) {
        if (QQuickItem *found = findItem(child, name)) {
            return found;
        }
    }
    return nullptr;
}

// The application window on a temporary folder, driven like a user would.
struct Ui : TestEnv {
    std::unique_ptr<QQmlApplicationEngine> engine;
    QQuickWindow *window = nullptr;

    Ui() { openWindow(); }
    ~Ui() override { engine.reset(); }

    void open() override
    {
        engine.reset(); // the window goes before its controller
        TestEnv::open();
    }

    void openWindow()
    {
        engine = std::make_unique<QQmlApplicationEngine>();
        QVERIFY(Application::load(*engine, app.get()));
        window = qobject_cast<QQuickWindow *>(engine->rootObjects().constFirst());
        QVERIFY(window);
        window->show();
        window->requestActivate();
        QVERIFY(QTest::qWaitForWindowActive(window));
    }

    void restart()
    {
        open();
        openWindow();
    }

    // An item currently shown in the window (also inside open dialogs and menus).
    QQuickItem *item(const QString &name) const { return findItem(window->contentItem(), name); }
    QQuickItem *row(const QString &taskId) const { return item("task-" + taskId); }
    // A dialog or other object declared in the window, open or not.
    QObject *object(const QString &name) const { return window->findChild<QObject *>(name); }
    QQuickItem *input() const { return item("newTaskInput"); }

    static QPoint center(QQuickItem *item) { return item->mapToScene(QPointF(item->width() / 2, item->height() / 2)).toPoint(); }
    // A point on the task's title, away from its checkbox and drag handle.
    QPoint onTitle(const QString &taskId) const { return center(item("title-" + taskId)); }

    void click(QQuickItem *item)
    {
        QVERIFY(item);
        QTest::qWait(30); // let a just-created item get its place
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, center(item));
    }
    void click(const QString &name)
    {
        QTRY_VERIFY2(item(name), qPrintable(name));
        click(item(name));
    }
    // Opens a postpone menu with its button and picks an entry.
    void pickFromMenu(const QString &button, const QString &menu, const QString &entry)
    {
        click(button);
        QTRY_VERIFY(object(menu)->property("opened").toBool());
        click(entry);
    }
    void clickTask(const QString &taskId) { QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, onTitle(taskId)); }
    void doubleClickTask(const QString &taskId)
    {
        clickTask(taskId);
        QTest::mouseDClick(window, Qt::LeftButton, Qt::NoModifier, onTitle(taskId));
    }
    void tick(const QString &taskId) { click("check-" + taskId); }
    void hover(const QString &taskId)
    {
        moveMouse(onTitle(taskId) - QPoint(3, 0));
        moveMouse(onTitle(taskId));
    }
    // A pointer move without a button, as the window system would send it.
    void moveMouse(const QPoint &point)
    {
        QMouseEvent move(QEvent::MouseMove, point, window->mapToGlobal(point), Qt::NoButton, Qt::NoButton, Qt::NoModifier);
        QCoreApplication::sendEvent(window, &move);
    }
    void key(int key, Qt::KeyboardModifiers modifiers = Qt::NoModifier) { QTest::keyClick(window, Qt::Key(key), modifiers); }
    void type(const QString &text)
    {
        for (const QChar ch : text) {
            QTest::keyClick(window, ch.toLatin1());
        }
    }
    void typeTask(const QString &title)
    {
        type(title);
        key(Qt::Key_Return);
    }
    void showTab(AppController::Tab tab) { app->setCurrentTab(tab); }

    QString inputText() const { return input()->property("text").toString(); }
    bool isOpen(const QString &dialog) const { return object(dialog)->property("opened").toBool(); }
    bool isClosed(const QString &dialog) const { return !object(dialog)->property("visible").toBool(); }
    QString highlighted() const { return list()->currentId(); }
};
}

class TestUi : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    // --- creating tasks ---

    void inputIsAboveTheListWithMargins()
    {
        Ui ui;
        ui.add("A");
        QQuickItem *input = ui.input();
        QQuickItem *list = ui.item("activeList");
        QQuickItem *tabs = ui.item("tabBar");
        QVERIFY(input && list && tabs);
        const qreal inputTop = input->mapToScene(QPointF(0, 0)).y();
        const qreal inputBottom = inputTop + input->height();
        const qreal tabsBottom = tabs->mapToScene(QPointF(0, tabs->height())).y();
        const qreal listTop = list->mapToScene(QPointF(0, 0)).y();
        QVERIFY(inputTop - tabsBottom >= 6); // margin above the input
        QVERIFY(listTop - inputBottom >= 6); // margin below it, then the list
    }

    void typingAndEnterAddsTaskOnTopAndKeepsInputFocused()
    {
        Ui ui;
        ui.add("Old");
        QVERIFY(ui.input()->hasActiveFocus()); // ready for typing right away
        ui.type("First");
        QVERIFY(ui.store()->tasks().size() == 1); // typing alone does not create it
        ui.key(Qt::Key_Return);
        QCOMPARE(ui.shown(), QStringList({"First", "Old"}));
        QVERIFY(ui.inputText().isEmpty());
        QVERIFY(ui.input()->hasActiveFocus());

        ui.typeTask("Second"); // straight away
        QCOMPARE(ui.shown(), QStringList({"Second", "First", "Old"}));
        QCOMPARE(ui.savedTitles(), QStringList({"Second", "First", "Old"}));
        QVERIFY(ui.row(ui.store()->tasks().at(0).id)); // and it is on the list
    }

    void singleLetterIsATask()
    {
        Ui ui;
        ui.typeTask("x");
        QCOMPARE(ui.shown(), QStringList({"x"}));
    }

    void descriptionIsEnteredWithShiftEnter()
    {
        Ui ui;
        ui.type("Call mum");
        ui.key(Qt::Key_Return, Qt::ShiftModifier);
        QVERIFY(ui.store()->tasks().isEmpty()); // not finalized yet
        ui.type("about Sunday");
        ui.key(Qt::Key_Return, Qt::ShiftModifier);
        ui.type("and the cake");
        ui.key(Qt::Key_Return);
        QCOMPARE(ui.store()->tasks().size(), 1);
        QCOMPARE(ui.store()->tasks().at(0).title, QString("Call mum"));
        QCOMPARE(ui.store()->tasks().at(0).description, QString("about Sunday\nand the cake"));
        QCOMPARE(ui.shown(), QStringList({"Call mum"})); // the list shows the title only
    }

    void emptyInputCreatesNoTask()
    {
        Ui ui;
        ui.key(Qt::Key_Return);
        ui.type("   ");
        ui.key(Qt::Key_Return);
        QVERIFY(ui.store()->tasks().isEmpty());
        QVERIFY(ui.inputText().isEmpty());
    }

    void unfinishedTextStaysAsDraftOnFocusLoss()
    {
        Ui ui;
        ui.type("half typed");
        ui.click("labelsButton"); // the dialog takes the focus
        QTRY_VERIFY(ui.isOpen("labelsDialog"));
        QVERIFY(!ui.input()->hasActiveFocus());
        QVERIFY(ui.store()->tasks().isEmpty());
        QCOMPARE(ui.inputText(), QString("half typed"));
    }

    void clearedInputIsDroppedOnFocusLoss()
    {
        Ui ui;
        ui.type("ab");
        ui.key(Qt::Key_Backspace);
        ui.key(Qt::Key_Backspace);
        ui.key(Qt::Key_Return, Qt::ShiftModifier); // only a blank line is left
        QVERIFY(!ui.inputText().isEmpty());
        ui.click("labelsButton");
        QTRY_VERIFY(ui.isOpen("labelsDialog"));
        QVERIFY(ui.inputText().isEmpty());
        QVERIFY(ui.store()->tasks().isEmpty());
    }

    // --- highlight and task card ---

    void oneClickHighlightsTwoClicksOpenTheCard()
    {
        Ui ui;
        const QString a = ui.add("A");
        const QString b = ui.add("Title", "The description");
        ui.clickTask(b);
        QCOMPARE(ui.highlighted(), b);
        QVERIFY(ui.row(b)->property("highlighted").toBool());
        QVERIFY(!ui.row(a)->property("highlighted").toBool());
        QTest::qWait(50);
        QVERIFY(ui.isClosed("taskCard")); // one click does not open it

        ui.doubleClickTask(b);
        QTRY_VERIFY(ui.isOpen("taskCard"));
        QCOMPARE(ui.item("cardTitle")->property("text").toString(), QString("Title"));
        QCOMPARE(ui.item("cardDescription")->property("text").toString(), QString("The description"));
    }

    void enterOpensTheCardOfTheHighlightedTask()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.key(Qt::Key_Return); // nothing highlighted, nothing typed
        QTest::qWait(50);
        QVERIFY(ui.isClosed("taskCard"));
        ui.clickTask(a);
        ui.key(Qt::Key_Return);
        QTRY_VERIFY(ui.isOpen("taskCard"));
        QCOMPARE(ui.item("cardTitle")->property("text").toString(), QString("A"));
        QCOMPARE(ui.shown(), QStringList({"A"})); // and no task was created
    }

    void arrowsMoveTheHighlightAndEscapeRemovesIt()
    {
        Ui ui;
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        ui.key(Qt::Key_Down);
        QCOMPARE(ui.highlighted(), a);
        ui.key(Qt::Key_Down);
        QCOMPARE(ui.highlighted(), b);
        QVERIFY(ui.row(b)->property("highlighted").toBool());
        ui.key(Qt::Key_Up);
        QCOMPARE(ui.highlighted(), a);
        ui.key(Qt::Key_Escape);
        QVERIFY(ui.highlighted().isEmpty());
        QVERIFY(!ui.row(a)->property("highlighted").toBool());

        ui.clickTask(b);
        ui.key(Qt::Key_Escape); // also without the input having the focus
        QVERIFY(ui.highlighted().isEmpty());
    }

    void cardEditsTask()
    {
        Ui ui;
        const QString work = ui.store()->addLabel("Work", 0);
        const QString id = ui.add("Title", "old");
        ui.doubleClickTask(id);
        QTRY_VERIFY(ui.isOpen("taskCard"));
        ui.item("cardTitle")->setProperty("text", "New title");
        ui.item("cardDescription")->setProperty("text", "new description");
        ui.click("cardLabel-" + work);
        ui.click("cardSave");
        QTRY_VERIFY(ui.isClosed("taskCard"));
        QCOMPARE(ui.store()->task(id)->title, QString("New title"));
        QCOMPARE(ui.store()->task(id)->description, QString("new description"));
        QCOMPARE(ui.store()->task(id)->labelIds, QStringList({work}));
        QCOMPARE(ui.savedTitles(), QStringList({"New title"}));
        QTRY_VERIFY(ui.input()->hasActiveFocus()); // back to typing

        ui.doubleClickTask(id); // the label can be taken off again
        QTRY_VERIFY(ui.isOpen("taskCard"));
        QVERIFY(ui.item("cardLabel-" + work)->property("active").toBool());
        ui.click("cardLabel-" + work);
        ui.click("cardSave");
        QVERIFY(ui.store()->task(id)->labelIds.isEmpty());
    }

    void cardRejectsEmptyTitleAndCloseKeepsTask()
    {
        Ui ui;
        const QString id = ui.add("Title", "desc");
        ui.doubleClickTask(id);
        QTRY_VERIFY(ui.isOpen("taskCard"));
        ui.item("cardTitle")->setProperty("text", "");
        ui.click("cardSave");
        QTest::qWait(50);
        QVERIFY(ui.isOpen("taskCard"));
        ui.click("cardComplete"); // the state buttons need a title too
        QVERIFY(ui.isOpen("taskCard"));
        QCOMPARE(ui.store()->task(id)->state, TaskState::Active);

        ui.item("cardTitle")->setProperty("text", "Changed");
        ui.click("cardClose");
        QTRY_VERIFY(ui.isClosed("taskCard"));
        QCOMPARE(ui.store()->task(id)->title, QString("Title"));
    }

    void cardCompletesAndCancelsTask()
    {
        Ui ui;
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        ui.doubleClickTask(a);
        QTRY_VERIFY(ui.isOpen("taskCard"));
        ui.item("cardTitle")->setProperty("text", "Edited"); // edits are kept
        ui.click("cardComplete");
        QTRY_VERIFY(ui.isClosed("taskCard"));
        QCOMPARE(ui.store()->task(a)->state, TaskState::Completed);
        QCOMPARE(ui.shown(TaskState::Completed), QStringList({"Edited"}));

        ui.doubleClickTask(b);
        QTRY_VERIFY(ui.isOpen("taskCard"));
        ui.click("cardCancelTask");
        QTRY_VERIFY(ui.isClosed("taskCard"));
        QCOMPARE(ui.store()->task(b)->state, TaskState::Cancelled);
    }

    void cardPostponesTask()
    {
        Ui ui;
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        ui.doubleClickTask(a);
        QTRY_VERIFY(ui.isOpen("taskCard"));
        ui.pickFromMenu("cardPostpone", "cardPostponeMenu", "postponeOneWeek");
        QTRY_VERIFY(ui.isClosed("taskCard"));
        QCOMPARE(ui.store()->task(a)->state, TaskState::Postponed);
        QCOMPARE(ui.store()->task(a)->postponedUntil, QDate(2026, 11, 19).startOfDay());

        ui.doubleClickTask(b); // custom time
        QTRY_VERIFY(ui.isOpen("taskCard"));
        ui.pickFromMenu("cardPostpone", "cardPostponeMenu", "postponeCustom");
        QTRY_VERIFY(ui.isOpen("postponeDialog"));
        QCOMPARE(ui.item("postponeDay")->property("value").toInt(), 13); // suggests tomorrow
        ui.item("postponeYear")->setProperty("value", 2027);
        ui.item("postponeMonth")->setProperty("value", 1);
        ui.item("postponeDay")->setProperty("value", 2);
        ui.item("postponeHour")->setProperty("value", 9);
        ui.item("postponeMinute")->setProperty("value", 30);
        QMetaObject::invokeMethod(ui.object("postponeDialog"), "accept");
        QTRY_VERIFY(ui.isClosed("taskCard"));
        QCOMPARE(ui.store()->task(b)->postponedUntil, QDateTime(QDate(2027, 1, 2), QTime(9, 30)));
    }

    void cardStateButtonsFollowTheTaskState()
    {
        Ui ui;
        const QString done = ui.add("Done");
        ui.app->complete({done});
        ui.showTab(AppController::CompletedTab);
        QTRY_VERIFY(ui.row(done));
        ui.doubleClickTask(done);
        QTRY_VERIFY(ui.isOpen("taskCard"));
        QVERIFY(!ui.item("cardComplete")->isEnabled());
        QVERIFY(ui.item("cardCancelTask")->isEnabled());
        QVERIFY(!ui.item("cardPostpone")->isEnabled());
    }

    // --- ticking and the toolbar ---

    void checkboxTicksWithoutHighlightingOrOpening()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.add("B");
        QVERIFY(!ui.item("completeButton")->isEnabled());
        ui.tick(a);
        QCOMPARE(ui.list()->checkedIds(), QStringList({a}));
        QVERIFY(ui.highlighted().isEmpty());
        QVERIFY(ui.isClosed("taskCard"));
        QVERIFY(ui.item("completeButton")->isEnabled());
        ui.tick(a);
        QCOMPARE(ui.list()->checkedCount(), 0);
        QVERIFY(!ui.item("completeButton")->isEnabled());
    }

    void toolbarCompletesAndCancelsTickedTasks()
    {
        Ui ui;
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        const QString c = ui.add("C");
        ui.add("D");
        ui.tick(a);
        ui.tick(c);
        ui.click("completeButton");
        QCOMPARE(ui.shown(), QStringList({"B", "D"}));
        QCOMPARE(ui.shown(TaskState::Completed), QStringList({"A", "C"}));
        QTRY_VERIFY(ui.row(b));
        ui.tick(b);
        ui.click("cancelButton");
        QCOMPARE(ui.shown(TaskState::Cancelled), QStringList({"B"}));
    }

    void toolbarPostponesTickedTasks()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.tick(a);
        ui.pickFromMenu("postponeButton", "postponeMenu", "postponeThreeDays");
        QTRY_COMPARE(ui.store()->task(a)->state, TaskState::Postponed);
        QCOMPARE(ui.store()->task(a)->postponedUntil, QDate(2026, 11, 15).startOfDay());
    }

    void toolbarRestoresFromOtherTabs()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.app->cancel({a});
        QVERIFY(!ui.item("restoreButton")->isEnabled());
        ui.click("tab-cancelled");
        QTRY_COMPARE(ui.app->currentTab(), int(AppController::CancelledTab));
        QTRY_VERIFY(ui.row(a));
        ui.tick(a);
        QVERIFY(ui.item("restoreButton")->isEnabled());
        QVERIFY(!ui.item("cancelButton")->isEnabled());
        ui.click("restoreButton");
        QCOMPARE(ui.shown(), QStringList({"A"}));
    }

    void batchDeleteAsksForConfirmation()
    {
        Ui ui;
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        ui.add("C");
        ui.tick(a);
        ui.tick(b);
        ui.click("deleteButton");
        QTRY_VERIFY(ui.isOpen("confirmDeleteDialog"));
        QCOMPARE(ui.shown().size(), 3);
        ui.click("rejectDeleteButton");
        QTRY_VERIFY(ui.isClosed("confirmDeleteDialog"));
        QCOMPARE(ui.shown().size(), 3);

        ui.click("deleteButton");
        QTRY_VERIFY(ui.isOpen("confirmDeleteDialog"));
        ui.click("confirmDeleteButton");
        QTRY_VERIFY(ui.isClosed("confirmDeleteDialog"));
        QCOMPARE(ui.shown(), QStringList({"C"}));
        QCOMPARE(ui.savedTitles(), QStringList({"C"}));
    }

    void singleDeleteDoesNotAsk()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.add("B");
        ui.tick(a);
        ui.click("deleteButton");
        QCOMPARE(ui.shown(), QStringList({"B"}));
        QVERIFY(ui.isClosed("confirmDeleteDialog"));
    }

    void tabsShowCountsAndTheirTasks()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.add("B");
        ui.app->complete({a});
        QStringList texts;
        for (const char *tab : {"tab-active", "tab-postponed", "tab-completed", "tab-cancelled"}) {
            QVERIFY(ui.item(tab));
            texts.append(ui.item(tab)->property("text").toString());
        }
        QCOMPARE(texts, QStringList({"Tasks (1)", "Postponed (0)", "Completed (1)", "Cancelled (0)"}));
        QVERIFY(ui.item("completedList"));
        QVERIFY(ui.item("postponedList"));
        QVERIFY(ui.item("cancelledList"));
    }

    // --- keyboard ---

    void ctrlATicksAllTasks()
    {
        Ui ui;
        ui.add("A");
        ui.add("B");
        ui.add("C");
        ui.key(Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(ui.list()->checkedCount(), 3);
        QVERIFY(ui.item("completeButton")->isEnabled());
    }

    void ctrlAWithTextInTheInputSelectsTheText()
    {
        Ui ui;
        ui.add("A");
        ui.type("draft");
        ui.key(Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(ui.list()->checkedCount(), 0);
        QCOMPARE(ui.input()->property("selectedText").toString(), QString("draft"));
    }

    void deleteKeyCancelsHighlightedOrTickedTasks()
    {
        Ui ui;
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        const QString c = ui.add("C");
        ui.clickTask(a);
        ui.key(Qt::Key_Delete);
        QCOMPARE(ui.store()->task(a)->state, TaskState::Cancelled);
        QTRY_VERIFY(ui.row(b));
        ui.tick(b);
        ui.tick(c);
        ui.key(Qt::Key_Delete);
        QCOMPARE(ui.shown(TaskState::Cancelled), QStringList({"A", "B", "C"}));
        QCOMPARE(ui.savedTitles().size(), 3); // cancelled, not deleted
    }

    void ctrlDDuplicatesHighlightedOrTickedTasks()
    {
        Ui ui;
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        ui.clickTask(a);
        ui.key(Qt::Key_D, Qt::ControlModifier);
        QCOMPARE(ui.shown(), QStringList({"A", "B", "A"}));
        QTRY_VERIFY(ui.row(b));
        ui.tick(a);
        ui.tick(b);
        ui.key(Qt::Key_D, Qt::ControlModifier);
        QCOMPARE(ui.shown(), QStringList({"A", "B", "A", "A", "B"}));
    }

    void ctrlCAndCtrlVDoNotDuplicate()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.clickTask(a);
        QGuiApplication::clipboard()->setText("from elsewhere");
        ui.key(Qt::Key_C, Qt::ControlModifier);
        ui.key(Qt::Key_V, Qt::ControlModifier); // an ordinary paste into the input
        QCOMPARE(ui.shown(), QStringList({"A"}));
        QCOMPARE(ui.inputText(), QString("from elsewhere"));
    }

    void escapeLeavesTheInputAndKeysGoToTheTasks()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.add("B");
        ui.type("draft");
        QVERIFY(ui.input()->hasActiveFocus());
        ui.key(Qt::Key_Escape);
        QVERIFY(!ui.input()->hasActiveFocus());
        QCOMPARE(ui.inputText(), QString("draft")); // the draft stays

        ui.key(Qt::Key_X); // typing no longer reaches the input
        QCOMPARE(ui.inputText(), QString("draft"));
        ui.key(Qt::Key_Down); // the task keys work although a draft is there
        QCOMPARE(ui.highlighted(), a);
        ui.key(Qt::Key_D, Qt::ControlModifier);
        QCOMPARE(ui.shown(), QStringList({"A", "B", "A"}));
        ui.key(Qt::Key_Delete);
        QCOMPARE(ui.store()->task(a)->state, TaskState::Cancelled);
        ui.key(Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(ui.list()->checkedCount(), 2);

        ui.click("newTaskInput"); // a click brings the input back
        QVERIFY(ui.input()->hasActiveFocus());
    }

    void toolbarActsOnTheHighlightedTask()
    {
        Ui ui;
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        const QString c = ui.add("C");
        QVERIFY(!ui.item("completeButton")->isEnabled());
        ui.clickTask(a);
        QVERIFY(ui.item("completeButton")->isEnabled());
        QVERIFY(ui.item("deleteButton")->isEnabled());
        ui.click("completeButton");
        QCOMPARE(ui.store()->task(a)->state, TaskState::Completed);

        ui.clickTask(b);
        ui.click("cancelButton");
        QCOMPARE(ui.store()->task(b)->state, TaskState::Cancelled);

        ui.clickTask(c);
        ui.pickFromMenu("postponeButton", "postponeMenu", "postponeOneWeek");
        QTRY_COMPARE(ui.store()->task(c)->state, TaskState::Postponed);

        ui.click("tab-cancelled");
        QTRY_VERIFY(ui.row(b));
        ui.clickTask(b);
        ui.click("restoreButton");
        QCOMPARE(ui.store()->task(b)->state, TaskState::Active);
        ui.click("tab-active");
        QTRY_VERIFY(ui.row(b));
        ui.clickTask(b);
        ui.click("deleteButton"); // one task: deleted at once
        QVERIFY(!ui.store()->task(b));
    }

    void reorderWithCtrlArrows()
    {
        Ui ui;
        ui.add("A");
        ui.add("B");
        const QString c = ui.add("C");
        ui.clickTask(c);
        ui.key(Qt::Key_Up, Qt::ControlModifier);
        QCOMPARE(ui.shown(), QStringList({"A", "C", "B"}));
        QCOMPARE(ui.highlighted(), c);
        ui.key(Qt::Key_Up, Qt::ControlModifier);
        ui.key(Qt::Key_Up, Qt::ControlModifier); // already first
        QCOMPARE(ui.shown(), QStringList({"C", "A", "B"}));
        ui.key(Qt::Key_Down, Qt::ControlModifier);
        QCOMPARE(ui.savedTitles(), QStringList({"A", "C", "B"}));
    }

    void reorderByDragging()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.add("B");
        const QString c = ui.add("C");
        QQuickItem *handle = ui.item("handle-" + c);
        QVERIFY(handle);
        QVERIFY(handle->isVisible());
        const QPoint from = Ui::center(handle);
        const QPoint to(from.x(), Ui::center(ui.row(a)).y() - 4);
        QTest::mousePress(ui.window, Qt::LeftButton, Qt::NoModifier, from);
        for (int y = from.y(); y >= to.y(); y -= 6) {
            QTest::mouseMove(ui.window, QPoint(from.x(), y), 10);
        }
        QTest::mouseMove(ui.window, to, 10);
        QTest::mouseRelease(ui.window, Qt::LeftButton, Qt::NoModifier, to, 10);
        QTRY_COMPARE(ui.shown(), QStringList({"C", "A", "B"}));
        QCOMPARE(ui.savedTitles(), QStringList({"C", "A", "B"}));
        QVERIFY(ui.isClosed("taskCard"));
    }

    void onlyTheTasksTabHasDragHandles()
    {
        Ui ui;
        const QString done = ui.add("Done");
        ui.app->complete({done});
        ui.showTab(AppController::CompletedTab);
        QTRY_VERIFY(ui.row(done));
        QVERIFY(!ui.item("handle-" + done)->isVisible());
    }

    // --- quick complete ---

    void hoveringShowsCheckmarkThatCompletes()
    {
        Ui ui;
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        QQuickItem *button = ui.item("complete-" + a);
        QVERIFY(button);
        QVERIFY(!button->isVisible());
        ui.hover(a);
        QTRY_VERIFY(button->isVisible());
        QVERIFY(!ui.item("complete-" + b)->isVisible()); // only on the hovered task

        // It sits at the end of the line, over the title.
        QQuickItem *row = ui.row(a);
        QQuickItem *title = ui.item("title-" + a);
        const QRectF buttonRect = button->mapRectToScene(QRectF(0, 0, button->width(), button->height()));
        const QRectF titleRect = title->mapRectToScene(QRectF(0, 0, title->width(), title->height()));
        QVERIFY(buttonRect.intersects(titleRect));
        QVERIFY(buttonRect.right() > row->mapToScene(QPointF(row->width(), 0)).x() - 40);

        ui.click(button);
        QCOMPARE(ui.store()->task(a)->state, TaskState::Completed);
        QCOMPARE(ui.shown(), QStringList({"B"}));
    }

    void checkmarkAppearsOnlyOnTheTasksTab()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.app->cancel({a});
        ui.showTab(AppController::CancelledTab);
        QTRY_VERIFY(ui.row(a));
        ui.hover(a);
        QTest::qWait(100);
        QVERIFY(!ui.item("complete-" + a)->isVisible());
    }

    // --- labels ---

    void clickingLabelFilters()
    {
        Ui ui;
        const QString work = ui.store()->addLabel("Work", 0);
        const QString w = ui.add("W");
        ui.add("None");
        ui.store()->setTaskLabels(w, {work});
        QTRY_VERIFY(ui.item("filter-" + work));
        QCOMPARE(ui.item("filter-" + work)->property("text").toString(), QString("Work"));
        ui.click("filter-" + work);
        QCOMPARE(ui.shown(), QStringList({"W"}));
        QTRY_VERIFY(ui.item("filter-" + work)->property("active").toBool());
        ui.click("filter-" + work);
        QCOMPARE(ui.shown(), QStringList({"W", "None"}));
    }

    void clickingLabelWithTickedTasksLabelsThem()
    {
        Ui ui;
        const QString work = ui.store()->addLabel("Work", 0);
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        QTRY_VERIFY(ui.item("filter-" + work));
        ui.tick(a);
        ui.tick(b);
        ui.click("filter-" + work);
        QCOMPARE(ui.store()->task(a)->labelIds, QStringList({work}));
        QCOMPARE(ui.store()->task(b)->labelIds, QStringList({work}));
        QVERIFY(ui.app->labelFilter().isEmpty());
        QTRY_VERIFY(ui.item("filter-" + work));
        ui.click("filter-" + work); // all have it: removed
        QVERIFY(ui.store()->task(a)->labelIds.isEmpty());
    }

    void labelsDialogDefinesLabels()
    {
        Ui ui;
        ui.click("labelsButton");
        QTRY_VERIFY(ui.isOpen("labelsDialog"));
        for (int i = 0; i < 12; ++i) {
            QVERIFY(ui.item(QString("color-%1").arg(i)));
        }
        QVERIFY(!ui.item("color-12")); // a choice of 12 colors

        ui.item("labelName")->setProperty("text", "Work");
        ui.click("color-4");
        ui.click("labelAdd");
        QCOMPARE(ui.store()->labels().size(), 1);
        QCOMPARE(ui.store()->labels().at(0).name, QString("Work"));
        QCOMPARE(ui.store()->labels().at(0).color, 4);

        const QString id = ui.store()->labels().at(0).id;
        QTRY_VERIFY(ui.item("label-" + id));
        QVERIFY(!ui.item("labelSave")->isEnabled());
        ui.click("label-" + id); // select it to edit
        QCOMPARE(ui.item("labelName")->property("text").toString(), QString("Work"));
        ui.item("labelName")->setProperty("text", "Job");
        ui.click("color-9");
        ui.click("labelSave");
        QCOMPARE(ui.store()->label(id)->name, QString("Job"));
        QCOMPARE(ui.store()->label(id)->color, 9);

        ui.click("labelDelete");
        QVERIFY(ui.store()->labels().isEmpty());
        ui.click("labelsClose");
        QTRY_VERIFY(ui.isClosed("labelsDialog"));
    }

    // --- window ---

    void toolbarButtonsAllFitInTheInitialWindow()
    {
        Ui ui;
        const QStringList buttons = {"completeButton", "cancelButton", "postponeButton", "restoreButton", "deleteButton", "labelsButton", "storageButton"};
        qreal previousRight = 0;
        for (const QString &name : buttons) {
            QQuickItem *button = ui.item(name);
            QVERIFY2(button && button->isVisible(), qPrintable(name));
            const QRectF rect = button->mapRectToScene(QRectF(0, 0, button->width(), button->height()));
            QVERIFY2(rect.left() >= previousRight - 0.5, qPrintable(name)); // no overlap
            QVERIFY2(rect.right() <= ui.window->width(), qPrintable(name));
            QVERIFY2(button->width() >= button->implicitWidth() - 0.5, qPrintable(name)); // not squeezed
            previousRight = rect.right();
        }
    }

    void thereIsNoMenuBarOrHelpMenu()
    {
        Ui ui;
        QVERIFY(!ui.window->property("menuBar").value<QObject *>());
        QVERIFY(!ui.item("labelButton")); // labelling is done with the label chips
    }

    void tasksUseLargerFontAndSpacing()
    {
        Ui ui;
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        const int base = QGuiApplication::font().pointSize();
        QCOMPARE(ui.item("title-" + a)->property("font").value<QFont>().pointSize(), base + 2);
        QCOMPARE(ui.input()->property("font").value<QFont>().pointSize(), base + 2);
        const qreal firstBottom = ui.row(a)->mapToScene(QPointF(0, ui.row(a)->height())).y();
        const qreal secondTop = ui.row(b)->mapToScene(QPointF(0, 0)).y();
        QVERIFY(secondTop - firstBottom >= 6); // a gap between the tasks
    }

    void storageFolderConflictIsAsked()
    {
        QTemporaryDir other;
        TodoStore existing;
        existing.addTask("Existing");
        QVERIFY(Storage::save(existing, other.filePath("todos.json")));

        Ui ui;
        ui.add("Mine");
        QMetaObject::invokeMethod(ui.window, "chooseFolder", Q_ARG(QVariant, other.path()));
        QTRY_VERIFY(ui.isOpen("folderConflictDialog"));
        QCOMPARE(ui.shown(), QStringList({"Mine"}));
        ui.click("conflictCancelButton");
        QTRY_VERIFY(ui.isClosed("folderConflictDialog"));
        QCOMPARE(ui.app->storageDir(), ui.dir.path());

        QMetaObject::invokeMethod(ui.window, "chooseFolder", Q_ARG(QVariant, other.path()));
        QTRY_VERIFY(ui.isOpen("folderConflictDialog"));
        ui.click("conflictLoadButton");
        QTRY_VERIFY(ui.isClosed("folderConflictDialog"));
        QCOMPARE(ui.shown(), QStringList({"Existing"}));
        QCOMPARE(ui.app->storageDir(), other.path());
    }

    void storageFolderWithoutConflictMovesAtOnce()
    {
        Ui ui;
        ui.add("Task");
        QTemporaryDir other;
        QMetaObject::invokeMethod(ui.window, "chooseFolder", Q_ARG(QVariant, other.path()));
        QCOMPARE(ui.app->storageDir(), other.path());
        QVERIFY(QFile::exists(other.filePath("todos.json")));
        QVERIFY(ui.isClosed("folderConflictDialog"));
    }

    void unreadableFileShowsAnError()
    {
        Ui ui;
        ui.engine.reset();
        ui.app.reset();
        QFile file(ui.dir.filePath("todos.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("garbage");
        file.close();
        ui.restart();
        QQuickItem *message = ui.item("errorMessage");
        QVERIFY(message);
        QTRY_VERIFY(message->isVisible());
        QVERIFY(message->property("text").toString().contains("todos.json"));
    }

    void postponedTasksReturnWhenTheAppIsOpenedLater()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.tick(a);
        ui.pickFromMenu("postponeButton", "postponeMenu", "postponeTomorrow");
        QTRY_VERIFY(ui.shown().isEmpty());
        ui.now = fixedNow.addDays(1);
        ui.restart();
        QCOMPARE(ui.shown(), QStringList({"A"}));
        QTRY_VERIFY(ui.row(a));
    }
};

int main(int argc, char **argv)
{
    Application::prepare();
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("kdetodo");
    TestUi test;
    return QTest::qExec(&test, argc, argv);
}
#include "test_ui.moc"
