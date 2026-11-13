#include "console.h"

#include <QByteArray>
#include <QString>

#include <fcntl.h>
#include <unistd.h>

namespace
{
QtMessageHandler g_console = nullptr;
bool g_passEverything = false;
int g_consoleFd = -1;

void routeMessage(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const bool serious = type == QtCriticalMsg || type == QtFatalMsg;
    if ((serious || g_passEverything) && g_console) {
        g_console(type, context, message);
    }
}

void writeToConsole(QtMsgType type, const QMessageLogContext &context, const QString &message)
{
    const QByteArray line = (qFormatLogMessage(type, context, message) + QLatin1Char('\n')).toLocal8Bit();
    qint64 done = 0;
    while (done < line.size()) {
        const ssize_t n = ::write(g_consoleFd, line.constData() + done, size_t(line.size() - done));
        if (n <= 0) {
            break;
        }
        done += n;
    }
}
}

QtMessageHandler Console::installMessageRouting(QtMessageHandler console)
{
    g_passEverything = !qEnvironmentVariableIsEmpty("QT_LOGGING_RULES");
    const QtMessageHandler previous = qInstallMessageHandler(routeMessage);
    g_console = console ? console : previous;
    return previous;
}

bool Console::isolate()
{
    if (g_passEverything || g_consoleFd >= 0) {
        return false;
    }
    // close-on-exec: programs started later do not get this copy of the terminal
    const int console = ::fcntl(STDERR_FILENO, F_DUPFD_CLOEXEC, 3);
    const int null = ::open("/dev/null", O_WRONLY | O_CLOEXEC);
    if (console < 0 || null < 0) {
        if (console >= 0) {
            ::close(console);
        }
        if (null >= 0) {
            ::close(null);
        }
        return false;
    }
    ::dup2(null, STDOUT_FILENO);
    ::dup2(null, STDERR_FILENO);
    ::close(null);
    g_consoleFd = console;
    g_console = writeToConsole;
    return true;
}
