#include "core/storage.h"
#include "core/todostore.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QTest>

class TestStorage : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void fileLivesInTheChosenFolder() { QCOMPARE(Storage::filePath("/some/dir"), QString("/some/dir/todos.json")); }

    void fileIsReadableJsonText()
    {
        QTemporaryDir dir;
        TodoStore store;
        store.addTask("Buy milk", "2 litres");
        QVERIFY(Storage::save(store, Storage::filePath(dir.path())));

        QFile file(Storage::filePath(dir.path()));
        QVERIFY(file.open(QIODevice::ReadOnly | QIODevice::Text));
        const QJsonObject root = QJsonDocument::fromJson(file.readAll()).object();
        const QJsonObject task = root.value("tasks").toArray().at(0).toObject();
        QCOMPARE(task.value("title").toString(), QString("Buy milk"));
        QCOMPARE(task.value("description").toString(), QString("2 litres"));
        QCOMPARE(task.value("state").toString(), QString("active"));
    }

    void roundTripKeepsEverything()
    {
        QTemporaryDir dir;
        const QString path = Storage::filePath(dir.path());
        const QDateTime until(QDate(2027, 2, 3), QTime(4, 5));

        TodoStore store;
        const QString work = store.addLabel("Work", 7);
        const QString home = store.addLabel("Home", 2);
        const QString a = store.addTask("A", "line one\nline two");
        const QString b = store.addTask("B");
        const QString c = store.addTask("C");
        const QString d = store.addTask("D");
        store.setTaskLabels(a, {work, home});
        store.complete({b});
        store.cancel({c});
        store.postpone({d}, until);
        store.reorder({d, c, b, a});
        QVERIFY(Storage::save(store, path));

        TodoStore loaded;
        QVERIFY(Storage::load(loaded, path));
        QCOMPARE(loaded.tasks().size(), 4);
        QCOMPARE(loaded.tasks().at(0).id, d);
        QCOMPARE(loaded.tasks().at(3).id, a);
        QCOMPARE(loaded.task(a)->description, QString("line one\nline two"));
        QCOMPARE(loaded.task(a)->labelIds, QStringList({work, home}));
        QCOMPARE(loaded.task(a)->state, TaskState::Active);
        QCOMPARE(loaded.task(b)->state, TaskState::Completed);
        QCOMPARE(loaded.task(c)->state, TaskState::Cancelled);
        QCOMPARE(loaded.task(d)->state, TaskState::Postponed);
        QCOMPARE(loaded.task(d)->postponedUntil, until);
        QCOMPARE(loaded.labels().size(), 2);
        QCOMPARE(loaded.label(work)->name, QString("Work"));
        QCOMPARE(loaded.label(work)->color, 7);
    }

    void missingFileGivesEmptyList()
    {
        QTemporaryDir dir;
        TodoStore store;
        store.addTask("leftover");
        QVERIFY(Storage::load(store, Storage::filePath(dir.path())));
        QVERIFY(store.tasks().isEmpty());
    }

    void brokenFileIsReported()
    {
        QTemporaryDir dir;
        const QString path = Storage::filePath(dir.path());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write("{ this is not json");
        file.close();

        TodoStore store;
        QString error;
        QVERIFY(!Storage::load(store, path, &error));
        QVERIFY(!error.isEmpty());
    }

    void invalidEntriesAreSkipped()
    {
        QTemporaryDir dir;
        const QString path = Storage::filePath(dir.path());
        QFile file(path);
        QVERIFY(file.open(QIODevice::WriteOnly));
        file.write(R"({"labels":[{"id":"l1","name":"Work","color":99},{"id":"","name":"x","color":1}],
                       "tasks":[{"id":"t1","title":"Ok","state":"weird","labels":["l1","gone"]},
                                {"id":"t2","title":""},
                                {"id":"t1","title":"Duplicate"}]})");
        file.close();

        TodoStore store;
        QVERIFY(Storage::load(store, path));
        QCOMPARE(store.labels().size(), 1);
        QCOMPARE(store.labels().at(0).color, 0);
        QCOMPARE(store.tasks().size(), 1);
        QCOMPARE(store.tasks().at(0).state, TaskState::Active);
        QCOMPARE(store.tasks().at(0).labelIds, QStringList({"l1"}));
    }

    void saveFailsForMissingFolder()
    {
        TodoStore store;
        QString error;
        QVERIFY(!Storage::save(store, "/nonexistent-dir-for-kdetodo/todos.json", &error));
        QVERIFY(!error.isEmpty());
    }
};

QTEST_GUILESS_MAIN(TestStorage)
#include "test_storage.moc"
