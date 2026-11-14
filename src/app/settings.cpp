#include "settings.h"

#include <KConfig>
#include <KConfigGroup>

#include <QDir>

namespace
{
const char groupName[] = "Storage";
const char dirKey[] = "Directory";
}

Settings::Settings(const QString &configFile)
    : m_configFile(configFile.isEmpty() ? QStringLiteral("kdetodorc") : configFile)
{
}

QString Settings::storageDir() const
{
    const KConfig config(m_configFile);
    const QString dir = config.group(groupName).readEntry(dirKey, QString());
    return dir.isEmpty() ? QDir::homePath() : dir;
}

void Settings::setStorageDir(const QString &dir)
{
    KConfig config(m_configFile);
    config.group(groupName).writeEntry(dirKey, dir);
    config.sync();
}
