#pragma once

#include <QString>

class TodoStore;

// Reads and writes the todo list as a JSON text file.
namespace Storage
{
// Name of the file kept inside the storage folder.
QString fileName();
QString filePath(const QString &directory);

bool save(const TodoStore &store, const QString &path, QString *error = nullptr);
// A missing file is not an error: the store is left empty.
bool load(TodoStore &store, const QString &path, QString *error = nullptr);
}
