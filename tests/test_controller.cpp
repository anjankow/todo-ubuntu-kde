#include "testenv.h"

#include "app/trayicon.h"

#include <QSignalSpy>
#include <QStandardPaths>
#include <QTest>

class TestController : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void initTestCase() { QStandardPaths::setTestModeEnabled(true); }

    // --- storage location ---

    void homeDirectoryIsTheDefaultLocation()
    {
        QTemporaryDir dir;
        const Settings settings(dir.filePath("emptyrc"));
        QCOMPARE(settings.storageDir(), QDir::homePath());
    }

    void todosAreStoredAsTextFileInSelectedFolder()
    {
        TestEnv t;
        QVERIFY(t.app->addTask("Buy milk"));
        QCOMPARE(t.app->storageDir(), t.dir.path());
        QCOMPARE(t.app->storagePath(), t.dir.filePath("todos.json"));
        QVERIFY(QFile::exists(t.dir.filePath("todos.json")));
        QCOMPARE(t.savedTitles(), QStringList({"Buy milk"}));
    }

    void tasksSurviveRestart()
    {
        TestEnv t;
        t.add("One");
        t.add("Two");
        t.open();
        QCOMPARE(t.shown(), QStringList({"One", "Two"}));
    }

    void changingFolderMovesTheFile()
    {
        TestEnv t;
        t.add("Task");
        QTemporaryDir other;
        QSignalSpy spy(t.app.get(), &AppController::storageDirChanged);
        QCOMPARE(t.app->changeStorageDir(other.path()), AppController::FolderChanged);
        QCOMPARE(spy.count(), 1);
        QVERIFY(QFile::exists(other.filePath("todos.json")));
        QVERIFY(!QFile::exists(t.dir.filePath("todos.json")));
        QCOMPARE(Settings(t.configFile).storageDir(), other.path());

        t.add("Later");
        TodoStore disk;
        QVERIFY(Storage::load(disk, other.filePath("todos.json")));
        QCOMPARE(disk.tasks().size(), 2);

        t.open(); // the new location is remembered
        QCOMPARE(t.shown(), QStringList({"Task", "Later"}));
    }

    void changingFolderReportsAnExistingTodoFile()
    {
        QTemporaryDir other;
        TodoStore existing;
        existing.addTask("Existing");
        QVERIFY(Storage::save(existing, other.filePath("todos.json")));

        TestEnv t;
        t.add("Mine");
        QCOMPARE(t.app->changeStorageDir(other.path()), AppController::FolderConflict);
        QCOMPARE(t.app->storagePath(), t.dir.filePath("todos.json")); // nothing happened yet
        QCOMPARE(t.shown(), QStringList({"Mine"}));

        QCOMPARE(t.app->changeStorageDir(other.path(), AppController::LoadExisting), AppController::FolderChanged);
        QCOMPARE(t.shown(), QStringList({"Existing"}));
        QVERIFY(QFile::exists(t.dir.filePath("todos.json"))); // nothing destroyed
    }

    void changingFolderCanOverwriteExistingFile()
    {
        QTemporaryDir other;
        TodoStore existing;
        existing.addTask("Existing");
        QVERIFY(Storage::save(existing, other.filePath("todos.json")));

        TestEnv t;
        t.add("Mine");
        QCOMPARE(t.app->changeStorageDir(other.path(), AppController::Overwrite), AppController::FolderChanged);
        QCOMPARE(t.shown(), QStringList({"Mine"}));
        QVERIFY(!QFile::exists(t.dir.filePath("todos.json")));
        TodoStore disk;
        QVERIFY(Storage::load(disk, other.filePath("todos.json")));
        QCOMPARE(disk.tasks().size(), 1);
        QCOMPARE(disk.tasks().at(0).title, QString("Mine"));
    }

    void changingToMissingFolderFails()
    {
        TestEnv t;
        QCOMPARE(t.app->changeStorageDir(t.dir.filePath("does-not-exist")), AppController::FolderError);
        QVERIFY(!t.app->errorMessage().isEmpty());
        QCOMPARE(t.app->storagePath(), t.dir.filePath("todos.json"));
        t.app->clearError();
        QVERIFY(t.app->errorMessage().isEmpty());
    }

    void unreadableFileIsKeptAside()
    {
        TestEnv t;
        t.app.reset();
        QFile file(t.dir.filePath("todos.json"));
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("garbage");
        file.close();
        t.open();
        QVERIFY(!t.app->errorMessage().isEmpty());
        QVERIFY(t.shown().isEmpty());
        QCOMPARE(QDir(t.dir.path()).entryList({"todos.json.unreadable-*"}).size(), 1);
    }

    // --- tasks ---

    void newTaskGoesOnTopWithTitleAndDescription()
    {
        TestEnv t;
        t.add("Old");
        QVERIFY(t.app->addTask("Call mum\nabout Sunday\nand the cake"));
        QCOMPARE(t.shown(), QStringList({"Call mum", "Old"}));
        const Task task = t.store()->tasks().at(0);
        QCOMPARE(task.title, QString("Call mum"));
        QCOMPARE(task.description, QString("about Sunday\nand the cake"));
        QVERIFY(!t.app->addTask("  \n "));
        QCOMPARE(t.store()->tasks().size(), 2);
    }

    void listShowsTitlesAndCardDataHasTheRest()
    {
        TestEnv t;
        const QString work = t.store()->addLabel("Work", 5);
        const QString id = t.add("Title", "The description");
        t.store()->setTaskLabels(id, {work});
        TaskListModel *model = t.list();
        QCOMPARE(model->data(model->index(0), TaskListModel::TitleRole).toString(), QString("Title"));
        QCOMPARE(model->data(model->index(0), TaskListModel::LabelColorsRole).toStringList().size(), 1);

        const QVariantMap card = t.app->task(id);
        QCOMPARE(card.value("title").toString(), QString("Title"));
        QCOMPARE(card.value("description").toString(), QString("The description"));
        QCOMPARE(card.value("state").toString(), QString("active"));
        QCOMPARE(card.value("labelIds").toStringList(), QStringList({work}));
        QVERIFY(t.app->task("missing").isEmpty());
    }

    void updateTask()
    {
        TestEnv t;
        const QString work = t.store()->addLabel("Work", 0);
        const QString id = t.add("Title", "old");
        QVERIFY(t.app->updateTask(id, "New title", "new description", {work}));
        QCOMPARE(t.store()->task(id)->title, QString("New title"));
        QCOMPARE(t.store()->task(id)->description, QString("new description"));
        QCOMPARE(t.store()->task(id)->labelIds, QStringList({work}));
        QCOMPARE(t.savedTitles(), QStringList({"New title"}));
        QVERIFY(!t.app->updateTask(id, "  ", "x", {})); // a task needs a title
        QCOMPARE(t.store()->task(id)->title, QString("New title"));
    }

    void tabsShowTasksByState()
    {
        TestEnv t;
        const QString a = t.add("A");
        const QString b = t.add("B");
        const QString c = t.add("C");
        t.add("D");
        t.app->complete({a});
        t.app->cancel({b});
        t.app->postponeUntil({c}, fixedNow.addDays(2));

        QCOMPARE(t.shown(), QStringList({"D"}));
        QCOMPARE(t.shown(TaskState::Completed), QStringList({"A"}));
        QCOMPARE(t.shown(TaskState::Cancelled), QStringList({"B"}));
        QCOMPARE(t.shown(TaskState::Postponed), QStringList({"C"}));
        QVERIFY(!t.list(TaskState::Postponed)->data(t.list(TaskState::Postponed)->index(0), TaskListModel::PostponedTextRole).toString().isEmpty());
        QCOMPARE(t.list()->totalCount(), 1);
        QCOMPARE(t.app->currentModel(), t.list());
        t.app->setCurrentTab(AppController::CompletedTab);
        QCOMPARE(t.app->currentModel(), t.list(TaskState::Completed));
    }

    // --- ticking and the toolbar actions ---

    void actionsNeedTickedOrHighlightedTasks()
    {
        TestEnv t;
        const QString a = t.add("A");
        QVERIFY(!t.app->canComplete());
        QVERIFY(!t.app->canDelete());
        QSignalSpy spy(t.app.get(), &AppController::actionsChanged);
        t.list()->setChecked(a, true);
        QVERIFY(spy.count() > 0);
        QVERIFY(t.app->canComplete());
        QVERIFY(t.app->canCancel());
        QVERIFY(t.app->canPostpone());
        QVERIFY(t.app->canDelete());
        QVERIFY(!t.app->canRestore()); // already on the list
        t.list()->setChecked(a, false);
        QVERIFY(!t.app->canComplete());

        spy.clear();
        t.list()->setCurrentId(a); // highlighted is enough
        QVERIFY(spy.count() > 0);
        QVERIFY(t.app->canComplete());
        QVERIFY(t.app->canDelete());
        t.list()->setCurrentId(QString());
        QVERIFY(!t.app->canComplete());
    }

    void actionsWorkOnTheHighlightedTask()
    {
        TestEnv t;
        const QString a = t.add("A");
        const QString b = t.add("B");
        const QString c = t.add("C");
        const QString d = t.add("D");
        t.list()->setCurrentId(a);
        t.app->completeSelected();
        QCOMPARE(t.store()->task(a)->state, TaskState::Completed);
        t.list()->setCurrentId(b);
        t.app->postponeSelected(AppController::Tomorrow);
        QCOMPARE(t.store()->task(b)->state, TaskState::Postponed);
        t.list()->setCurrentId(c);
        t.app->deleteSelected(); // a single task: no confirmation
        QVERIFY(!t.store()->task(c));

        t.app->setCurrentTab(AppController::CompletedTab);
        t.app->currentModel()->setCurrentId(a);
        QVERIFY(t.app->canRestore());
        t.app->restoreSelected();
        QCOMPARE(t.shown(), QStringList({"A", "D"}));

        // Ticked tasks win over the highlight.
        t.app->setCurrentTab(AppController::ActiveTab);
        t.list()->setCurrentId(a);
        t.list()->setChecked(d, true);
        t.app->cancelSelected();
        QCOMPARE(t.store()->task(d)->state, TaskState::Cancelled);
        QCOMPARE(t.store()->task(a)->state, TaskState::Active);
    }

    void batchCompleteAndCancel()
    {
        TestEnv t;
        const QString a = t.add("A");
        t.add("B");
        const QString c = t.add("C");
        const QString d = t.add("D");
        t.list()->setChecked(a, true);
        t.list()->setChecked(c, true);
        t.app->completeSelected();
        QCOMPARE(t.shown(), QStringList({"B", "D"}));
        QCOMPARE(t.shown(TaskState::Completed), QStringList({"A", "C"}));
        QCOMPARE(t.list()->checkedCount(), 0);

        t.list()->checkAll();
        t.app->cancelSelected();
        QVERIFY(t.shown().isEmpty());
        QCOMPARE(t.shown(TaskState::Cancelled), QStringList({"B", "D"}));
        Q_UNUSED(d)
    }

    void batchDeleteNeedsConfirmation()
    {
        TestEnv t;
        const QString a = t.add("A");
        const QString b = t.add("B");
        t.add("C");
        t.list()->setChecked(a, true);
        t.list()->setChecked(b, true);

        QSignalSpy spy(t.app.get(), &AppController::deleteConfirmationRequested);
        t.app->deleteSelected();
        QCOMPARE(spy.count(), 1);
        QCOMPARE(spy.at(0).at(0).toInt(), 2);
        QCOMPARE(t.shown(), QStringList({"A", "B", "C"})); // nothing deleted without the confirmation

        t.app->confirmDelete();
        QCOMPARE(t.shown(), QStringList({"C"}));
        QCOMPARE(t.savedTitles(), QStringList({"C"})); // gone for good
        for (TaskState state : {TaskState::Postponed, TaskState::Completed, TaskState::Cancelled}) {
            QVERIFY(t.shown(state).isEmpty());
        }
    }

    void singleDeleteIsImmediateAndPermanent()
    {
        TestEnv t;
        const QString a = t.add("A");
        t.add("B");
        t.list()->setChecked(a, true);
        QSignalSpy spy(t.app.get(), &AppController::deleteConfirmationRequested);
        t.app->deleteSelected();
        QCOMPARE(spy.count(), 0);
        QCOMPARE(t.shown(), QStringList({"B"}));
        QCOMPARE(t.savedTitles(), QStringList({"B"}));
    }

    void restoreFromOtherTabs()
    {
        TestEnv t;
        const QString a = t.add("A");
        const QString b = t.add("B");
        const QString c = t.add("C");
        t.app->cancel({a});
        t.app->complete({b});
        t.app->postponeUntil({c}, fixedNow.addDays(5));

        const QList<QPair<int, QString>> tabs = {{AppController::CancelledTab, a}, {AppController::CompletedTab, b}, {AppController::PostponedTab, c}};
        for (const auto &tab : tabs) {
            t.app->setCurrentTab(tab.first);
            t.app->currentModel()->setChecked(tab.second, true);
            QVERIFY(t.app->canRestore());
            t.app->restoreSelected();
        }
        QCOMPARE(t.shown(), QStringList({"A", "B", "C"}));
        QVERIFY(!t.app->canCancel()); // nothing selected any more
    }

    void selectAllTicksTheCurrentTabOnly()
    {
        TestEnv t;
        t.add("A");
        t.add("B");
        const QString done = t.add("Done");
        t.app->complete({done});
        t.app->currentModel()->checkAll();
        QCOMPARE(t.list()->checkedCount(), 2);
        QCOMPARE(t.list(TaskState::Completed)->checkedCount(), 0);
    }

    // --- postponing ---

    void postponeWithPresetHidesTaskUntilDue()
    {
        TestEnv t;
        const QString later = t.add("Later");
        t.add("Now");
        t.list()->setChecked(later, true);
        t.app->postponeSelected(AppController::ThreeDays);

        QCOMPARE(t.shown(), QStringList({"Now"}));
        QCOMPARE(t.shown(TaskState::Postponed), QStringList({"Later"}));
        QCOMPARE(t.store()->task(later)->postponedUntil, QDate(2026, 11, 15).startOfDay());

        t.now = QDateTime(QDate(2026, 11, 14), QTime(23, 0)); // opened too early: still hidden
        t.open();
        QCOMPARE(t.shown(), QStringList({"Now"}));
        QCOMPARE(t.shown(TaskState::Postponed), QStringList({"Later"}));

        t.now = QDateTime(QDate(2026, 11, 15), QTime(8, 0)); // opened once the time has come
        t.open();
        QCOMPARE(t.shown(), QStringList({"Later", "Now"}));
        QVERIFY(t.shown(TaskState::Postponed).isEmpty());
        QCOMPARE(t.savedTitles().size(), 2);
    }

    void postponePresetsAndCustomTime()
    {
        TestEnv t;
        const QString a = t.add("A");
        const QString b = t.add("B");
        const QString c = t.add("C");
        const QString d = t.add("D");
        const QString e = t.add("E");
        t.app->postpone({a}, AppController::Tomorrow);
        t.app->postpone({b}, AppController::OneWeek);
        t.app->postpone({c}, AppController::OneMonth);
        QCOMPARE(t.store()->task(a)->postponedUntil, QDate(2026, 11, 13).startOfDay());
        QCOMPARE(t.store()->task(b)->postponedUntil, QDate(2026, 11, 19).startOfDay());
        QCOMPARE(t.store()->task(c)->postponedUntil, QDate(2026, 12, 12).startOfDay());

        const QDateTime custom(QDate(2026, 12, 24), QTime(18, 0));
        t.list()->setChecked(d, true);
        t.app->postponeSelectedUntil(custom);
        QCOMPARE(t.store()->task(d)->postponedUntil, custom);
        t.app->postponeUntil({e}, QDateTime()); // no time: nothing happens
        QCOMPARE(t.store()->task(e)->state, TaskState::Active);
        QCOMPARE(t.app->suggestedPostponeTime(), fixedNow.addDays(1));
    }

    // --- tray popup ---

    void topTasksAreTheFirstFiveOfTheTasksList()
    {
        TestEnv t;
        QVERIFY(t.app->topTasks().isEmpty());
        const QString work = t.store()->addLabel("Work", 0);
        QStringList ids;
        for (const char *title : {"A", "B", "C", "D", "E", "F", "G"}) {
            ids.append(t.add(title));
        }
        t.app->cancel({ids.at(1)}); // B is not on the Tasks list any more
        QSignalSpy spy(t.app.get(), &AppController::topTasksChanged);

        auto titles = [&t] {
            QStringList result;
            const QVariantList tasks = t.app->topTasks();
            for (const QVariant &task : tasks) {
                result.append(task.toMap().value("title").toString());
            }
            return result;
        };
        QCOMPARE(titles(), QStringList({"A", "C", "D", "E", "F"}));
        QCOMPARE(t.app->topTasks().at(0).toMap().value("id").toString(), ids.at(0));

        t.app->complete({ids.at(0)}); // completing one lets the next move up
        QVERIFY(spy.count() > 0);
        QCOMPARE(titles(), QStringList({"C", "D", "E", "F", "G"}));

        t.app->labelClicked(work); // the label filter of the window does not matter
        QVERIFY(t.shown().isEmpty());
        QCOMPARE(titles().size(), 5);
    }

    void trayPopupOpensNextToTheIconInsideTheScreen()
    {
        const QRect screen(0, 0, 1920, 1080);
        const QSize popup(300, 200);
        // Panel at the bottom: above the icon, centred on it.
        QCOMPARE(trayPopupPosition(QRect(1700, 1050, 24, 24), popup, screen), QPoint(1561, 846));
        // Panel at the top: below the icon.
        QCOMPARE(trayPopupPosition(QRect(1700, 4, 24, 24), popup, screen), QPoint(1561, 32));
        // Icon in the corner: kept on the screen.
        const QPoint corner = trayPopupPosition(QRect(1900, 1060, 20, 20), popup, screen);
        QCOMPARE(corner.x(), 1620);
        QVERIFY(QRect(corner, popup).bottom() < 1060);
        QVERIFY(screen.contains(QRect(corner, popup)));
        // Only the pointer position is known.
        QVERIFY(screen.contains(QRect(trayPopupPosition(QRect(QPoint(5, 1075), QSize(1, 1)), popup, screen), popup)));
    }

    // --- bringing postponed tasks back while running ---

    void releaseTimerFollowsTheEarliestPostponedTask()
    {
        TestEnv t;
        QVERIFY(!t.app->releaseTimerActive());
        const QString a = t.add("A");
        const QString b = t.add("B");
        t.app->postponeUntil({a}, fixedNow.addSecs(90));
        QVERIFY(t.app->releaseTimerActive()); // set when a task is postponed
        QCOMPARE(t.app->nextReleaseAt(), fixedNow.addSecs(90));
        QCOMPARE(t.app->releaseTimerInterval(), 90 * 1000);
        t.app->postponeUntil({b}, fixedNow.addDays(3)); // far away: looks again after a while
        QCOMPARE(t.app->releaseTimerInterval(), 90 * 1000);
        t.store()->restore({a});
        QCOMPARE(t.app->releaseTimerInterval(), AppController::maxReleaseWaitMs);

        t.now = fixedNow.addDays(3);
        t.app->releaseDue();
        QCOMPARE(t.shown(), QStringList({"A", "B"}));
        QVERIFY(t.store()->task(b)->returned);
        QVERIFY(!t.app->releaseTimerActive()); // nothing postponed any more
        QCOMPARE(t.list()->data(t.list()->index(1), TaskListModel::ReturnedRole).toBool(), true);

        t.app->dismissReturned(b);
        QVERIFY(!t.store()->task(b)->returned);
        QCOMPARE(t.savedTitles(), QStringList({"A", "B"}));
    }

    void timerIsSetWhenTheApplicationStarts()
    {
        TestEnv t;
        const QString a = t.add("A");
        t.app->postponeUntil({a}, fixedNow.addSecs(30));
        t.open();
        QVERIFY(t.app->releaseTimerActive());
        QCOMPARE(t.app->releaseTimerInterval(), 30 * 1000);
    }

    void postponedTaskComesBackAtItsTimeWhileRunning()
    {
        QTemporaryDir dir;
        const QString config = dir.filePath("kdetodorc");
        Settings(config).setStorageDir(dir.path());
        AppController app(config); // the real clock
        const QString a = app.store()->addTask("A");
        app.postponeUntil({a}, QDateTime::currentDateTime().addMSecs(300));
        QCOMPARE(app.store()->task(a)->state, TaskState::Postponed);
        QTRY_COMPARE_WITH_TIMEOUT(app.store()->task(a)->state, TaskState::Active, 5000);
        QVERIFY(app.store()->task(a)->returned);
    }

    // --- shortcuts the user can change ---

    void shortcutsHaveTheirDefaults()
    {
        TestEnv t;
        const QVariantList list = t.app->configurableShortcuts();
        QCOMPARE(list.size(), 4);
        const QList<QPair<QString, QString>> expected = {{"complete", "K"}, {"cancel", "Del"}, {"postpone", "P"}, {"restore", "R"}};
        for (int i = 0; i < expected.size(); ++i) {
            const QVariantMap entry = list.at(i).toMap();
            QCOMPARE(entry.value("action").toString(), expected.at(i).first);
            QCOMPARE(entry.value("keys").toString(), expected.at(i).second);
            QCOMPARE(entry.value("defaultKeys").toString(), expected.at(i).second);
            QVERIFY(entry.value("isDefault").toBool());
            QVERIFY(!entry.value("name").toString().isEmpty());
            QVERIFY(!entry.value("name").toString().contains("…"));
        }
    }

    void keyTextIsPortableAndIgnoresBareModifiers()
    {
        TestEnv t;
        QCOMPARE(t.app->keyText(Qt::Key_K, Qt::NoModifier), QString("K"));
        QCOMPARE(t.app->keyText(Qt::Key_Delete, Qt::NoModifier), QString("Del"));
        QCOMPARE(t.app->keyText(Qt::Key_K, Qt::ControlModifier), QString("Ctrl+K"));
        QCOMPARE(t.app->keyText(Qt::Key_K, Qt::ControlModifier | Qt::ShiftModifier), QString("Ctrl+Shift+K"));
        QCOMPARE(t.app->keyText(Qt::Key_K, Qt::KeypadModifier), QString("K")); // keypad flag is not part of it
        QCOMPARE(t.app->keyText(Qt::Key_F5, Qt::NoModifier), QString("F5"));
        for (int key : {Qt::Key_Shift, Qt::Key_Control, Qt::Key_Alt, Qt::Key_Meta, Qt::Key_CapsLock, Qt::Key_unknown}) {
            QVERIFY(t.app->keyText(key, Qt::NoModifier).isEmpty());
        }
    }

    void actionForKeyFollowsTheShortcuts()
    {
        TestEnv t;
        QCOMPARE(t.app->actionForKey(Qt::Key_K, Qt::NoModifier), QString("complete"));
        QCOMPARE(t.app->actionForKey(Qt::Key_Delete, Qt::NoModifier), QString("cancel"));
        QCOMPARE(t.app->actionForKey(Qt::Key_P, Qt::NoModifier), QString("postpone"));
        QCOMPARE(t.app->actionForKey(Qt::Key_R, Qt::NoModifier), QString("restore"));
        QVERIFY(t.app->actionForKey(Qt::Key_K, Qt::ControlModifier).isEmpty()); // a different combination
        QVERIFY(t.app->actionForKey(Qt::Key_Delete, Qt::ShiftModifier).isEmpty()); // Shift+Delete deletes
        QVERIFY(t.app->actionForKey(Qt::Key_X, Qt::NoModifier).isEmpty());
        QVERIFY(t.app->actionForKey(Qt::Key_Shift, Qt::NoModifier).isEmpty());

        QVERIFY(t.app->setShortcut("complete", "Ctrl+K").isEmpty());
        QCOMPARE(t.app->actionForKey(Qt::Key_K, Qt::ControlModifier), QString("complete"));
        QVERIFY(t.app->actionForKey(Qt::Key_K, Qt::NoModifier).isEmpty()); // the old key is free
    }

    void shortcutsCanBeChangedAndAreKept()
    {
        TestEnv t;
        QSignalSpy spy(t.app.get(), &AppController::shortcutsChanged);
        QVERIFY(t.app->setShortcut("complete", "X").isEmpty());
        QVERIFY(t.app->setShortcut("postpone", "Ctrl+P").isEmpty());
        QVERIFY(t.app->setShortcut("restore", "").isEmpty()); // removed
        QCOMPARE(spy.count(), 3);
        QCOMPARE(t.app->shortcutKeys("complete"), QString("X"));
        QCOMPARE(t.app->shortcutKeys("cancel"), QString("Del")); // untouched
        QCOMPARE(t.app->shortcutKeys("restore"), QString());
        QVERIFY(!t.app->configurableShortcuts().at(0).toMap().value("isDefault").toBool());

        t.open(); // the choice lives in the settings file
        QCOMPARE(t.app->shortcutKeys("complete"), QString("X"));
        QCOMPARE(t.app->shortcutKeys("postpone"), QString("Ctrl+P"));
        QCOMPARE(t.app->shortcutKeys("restore"), QString()); // removed stays removed
        QCOMPARE(t.app->shortcutKeys("cancel"), QString("Del"));
        QCOMPARE(Settings(t.configFile).shortcut("complete", "?"), QString("X"));
        QCOMPARE(Settings(t.configFile).shortcut("nothing", "fallback"), QString("fallback"));
    }

    void shortcutsAreRefusedWhenUsedElsewhere()
    {
        TestEnv t;
        // another action
        QString why = t.app->setShortcut("complete", "Del");
        QVERIFY2(why.contains("Cancel task"), qPrintable(why));
        QCOMPARE(t.app->shortcutKeys("complete"), QString("K")); // nothing changed
        QCOMPARE(t.app->shortcutKeys("cancel"), QString("Del"));
        // keys with a fixed meaning
        for (const char *fixed : {"Space", "Up", "Down", "Shift+Up", "Ctrl+Up", "Return", "Enter", "Shift+Return", "Esc", "Ctrl+A", "Ctrl+D", "Shift+Del",
                 "Ctrl+PgUp", "Ctrl+PgDown", "Ctrl+Q"}) {
            why = t.app->setShortcut("restore", fixed);
            QVERIFY2(!why.isEmpty(), fixed);
            QCOMPARE(t.app->shortcutKeys("restore"), QString("R"));
        }
        QVERIFY(t.app->setShortcut("restore", "Space").contains("tick"));
        // not a single key
        QVERIFY(!t.app->setShortcut("restore", "Ctrl+K, Ctrl+L").isEmpty());
        QVERIFY(!t.app->setShortcut("restore", "no such key").isEmpty());
        QVERIFY(!t.app->setShortcut("unknown", "Z").isEmpty());
        // the same keys as it already has are fine
        QVERIFY(t.app->setShortcut("complete", "K").isEmpty());
        // a key freed by one action can go to another
        QVERIFY(t.app->setShortcut("cancel", "").isEmpty());
        QVERIFY(t.app->setShortcut("complete", "Del").isEmpty());
        QCOMPARE(t.app->actionForKey(Qt::Key_Delete, Qt::NoModifier), QString("complete"));
    }

    void eachShortcutResetsOnItsOwn()
    {
        TestEnv t;
        QVERIFY(t.app->setShortcut("complete", "X").isEmpty());
        QVERIFY(t.app->setShortcut("postpone", "Y").isEmpty());
        QVERIFY(t.app->setShortcut("restore", "").isEmpty());

        QVERIFY(t.app->resetShortcut("complete").isEmpty());
        QCOMPARE(t.app->shortcutKeys("complete"), QString("K"));
        QCOMPARE(t.app->shortcutKeys("postpone"), QString("Y")); // the others keep theirs
        QCOMPARE(t.app->shortcutKeys("restore"), QString());
        QVERIFY(t.app->configurableShortcuts().at(0).toMap().value("isDefault").toBool());
        QVERIFY(!t.app->configurableShortcuts().at(2).toMap().value("isDefault").toBool());

        QVERIFY(t.app->resetShortcut("restore").isEmpty()); // a removed one comes back too
        QCOMPARE(t.app->shortcutKeys("restore"), QString("R"));
        QCOMPARE(t.app->shortcutKeys("postpone"), QString("Y"));

        // The default keys were taken meanwhile: refused, nothing else changes.
        QVERIFY(t.app->setShortcut("complete", "X").isEmpty());
        QVERIFY(t.app->setShortcut("restore", "K").isEmpty());
        const QString why = t.app->resetShortcut("complete");
        QVERIFY2(why.contains("Restore"), qPrintable(why));
        QCOMPARE(t.app->shortcutKeys("complete"), QString("X"));
        QCOMPARE(t.app->shortcutKeys("restore"), QString("K"));
    }

    void brokenSettingsEntryMeansNoShortcut()
    {
        TestEnv t;
        Settings(t.configFile).setShortcut("complete", "not a key at all");
        QCOMPARE(t.app->shortcutKeys("complete"), QString());
        QVERIFY(t.app->actionForKey(Qt::Key_K, Qt::NoModifier).isEmpty());
    }

    // --- highlight ---

    void highlightMovesWithArrowsAndCanBeCleared()
    {
        TestEnv t;
        const QString a = t.add("A");
        const QString b = t.add("B");
        const QString c = t.add("C");
        TaskListModel *list = t.list();
        QVERIFY(list->currentId().isEmpty());
        list->moveHighlight(1); // nothing highlighted: starts at the top
        QCOMPARE(list->currentId(), a);
        list->moveHighlight(1);
        QCOMPARE(list->currentId(), b);
        list->moveHighlight(1);
        list->moveHighlight(1); // stays on the last task
        QCOMPARE(list->currentId(), c);
        list->moveHighlight(-1);
        QCOMPARE(list->currentId(), b);
        list->setCurrentId(QString()); // Escape
        QVERIFY(list->currentId().isEmpty());
        list->moveHighlight(-1); // starts at the bottom
        QCOMPARE(list->currentId(), c);
        list->setCurrentId("missing");
        QVERIFY(list->currentId().isEmpty());
    }

    void highlightIsDroppedWhenTheTaskLeavesTheList()
    {
        TestEnv t;
        const QString a = t.add("A");
        t.list()->setCurrentId(a);
        t.app->complete({a});
        QVERIFY(t.list()->currentId().isEmpty());
    }

    void toggleAndExtendSelection()
    {
        TestEnv t;
        const QString a = t.add("A");
        const QString b = t.add("B");
        const QString c = t.add("C");
        TaskListModel *list = t.list();
        list->toggleCurrentChecked(); // nothing highlighted
        QCOMPARE(list->checkedCount(), 0);
        list->extendSelection(1); // starts at the top and ticks it
        QCOMPARE(list->currentId(), a);
        QCOMPARE(list->checkedIds(), QStringList({a}));
        list->extendSelection(1);
        QCOMPARE(list->checkedIds(), QStringList({a, b}));
        list->extendSelection(-1); // never unticks
        QCOMPARE(list->currentId(), a);
        QCOMPARE(list->checkedIds(), QStringList({a, b}));
        list->toggleCurrentChecked();
        QCOMPARE(list->checkedIds(), QStringList({b}));
        list->setCurrentId(c);
        list->toggleCurrentChecked();
        QCOMPARE(list->checkedIds(), QStringList({b, c}));
    }

    void selectedMeansTickedOrElseHighlighted()
    {
        TestEnv t;
        const QString a = t.add("A");
        const QString b = t.add("B");
        QVERIFY(t.list()->selectedIds().isEmpty());
        t.list()->setCurrentId(a);
        QCOMPARE(t.list()->selectedIds(), QStringList({a}));
        t.list()->setChecked(b, true);
        QCOMPARE(t.list()->selectedIds(), QStringList({b}));
    }

    // --- keyboard actions ---

    void deleteKeyCancelsSelectedTasks()
    {
        TestEnv t;
        const QString a = t.add("A");
        const QString b = t.add("B");
        const QString c = t.add("C");
        t.app->cancelSelected(); // nothing selected
        QCOMPARE(t.shown().size(), 3);

        t.list()->setCurrentId(a); // highlighted only
        t.app->cancelSelected();
        QCOMPARE(t.store()->task(a)->state, TaskState::Cancelled);

        t.list()->setChecked(b, true);
        t.list()->setChecked(c, true);
        t.app->cancelSelected();
        QVERIFY(t.shown().isEmpty());
        QCOMPARE(t.shown(TaskState::Cancelled), QStringList({"A", "B", "C"}));
        QCOMPARE(t.savedTitles().size(), 3); // cancelled, not deleted
    }

    void duplicateSelectedTasks()
    {
        TestEnv t;
        const QString work = t.store()->addLabel("Work", 0);
        const QString a = t.add("A", "desc");
        const QString b = t.add("B");
        t.add("C");
        t.store()->setTaskLabels(a, {work});
        QVERIFY(!t.app->duplicateSelected()); // nothing selected

        t.list()->setCurrentId(b); // highlighted only
        QVERIFY(t.app->duplicateSelected());
        QCOMPARE(t.shown(), QStringList({"A", "B", "C", "B"}));

        t.list()->setChecked(a, true);
        t.list()->setChecked(b, true);
        QVERIFY(t.app->duplicateSelected());
        QCOMPARE(t.shown(), QStringList({"A", "B", "C", "B", "A", "B"})); // copies at the bottom
        const Task copy = t.store()->tasks().at(4);
        QCOMPARE(copy.description, QString("desc"));
        QCOMPARE(copy.labelIds, QStringList({work}));
        QCOMPARE(t.savedTitles().size(), 6);
    }

    // --- reordering ---

    void reorder()
    {
        TestEnv t;
        t.add("A");
        t.add("B");
        const QString c = t.add("C");
        TaskListModel *list = t.list();
        QSignalSpy moved(list, &QAbstractItemModel::rowsMoved);
        QVERIFY(list->moveTask(2, 0)); // drag & drop
        QCOMPARE(moved.count(), 1);
        QCOMPARE(t.shown(), QStringList({"C", "A", "B"}));
        QCOMPARE(t.savedTitles(), QStringList({"C", "A", "B"}));

        list->setCurrentId(c); // Ctrl+Down / Ctrl+Up
        QVERIFY(list->moveCurrent(1));
        QCOMPARE(t.shown(), QStringList({"A", "C", "B"}));
        QCOMPARE(list->currentId(), c); // the moved task stays highlighted
        QVERIFY(list->moveCurrent(-1));
        QVERIFY(!list->moveCurrent(-1)); // already first
        QCOMPARE(t.shown(), QStringList({"C", "A", "B"}));
        QVERIFY(!list->moveTask(0, 5));

        t.open();
        QCOMPARE(t.shown(), QStringList({"C", "A", "B"}));
    }

    void onlyTheActiveListCanBeReordered()
    {
        TestEnv t;
        const QString a = t.add("A");
        const QString b = t.add("B");
        t.app->complete({a, b});
        QVERIFY(!t.list(TaskState::Completed)->moveTask(0, 1));
        QCOMPARE(t.shown(TaskState::Completed), QStringList({"A", "B"}));
    }

    // --- labels ---

    void labelsHaveNameAndOneOfTwelveColors()
    {
        TestEnv t;
        QCOMPARE(t.app->palette().size(), 12);
        QVERIFY(t.app->addLabel("Work", 4));
        QVERIFY(!t.app->addLabel("", 1));
        QVERIFY(!t.app->addLabel("Other", 12));
        const QVariantList labels = t.app->labels();
        QCOMPARE(labels.size(), 1);
        const QVariantMap label = labels.at(0).toMap();
        QCOMPARE(label.value("name").toString(), QString("Work"));
        QCOMPARE(label.value("colorIndex").toInt(), 4);
        QCOMPARE(label.value("color").toString(), t.app->palette().at(4));

        const QString id = label.value("id").toString();
        QVERIFY(t.app->updateLabel(id, "Job", 9));
        QCOMPARE(t.app->labels().at(0).toMap().value("name").toString(), QString("Job"));
        t.open(); // labels live in the todo file
        QCOMPARE(t.app->labels().at(0).toMap().value("colorIndex").toInt(), 9);
        QVERIFY(t.app->removeLabel(id));
        QVERIFY(t.app->labels().isEmpty());
    }

    void filterByLabels()
    {
        TestEnv t;
        const QString work = t.store()->addLabel("Work", 0);
        const QString home = t.store()->addLabel("Home", 1);
        t.store()->setTaskLabels(t.add("W"), {work});
        t.store()->setTaskLabels(t.add("H"), {home});
        t.store()->setTaskLabels(t.add("WH"), {work, home});
        t.add("None");
        const QString done = t.add("Done work");
        t.store()->setTaskLabels(done, {work});
        t.app->complete({done});

        t.app->labelClicked(work);
        QCOMPARE(t.shown(), QStringList({"W", "WH"}));
        QCOMPARE(t.shown(TaskState::Completed), QStringList({"Done work"}));
        QCOMPARE(t.app->labels().at(0).toMap().value("filtering").toBool(), true);
        QCOMPARE(t.list()->totalCount(), 4); // the tab count ignores the filter

        t.app->labelClicked(home); // any of the selected labels
        QCOMPARE(t.shown(), QStringList({"W", "H", "WH"}));
        t.app->labelClicked(work);
        QCOMPARE(t.shown(), QStringList({"H", "WH"}));
        QVERIFY(t.shown(TaskState::Completed).isEmpty());
        t.app->labelClicked(home);
        QCOMPARE(t.shown().size(), 4);
    }

    void deletingLabelRemovesItsFilter()
    {
        TestEnv t;
        const QString work = t.store()->addLabel("Work", 0);
        t.store()->setTaskLabels(t.add("W"), {work});
        t.add("None");
        t.app->labelClicked(work);
        QCOMPARE(t.shown(), QStringList({"W"}));
        t.app->removeLabel(work);
        QVERIFY(t.app->labelFilter().isEmpty());
        QCOMPARE(t.shown(), QStringList({"W", "None"}));
    }

    void clickingLabelTogglesItOnTickedTasks()
    {
        TestEnv t;
        const QString work = t.store()->addLabel("Work", 0);
        const QString home = t.store()->addLabel("Home", 1);
        const QString a = t.add("A");
        const QString b = t.add("B");
        const QString c = t.add("C");
        t.store()->setTaskLabels(a, {work});
        t.list()->setChecked(a, true);
        t.list()->setChecked(b, true);

        t.app->labelClicked(work); // not all have it: added to those missing it
        QCOMPARE(t.store()->task(a)->labelIds, QStringList({work}));
        QCOMPARE(t.store()->task(b)->labelIds, QStringList({work}));
        QVERIFY(t.store()->task(c)->labelIds.isEmpty());
        QCOMPARE(t.list()->checkedCount(), 2); // the ticks are kept
        QVERIFY(t.app->labelFilter().isEmpty()); // and nothing is filtered
        QCOMPARE(t.shown().size(), 3);

        t.app->labelClicked(home);
        QCOMPARE(t.store()->task(a)->labelIds, QStringList({work, home}));
        t.app->labelClicked(work); // all have it: removed
        QCOMPARE(t.store()->task(a)->labelIds, QStringList({home}));
        QCOMPARE(t.store()->task(b)->labelIds, QStringList({home}));
    }
};

QTEST_MAIN(TestController)
#include "test_controller.moc"
