#include "IncidentTracker.h"

#include <QUuid>

namespace tg {
namespace {

int thresholdFor(ModuleStatus status, int configured)
{
    if (status == ModuleStatus::Critical) return 2;
    if (status == ModuleStatus::AuthRejected) return 2;
    if (status == ModuleStatus::ConnectionError || status == ModuleStatus::Warning) {
        if (configured >= 2) return qBound(2, configured, 10);
        return 3;
    }
    return 99;
}

QString kindFor(ModuleStatus status)
{
    if (status == ModuleStatus::ConnectionError) return QStringLiteral("connection_lost");
    if (status == ModuleStatus::AuthRejected) return QStringLiteral("auth_rejected");
    if (status == ModuleStatus::Critical) return QStringLiteral("dependency_down");
    return QStringLiteral("degraded");
}

} // namespace

void IncidentTracker::restore(const QList<Incident>& incidents)
{
    incidents_ = incidents;
}

QList<Incident> IncidentTracker::all() const
{
    return incidents_;
}

int IncidentTracker::openCriticalCount() const
{
    int count = 0;
    for (const Incident& incident : incidents_) {
        if (incident.state != QLatin1String("resolved") && incident.severity == QLatin1String("critical")) {
            ++count;
        }
    }
    return count;
}

void IncidentTracker::acknowledge(const QString& id)
{
    for (Incident& incident : incidents_) {
        if (incident.id == id && incident.state == QLatin1String("open")) {
            incident.state = QStringLiteral("acknowledged");
        }
    }
}

QList<Incident> IncidentTracker::newlyOpened(const QList<ModuleSnapshot>& modules, const QString& logSource)
{
    QList<Incident> created;
    const QDateTime now = QDateTime::currentDateTimeUtc();
    for (const ModuleSnapshot& module : modules) {
        Streak& streak = streaks_[module.id];
        Incident* existing = nullptr;
        for (Incident& incident : incidents_) {
            if (incident.moduleId == module.id && incident.state != QLatin1String("resolved")) {
                existing = &incident;
                break;
            }
        }
        if (!isBadStatus(module.status)) {
            streak.bad = 0;
            streak.good += 1;
            if (existing && streak.good >= 2) {
                existing->state = QStringLiteral("resolved");
                existing->resolvedAt = now;
                existing->lastSeen = now;
            }
            continue;
        }
        streak.good = 0;
        streak.bad += 1;
        const int need = thresholdFor(module.status, module.alertAfter);
        if (streak.bad < need) {
            continue;
        }
        const QString severity = (module.status == ModuleStatus::Critical || module.status == ModuleStatus::ConnectionError) ? QStringLiteral("critical") : QStringLiteral("warning");
        if (existing) {
            existing->lastSeen = now;
            existing->repeatCount += 1;
            if (module.lastHttp > 0) existing->httpStatus = module.lastHttp;
            existing->message = module.evidence;
            existing->causeLabel = module.causeLabel;
            existing->causeText = module.causeText;
            if (severity == QLatin1String("critical")) existing->severity = severity;
            continue;
        }
        Incident incident;
        incident.id = QUuid::createUuid().toString(QUuid::WithoutBraces);
        incident.moduleId = module.id;
        incident.application = module.group == QLatin1String("gochat") ? QStringLiteral("Go-Chat") : (module.group == QLatin1String("custom") ? module.name : QStringLiteral("Ticket-Go"));
        incident.moduleName = module.name;
        incident.service = module.id;
        incident.kind = kindFor(module.status);
        incident.httpStatus = module.lastHttp;
        incident.message = module.evidence;
        incident.firstSeen = now;
        incident.lastSeen = now;
        incident.repeatCount = streak.bad;
        incident.severity = severity;
        incident.logSource = logSource.isEmpty() ? QStringLiteral("Log kaynağı bağlı değil") : logSource;
        incident.causeLabel = module.causeLabel.isEmpty() ? QStringLiteral("Olası neden") : module.causeLabel;
        incident.causeText = module.causeText;
        incident.recommendation = module.recommendation;
        incident.state = QStringLiteral("open");
        incidents_.push_front(incident);
        created.push_back(incident);
    }
    return created;
}

} // namespace tg
