#pragma once

#include <QString>

// Splits what was typed into the new-task input into the title (first
// non-blank line) and the description (the lines after it).
void parseTaskInput(const QString &text, QString *title, QString *description);
