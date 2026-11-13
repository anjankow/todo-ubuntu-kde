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
    // Came back by itself from Postponed and the user has not looked at it
    // yet. Cleared by dismissing it or by any change of the state.
    bool returned = false;
    QStringList labelIds;
};

struct Label {
    QString id;
    QString name;
    int color = 0; // index into labelPalette()
};
