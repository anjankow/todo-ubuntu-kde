#pragma once

#include <QDateTime>

enum class PostponePreset { Tomorrow, ThreeDays, OneWeek, OneMonth };

// Returns the moment a task postponed with `preset` at `now` becomes due:
// the start of the day that many days (or one month) later.
QDateTime postponeUntil(PostponePreset preset, const QDateTime &now);
