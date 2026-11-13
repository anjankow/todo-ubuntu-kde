#include "core/storage.h"
#include "core/todostore.h"
#include "ui/labelsdialog.h"
#include "ui/mainwindow.h"
#include "ui/newtaskedit.h"
#include "ui/settings.h"
#include "ui/taskdialog.h"
#include "ui/tasklistview.h"

#include <KHelpMenu>
#include <KLocalizedString>

#include <QAction>
#include <QLineEdit>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QPlainTextEdit>
#include <QPushButton>
#include <QStandardPaths>
#include <QTabWidget>
#include <QTemporaryDir>
#include <QTest>
#include <QToolBar>
#include <QToolButton>

#include <memory>

namespace
{
const QDateTime fixedNow(QDate(2026, 11, 12), QTime(10, 0));

// A main window working on a temporary folder, with all dialogs replaced.
struct Fixture {
    QTemporaryDir dir;
    QString configFile;
    QDateTime now = fixedNow;
    int confirmCalls = 0;
    int confirmCount = 0;
    bool confirmAnswer = true;
    MainWindow::FolderConflict conflictAnswer = MainWindow::FolderConflict::Abort;
    int conflictCalls = 0;
    QDateTime customPostpone;
    QString chosenFolder;
    QStringList errors;
    std::unique_ptr<MainWindow> window;

    Fixture()
    {
        configFile = dir.filePath("kdetodorc");
        Settings(configFile).setStorageDir(dir.path());
        open();
    }

    // (Re)opens the application window, like starting the app again.
    void open()
    {
        MainWindow::Hooks hooks;
        hooks.now = [this] {
            return now;
        };
        hooks.confirmDelete = [this](int count) {
            ++confirmCalls;
            confirmCount = count;
            return confirmAnswer;
        };
        hooks.resolveFolderConflict = [this] {
            ++conflictCalls;
            return conflictAnswer;
        };
        hooks.askPostponeTime = [this] {
            return customPostpone;
        };
        hooks.askFolder = [this] {
            return chosenFolder;
        };
        hooks.reportError = [this](const QString &message) {
            errors.append(message);
        };
        window.reset();
        window = std::make_unique<MainWindow>(configFile, hooks);
        window->show();
        QVERIFY(QTest::qWaitForWindowExposed(window.get()));
    }

    TodoStore *store() const { return window->store(); }
    TaskListView *view(TaskState state = TaskState::Active) const { return window->view(state); }

    // Adds a task at the end of the list (setup for tests not about typing).
    void add(const QString &title) { store()->addTask(title); }

    void type(const QString &title)
    {
        QTest::keyClicks(window->newTaskEdit(), title);
        QTest::keyClick(window->newTaskEdit(), Qt::Key_Return);
    }

    void showTab(TaskState state)
    {
        const QList<TaskState> order = {TaskState::Active, TaskState::Postponed, TaskState::Completed, TaskState::Cancelled};
        window->tabs()->setCurrentIndex(order.indexOf(state));
    }

    void clickTitle(int row, TaskState state = TaskState::Active)
    {
        TaskListView *v = view(state);
        const QRect rect = v->visualRect(v->model()->index(row, 0));
        QTest::mouseClick(v->viewport(), Qt::LeftButton, Qt::NoModifier, QPoint(rect.right() - 20, rect.center().y()));
    }

    void clickCheckbox(int row, TaskState state = TaskState::Active)
    {
        TaskListView *v = view(state);
        QTest::mouseClick(v->viewport(), Qt::LeftButton, Qt::NoModifier, v->checkRect(row).center());
    }

    QStringList shown(TaskState state = TaskState::Active) const
    {
        QStringList result;
        for (int i = 0; i < view(state)->count(); ++i) {
            result.append(view(state)->item(i)->text());
        }
        return result;
    }

    QStringList savedTitles() const
    {
        TodoStore disk;
        Storage::load(disk, Storage::filePath(dir.path()));
        QStringList result;
        for (const Task &t : disk.tasks()) {
            result.append(t.title);
        }
        return result;
    }
};
}

class TestMainWindow : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase()
    {
        QStandardPaths::setTestModeEnabled(true);
        KLocalizedString::setApplicationDomain("kdetodo");
    }

    // --- storage location ---

    void homeDirectoryIsTheDefaultLocation()
    {
        QTemporaryDir dir;
        const Settings settings(dir.filePath("emptyrc"));
        QCOMPARE(settings.storageDir(), QDir::homePath());
    }

    void todosAreStoredAsTextFileInSelectedFolder()
    {
        Fixture f;
        f.type("Buy milk");
        QCOMPARE(f.window->storagePath(), f.dir.filePath("todos.json"));
        QVERIFY(QFile::exists(f.dir.filePath("todos.json")));
        QCOMPARE(f.savedTitles(), QStringList({"Buy milk"}));
    }

    void tasksSurviveRestart()
    {
        Fixture f;
        f.add("One");
        f.add("Two");
        f.open();
        QCOMPARE(f.shown(), QStringList({"One", "Two"}));
    }

    void changingFolderMovesTheFile()
    {
        Fixture f;
        f.type("Task");
        QTemporaryDir other;
        QVERIFY(f.window->changeStorageDir(other.path()));
        QVERIFY(QFile::exists(other.filePath("todos.json")));
        QVERIFY(!QFile::exists(f.dir.filePath("todos.json")));
        QCOMPARE(Settings(f.configFile).storageDir(), other.path());
        QCOMPARE(f.conflictCalls, 0);

        f.add("Later");
        TodoStore disk;
        QVERIFY(Storage::load(disk, other.filePath("todos.json")));
        QCOMPARE(disk.tasks().size(), 2);

        f.open(); // the new location is remembered
        QCOMPARE(f.shown(), QStringList({"Task", "Later"}));
    }

    void changingFolderAsksWhenATodoFileExistsThere()
    {
        QTemporaryDir other;
        TodoStore existing;
        existing.addTask("Existing");
        QVERIFY(Storage::save(existing, other.filePath("todos.json")));

        Fixture f;
        f.type("Mine");

        f.conflictAnswer = MainWindow::FolderConflict::Abort;
        QVERIFY(!f.window->changeStorageDir(other.path()));
        QCOMPARE(f.conflictCalls, 1);
        QCOMPARE(f.window->storagePath(), f.dir.filePath("todos.json"));
        QCOMPARE(f.shown(), QStringList({"Mine"}));

        f.conflictAnswer = MainWindow::FolderConflict::LoadExisting;
        QVERIFY(f.window->changeStorageDir(other.path()));
        QCOMPARE(f.shown(), QStringList({"Existing"}));
        QVERIFY(QFile::exists(f.dir.filePath("todos.json"))); // nothing destroyed
    }

    void changingFolderCanOverwriteExistingFile()
    {
        QTemporaryDir other;
        TodoStore existing;
        existing.addTask("Existing");
        QVERIFY(Storage::save(existing, other.filePath("todos.json")));

        Fixture f;
        f.type("Mine");
        f.conflictAnswer = MainWindow::FolderConflict::Overwrite;
        QVERIFY(f.window->changeStorageDir(other.path()));
        QCOMPARE(f.shown(), QStringList({"Mine"}));
        QVERIFY(!QFile::exists(f.dir.filePath("todos.json")));
        TodoStore disk;
        QVERIFY(Storage::load(disk, other.filePath("todos.json")));
        QCOMPARE(disk.tasks().size(), 1);
        QCOMPARE(disk.tasks().at(0).title, QString("Mine"));
    }

    void changingToMissingFolderFails()
    {
        Fixture f;
        QVERIFY(!f.window->changeStorageDir(f.dir.filePath("does-not-exist")));
        QCOMPARE(f.errors.size(), 1);
        QCOMPARE(f.window->storagePath(), f.dir.filePath("todos.json"));
    }

    void unreadableFileIsKeptAside()
    {
        Fixture f;
        f.window.reset();
        QFile file(f.dir.filePath("todos.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("garbage");
        file.close();
        f.open();
        QCOMPARE(f.errors.size(), 1);
        QVERIFY(f.shown().isEmpty());
        const QStringList backups = QDir(f.dir.path()).entryList({"todos.json.unreadable-*"});
        QCOMPARE(backups.size(), 1);
    }

    // --- creating tasks ---

    void typingAndEnterAddsTaskAndKeepsInputFocused()
    {
        Fixture f;
        f.window->newTaskEdit()->setFocus();
        f.type("First");
        QCOMPARE(f.shown(), QStringList({"First"}));
        QVERIFY(f.window->newTaskEdit()->toPlainText().isEmpty());
        QCOMPARE(f.window->focusWidget(), f.window->newTaskEdit());

        f.type("Second"); // straight away, the input is ready again
        QCOMPARE(f.shown(), QStringList({"Second", "First"})); // newest on top
        QCOMPARE(f.window->focusWidget(), f.window->newTaskEdit());
    }

    void inputSitsAboveTheList()
    {
        Fixture f;
        f.type("First");
        NewTaskEdit *input = f.window->newTaskEdit();
        const QPoint inputBottom = input->mapTo(f.window.get(), QPoint(0, input->height()));
        const QPoint listTop = f.view()->mapTo(f.window.get(), QPoint(0, 0));
        QVERIFY(listTop.y() >= inputBottom.y());
    }

    void inputHasMarginAboveAndBelow()
    {
        Fixture f;
        f.add("A");
        NewTaskEdit *input = f.window->newTaskEdit();
        QWidget *page = input->parentWidget();
        QVERIFY(input->geometry().top() >= 8); // space above
        const int listTop = f.view()->mapTo(page, QPoint(0, 0)).y();
        QVERIFY(listTop - input->geometry().bottom() >= 8); // space below
    }

    void newTaskAppearsOnTopOfTheList()
    {
        Fixture f;
        f.add("Old 1");
        f.add("Old 2");
        f.type("New");
        QCOMPARE(f.shown(), QStringList({"New", "Old 1", "Old 2"}));
        QCOMPARE(f.savedTitles(), QStringList({"New", "Old 1", "Old 2"}));
    }

    void emptyInputCreatesNoTask()
    {
        Fixture f;
        QTest::keyClick(f.window->newTaskEdit(), Qt::Key_Return);
        f.type("   ");
        QVERIFY(f.store()->tasks().isEmpty());
        QVERIFY(f.savedTitles().isEmpty());
    }

    void descriptionIsEnteredWithShiftEnter()
    {
        Fixture f;
        QTest::keyClicks(f.window->newTaskEdit(), "Call mum");
        QTest::keyClick(f.window->newTaskEdit(), Qt::Key_Return, Qt::ShiftModifier);
        QVERIFY(f.store()->tasks().isEmpty()); // not finalized yet
        QTest::keyClicks(f.window->newTaskEdit(), "about Sunday");
        QTest::keyClick(f.window->newTaskEdit(), Qt::Key_Return);

        QCOMPARE(f.store()->tasks().size(), 1);
        QCOMPARE(f.store()->tasks().at(0).title, QString("Call mum"));
        QCOMPARE(f.store()->tasks().at(0).description, QString("about Sunday"));
    }

    void draftIsNotATaskUntilEnter()
    {
        Fixture f;
        f.window->newTaskEdit()->setFocus();
        QTest::keyClicks(f.window->newTaskEdit(), "draft");
        f.view()->setFocus();
        QVERIFY(f.store()->tasks().isEmpty());
        QCOMPARE(f.window->newTaskEdit()->toPlainText(), QString("draft"));
    }

    // --- list and task card ---

    void listShowsOnlyTitles()
    {
        Fixture f;
        f.store()->addTask("Title", "A long description");
        QCOMPARE(f.shown(), QStringList({"Title"}));
    }

    void clickingTaskOpensCardWithDescription()
    {
        Fixture f;
        f.store()->addTask("Other");
        f.store()->addTask("Title", "The description");
        QVERIFY(!f.window->findChild<TaskDialog *>());
        f.clickTitle(1);

        auto *card = f.window->findChild<TaskDialog *>();
        QVERIFY(card);
        QVERIFY(card->isVisible());
        QCOMPARE(card->titleEdit()->text(), QString("Title"));
        QCOMPARE(card->descriptionEdit()->toPlainText(), QString("The description"));
        card->reject();
    }

    void cardEditsTask()
    {
        Fixture f;
        const QString id = f.store()->addTask("Title", "old");
        TaskDialog *card = f.window->openTaskCard(id);
        QVERIFY(card);
        card->titleEdit()->setText("New title");
        card->descriptionEdit()->setPlainText("new description");
        card->accept();
        QCOMPARE(f.store()->task(id)->title, QString("New title"));
        QCOMPARE(f.store()->task(id)->description, QString("new description"));
        QCOMPARE(f.shown(), QStringList({"New title"}));
        QCOMPARE(f.savedTitles(), QStringList({"New title"}));
    }

    void cardRejectsEmptyTitleAndCancelKeepsTask()
    {
        Fixture f;
        const QString id = f.store()->addTask("Title", "desc");
        TaskDialog *card = f.window->openTaskCard(id);
        card->titleEdit()->clear();
        card->accept();
        QVERIFY(card->isVisible());
        QCOMPARE(f.store()->task(id)->title, QString("Title"));
        card->titleEdit()->setText("Changed");
        card->reject();
        QCOMPARE(f.store()->task(id)->title, QString("Title"));
    }

    void cardAssignsLabels()
    {
        Fixture f;
        const QString work = f.store()->addLabel("Work", 0);
        const QString home = f.store()->addLabel("Home", 1);
        const QString id = f.store()->addTask("Title");
        f.store()->setTaskLabels(id, {work});

        TaskDialog *card = f.window->openTaskCard(id);
        QCOMPARE(card->labelList()->count(), 2);
        QCOMPARE(card->labelList()->item(0)->checkState(), Qt::Checked);
        QCOMPARE(card->labelList()->item(1)->checkState(), Qt::Unchecked);
        card->labelList()->item(0)->setCheckState(Qt::Unchecked);
        card->labelList()->item(1)->setCheckState(Qt::Checked);
        card->accept();
        QCOMPARE(f.store()->task(id)->labelIds, QStringList({home}));
    }

    // --- selection and state changes ---

    void checkboxSelectsWithoutOpeningCard()
    {
        Fixture f;
        f.store()->addTask("A");
        f.store()->addTask("B");
        QVERIFY(!f.window->completeAction()->isEnabled());
        f.clickCheckbox(1);
        QVERIFY(!f.window->findChild<TaskDialog *>());
        QCOMPARE(f.view()->checkedIds().size(), 1);
        QVERIFY(f.window->completeAction()->isEnabled());
        f.clickCheckbox(1);
        QVERIFY(f.view()->checkedIds().isEmpty());
        QVERIFY(!f.window->completeAction()->isEnabled());
    }

    void completeSingleTask()
    {
        Fixture f;
        f.add("A");
        f.add("B");
        f.clickCheckbox(0);
        f.window->completeAction()->trigger();
        QCOMPARE(f.shown(), QStringList({"B"}));
        QCOMPARE(f.shown(TaskState::Completed), QStringList({"A"}));
    }

    void batchComplete()
    {
        Fixture f;
        f.add("A");
        f.add("B");
        f.add("C");
        f.clickCheckbox(0);
        f.clickCheckbox(2);
        f.window->completeAction()->trigger();
        QCOMPARE(f.shown(), QStringList({"B"}));
        QCOMPARE(f.shown(TaskState::Completed), QStringList({"A", "C"}));
        QVERIFY(f.view()->checkedIds().isEmpty());
    }

    void batchCancel()
    {
        Fixture f;
        f.add("A");
        f.add("B");
        f.add("C");
        f.clickCheckbox(0);
        f.clickCheckbox(1);
        f.window->cancelAction()->trigger();
        QCOMPARE(f.shown(), QStringList({"C"}));
        QCOMPARE(f.shown(TaskState::Cancelled), QStringList({"A", "B"}));
    }

    void batchDeleteNeedsConfirmation()
    {
        Fixture f;
        f.add("A");
        f.add("B");
        f.add("C");
        f.clickCheckbox(0);
        f.clickCheckbox(1);

        f.confirmAnswer = false;
        f.window->deleteAction()->trigger();
        QCOMPARE(f.confirmCalls, 1);
        QCOMPARE(f.confirmCount, 2);
        QCOMPARE(f.shown(), QStringList({"A", "B", "C"}));

        f.confirmAnswer = true;
        f.window->deleteAction()->trigger();
        QCOMPARE(f.confirmCalls, 2);
        QCOMPARE(f.shown(), QStringList({"C"}));
        QCOMPARE(f.savedTitles(), QStringList({"C"})); // gone for good
        for (TaskState state : {TaskState::Postponed, TaskState::Completed, TaskState::Cancelled}) {
            QVERIFY(f.shown(state).isEmpty());
        }
    }

    void singleDeleteIsImmediateAndPermanent()
    {
        Fixture f;
        f.add("A");
        f.add("B");
        f.clickCheckbox(0);
        f.window->deleteAction()->trigger();
        QCOMPARE(f.confirmCalls, 0);
        QCOMPARE(f.shown(), QStringList({"B"}));
        QCOMPARE(f.savedTitles(), QStringList({"B"}));
    }

    void tabsShowTasksByState()
    {
        Fixture f;
        QCOMPARE(f.window->tabs()->count(), 4);
        const QString a = f.store()->addTask("A");
        const QString b = f.store()->addTask("B");
        const QString c = f.store()->addTask("C");
        f.store()->addTask("D");
        f.store()->complete({a});
        f.store()->cancel({b});
        f.store()->postpone({c}, fixedNow.addDays(2));

        QCOMPARE(f.shown(), QStringList({"D"}));
        QCOMPARE(f.shown(TaskState::Completed), QStringList({"A"}));
        QCOMPARE(f.shown(TaskState::Cancelled), QStringList({"B"}));
        QCOMPARE(f.shown(TaskState::Postponed).size(), 1);
        QVERIFY(f.shown(TaskState::Postponed).at(0).startsWith("C"));
        QCOMPARE(f.window->tabs()->tabText(0), QString("Tasks (1)"));
        QCOMPARE(f.window->tabs()->tabText(1), QString("Postponed (1)"));
        QCOMPARE(f.window->tabs()->tabText(2), QString("Completed (1)"));
        QCOMPARE(f.window->tabs()->tabText(3), QString("Cancelled (1)"));
    }

    void restoreFromCancelledAndCompletedTabs()
    {
        Fixture f;
        const QString a = f.store()->addTask("A");
        const QString b = f.store()->addTask("B");
        f.store()->cancel({a});
        f.store()->complete({b});

        QVERIFY(!f.window->restoreAction()->isEnabled());
        f.showTab(TaskState::Cancelled);
        f.clickCheckbox(0, TaskState::Cancelled);
        QVERIFY(f.window->restoreAction()->isEnabled());
        QVERIFY(!f.window->cancelAction()->isEnabled());
        f.window->restoreAction()->trigger();

        f.showTab(TaskState::Completed);
        f.clickCheckbox(0, TaskState::Completed);
        f.window->restoreAction()->trigger();

        QCOMPARE(f.shown(), QStringList({"A", "B"}));
        QVERIFY(f.shown(TaskState::Cancelled).isEmpty());
        QVERIFY(f.shown(TaskState::Completed).isEmpty());
    }

    // --- postponing ---

    void postponeWithPresetHidesTaskUntilDue()
    {
        Fixture f;
        f.add("Later");
        f.add("Now");
        f.clickCheckbox(0);
        f.window->postponeAction(PostponePreset::ThreeDays)->trigger();

        QCOMPARE(f.shown(), QStringList({"Now"}));
        QCOMPARE(f.view(TaskState::Postponed)->count(), 1);
        const Task task = f.store()->tasksIn(TaskState::Postponed).at(0);
        QCOMPARE(task.postponedUntil, QDate(2026, 11, 15).startOfDay());

        // Opened too early: still hidden.
        f.now = QDateTime(QDate(2026, 11, 14), QTime(23, 0));
        f.open();
        QCOMPARE(f.shown(), QStringList({"Now"}));
        QCOMPARE(f.view(TaskState::Postponed)->count(), 1);

        // Opened once the time has come: back on the list.
        f.now = QDateTime(QDate(2026, 11, 15), QTime(8, 0));
        f.open();
        QCOMPARE(f.shown(), QStringList({"Later", "Now"}));
        QCOMPARE(f.view(TaskState::Postponed)->count(), 0);
        QCOMPARE(f.savedTitles().size(), 2);
    }

    void postponeWithCustomTime()
    {
        Fixture f;
        f.add("A");
        f.clickCheckbox(0);
        f.customPostpone = QDateTime(); // dialog cancelled
        f.window->postponeCustomAction()->trigger();
        QCOMPARE(f.shown(), QStringList({"A"}));

        f.customPostpone = QDateTime(QDate(2026, 12, 24), QTime(18, 0));
        f.window->postponeCustomAction()->trigger();
        QVERIFY(f.shown().isEmpty());
        QCOMPARE(f.store()->tasksIn(TaskState::Postponed).at(0).postponedUntil, f.customPostpone);
    }

    void postponedTaskCanBeRestoredManually()
    {
        Fixture f;
        const QString a = f.store()->addTask("A");
        f.store()->postpone({a}, fixedNow.addDays(5));
        f.showTab(TaskState::Postponed);
        f.clickCheckbox(0, TaskState::Postponed);
        f.window->restoreAction()->trigger();
        QCOMPARE(f.shown(), QStringList({"A"}));
        QCOMPARE(f.view(TaskState::Postponed)->count(), 0);
    }

    // --- reordering ---

    void reorderWithKeyboard()
    {
        Fixture f;
        f.add("A");
        f.add("B");
        f.add("C");
        f.view()->setCurrentRow(2);
        QTest::keyClick(f.view(), Qt::Key_Up, Qt::ControlModifier);
        QCOMPARE(f.shown(), QStringList({"A", "C", "B"}));
        QCOMPARE(f.view()->currentRow(), 1); // the moved task stays current
        QTest::keyClick(f.view(), Qt::Key_Up, Qt::ControlModifier);
        QCOMPARE(f.shown(), QStringList({"C", "A", "B"}));
        QTest::keyClick(f.view(), Qt::Key_Up, Qt::ControlModifier); // already first
        QCOMPARE(f.shown(), QStringList({"C", "A", "B"}));
        QTest::keyClick(f.view(), Qt::Key_Down, Qt::ControlModifier);
        QCOMPARE(f.shown(), QStringList({"A", "C", "B"}));
        QCOMPARE(f.savedTitles(), QStringList({"A", "C", "B"}));
    }

    void reorderByDragAndDrop()
    {
        Fixture f;
        f.add("A");
        f.add("B");
        f.add("C");
        QCOMPARE(f.view()->dragDropMode(), QAbstractItemView::InternalMove);
        QCOMPARE(f.view(TaskState::Completed)->dragDropMode(), QAbstractItemView::NoDragDrop);

        // What a finished drag leaves behind: the row moved inside the widget.
        f.view()->insertItem(0, f.view()->takeItem(2));
        f.view()->applyVisualOrder();
        QCOMPARE(f.shown(), QStringList({"C", "A", "B"}));
        QCOMPARE(f.savedTitles(), QStringList({"C", "A", "B"}));
        f.open();
        QCOMPARE(f.shown(), QStringList({"C", "A", "B"}));
    }

    // --- task card state buttons ---

    void cardCompletesTask()
    {
        Fixture f;
        const QString id = f.store()->addTask("Title");
        f.add("Other");
        f.clickTitle(0);
        auto *card = f.window->findChild<TaskDialog *>();
        QVERIFY(card);
        card->titleEdit()->setText("Edited"); // edits are kept
        card->completeButton()->click();
        QVERIFY(!card->isVisible());
        QCOMPARE(f.store()->task(id)->state, TaskState::Completed);
        QCOMPARE(f.shown(), QStringList({"Other"}));
        QCOMPARE(f.shown(TaskState::Completed), QStringList({"Edited"}));
    }

    void cardCancelsTask()
    {
        Fixture f;
        const QString id = f.store()->addTask("Title");
        TaskDialog *card = f.window->openTaskCard(id);
        card->cancelTaskButton()->click();
        QVERIFY(!card->isVisible());
        QCOMPARE(f.store()->task(id)->state, TaskState::Cancelled);
        QCOMPARE(f.shown(TaskState::Cancelled), QStringList({"Title"}));
    }

    void cardPostponesTask()
    {
        Fixture f;
        const QString a = f.store()->addTask("A");
        const QString b = f.store()->addTask("B");

        TaskDialog *card = f.window->openTaskCard(a);
        QVERIFY(card->postponeButton()->menu());
        card->postponeAction(PostponePreset::OneWeek)->trigger();
        QVERIFY(!card->isVisible());
        QCOMPARE(f.store()->task(a)->state, TaskState::Postponed);
        QCOMPARE(f.store()->task(a)->postponedUntil, QDate(2026, 11, 19).startOfDay());

        card = f.window->openTaskCard(b);
        f.customPostpone = QDateTime(); // time dialog cancelled: nothing happens
        card->postponeCustomAction()->trigger();
        QVERIFY(card->isVisible());
        QCOMPARE(f.store()->task(b)->state, TaskState::Active);
        f.customPostpone = QDateTime(QDate(2027, 1, 1), QTime(9, 0));
        card->postponeCustomAction()->trigger();
        QVERIFY(!card->isVisible());
        QCOMPARE(f.store()->task(b)->postponedUntil, f.customPostpone);
        QVERIFY(f.shown().isEmpty());
    }

    void cardStateButtonsFollowTheTaskState()
    {
        Fixture f;
        const QString done = f.store()->addTask("Done");
        const QString cancelled = f.store()->addTask("Cancelled");
        f.store()->complete({done});
        f.store()->cancel({cancelled});

        TaskDialog *card = f.window->openTaskCard(done);
        QVERIFY(!card->completeButton()->isEnabled());
        QVERIFY(card->cancelTaskButton()->isEnabled());
        QVERIFY(!card->postponeButton()->isEnabled());
        card->reject();

        card = f.window->openTaskCard(cancelled);
        QVERIFY(card->completeButton()->isEnabled());
        QVERIFY(!card->cancelTaskButton()->isEnabled());
        card->completeButton()->click();
        QCOMPARE(f.store()->task(cancelled)->state, TaskState::Completed);
    }

    void cardStateButtonNeedsATitle()
    {
        Fixture f;
        const QString id = f.store()->addTask("Title");
        TaskDialog *card = f.window->openTaskCard(id);
        card->titleEdit()->clear();
        card->completeButton()->click();
        QVERIFY(card->isVisible());
        QCOMPARE(f.store()->task(id)->state, TaskState::Active);
        card->reject();
    }

    // --- select all ---

    void ctrlASelectsAllTasks()
    {
        Fixture f;
        f.add("A");
        f.add("B");
        f.add("C");
        f.view()->setFocus();
        QTest::keyClick(f.view(), Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(f.view()->checkedIds().size(), 3);
        QVERIFY(f.window->completeAction()->isEnabled());
        f.window->completeAction()->trigger();
        QVERIFY(f.shown().isEmpty());
        QCOMPARE(f.shown(TaskState::Completed), QStringList({"A", "B", "C"}));
    }

    void ctrlAInEmptyInputSelectsAllTasks()
    {
        Fixture f;
        f.add("A");
        f.add("B");
        f.window->newTaskEdit()->setFocus();
        QTest::keyClick(f.window->newTaskEdit(), Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(f.view()->checkedIds().size(), 2);
    }

    void ctrlAInInputWithTextSelectsTheText()
    {
        Fixture f;
        f.add("A");
        f.window->newTaskEdit()->setFocus();
        QTest::keyClicks(f.window->newTaskEdit(), "draft");
        QTest::keyClick(f.window->newTaskEdit(), Qt::Key_A, Qt::ControlModifier);
        QVERIFY(f.view()->checkedIds().isEmpty());
        QCOMPARE(f.window->newTaskEdit()->textCursor().selectedText(), QString("draft"));
    }

    void ctrlASelectsOnlyTheCurrentTab()
    {
        Fixture f;
        f.add("A");
        const QString done = f.store()->addTask("Done");
        f.store()->complete({done});
        f.showTab(TaskState::Completed);
        f.view(TaskState::Completed)->setFocus();
        QTest::keyClick(f.view(TaskState::Completed), Qt::Key_A, Qt::ControlModifier);
        QCOMPARE(f.view(TaskState::Completed)->checkedIds(), QStringList({done}));
        QVERIFY(f.view()->checkedIds().isEmpty());
    }

    // --- window layout ---

    void toolbarButtonsAllFitInTheInitialWindow()
    {
        Fixture f;
        auto *toolbar = f.window->findChild<QToolBar *>("mainToolBar");
        QVERIFY(toolbar);
        const QList<QAction *> actions = {f.window->completeAction(), f.window->cancelAction(), f.window->postponeAction(), f.window->restoreAction(),
            f.window->deleteAction(), f.window->labelsDialogAction(), f.window->storageFolderAction()};
        for (QAction *action : actions) {
            QWidget *button = toolbar->widgetForAction(action);
            QVERIFY2(button, qPrintable(action->text()));
            QVERIFY2(button->isVisible(), qPrintable(action->text()));
            QVERIFY2(toolbar->rect().contains(button->geometry()), qPrintable(action->text()));
        }
        QVERIFY(!toolbar->findChild<QWidget *>("qt_toolbar_ext_button")->isVisible());
    }

    void tasksUseLargerFontAndSpacing()
    {
        Fixture f;
        f.add("A");
        f.add("B");
        const int base = QApplication::font().pointSize();
        QCOMPARE(f.view()->font().pointSize(), base + 2);
        QCOMPARE(f.window->newTaskEdit()->font().pointSize(), base + 2);
        QCOMPARE(f.view()->spacing(), 4);
        const QRect first = f.view()->visualRect(f.view()->model()->index(0, 0));
        const QRect second = f.view()->visualRect(f.view()->model()->index(1, 0));
        QVERIFY(second.top() - first.bottom() >= 8); // a gap between the rows
    }

    void thereIsNoMenuBar()
    {
        Fixture f;
        QVERIFY(!f.window->findChild<QMenuBar *>());
        QVERIFY(!f.window->findChild<KHelpMenu *>());
    }

    void labelsButtonOpensLabelsDialog()
    {
        Fixture f;
        QVERIFY(!f.window->findChild<LabelsDialog *>());
        f.window->labelsDialogAction()->trigger();
        auto *dialog = f.window->findChild<LabelsDialog *>();
        QVERIFY(dialog);
        QVERIFY(dialog->isVisible());
        dialog->reject();
    }

    void storageFolderButtonChangesTheFolder()
    {
        Fixture f;
        f.add("Task");
        QTemporaryDir other;
        f.chosenFolder = other.path();
        f.window->storageFolderAction()->trigger();
        QCOMPARE(f.window->storagePath(), other.filePath("todos.json"));
        QVERIFY(QFile::exists(other.filePath("todos.json")));
    }

    // --- labels ---

    void clickingFilterLabelsTheSelectedTasks()
    {
        Fixture f;
        const QString work = f.store()->addLabel("Work", 0);
        const QString home = f.store()->addLabel("Home", 1);
        const QString a = f.store()->addTask("A");
        const QString b = f.store()->addTask("B");
        const QString c = f.store()->addTask("C");
        f.store()->setTaskLabels(a, {work});

        f.clickCheckbox(0);
        f.clickCheckbox(1);

        // Not all selected tasks have it yet: it is added to those missing it.
        f.window->filterButton(work)->click();
        QCOMPARE(f.store()->task(a)->labelIds, QStringList({work}));
        QCOMPARE(f.store()->task(b)->labelIds, QStringList({work}));
        QVERIFY(f.store()->task(c)->labelIds.isEmpty());
        QCOMPARE(f.view()->checkedIds().size(), 2); // selection is kept

        // The lists are not filtered by that click.
        QVERIFY(f.window->labelFilter().isEmpty());
        QVERIFY(!f.window->filterButton(work)->isChecked());
        QCOMPARE(f.shown(), QStringList({"A", "B", "C"}));

        f.window->filterButton(home)->click();
        QCOMPARE(f.store()->task(a)->labelIds, QStringList({work, home}));

        // All selected tasks have it: it is removed from them.
        f.window->filterButton(work)->click();
        QCOMPARE(f.store()->task(a)->labelIds, QStringList({home}));
        QCOMPARE(f.store()->task(b)->labelIds, QStringList({home}));
        QVERIFY(f.window->labelFilter().isEmpty());
    }

    void filterStaysAsItWasWhenLabellingSelectedTasks()
    {
        Fixture f;
        const QString work = f.store()->addLabel("Work", 0);
        const QString home = f.store()->addLabel("Home", 1);
        const QString a = f.store()->addTask("A");
        f.store()->setTaskLabels(a, {work});
        f.add("B");

        f.window->filterButton(work)->click(); // nothing ticked: filters
        QCOMPARE(f.shown(), QStringList({"A"}));
        f.clickCheckbox(0);
        f.window->filterButton(home)->click(); // ticked: labels
        QCOMPARE(f.store()->task(a)->labelIds, QStringList({work, home}));
        QCOMPARE(f.window->labelFilter(), QSet<QString>({work}));
        QVERIFY(f.window->filterButton(work)->isChecked());
        QVERIFY(!f.window->filterButton(home)->isChecked());
        QCOMPARE(f.shown(), QStringList({"A"}));
    }

    void thereIsNoLabelToolbarButton()
    {
        Fixture f;
        f.store()->addLabel("Work", 0);
        auto *toolbar = f.window->findChild<QToolBar *>("mainToolBar");
        for (const QAction *action : toolbar->actions()) {
            QVERIFY(action->text() != QString("Label"));
        }
    }

    void filterByLabels()
    {
        Fixture f;
        const QString work = f.store()->addLabel("Work", 0);
        const QString home = f.store()->addLabel("Home", 1);
        f.store()->setTaskLabels(f.store()->addTask("W"), {work});
        f.store()->setTaskLabels(f.store()->addTask("H"), {home});
        f.store()->setTaskLabels(f.store()->addTask("WH"), {work, home});
        f.store()->addTask("None");
        const QString done = f.store()->addTask("Done work");
        f.store()->setTaskLabels(done, {work});
        f.store()->complete({done});

        QVERIFY(f.window->filterButton(work));
        QCOMPARE(f.window->filterButton(work)->text(), QString("Work"));
        QCOMPARE(f.shown().size(), 4);

        f.window->filterButton(work)->click();
        QCOMPARE(f.shown(), QStringList({"W", "WH"}));
        QCOMPARE(f.shown(TaskState::Completed), QStringList({"Done work"}));

        f.window->filterButton(home)->click(); // any of the selected labels
        QCOMPARE(f.shown(), QStringList({"W", "H", "WH"}));

        f.window->filterButton(work)->click();
        QCOMPARE(f.shown(), QStringList({"H", "WH"}));
        QVERIFY(f.shown(TaskState::Completed).isEmpty());

        f.window->filterButton(home)->click();
        QCOMPARE(f.shown().size(), 4);
    }

    void deletingLabelRemovesItsFilter()
    {
        Fixture f;
        const QString work = f.store()->addLabel("Work", 0);
        f.store()->setTaskLabels(f.store()->addTask("W"), {work});
        f.store()->addTask("None");
        f.window->filterButton(work)->click();
        QCOMPARE(f.shown(), QStringList({"W"}));
        f.store()->removeLabel(work);
        QVERIFY(!f.window->filterButton(work));
        QVERIFY(f.window->labelFilter().isEmpty());
        QCOMPARE(f.shown(), QStringList({"W", "None"}));
    }

    void labelsArePersistedInTheTodoFile()
    {
        Fixture f;
        const QString work = f.store()->addLabel("Work", 6);
        const QString id = f.store()->addTask("W");
        f.store()->setTaskLabels(id, {work});
        f.open();
        QCOMPARE(f.store()->labels().size(), 1);
        QCOMPARE(f.store()->labels().at(0).name, QString("Work"));
        QCOMPARE(f.store()->labels().at(0).color, 6);
        QCOMPARE(f.store()->task(id)->labelIds, QStringList({work}));
    }
};

QTEST_MAIN(TestMainWindow)
#include "test_mainwindow.moc"
