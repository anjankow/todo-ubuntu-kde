#pragma once

#include <QString>

// Persistent application settings (kept in the user's config directory).
class Settings
{
public:
    // An empty configFile uses the application's default config file.
    explicit Settings(const QString &configFile = {});

    // Folder holding the todo file; the home directory by default.
    QString storageDir() const;
    void setStorageDir(const QString &dir);

private:
    QString m_configFile;
};
