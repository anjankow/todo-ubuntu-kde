#include "core/palette.h"
#include "core/postpone.h"
#include "core/todostore.h"

#include <QSignalSpy>
#include <QTest>

namespace
{
QStringList titles(const QVector<Task> &tasks)
{
    QStringList result;
    for (const Task &t : tasks) {
        result.append(t.title);
    }
    return result;
}
}

class TestStore : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void taskHasTitleAndOptionalDescription()
    {
        TodoStore store;
        const QString a = store.addTask("  Buy milk ");
        const QString b = store.addTask("Call mum", "about Sunday\nand the cake");
        QVERIFY(!a.isEmpty());
        QCOMPARE(store.task(a)->title, QString("Buy milk"));
        QVERIFY(store.task(a)->description.isEmpty());
        QCOMPARE(store.task(b)->description, QString("about Sunday\nand the cake"));
        QCOMPARE(store.task(a)->state, TaskState::Active);
    }

    void blankTitleIsNotATask()
    {
        TodoStore store;
        QSignalSpy spy(&store, &TodoStore::changed);
        QVERIFY(store.addTask("").isEmpty());
        QVERIFY(store.addTask("   \t").isEmpty());
        QVERIFY(store.tasks().isEmpty());
        QCOMPARE(spy.count(), 0);
    }

    void taskCanBeAddedOnTop()
    {
        TodoStore store;
        store.addTask("A");
        store.addTask("B");
        store.addTask("Top", {}, true);
        QCOMPARE(titles(store.tasks()), QStringList({"Top", "A", "B"}));
    }

    void updateTask()
    {
        TodoStore store;
        const QString id = store.addTask("Old");
        QVERIFY(store.updateTask(id, "New", "desc"));
        QCOMPARE(store.task(id)->title, QString("New"));
        QCOMPARE(store.task(id)->description, QString("desc"));
        QVERIFY(!store.updateTask(id, "  ", "x"));
        QCOMPARE(store.task(id)->title, QString("New"));
        QVERIFY(!store.updateTask("missing", "A", ""));
    }

    void completeAndCancelMoveTasksOutOfTheActiveList()
    {
        TodoStore store;
        const QString a = store.addTask("A");
        const QString b = store.addTask("B");
        const QString c = store.addTask("C");
        store.complete({a});
        store.cancel({b});
        QCOMPARE(titles(store.tasksIn(TaskState::Active)), QStringList({"C"}));
        QCOMPARE(titles(store.tasksIn(TaskState::Completed)), QStringList({"A"}));
        QCOMPARE(titles(store.tasksIn(TaskState::Cancelled)), QStringList({"B"}));
        Q_UNUSED(c)
    }

    void batchOperations()
    {
        TodoStore store;
        const QString a = store.addTask("A");
        const QString b = store.addTask("B");
        const QString c = store.addTask("C");
        const QString d = store.addTask("D");
        store.complete({a, b});
        QCOMPARE(store.tasksIn(TaskState::Completed).size(), 2);
        store.cancel({c, d});
        QCOMPARE(store.tasksIn(TaskState::Cancelled).size(), 2);
        store.remove({a, c});
        QCOMPARE(titles(store.tasks()), QStringList({"B", "D"}));
    }

    void deleteIsPermanent()
    {
        TodoStore store;
        const QString a = store.addTask("A");
        store.remove({a});
        QVERIFY(store.tasks().isEmpty());
        QVERIFY(!store.task(a));
        store.restore({a});
        QVERIFY(store.tasks().isEmpty());
    }

    void restoreFromEveryState()
    {
        TodoStore store;
        const QString a = store.addTask("A");
        const QString b = store.addTask("B");
        const QString c = store.addTask("C");
        store.complete({a});
        store.cancel({b});
        store.postpone({c}, QDateTime(QDate(2030, 1, 1), QTime(0, 0)));
        QVERIFY(store.tasksIn(TaskState::Active).isEmpty());
        store.restore({a, b, c});
        QCOMPARE(titles(store.tasksIn(TaskState::Active)), QStringList({"A", "B", "C"}));
        QVERIFY(!store.task(c)->postponedUntil.isValid());
    }

    void postponedTasksAreHiddenUntilDue()
    {
        TodoStore store;
        const QString soon = store.addTask("Soon");
        const QString later = store.addTask("Later");
        const QDateTime now(QDate(2026, 11, 12), QTime(10, 0));
        store.postpone({soon}, now.addDays(1));
        store.postpone({later}, now.addDays(7));
        QVERIFY(store.tasksIn(TaskState::Active).isEmpty());
        QCOMPARE(store.tasksIn(TaskState::Postponed).size(), 2);

        QCOMPARE(store.releaseDuePostponed(now), 0);
        QCOMPARE(store.releaseDuePostponed(now.addDays(1)), 1);
        QCOMPARE(titles(store.tasksIn(TaskState::Active)), QStringList({"Soon"}));
        QCOMPARE(titles(store.tasksIn(TaskState::Postponed)), QStringList({"Later"}));
        QCOMPARE(store.releaseDuePostponed(now.addDays(30)), 1);
        QVERIFY(store.tasksIn(TaskState::Postponed).isEmpty());
    }

    void postponeNeedsAValidTime()
    {
        TodoStore store;
        const QString a = store.addTask("A");
        store.postpone({a}, QDateTime());
        QCOMPARE(store.task(a)->state, TaskState::Active);
    }

    void postponePresets()
    {
        const QDateTime now(QDate(2026, 11, 12), QTime(15, 30));
        QCOMPARE(postponeUntil(PostponePreset::Tomorrow, now), QDate(2026, 11, 13).startOfDay());
        QCOMPARE(postponeUntil(PostponePreset::ThreeDays, now), QDate(2026, 11, 15).startOfDay());
        QCOMPARE(postponeUntil(PostponePreset::OneWeek, now), QDate(2026, 11, 19).startOfDay());
        QCOMPARE(postponeUntil(PostponePreset::OneMonth, now), QDate(2026, 12, 12).startOfDay());
    }

    void reorder()
    {
        TodoStore store;
        const QString a = store.addTask("A");
        const QString b = store.addTask("B");
        const QString c = store.addTask("C");
        QVERIFY(store.reorder({c, a, b}));
        QCOMPARE(titles(store.tasks()), QStringList({"C", "A", "B"}));
        QVERIFY(!store.reorder({a, "missing"}));
        QVERIFY(!store.reorder({a, a}));
        QCOMPARE(titles(store.tasks()), QStringList({"C", "A", "B"}));
    }

    void reorderSubsetKeepsOtherTasksInPlace()
    {
        TodoStore store;
        const QString a = store.addTask("A");
        const QString hidden = store.addTask("Hidden");
        const QString b = store.addTask("B");
        const QString c = store.addTask("C");
        store.complete({hidden});
        QVERIFY(store.reorder({c, b, a}));
        QCOMPARE(titles(store.tasks()), QStringList({"C", "Hidden", "B", "A"}));
    }

    void paletteOffersTwelveDistinctColors()
    {
        QCOMPARE(labelPalette().size(), 12);
        QSet<QRgb> distinct;
        for (const QColor &c : labelPalette()) {
            distinct.insert(c.rgb());
        }
        QCOMPARE(distinct.size(), 12);
    }

    void labelsHaveNameAndPaletteColor()
    {
        TodoStore store;
        const QString work = store.addLabel(" Work ", 3);
        QVERIFY(!work.isEmpty());
        QCOMPARE(store.label(work)->name, QString("Work"));
        QCOMPARE(store.label(work)->color, 3);
        QVERIFY(!store.addLabel("Home", 11).isEmpty());

        QVERIFY(store.addLabel("", 1).isEmpty());
        QVERIFY(store.addLabel("work", 1).isEmpty()); // duplicate name
        QVERIFY(store.addLabel("Other", 12).isEmpty()); // not in the palette
        QVERIFY(store.addLabel("Other", -1).isEmpty());
        QCOMPARE(store.labels().size(), 2);
    }

    void updateAndRemoveLabel()
    {
        TodoStore store;
        const QString work = store.addLabel("Work", 0);
        const QString home = store.addLabel("Home", 1);
        QVERIFY(store.updateLabel(work, "Job", 5));
        QCOMPARE(store.label(work)->name, QString("Job"));
        QCOMPARE(store.label(work)->color, 5);
        QVERIFY(!store.updateLabel(work, "home", 5));
        QVERIFY(!store.updateLabel(work, "Job", 99));

        const QString t = store.addTask("T");
        QVERIFY(store.setTaskLabels(t, {work, home}));
        QVERIFY(store.removeLabel(work));
        QCOMPARE(store.task(t)->labelIds, QStringList({home}));
        QVERIFY(!store.removeLabel(work));
    }

    void tasksCanCarrySeveralLabels()
    {
        TodoStore store;
        const QString work = store.addLabel("Work", 0);
        const QString home = store.addLabel("Home", 1);
        const QString t = store.addTask("T");
        QVERIFY(store.setTaskLabels(t, {work, home, "unknown", work}));
        QCOMPARE(store.task(t)->labelIds, QStringList({work, home}));
    }

    void toggleLabelOnSeveralTasks()
    {
        TodoStore store;
        const QString work = store.addLabel("Work", 0);
        const QString a = store.addTask("A");
        const QString b = store.addTask("B");
        const QString c = store.addTask("C");
        store.setTaskLabels(a, {work});

        QVERIFY(store.toggleLabel({a, b}, work)); // mixed: added to the rest
        QCOMPARE(store.task(a)->labelIds, QStringList({work}));
        QCOMPARE(store.task(b)->labelIds, QStringList({work}));
        QVERIFY(store.task(c)->labelIds.isEmpty());

        QVERIFY(store.toggleLabel({a, b}, work)); // all have it: removed
        QVERIFY(store.task(a)->labelIds.isEmpty());
        QVERIFY(store.task(b)->labelIds.isEmpty());

        QVERIFY(!store.toggleLabel({a}, "unknown"));
        QVERIFY(!store.toggleLabel({}, work));
        QVERIFY(!store.toggleLabel({"missing"}, work));
    }

    void filterMatchesAnySelectedLabel()
    {
        TodoStore store;
        const QString work = store.addLabel("Work", 0);
        const QString home = store.addLabel("Home", 1);
        const QString fun = store.addLabel("Fun", 2);
        store.setTaskLabels(store.addTask("W"), {work});
        store.setTaskLabels(store.addTask("H"), {home});
        store.setTaskLabels(store.addTask("WH"), {work, home});
        store.setTaskLabels(store.addTask("F"), {fun});
        store.addTask("None");

        QCOMPARE(store.tasksIn(TaskState::Active).size(), 5);
        QCOMPARE(titles(store.tasksIn(TaskState::Active, {work})), QStringList({"W", "WH"}));
        QCOMPARE(titles(store.tasksIn(TaskState::Active, {work, home})), QStringList({"W", "H", "WH"}));
        QCOMPARE(titles(store.tasksIn(TaskState::Active, {fun})), QStringList({"F"}));
    }
};

QTEST_GUILESS_MAIN(TestStore)
#include "test_store.moc"
