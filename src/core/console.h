#pragma once

#include <QtGlobal>

// Keeps the terminal for the application's own messages.
//
// The Qt and KDE libraries and the plugins loaded into the process print
// debug and warning lines ("true", "actions!!!", ...), and some libraries
// (e.g. fontconfig) write to stderr directly. None of it means anything to
// the user, so it does not reach the terminal: only critical and fatal
// messages do. When QT_LOGGING_RULES is set (someone debugging the
// libraries), every message is shown as usual and nothing is changed.
namespace Console
{
// Routes Qt messages: critical and fatal ones go on to `console` (by default
// the handler installed before, which prints to stderr), the rest is
// dropped. Call it first in main(): plugins may print while the application
// is created. Returns the handler installed before.
QtMessageHandler installMessageRouting(QtMessageHandler console = nullptr);

// Sends stdout and stderr of the process, and so of everything it starts, to
// /dev/null. The critical messages let through by installMessageRouting()
// still reach the terminal through a private copy of stderr. Call it after
// the command line was handled (--help and --version print to stdout).
// Returns false when the terminal was left as it is.
bool isolate();
}
