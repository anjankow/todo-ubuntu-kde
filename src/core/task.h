#pragma once

#include <QDateTime>
#include <QString>
#include <QStringList>

enum class TaskState { Active, Completed, Cancelled, Postponed };

struct Task {
    QString id;
    QString title;
    QString description;
    TaskState state = TaskState::Active;
    QDateTime postponedUntil; // only meaningful when state == Postponed
    QStringList labelIds;
};

struct Label {
    QString id;
    QString name;
    int color = 0; // index into labelPalette()
};
