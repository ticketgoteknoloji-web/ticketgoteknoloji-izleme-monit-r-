#pragma once

#include "OpsTypes.h"

#include <QHash>

namespace tg {

class IncidentTracker {
public:
    void restore(const QList<Incident>& incidents);
    QList<Incident> all() const;
    QList<Incident> newlyOpened(const QList<ModuleSnapshot>& modules, const QString& logSource);
    void acknowledge(const QString& id);
    int openCriticalCount() const;

private:
    struct Streak {
        int bad = 0;
        int good = 0;
    };
    QList<Incident> incidents_;
    QHash<QString, Streak> streaks_;
};

inline bool incidentSilenceActive(const QString& stored, const QString& id, const QDateTime& now)
{
    const QString prefix = id + QLatin1Char('|');
    for (const QString& line : stored.split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
        if (!line.startsWith(prefix)) continue;
        const QDateTime until = QDateTime::fromString(line.mid(prefix.size()), Qt::ISODate);
        return until.isValid() && until > now;
    }
    return false;
}

} // namespace tg
