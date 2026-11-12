#include "storage.h"

#include "palette.h"
#include "todostore.h"

#include <QDir>
#include <QFile>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSaveFile>

namespace
{
QString stateToString(TaskState state)
{
    switch (state) {
    case TaskState::Completed:
        return QStringLiteral("completed");
    case TaskState::Cancelled:
        return QStringLiteral("cancelled");
    case TaskState::Postponed:
        return QStringLiteral("postponed");
    case TaskState::Active:
        break;
    }
    return QStringLiteral("active");
}

TaskState stateFromString(const QString &s)
{
    if (s == QLatin1String("completed")) {
        return TaskState::Completed;
    }
    if (s == QLatin1String("cancelled")) {
        return TaskState::Cancelled;
    }
    if (s == QLatin1String("postponed")) {
        return TaskState::Postponed;
    }
    return TaskState::Active;
}

void setError(QString *error, const QString &message)
{
    if (error) {
        *error = message;
    }
}
}

QString Storage::fileName()
{
    return QStringLiteral("todos.json");
}

QString Storage::filePath(const QString &directory)
{
    return QDir(directory).filePath(fileName());
}

bool Storage::save(const TodoStore &store, const QString &path, QString *error)
{
    QJsonArray labels;
    for (const Label &l : store.labels()) {
        labels.append(QJsonObject {{QStringLiteral("id"), l.id}, {QStringLiteral("name"), l.name}, {QStringLiteral("color"), l.color}});
    }
    QJsonArray tasks;
    for (const Task &t : store.tasks()) {
        QJsonObject o {{QStringLiteral("id"), t.id}, {QStringLiteral("title"), t.title}, {QStringLiteral("description"), t.description},
            {QStringLiteral("state"), stateToString(t.state)}, {QStringLiteral("labels"), QJsonArray::fromStringList(t.labelIds)}};
        if (t.state == TaskState::Postponed) {
            o.insert(QStringLiteral("postponedUntil"), t.postponedUntil.toString(Qt::ISODate));
        }
        tasks.append(o);
    }
    const QJsonObject root {{QStringLiteral("version"), 1}, {QStringLiteral("labels"), labels}, {QStringLiteral("tasks"), tasks}};

    QSaveFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        setError(error, file.errorString());
        return false;
    }
    file.write(QJsonDocument(root).toJson(QJsonDocument::Indented));
    if (!file.commit()) {
        setError(error, file.errorString());
        return false;
    }
    return true;
}

bool Storage::load(TodoStore &store, const QString &path, QString *error)
{
    QFile file(path);
    if (!file.exists()) {
        store.setContent({}, {});
        return true;
    }
    if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
        setError(error, file.errorString());
        return false;
    }
    QJsonParseError parseError {};
    const QJsonDocument doc = QJsonDocument::fromJson(file.readAll(), &parseError);
    if (parseError.error != QJsonParseError::NoError || !doc.isObject()) {
        setError(error, parseError.error != QJsonParseError::NoError ? parseError.errorString() : QStringLiteral("not a JSON object"));
        return false;
    }
    const QJsonObject root = doc.object();

    QVector<Label> labels;
    QStringList labelIds;
    const QJsonArray labelArray = root.value(QStringLiteral("labels")).toArray();
    for (const QJsonValue &v : labelArray) {
        const QJsonObject o = v.toObject();
        Label l;
        l.id = o.value(QStringLiteral("id")).toString();
        l.name = o.value(QStringLiteral("name")).toString();
        l.color = o.value(QStringLiteral("color")).toInt();
        if (l.id.isEmpty() || l.name.isEmpty() || labelIds.contains(l.id)) {
            continue;
        }
        if (l.color < 0 || l.color >= labelPalette().size()) {
            l.color = 0;
        }
        labelIds.append(l.id);
        labels.append(l);
    }

    QVector<Task> tasks;
    QStringList taskIds;
    const QJsonArray taskArray = root.value(QStringLiteral("tasks")).toArray();
    for (const QJsonValue &v : taskArray) {
        const QJsonObject o = v.toObject();
        Task t;
        t.id = o.value(QStringLiteral("id")).toString();
        t.title = o.value(QStringLiteral("title")).toString();
        if (t.id.isEmpty() || t.title.isEmpty() || taskIds.contains(t.id)) {
            continue;
        }
        t.description = o.value(QStringLiteral("description")).toString();
        t.state = stateFromString(o.value(QStringLiteral("state")).toString());
        if (t.state == TaskState::Postponed) {
            t.postponedUntil = QDateTime::fromString(o.value(QStringLiteral("postponedUntil")).toString(), Qt::ISODate);
        }
        const QJsonArray taskLabels = o.value(QStringLiteral("labels")).toArray();
        for (const QJsonValue &lv : taskLabels) {
            const QString labelId = lv.toString();
            if (labelIds.contains(labelId) && !t.labelIds.contains(labelId)) {
                t.labelIds.append(labelId);
            }
        }
        taskIds.append(t.id);
        tasks.append(t);
    }

    store.setContent(tasks, labels);
    return true;
}
