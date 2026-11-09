#pragma once

#include "OpsTypes.h"
#include "ReportStats.h"

#include <QVariantMap>

#include <QSqlDatabase>

namespace tg {

class Store {
public:
    ~Store();
    bool open(const QString& filePath, QString* error);
    QString path() const;
    QList<Integration> integrations() const;
    bool saveIntegration(const Integration& integration, QString* error);
    bool removeIntegration(const QString& id, QString* error);
    void setPaused(const QString& id, bool paused);
    void addSample(const QString& moduleId, ModuleStatus status, qint64 latencyMs, int httpStatus, const QString& evidence);
    QVariantMap latestSample(const QString& moduleId) const;
    QVariantMap latestSampleMatching(const QString& moduleId, const QStringList& statuses) const;
    QList<QPair<qint64, double>> latencySeries(const QString& moduleId, const QDateTime& since) const;
    int sampleCount(const QString& moduleId, const QDateTime& since, int* badOut) const;
    void pruneSamples(const QDateTime& cutoff);
    void saveIncident(const Incident& incident);
    QList<Incident> incidents() const;
    void setSetting(const QString& key, const QString& value);
    QString setting(const QString& key, const QString& fallback = {}) const;
    void appendAudit(const QString& action, const QString& target, const QString& result);
    int auditCount() const;
    QList<ProbeSample> probeSamples(const QDateTime& from, const QDateTime& to, const QString& moduleId) const;
    bool insertSavedReport(const QString& id, const QString& name, const QString& kind, const QString& period, const QString& system, const QString& format, const QString& status, const QString& path, const QString& summary, QString* error);
    QList<QVariantMap> savedReports() const;
    QVariantMap savedReport(const QString& id) const;
    bool removeSavedReport(const QString& id, QString* error);

private:
    Integration readIntegration(class QSqlQuery& query) const;
    QSqlDatabase db_;
    QString path_;
    QString connectionName_;
};

} // namespace tg
