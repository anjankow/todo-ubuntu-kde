// Helper of test_console: prints in every way the application, its libraries
// and the programs it starts can print, after setting up the console like
// main() does.
#include "core/console.h"

#include <QCoreApplication>
#include <QProcess>

#include <cstdio>

int main(int argc, char **argv)
{
    Console::installMessageRouting();
    QCoreApplication app(argc, argv);
    Console::isolate();

    std::fprintf(stderr, "library-raw-stderr\n"); // e.g. fontconfig
    std::fflush(stderr);
    std::printf("library-raw-stdout\n");
    std::fflush(stdout);
    qDebug("actions!!!"); // leftover debug prints of plugins
    qInfo("library-info");
    qWarning("library-warning");
    // a started program inherits stdout and stderr
    QProcess::execute(QStringLiteral("sh"), {QStringLiteral("-c"), QStringLiteral("echo child-stdout; echo child-stderr >&2")});
    qCritical("own-critical");
    return 0;
}
