#include "testenv.h"

#include "app/application.h"

#include <KLocalizedString>

#include <QClipboard>
#include <QQmlApplicationEngine>
#include <QQmlProperty>
#include <QQuickItem>
#include <QQuickWindow>
#include <QStandardPaths>
#include <QTest>

namespace
{
QtMessageHandler g_defaultHandler = nullptr;
QStringList g_qmlProblems;

// Binding loops and other warnings of the application's own QML (qrc:/qml)
// make the running test fail; those of the libraries are only printed.
void collectQmlProblems(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    if (type != QtDebugMsg && type != QtInfoMsg && message.contains(QLatin1String("qrc:/qml/"))) {
        g_qmlProblems.append(message);
    }
    g_defaultHandler(type, context, message);
}
QByteArray readFile(const QString &path)
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly) ? file.readAll() : QByteArray();
}

QQuickItem *findItem(QQuickItem *parent, const QString &name, bool visibleOnly)
{
    if (visibleOnly && !parent->isVisible()) {
        return nullptr; // nothing below a hidden item is shown
    }
    if (parent->objectName() == name) {
        return parent;
    }
    const QList<QQuickItem *> children = parent->childItems();
    for (QQuickItem *child : children) {
        if (QQuickItem *found = findItem(child, name, visibleOnly)) {
            return found;
        }
    }
    return nullptr;
}

// The item the user sees under that name. A task has a row in the list of
// its tab only, but a row of another tab may still be around, hidden, right
// after the task changed its state: shown items win, hidden ones are a fallback.
QQuickItem *findItem(QQuickItem *root, const QString &name)
{
    QQuickItem *shown = findItem(root, name, true);
    return shown ? shown : findItem(root, name, false);
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
        QTRY_VERIFY(!object(menu)->property("visible").toBool()); // closed again, clicks reach the window
    }
    void clickTask(const QString &taskId)
    {
        QTRY_VERIFY(row(taskId)); // the lists are rebuilt after every change
        QTest::qWait(30); // and a new row needs a moment to get its place
        QTest::mouseClick(window, Qt::LeftButton, Qt::NoModifier, onTitle(taskId));
        QTRY_COMPARE(app->currentModel()->currentId(), taskId);
    }
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
    // Shift+Enter with nothing highlighted: the key way into the input.
    void focusInput()
    {
        key(Qt::Key_Return, Qt::ShiftModifier);
        QTRY_VERIFY(input()->hasActiveFocus());
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
    bool listHasFocus() const { return item("listKeys")->hasActiveFocus(); }
    // An entry of the Settings menu.
    void settings(const QString &entry) { pickFromMenu("settingsButton", "settingsMenu", entry); }
    QVariant buttonIcon(const QString &button, const char *part) const { return QQmlProperty::read(item(button), QString("icon.") + part); }
};
}

class TestUi : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        // Here and not in main(): QtTest installs its own handler when it starts.
        g_defaultHandler = qInstallMessageHandler(collectQmlProblems);
    }
    void init() { g_qmlProblems.clear(); }
    void cleanup()
    {
        const QStringList problems = g_qmlProblems;
        g_qmlProblems.clear();
        QVERIFY2(problems.isEmpty(), qPrintable(problems.join('\n')));
    }

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
        ui.focusInput();
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
        ui.focusInput();
        ui.typeTask("x");
        QCOMPARE(ui.shown(), QStringList({"x"}));
    }

    void descriptionIsEnteredWithShiftEnter()
    {
        Ui ui;
        ui.focusInput();
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
        ui.focusInput();
        ui.key(Qt::Key_Return);
        ui.type("   ");
        ui.key(Qt::Key_Return);
        QVERIFY(ui.store()->tasks().isEmpty());
        QVERIFY(ui.inputText().isEmpty());
    }

    void unfinishedTextStaysAsDraftOnFocusLoss()
    {
        Ui ui;
        ui.focusInput();
        ui.type("half typed");
        ui.settings("settingsLabels"); // the dialog takes the focus
        QTRY_VERIFY(ui.isOpen("labelsDialog"));
        QVERIFY(!ui.input()->hasActiveFocus());
        QVERIFY(ui.store()->tasks().isEmpty());
        QCOMPARE(ui.inputText(), QString("half typed"));
    }

    void clearedInputIsDroppedOnFocusLoss()
    {
        Ui ui;
        ui.focusInput();
        ui.type("ab");
        ui.key(Qt::Key_Backspace);
        ui.key(Qt::Key_Backspace);
        ui.key(Qt::Key_Return, Qt::ShiftModifier); // only a blank line is left
        QVERIFY(!ui.inputText().isEmpty());
        ui.settings("settingsLabels");
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
        QTRY_VERIFY(ui.listHasFocus()); // the keys go to the list again

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
        QVERIFY(!ui.item("cardComplete")->isVisible());
        QVERIFY(ui.item("cardCancelTask")->isVisible());
        QVERIFY(!ui.item("cardPostpone")->isVisible());
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
        QVERIFY(!ui.item("restoreButton")->isVisible()); // not on the Tasks tab
        ui.click("tab-cancelled");
        QTRY_COMPARE(ui.app->currentTab(), int(AppController::CancelledTab));
        QTRY_VERIFY(ui.row(a));
        ui.tick(a);
        QVERIFY(ui.item("restoreButton")->isEnabled());
        QVERIFY(!ui.item("cancelButton")->isVisible());
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
        ui.focusInput();
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

    // --- shortcuts the user can change ---

    void defaultShortcutsCompleteRestoreAndPostpone()
    {
        Ui ui;
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        const QString c = ui.add("C");
        ui.clickTask(a);
        ui.key(Qt::Key_K); // complete
        QCOMPARE(ui.store()->task(a)->state, TaskState::Completed);

        ui.clickTask(b);
        ui.key(Qt::Key_P); // postpone: the menu asks for how long
        QTRY_VERIFY(ui.object("postponeMenu")->property("opened").toBool());
        ui.click("postponeOneWeek");
        QTRY_COMPARE(ui.store()->task(b)->state, TaskState::Postponed);
        QTRY_VERIFY(ui.listHasFocus()); // the keys work again after the menu

        ui.showTab(AppController::CompletedTab);
        QTRY_VERIFY(ui.row(a));
        ui.clickTask(a);
        ui.key(Qt::Key_R); // restore
        QCOMPARE(ui.store()->task(a)->state, TaskState::Active);
        QVERIFY(ui.store()->task(c)->state == TaskState::Active);
    }

    void shortcutsActOnTickedTasksAndNeverInTheInput()
    {
        Ui ui;
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        ui.tick(a);
        ui.tick(b);
        ui.key(Qt::Key_K);
        QCOMPARE(ui.store()->task(a)->state, TaskState::Completed);
        QCOMPARE(ui.store()->task(b)->state, TaskState::Completed);

        ui.add("C");
        ui.focusInput();
        ui.type("kpr"); // typed text, not shortcuts
        QCOMPARE(ui.inputText(), QString("kpr"));
        QCOMPARE(ui.shown(), QStringList({"C"}));
    }

    void shortcutKeysThatDoNotApplyDoNothing()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.clickTask(a);
        ui.key(Qt::Key_R); // nothing to restore on the Tasks tab
        QCOMPARE(ui.store()->task(a)->state, TaskState::Active);
        ui.showTab(AppController::CancelledTab);
        ui.key(Qt::Key_P); // nothing to postpone there
        QTest::qWait(50);
        QVERIFY(!ui.object("postponeMenu")->property("opened").toBool());
    }

    void changedShortcutsWorkAndTheOldKeysDoNot()
    {
        Ui ui;
        QVERIFY(ui.app->setShortcut("complete", "Ctrl+K").isEmpty());
        QVERIFY(ui.app->setShortcut("cancel", "X").isEmpty());
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        ui.clickTask(a);
        ui.key(Qt::Key_K); // the old key
        ui.key(Qt::Key_Delete);
        QCOMPARE(ui.store()->task(a)->state, TaskState::Active);
        ui.key(Qt::Key_K, Qt::ControlModifier);
        QCOMPARE(ui.store()->task(a)->state, TaskState::Completed);
        ui.clickTask(b);
        ui.key(Qt::Key_X);
        QCOMPARE(ui.store()->task(b)->state, TaskState::Cancelled);
    }

    void shortcutsDialogChangesShortcutsByPressingKeys()
    {
        Ui ui;
        ui.settings("settingsShortcuts");
        QTRY_VERIFY(ui.isOpen("shortcutsDialog"));
        for (const char *action : {"complete", "cancel", "postpone", "restore"}) {
            QVERIFY2(ui.item(QString("shortcutField-") + action), action);
        }
        QCOMPARE(ui.item("shortcutField-complete")->property("text").toString(), QString("K"));
        QCOMPARE(ui.item("shortcutField-cancel")->property("text").toString(), QString("Del"));
        QCOMPARE(ui.item("shortcutName-complete")->property("text").toString(), QString("Complete"));
        QVERIFY(!ui.item("shortcutReset-complete")->isEnabled()); // already the default

        ui.click("shortcutField-complete"); // listens for the next key
        QCOMPARE(ui.item("shortcutField-complete")->property("text").toString(), QString("Press a key"));
        QTest::keyClick(ui.window, Qt::Key_Shift, Qt::NoModifier); // a bare modifier is not a shortcut
        QCOMPARE(ui.item("shortcutField-complete")->property("text").toString(), QString("Press a key"));
        QTest::keyClick(ui.window, Qt::Key_J, Qt::ControlModifier);
        QTRY_COMPARE(ui.item("shortcutField-complete")->property("text").toString(), QString("Ctrl+J"));
        QCOMPARE(ui.app->shortcutKeys("complete"), QString("Ctrl+J"));
        QVERIFY(ui.item("shortcutReset-complete")->isEnabled());

        // Esc gives up and keeps the shortcut.
        ui.click("shortcutField-postpone");
        QTest::keyClick(ui.window, Qt::Key_Escape);
        QCOMPARE(ui.item("shortcutField-postpone")->property("text").toString(), QString("P"));
        QVERIFY(ui.isOpen("shortcutsDialog")); // and does not close the dialog first

        // Ctrl+Q while listening is a key to try, not a quit; it is refused.
        ui.click("shortcutField-restore");
        QTest::keyClick(ui.window, Qt::Key_Q, Qt::ControlModifier);
        QTRY_VERIFY(ui.item("shortcutError-restore")->isVisible());
        QVERIFY(ui.item("shortcutError-restore")->property("text").toString().contains("quit"));
        QCOMPARE(ui.app->shortcutKeys("restore"), QString("R"));

        // The shortcut goes on the keyboard right away.
        QMetaObject::invokeMethod(ui.object("shortcutsDialog"), "close");
        QTRY_VERIFY(ui.isClosed("shortcutsDialog"));
        const QString a = ui.add("A");
        ui.clickTask(a);
        ui.key(Qt::Key_K);
        QCOMPARE(ui.store()->task(a)->state, TaskState::Active);
        ui.key(Qt::Key_J, Qt::ControlModifier);
        QCOMPARE(ui.store()->task(a)->state, TaskState::Completed);
    }

    void shortcutsChosenInTheDialogAreSavedInTheTodoFile()
    {
        Ui ui;
        ui.settings("settingsShortcuts");
        QTRY_VERIFY(ui.isOpen("shortcutsDialog"));
        ui.click("shortcutField-complete");
        QTest::keyClick(ui.window, Qt::Key_J, Qt::ControlModifier);
        QTRY_COMPARE(ui.item("shortcutField-complete")->property("text").toString(), QString("Ctrl+J"));
        ui.click("shortcutClear-restore");
        QTRY_COMPARE(ui.item("shortcutField-restore")->property("text").toString(), QString("None"));

        TodoStore disk;
        QVERIFY(Storage::load(disk, ui.app->storagePath()));
        QCOMPARE(disk.shortcuts().value("complete"), QString("Ctrl+J"));
        QCOMPARE(disk.shortcuts().value("restore"), QString());
        QCOMPARE(disk.shortcuts().size(), 4);

        // After a restart the dialog shows what the file says.
        QMetaObject::invokeMethod(ui.object("shortcutsDialog"), "close");
        QTRY_VERIFY(ui.isClosed("shortcutsDialog"));
        ui.restart();
        ui.settings("settingsShortcuts");
        QTRY_VERIFY(ui.isOpen("shortcutsDialog"));
        QTRY_COMPARE(ui.item("shortcutField-complete")->property("text").toString(), QString("Ctrl+J"));
        QCOMPARE(ui.item("shortcutField-restore")->property("text").toString(), QString("None"));
        QCOMPARE(ui.item("shortcutField-cancel")->property("text").toString(), QString("Del"));
    }

    void shortcutsDialogRefusesKeysInUseAndSaysWhy()
    {
        Ui ui;
        ui.settings("settingsShortcuts");
        QTRY_VERIFY(ui.isOpen("shortcutsDialog"));
        // The rows are rebuilt after every change: always look the item up again.
        auto error = [&ui](const char *action) {
            return ui.item(QString("shortcutError-") + action);
        };
        auto errorShown = [&](const char *action) {
            return error(action) && error(action)->isVisible();
        };
        QVERIFY(!errorShown("complete"));

        ui.click("shortcutField-complete");
        QTest::keyClick(ui.window, Qt::Key_Delete); // used by Cancel task
        QTRY_VERIFY(errorShown("complete"));
        QVERIFY(error("complete")->property("text").toString().contains("Cancel task"));
        QCOMPARE(ui.item("shortcutField-complete")->property("text").toString(), QString("K"));

        ui.click("shortcutField-complete");
        QTest::keyClick(ui.window, Qt::Key_Space); // fixed
        QTRY_VERIFY(error("complete")->property("text").toString().contains("tick"));

        // The message goes away with a change that works, and only on its own row.
        ui.click("shortcutField-complete");
        QTest::keyClick(ui.window, Qt::Key_L);
        QTRY_VERIFY(!errorShown("complete"));
        QCOMPARE(ui.app->shortcutKeys("complete"), QString("L"));
        QVERIFY(!errorShown("cancel"));
    }

    void shortcutsDialogRemovesAndResetsEachShortcutOnItsOwn()
    {
        Ui ui;
        QVERIFY(ui.app->setShortcut("complete", "X").isEmpty());
        QVERIFY(ui.app->setShortcut("postpone", "Y").isEmpty());
        ui.settings("settingsShortcuts");
        QTRY_VERIFY(ui.isOpen("shortcutsDialog"));
        QVERIFY(ui.item("shortcutReset-complete")->isEnabled());
        QVERIFY(!ui.item("shortcutReset-cancel")->isEnabled());

        ui.click("shortcutClear-cancel");
        QCOMPARE(ui.app->shortcutKeys("cancel"), QString());
        QTRY_COMPARE(ui.item("shortcutField-cancel")->property("text").toString(), QString("None"));
        QVERIFY(!ui.item("shortcutClear-cancel")->isEnabled());

        ui.click("shortcutReset-complete");
        QTRY_COMPARE(ui.item("shortcutField-complete")->property("text").toString(), QString("K"));
        QCOMPARE(ui.app->shortcutKeys("postpone"), QString("Y")); // the others are untouched
        QCOMPARE(ui.app->shortcutKeys("cancel"), QString());

        ui.click("shortcutReset-cancel"); // back to Del
        QTRY_COMPARE(ui.item("shortcutField-cancel")->property("text").toString(), QString("Del"));
        QCOMPARE(ui.app->shortcutKeys("postpone"), QString("Y"));
    }

    void menuEntriesHaveNoDots()
    {
        Ui ui;
        ui.click("settingsButton");
        QTRY_VERIFY(ui.object("settingsMenu")->property("opened").toBool());
        for (const char *name : {"settingsLabels", "settingsStorage", "settingsShortcuts"}) {
            const QString text = ui.item(name)->property("text").toString();
            QVERIFY2(!text.isEmpty() && !text.contains("…") && !text.contains("..."), qPrintable(text));
        }
        QCOMPARE(ui.item("settingsLabels")->property("text").toString(), QString("Labels"));
        QCOMPARE(ui.item("settingsStorage")->property("text").toString(), QString("Storage folder"));
        QCOMPARE(ui.item("settingsShortcuts")->property("text").toString(), QString("Keyboard shortcuts"));
        ui.click("settingsLabels");
        QTRY_VERIFY(ui.isOpen("labelsDialog"));
        ui.click("labelsClose");
        QTRY_VERIFY(ui.isClosed("labelsDialog"));
        ui.click("tab-active");
        ui.add("A");
        ui.clickTask(ui.store()->tasks().at(0).id);
        ui.key(Qt::Key_P);
        QTRY_VERIFY(ui.object("postponeMenu")->property("opened").toBool());
        QCOMPARE(ui.item("postponeCustom")->property("text").toString(), QString("Custom"));
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
        ui.key(Qt::Key_C, Qt::ControlModifier);
        ui.key(Qt::Key_V, Qt::ControlModifier);
        QCOMPARE(ui.shown(), QStringList({"A"}));
    }

    // --- focus and keys ---

    void atStartNothingIsHighlightedAndTheInputHasNoFocus()
    {
        Ui ui;
        ui.add("A");
        QVERIFY(ui.highlighted().isEmpty());
        QVERIFY(!ui.input()->hasActiveFocus());
        QTRY_VERIFY(ui.listHasFocus());
        ui.type("abc"); // typing alone does not reach the input
        QVERIFY(ui.inputText().isEmpty());
    }

    void shiftEnterFocusesTheInputOnlyWithoutHighlight()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.key(Qt::Key_Down); // highlighted: Shift+Enter opens the card
        QCOMPARE(ui.highlighted(), a);
        ui.key(Qt::Key_Return, Qt::ShiftModifier);
        QTRY_VERIFY(ui.isOpen("taskCard"));
        QVERIFY(!ui.input()->hasActiveFocus());
        ui.click("cardClose");
        QTRY_VERIFY(ui.isClosed("taskCard"));
        QTRY_VERIFY(ui.listHasFocus());

        ui.key(Qt::Key_Escape); // removes the highlight
        QVERIFY(ui.highlighted().isEmpty());
        ui.key(Qt::Key_Return, Qt::ShiftModifier); // now it goes to the input
        QTRY_VERIFY(ui.input()->hasActiveFocus());
        QVERIFY(ui.isClosed("taskCard"));
        ui.typeTask("New");
        QCOMPARE(ui.shown(), QStringList({"New", "A"}));
        QVERIFY(ui.input()->hasActiveFocus()); // ready for the next one
    }

    void shiftEnterOnAnotherTabGoesToTheTasksTabInput()
    {
        Ui ui;
        ui.click("tab-completed");
        QTRY_COMPARE(ui.app->currentTab(), int(AppController::CompletedTab));
        ui.focusInput();
        QCOMPARE(ui.app->currentTab(), int(AppController::ActiveTab));
    }

    void arrowsInTheEmptyInputGoToTheList()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.focusInput();
        ui.key(Qt::Key_Down);
        QCOMPARE(ui.highlighted(), a);
        QTRY_VERIFY(ui.listHasFocus());
    }

    void clickingATaskGivesTheKeysToTheList()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.focusInput();
        ui.clickTask(a);
        QTRY_VERIFY(ui.listHasFocus());
        ui.key(Qt::Key_Space);
        QCOMPARE(ui.list()->checkedIds(), QStringList({a}));
    }

    void spaceTicksAndUnticksTheHighlightedTask()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.add("B");
        ui.key(Qt::Key_Space); // nothing highlighted: nothing happens
        QCOMPARE(ui.list()->checkedCount(), 0);
        ui.key(Qt::Key_Down);
        ui.key(Qt::Key_Space);
        QCOMPARE(ui.list()->checkedIds(), QStringList({a}));
        QVERIFY(ui.item("check-" + a)->property("checked").toBool());
        ui.key(Qt::Key_Space);
        QCOMPARE(ui.list()->checkedCount(), 0);
    }

    void shiftArrowsTickTheTasksTheHighlightPasses()
    {
        Ui ui;
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        const QString c = ui.add("C");
        const QString d = ui.add("D");
        ui.key(Qt::Key_Down); // A
        ui.key(Qt::Key_Down, Qt::ShiftModifier); // A and B
        QCOMPARE(ui.list()->checkedIds(), QStringList({a, b}));
        QCOMPARE(ui.highlighted(), b);
        ui.key(Qt::Key_Down, Qt::ShiftModifier);
        QCOMPARE(ui.list()->checkedIds(), QStringList({a, b, c}));
        ui.key(Qt::Key_Up, Qt::ShiftModifier); // going back does not untick
        QCOMPARE(ui.highlighted(), b);
        QCOMPARE(ui.list()->checkedIds(), QStringList({a, b, c}));
        ui.key(Qt::Key_Down);
        ui.key(Qt::Key_Down); // a plain arrow neither ticks nor unticks
        QCOMPARE(ui.highlighted(), d);
        QCOMPARE(ui.list()->checkedIds(), QStringList({a, b, c}));
    }

    void escapeTwiceUnticksAllTasks()
    {
        Ui ui;
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        ui.tick(a);
        ui.tick(b);
        ui.clickTask(a);
        ui.key(Qt::Key_Escape); // once: only the highlight goes
        QVERIFY(ui.highlighted().isEmpty());
        QCOMPARE(ui.list()->checkedCount(), 2);
        QTest::qWait(700);
        ui.key(Qt::Key_Escape); // not quick enough: still ticked
        QCOMPARE(ui.list()->checkedCount(), 2);
        ui.key(Qt::Key_Escape); // twice in a row
        QCOMPARE(ui.list()->checkedCount(), 0);
        QVERIFY(!ui.item("completeButton")->isEnabled());
    }

    void shiftDeleteDeletesTheSelectedTasks()
    {
        Ui ui;
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        const QString c = ui.add("C");
        ui.clickTask(a);
        ui.key(Qt::Key_Delete, Qt::ShiftModifier); // one task: at once, for good
        QVERIFY(!ui.store()->task(a));
        QCOMPARE(ui.savedTitles(), QStringList({"B", "C"}));

        QTRY_VERIFY(ui.row(b));
        ui.tick(b);
        ui.tick(c);
        ui.key(Qt::Key_Delete, Qt::ShiftModifier); // several: asks first
        QTRY_VERIFY(ui.isOpen("confirmDeleteDialog"));
        QCOMPARE(ui.shown().size(), 2);
        ui.click("confirmDeleteButton");
        QTRY_VERIFY(ui.shown().isEmpty());
    }

    void escapeLeavesTheInputAndKeysGoToTheTasks()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.add("B");
        ui.focusInput();
        ui.type("draft");
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
        QTRY_COMPARE(ui.app->currentTab(), int(AppController::CancelledTab));
        ui.clickTask(b);
        QVERIFY(ui.item("restoreButton")->isEnabled());
        ui.click("restoreButton");
        QCOMPARE(ui.store()->task(b)->state, TaskState::Active);
        ui.click("tab-active");
        QTRY_COMPARE(ui.app->currentTab(), int(AppController::ActiveTab));
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
        QCOMPARE(button->opacity(), 0.0); // hidden, but it keeps its place
        QVERIFY(!button->isEnabled());
        ui.hover(a);
        QTRY_COMPARE(button->opacity(), 1.0);
        QVERIFY(button->isEnabled());
        QCOMPARE(ui.item("complete-" + b)->opacity(), 0.0); // only on the hovered task

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
        ui.settings("settingsLabels");
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

    // --- tray popup ---

    void trayPopupShowsFirstFiveTasksAndCompletesThem()
    {
        Ui ui;
        QStringList ids;
        for (const char *title : {"A", "B", "C", "D", "E", "F"}) {
            ids.append(ui.add(title));
        }
        auto *popup = ui.window->findChild<QQuickWindow *>("trayPopup");
        QVERIFY(popup);
        QVERIFY(!popup->isVisible()); // only after a click on the tray icon
        popup->show();
        QVERIFY(QTest::qWaitForWindowExposed(popup));

        for (int i = 0; i < 5; ++i) {
            QQuickItem *title = findItem(popup->contentItem(), "trayTitle-" + ids.at(i));
            QVERIFY(title);
            QCOMPARE(title->property("text").toString(), ui.store()->task(ids.at(i))->title);
        }
        QVERIFY(!findItem(popup->contentItem(), "trayTitle-" + ids.at(5))); // the sixth is not shown
        QVERIFY(!findItem(popup->contentItem(), "trayEmpty")->isVisible());

        QQuickItem *complete = findItem(popup->contentItem(), "trayComplete-" + ids.at(0));
        QVERIFY(complete);
        QTest::qWait(30);
        QTest::mouseClick(popup, Qt::LeftButton, Qt::NoModifier, Ui::center(complete));
        QCOMPARE(ui.store()->task(ids.at(0))->state, TaskState::Completed);
        QTRY_VERIFY(findItem(popup->contentItem(), "trayTitle-" + ids.at(5))); // the next one moved up
        QVERIFY(!findItem(popup->contentItem(), "trayTitle-" + ids.at(0)));
    }

    void trayPopupOpensTheApplicationWindow()
    {
        Ui ui;
        auto *popup = ui.window->findChild<QQuickWindow *>("trayPopup");
        QVERIFY(popup);
        ui.window->hide();
        popup->show();
        QVERIFY(QTest::qWaitForWindowExposed(popup));
        QVERIFY(findItem(popup->contentItem(), "trayEmpty")->isVisible()); // no tasks yet
        QQuickItem *open = findItem(popup->contentItem(), "trayOpen");
        QVERIFY(open);
        QTest::qWait(30);
        QTest::mouseClick(popup, Qt::LeftButton, Qt::NoModifier, Ui::center(open));
        QTRY_VERIFY(ui.window->isVisible());
        QTRY_VERIFY(!popup->isVisible());
    }

    void closingTheWindowKeepsTheAppInTheTray()
    {
        Ui ui;
        ui.add("A");
        ui.app->setKeepRunningInTray(true);
        ui.window->close();
        QTRY_VERIFY(!ui.window->isVisible()); // hidden, not gone
        QVERIFY(!ui.engine->rootObjects().isEmpty());
        QMetaObject::invokeMethod(ui.window, "showMainWindow"); // what the tray and a second start do
        QTRY_VERIFY(ui.window->isVisible());
        QCOMPARE(ui.shown(), QStringList({"A"}));
    }

    // --- window ---

    void toolbarButtonsAllFitInTheInitialWindow()
    {
        Ui ui;
        // The Postponed tab shows all state buttons.
        ui.click("tab-postponed");
        QTRY_COMPARE(ui.app->currentTab(), int(AppController::PostponedTab));
        QTRY_VERIFY(ui.item("restoreButton")->isVisible());
        QVERIFY(QTest::qWaitForWindowExposed(ui.window));
        QTest::qWait(50); // the row lays out the buttons that just became visible
        const QStringList buttons = {"completeButton", "postponeButton", "cancelButton", "deleteButton", "restoreButton"};
        QQuickItem *settings = ui.item("settingsButton"); // alone in the bar at the top
        QVERIFY(settings && settings->isVisible());
        QVERIFY(settings->mapRectToScene(QRectF(0, 0, settings->width(), settings->height())).right() <= ui.window->width());
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

    void stateButtonsShowOnlyIconsWithTheirNameAsHint()
    {
        Ui ui;
        struct Expected {
            const char *button;
            const char *hint;
            const char *iconName;
            const char *iconSource;
        };
        const QList<Expected> buttons = {
            {"completeButton", "Complete", "", "qrc:/qml/icons/complete.svg"},
            {"postponeButton", "Postpone", "", "qrc:/qml/icons/postpone.svg"},
            {"cancelButton", "Cancel task", "", "qrc:/qml/icons/cancel.svg"},
            {"deleteButton", "Delete", "", "qrc:/qml/icons/delete.svg"},
            {"restoreButton", "Restore", "edit-undo", ""},
            {"settingsButton", "Settings", "configure", ""},
        };
        for (const Expected &e : buttons) {
            QQuickItem *button = ui.item(e.button);
            QVERIFY2(button, e.button);
            QCOMPARE(button->property("display").toInt(), 0); // AbstractButton.IconOnly
            QCOMPARE(button->property("text").toString(), QString(e.hint));
            QCOMPARE(ui.buttonIcon(e.button, "name").toString(), QString(e.iconName));
            QCOMPARE(ui.buttonIcon(e.button, "source").toUrl().toString(), QString(e.iconSource));
        }
        // The bundled icons bring their colours: green checkmark, red bin.
        QVERIFY(readFile(":/qml/icons/complete.svg").contains("#27ae60"));
        QVERIFY(readFile(":/qml/icons/delete.svg").contains("#da4453"));
        // The text is shown as the hover hint (IconButton binds the tool tip to
        // it); hovering toolbar buttons is not delivered on the test platform.
    }

    void cardStateButtonsShowOnlyIcons()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.doubleClickTask(a);
        QTRY_VERIFY(ui.isOpen("taskCard"));
        for (const char *name : {"cardComplete", "cardPostpone", "cardCancelTask"}) {
            QCOMPARE(ui.item(name)->property("display").toInt(), 0); // IconOnly
        }
        QCOMPARE(ui.item("cardComplete")->property("text").toString(), QString("Complete"));
        QCOMPARE(ui.buttonIcon("cardComplete", "source").toUrl().toString(), QString("qrc:/qml/icons/complete.svg"));
        QCOMPARE(ui.buttonIcon("cardPostpone", "source").toUrl().toString(), QString("qrc:/qml/icons/postpone.svg"));
        QCOMPARE(ui.buttonIcon("cardCancelTask", "source").toUrl().toString(), QString("qrc:/qml/icons/cancel.svg"));
    }

    void shortcutsDialogSurvivesResizingTheWindow()
    {
        Ui ui;
        // The dialog follows the width of the window. Resized before it was
        // ever opened (this is what looped), then while it is open.
        for (const QSize &size : {QSize(640, 760), QSize(420, 700), QSize(700, 500)}) {
            ui.window->resize(size);
            QTest::qWait(80);
        }
        ui.settings("settingsShortcuts");
        QTRY_VERIFY(ui.isOpen("shortcutsDialog"));
        for (const QSize &size : {QSize(900, 800), QSize(450, 600), QSize(510, 612)}) {
            ui.window->resize(size);
            QTest::qWait(80);
        }
        QQuickItem *keys = ui.item("shortcutKeys-0");
        QQuickItem *action = ui.item("shortcutAction-0");
        QVERIFY(keys && action);
        QVERIFY(action->x() >= keys->width()); // two columns, the text right of the keys
        QMetaObject::invokeMethod(ui.object("shortcutsDialog"), "close");
        QTRY_VERIFY(ui.isClosed("shortcutsDialog"));
    }

    void shortcutsDialogFitsASmallWindowAndScrolls()
    {
        Ui ui;
        ui.window->resize(430, 420);
        QTest::qWait(100);
        ui.settings("settingsShortcuts");
        QTRY_VERIFY(ui.isOpen("shortcutsDialog"));
        QTRY_VERIFY(ui.item("shortcutField-restore"));
        auto *dialog = ui.object("shortcutsDialog");
        QVERIFY(dialog->property("height").toDouble() <= ui.window->height());
        QVERIFY(dialog->property("height").toDouble() > 150); // not collapsed
        QVERIFY(ui.item("shortcutField-complete")->mapToScene(QPointF(0, 0)).y() >= 0);
        // More rows than room: the content is a scrolling area taller than the dialog.
        auto *content = qvariant_cast<QQuickItem *>(dialog->property("contentItem"));
        QVERIFY(content);
        QVERIFY(content->property("contentHeight").toDouble() > content->height() + 20);
        content->setProperty("contentY", content->property("contentHeight").toDouble() - content->height());
        QTRY_VERIFY(ui.item("shortcutKeys-13")->mapToScene(QPointF(0, 0)).y() < ui.window->height());
    }

    void settingsMenuHasLabelsStorageAndShortcuts()
    {
        QTemporaryDir other;
        Ui ui;
        ui.settings("settingsShortcuts");
        QTRY_VERIFY(ui.isOpen("shortcutsDialog"));
        QStringList keys;
        for (int i = 0; ui.item(QString("shortcutKeys-%1").arg(i)); ++i) {
            keys.append(ui.item(QString("shortcutKeys-%1").arg(i))->property("text").toString());
        }
        for (const char *key : {"Shift+Enter", "Enter", "Esc", "Esc Esc", "Up / Down", "Shift+Up / Shift+Down", "Space", "Ctrl+Up / Ctrl+Down",
                 "Ctrl+PgDown / Ctrl+PgUp", "Ctrl+A", "Ctrl+D", "Shift+Delete", "Ctrl+Q"}) {
            QVERIFY2(keys.contains(key), key);
        }
        QMetaObject::invokeMethod(ui.object("shortcutsDialog"), "close");
        QTRY_VERIFY(ui.isClosed("shortcutsDialog"));

        ui.settings("settingsLabels");
        QTRY_VERIFY(ui.isOpen("labelsDialog"));
        ui.click("labelsClose");
        QTRY_VERIFY(ui.isClosed("labelsDialog"));
        QVERIFY(ui.item("settingsButton"));
        QVERIFY(!ui.item("labelsButton")); // no separate buttons any more
        QVERIFY(!ui.item("storageButton"));
    }

    void stateIconsFollowTheColourScheme()
    {
        Ui ui;
        QQuickItem *postpone = ui.item("postponeButton");
        QQuickItem *cancel = ui.item("cancelButton");
        QCOMPARE(postpone->property("darkTheme").toBool(), false); // the test platform is light
        // A dark scheme (light text) switches to the light icons; the
        // checkmark and the bin keep their colours.
        postpone->setProperty("darkTheme", true);
        QCOMPARE(ui.buttonIcon("postponeButton", "source").toUrl().toString(), QString("qrc:/qml/icons/postpone-on-dark.svg"));
        cancel->setProperty("darkTheme", true);
        QCOMPARE(ui.buttonIcon("cancelButton", "source").toUrl().toString(), QString("qrc:/qml/icons/cancel-on-dark.svg"));
        ui.item("completeButton")->setProperty("darkTheme", true);
        QCOMPARE(ui.buttonIcon("completeButton", "source").toUrl().toString(), QString("qrc:/qml/icons/complete.svg"));
        ui.item("deleteButton")->setProperty("darkTheme", true);
        QCOMPARE(ui.buttonIcon("deleteButton", "source").toUrl().toString(), QString("qrc:/qml/icons/delete.svg"));
        for (const char *file : {"postpone-on-dark", "cancel-on-dark", "postpone", "cancel", "alarm"}) {
            QVERIFY2(!readFile(QString(":/qml/icons/%1.svg").arg(file)).isEmpty(), file);
        }
    }

    void stateButtonsAreBelowTheInputOnEveryTab()
    {
        Ui ui;
        ui.add("A");
        QQuickItem *input = ui.input();
        QQuickItem *row = ui.item("stateRow");
        QVERIFY(row);
        const qreal inputBottom = input->mapToScene(QPointF(0, input->height())).y();
        const qreal rowTop = row->mapToScene(QPointF(0, 0)).y();
        const qreal rowBottom = rowTop + row->height();
        const qreal listTop = ui.item("activeList")->mapToScene(QPointF(0, 0)).y();
        QVERIFY(rowTop >= inputBottom);
        QVERIFY(listTop >= rowBottom);
        // The toolbar at the top holds only Settings.
        const qreal settingsBottom = ui.item("settingsButton")->mapToScene(QPointF(0, ui.item("settingsButton")->height())).y();
        QVERIFY(settingsBottom <= input->mapToScene(QPointF(0, 0)).y());

        ui.click("tab-completed"); // other tabs have no input, the buttons stay
        QTRY_VERIFY(!input->isVisible());
        QVERIFY(ui.item("deleteButton")->isVisible());
        const qreal listTop2 = ui.item("completedList")->mapToScene(QPointF(0, 0)).y();
        QVERIFY(listTop2 >= ui.item("stateRow")->mapToScene(QPointF(0, ui.item("stateRow")->height())).y());
    }

    void ctrlPageDownAndUpSwitchTabsAndWrap()
    {
        Ui ui;
        QCOMPARE(ui.app->currentTab(), int(AppController::ActiveTab));
        ui.key(Qt::Key_PageDown, Qt::ControlModifier);
        QCOMPARE(ui.app->currentTab(), int(AppController::PostponedTab));
        ui.key(Qt::Key_PageDown, Qt::ControlModifier);
        ui.key(Qt::Key_PageDown, Qt::ControlModifier);
        QCOMPARE(ui.app->currentTab(), int(AppController::CancelledTab));
        ui.key(Qt::Key_PageDown, Qt::ControlModifier); // wraps to the first
        QCOMPARE(ui.app->currentTab(), int(AppController::ActiveTab));
        ui.key(Qt::Key_PageUp, Qt::ControlModifier); // and back to the last
        QCOMPARE(ui.app->currentTab(), int(AppController::CancelledTab));
        ui.key(Qt::Key_PageUp, Qt::ControlModifier);
        QCOMPARE(ui.app->currentTab(), int(AppController::CompletedTab));
        ui.key(Qt::Key_PageUp); // without Ctrl: nothing
        QCOMPARE(ui.app->currentTab(), int(AppController::CompletedTab));

        ui.key(Qt::Key_PageDown, Qt::ControlModifier);
        ui.key(Qt::Key_PageDown, Qt::ControlModifier);
        QCOMPARE(ui.app->currentTab(), int(AppController::ActiveTab));
        ui.focusInput(); // also while typing a task
        ui.type("draft");
        ui.key(Qt::Key_PageDown, Qt::ControlModifier);
        QCOMPARE(ui.app->currentTab(), int(AppController::PostponedTab));
        QTRY_VERIFY(ui.listHasFocus());
    }

    void eachTabShowsOnlyItsButtons()
    {
        Ui ui;
        const QList<QPair<const char *, QStringList>> tabs = {
            {"tab-active", {"completeButton", "postponeButton", "cancelButton", "deleteButton"}},
            {"tab-postponed", {"completeButton", "postponeButton", "cancelButton", "deleteButton", "restoreButton"}},
            {"tab-completed", {"cancelButton", "deleteButton", "restoreButton"}},
            {"tab-cancelled", {"completeButton", "deleteButton", "restoreButton"}},
        };
        const QStringList all = {"completeButton", "postponeButton", "cancelButton", "deleteButton", "restoreButton"};
        for (const auto &tab : tabs) {
            ui.click(tab.first);
            QTRY_VERIFY(ui.item(tab.first)->property("checked").toBool());
            for (const QString &button : all) {
                QVERIFY2(ui.item(button)->isVisible() == tab.second.contains(button), qPrintable(QString(tab.first) + " " + button));
            }
        }
    }

    void cardShowsOnlyTheStateButtonsThatApply()
    {
        Ui ui;
        const QString done = ui.add("Done");
        const QString cancelled = ui.add("Cancelled");
        const QString active = ui.add("Active");
        ui.app->complete({done});
        ui.app->cancel({cancelled});
        const QList<QPair<QString, QStringList>> cases = {
            {active, {"cardComplete", "cardPostpone", "cardCancelTask"}},
            {done, {"cardCancelTask"}},
            {cancelled, {"cardComplete"}},
        };
        for (const auto &c : cases) {
            QMetaObject::invokeMethod(ui.window, "openCard", Q_ARG(QVariant, c.first));
            QTRY_VERIFY(ui.isOpen("taskCard"));
            for (const char *button : {"cardComplete", "cardPostpone", "cardCancelTask"}) {
                QVERIFY2(ui.item(button)->isVisible() == c.second.contains(button), button);
            }
            ui.click("cardClose");
            QTRY_VERIFY(ui.isClosed("taskCard"));
        }
    }

    // --- tasks back from Postponed ---

    void returnedTaskShowsAlarmUntilClicked()
    {
        Ui ui;
        const QString a = ui.add("A");
        const QString b = ui.add("B");
        ui.app->postponeUntil({a}, fixedNow.addSecs(60));
        ui.now = fixedNow.addSecs(61);
        ui.app->releaseDue(); // what the timer does at the time
        QCOMPARE(ui.shown(), QStringList({"A", "B"}));
        QTRY_VERIFY(ui.row(a));
        QQuickItem *alarm = ui.item("returned-" + a);
        QVERIFY(alarm);
        QVERIFY(alarm->isVisible()); // without hovering
        QVERIFY(!ui.item("returned-" + b)->isVisible());
        QCOMPARE(QQmlProperty::read(alarm, "icon.source").toUrl().toString(), QString("qrc:/qml/icons/alarm.svg"));
        QCOMPARE(alarm->property("text").toString(), QString("Back from Postponed: time to do it"));

        // Left of the place of the quick-complete checkmark.
        QQuickItem *check = ui.item("complete-" + a);
        QVERIFY(alarm->mapToScene(QPointF(alarm->width(), 0)).x() <= check->mapToScene(QPointF(0, 0)).x());

        ui.restart(); // it stays after a restart
        QTRY_VERIFY(ui.item("returned-" + a));
        QVERIFY(ui.item("returned-" + a)->isVisible());

        ui.click("returned-" + a);
        QTRY_VERIFY(!ui.item("returned-" + a)->isVisible());
        QVERIFY(!ui.store()->task(a)->returned);
        QCOMPARE(ui.store()->task(a)->state, TaskState::Active); // only the mark goes
        ui.restart();
        QTRY_VERIFY(ui.row(a));
        QVERIFY(!ui.item("returned-" + a)->isVisible());
    }

    void alarmGoesWhenTheTaskChangesState()
    {
        Ui ui;
        const QString a = ui.add("A");
        ui.app->postponeUntil({a}, fixedNow.addSecs(60));
        ui.now = fixedNow.addSecs(61);
        ui.restart(); // came back when the application started
        QTRY_VERIFY(ui.row(a));
        QVERIFY(ui.item("returned-" + a)->isVisible());
        ui.app->complete({a});
        QVERIFY(!ui.store()->task(a)->returned);
        ui.store()->restore({a});
        QTRY_VERIFY(ui.row(a));
        QVERIFY(!ui.item("returned-" + a)->isVisible()); // restored by hand: no alarm
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
