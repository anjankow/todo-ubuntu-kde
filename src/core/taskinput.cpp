#include "taskinput.h"

#include <QStringList>

void parseTaskInput(const QString &text, QString *title, QString *description)
{
    const QStringList lines = text.split(QLatin1Char('\n'));
    int first = 0;
    while (first < lines.size() && lines.at(first).trimmed().isEmpty()) {
        ++first;
    }
    if (first == lines.size()) {
        title->clear();
        description->clear();
        return;
    }
    *title = lines.at(first).trimmed();
    *description = lines.mid(first + 1).join(QLatin1Char('\n')).trimmed();
}
