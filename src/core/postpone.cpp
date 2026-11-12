#include "postpone.h"

QDateTime postponeUntil(PostponePreset preset, const QDateTime &now)
{
    QDate date = now.date();
    switch (preset) {
    case PostponePreset::Tomorrow:
        date = date.addDays(1);
        break;
    case PostponePreset::ThreeDays:
        date = date.addDays(3);
        break;
    case PostponePreset::OneWeek:
        date = date.addDays(7);
        break;
    case PostponePreset::OneMonth:
        date = date.addMonths(1);
        break;
    }
    return date.startOfDay();
}
