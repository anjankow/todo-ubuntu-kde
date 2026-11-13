#include <QProcess>
#include <QTest>

class TestConsole : public QObject
{
    Q_OBJECT
private Q_SLOTS:
    void onlyCriticalMessagesReachTheTerminal()
    {
        QProcess probe;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.remove("QT_LOGGING_RULES");
        probe.setProcessEnvironment(env);
        probe.start(KDETODO_CONSOLE_PROBE, QStringList());
        QVERIFY(probe.waitForFinished(10000));
        const QString out = QString::fromLocal8Bit(probe.readAllStandardOutput());
        const QString err = QString::fromLocal8Bit(probe.readAllStandardError());
        QCOMPARE(out, QString());
        QVERIFY2(err.contains("own-critical"), qPrintable(err));
        QCOMPARE(err.count('\n'), 1); // nothing else: no library or child output
    }

    void everythingIsShownWhenDebuggingTheLibraries()
    {
        QProcess probe;
        QProcessEnvironment env = QProcessEnvironment::systemEnvironment();
        env.insert("QT_LOGGING_RULES", "*.debug=true");
        probe.setProcessEnvironment(env);
        probe.start(KDETODO_CONSOLE_PROBE, QStringList());
        QVERIFY(probe.waitForFinished(10000));
        const QString all = QString::fromLocal8Bit(probe.readAllStandardOutput() + probe.readAllStandardError());
        for (const char *text : {"library-raw-stderr", "library-raw-stdout", "actions!!!", "library-warning", "child-stdout", "child-stderr", "own-critical"}) {
            QVERIFY2(all.contains(QLatin1String(text)), text);
        }
    }
};

QTEST_GUILESS_MAIN(TestConsole)
#include "test_console.moc"
