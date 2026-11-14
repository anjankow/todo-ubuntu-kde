#include "settings.h"

#include <KConfig>
#include <KConfigGroup>

#include <QDir>

namespace
{
const char groupName[] = "Storage";
const char dirKey[] = "Directory";
const char shortcutsGroup[] = "Shortcuts";
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

QString Settings::shortcut(const QString &action, const QString &fallback) const
{
    const KConfig config(m_configFile);
    const KConfigGroup group = config.group(shortcutsGroup);
    return group.hasKey(action) ? group.readEntry(action, QString()) : fallback;
}

void Settings::setShortcut(const QString &action, const QString &keys)
{
    KConfig config(m_configFile);
    config.group(shortcutsGroup).writeEntry(action, keys);
    config.sync();
}
