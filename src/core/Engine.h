#pragma once

#include "CredentialStore.h"
#include "LogHub.h"
#include "HostMetrics.h"
#include "IncidentTracker.h"
#include "OpsTypes.h"
#include "PathProbe.h"
#include "ProbeClient.h"
#include "Store.h"

#include <QPair>
#include <QTimer>
#include <memory>

class QWebSocket;

namespace tg {

class Engine : public QObject {
    Q_OBJECT
public:
    explicit Engine(QObject* parent = nullptr);
    bool start(QString* error);
    QList<ModuleSnapshot> modules() const;
    Overview overview() const;
    QList<Incident> incidents() const;
    QList<Integration> integrations() const;
    HostSample host() const;
    int revision() const;
    QString dataDirectory() const;
    QString keychainNote() const;
    QString environmentText() const;
    QString regionText() const;
    QString uptimeText() const;
    QString queueText() const;
    QString logBanner() const;
    QString logBody(const QString& level) const;
    QList<LogSource> logSources() const;
    QList<LogRecord> logRows() const;
    QList<LogRecord> recentLogs() const;
    LogRecord selectedLog() const;
    QString logAccessNote() const;
    int logHubTotal() const;
    QString logRefreshedAt() const;
    bool logLive() const;
    bool logHubConnected() const;
    bool logStale() const;
    QString logLastRecordAt() const;
    int logPollSeconds() const;
    void setLogPollSeconds(int seconds);
    int logNewCount() const;
    void applyLogQuery(const LogQuery& query);
    void refreshLogs();
    void setLogLive(bool on);
    void selectLog(const QString& id);
    bool setLogToken(const QString& token, QString* error);
    QString logSessionText() const;
    QString logSessionPhase() const;
    bool logSessionBusy() const;
    QString logSessionMessage() const;
    QString logSessionAccount() const;
    QString logSessionVerifiedAt() const;
    QString logMfaHint() const;
    QString openLogSession(const QString& username, const QString& password);
    QString submitLogMfa(const QString& code);
    QString resendLogMfa();
    QString refreshLogSession();
    QString closeLogSession();
    QString validateLogSession();
    bool logTokenConfigured() const;
    void clearLogToken();
    void discardLogSession();
    void hideHubLog(const QString& id);
    void restoreHiddenHubLogs();
    int logPage() const;
    void setLogPage(int page);
    bool soundEnabled() const;
    void setSoundEnabled(bool on);
    QString overviewFilter() const;
    void setOverviewFilter(const QString& filter);
    void setPaused(const QString& id, bool paused);
    QString saveIntegration(Integration integration);
    QString removeIntegration(const QString& id);
    QString probeNow(const QString& id);
    QVariantMap latestSample(const QString& moduleId) const;
    QVariantMap latestHealthySample(const QString& moduleId) const;
    QVariantMap latestFailedSample(const QString& moduleId) const;
    QString measurePerformance(const QString& kind);
    QVariantMap performanceMetric(const QString& id, int minutes) const;
    void testUnsaved(const Integration& integration, const QString& secret);
    QString setCredential(const QString& id, const QString& secret);
    bool hasCredential(const QString& id) const;
    void clearCredential(const QString& id);
    QString acknowledge(const QString& id);
    QString silenceIncident(const QString& id, int minutes);
    bool incidentSilenced(const QString& id) const;
    QString hideIncident(const QString& id);
    QString restoreHiddenIncidents();
    QStringList hiddenIncidentIds() const;
    QString setLogFile(const QString& path);
    QString setting(const QString& key) const;
    QString setting(const QString& key, const QString& fallback) const;
    void setSetting(const QString& key, const QString& value);
    void audit(const QString& action, const QString& target, const QString& result);
    void clearLogFile();
    QList<double> latencySeries(const QString& moduleId, int minutes) const;
    QList<QPair<qint64, double>> latencyPoints(const QString& moduleId, int minutes) const;
    qint64 lastLatencyMs(const QString& moduleId, int minutes) const;
    QString exportReport(const QString& path, const QString& kind, const QString& format);
    QString savePreferences(int intervalSec, int timeoutSec, int retryCap, int latencyWarnMs, int retentionDays, bool monitorEnabled, bool rememberPage, bool clock24h, bool sound, bool desktopNotify, bool quiet, bool animations, bool lowPower, bool startFullscreen, const QString& quality, int* customUpdated);
    QString clearOldSamples();
    qint64 databaseBytes() const;
    QVariantMap reportDocument(const QString& kind, const QString& moduleId, const QString& rangeKey, const QString& fromIso, const QString& toIso) const;
    QString saveReportRecord(const QString& name, const QString& kind, const QString& period, const QString& system, const QString& format, const QString& path, const QString& summary);
    QList<QVariantMap> savedReports() const;
    QString deleteSavedReport(const QString& id);
    QVariantMap savedReport(const QString& id) const;

signals:
    void updated();
    void criticalOpened(const QString& title, const QString& body);
    void testFinished(const QString& summary);

private slots:
    void tick();

private:
    struct Chain {
        Integration integration;
        bool manual = false;
        bool draft = false;
        int step = 0;
        bool depsAttempted = false;
        QString secret;
        QString certPem;
        QString keyPem;
        QString bearer;
        HttpResult health;
        HttpResult live;
        HttpResult ready;
        HttpResult identity;
        HttpResult deps;
        HttpResult single;
        HttpResult parent;
    };

    void seedModules();
    void ensureCustomModules();
    void applyPauseState(const QString& id, bool paused);
    void begin(const Integration& integration, bool manual, bool draft, const QString& secretOverride);
    void advance(const QString& id);
    void onResult(const QString& id, const HttpResult& result);
    void finish(const QString& id);
    void applyModules(const QList<ModuleSnapshot>& list, int httpStatus);
    void decorate(ModuleSnapshot& module) const;
    void syncSockets();
    ProbeRequest baseRequest(const Chain& chain, const QString& url, bool withAuth) const;
    QString joinUrl(const QString& base, const QString& path) const;
    Integration* findIntegration(const QString& id);

    std::unique_ptr<CredentialStore> credentials_;
    Store store_;
    ProbeClient client_;
    IncidentTracker incidents_;
    HostMetrics hostMetrics_;
    QTimer timer_;
    QList<ModuleSnapshot> modules_;
    QList<Integration> integrations_;
    QHash<QString, Chain> chains_;
    QHash<QString, qint64> nextDue_;
    QHash<QString, int> transportFails_;
    QHash<QString, QWebSocket*> sockets_;
    HostSample host_;
    int revision_ = 0;
    QString dataDir_;
    QString environment_ = QStringLiteral("Ölçülmedi");
    QString region_ = QStringLiteral("Ölçülmedi");
    QString uptime_ = QStringLiteral("Ölçülmedi");
    qint64 lastHostMs_ = 0;
    qint64 lastPruneMs_ = 0;
    qint64 lastLogPollMs_ = 0;
    qint64 lastPathMs_ = 0;
    QHash<QString, qint64> lastManualMs_;
    PathProbe pathProbe_;
    void storePath(const PathResult& result);
    int pathIntervalMs() const;
    QList<LogRecord> hubLogs_;
    QList<LogRecord> localLogs_;
    LogQuery logQuery_;
    QString selectedLogId_;
    QString logAccessNote_ = QStringLiteral("Log veri kaynağına erişilemiyor. Güvenli oturum açın. Kimliksiz istek gönderilmedi.");
    QString hubDenyNote_ = QStringLiteral("Log veri kaynağına erişilemiyor. Güvenli oturum açın. Kimliksiz istek gönderilmedi.");
    int logHubTotal_ = -1;
    QString logRefreshedAt_;
    bool hubLogConnected_ = false;
    bool logLive_ = false;
    bool logStale_ = false;
    bool logFetchBusy_ = false;
    bool logRefreshUsed_ = false;
    bool logRefreshInFlight_ = false;
    bool logSessionBusy_ = false;
    QString logSessionPhase_ = QStringLiteral("none");
    QString logSessionMessage_;
    QString logSessionVerifiedAt_;
    QString logSessionAccount_;
    QString logAccessToken_;
    QString logRefreshToken_;
    QString logMfaHint_;
    QString mfaChallenge_;
    QString logLiveCursor_;
    int logBackoffMs_ = 0;
    int logNewCount_ = 0;
    void ingestHubLogs(const HttpResult& result);
    void ingestLiveLogs(const HttpResult& result);
    void noteLogFailure(const HttpResult& result);
    void startLogList();
    void pollSystemLogs();
    void storeLogSession(const AuthEnvelope& envelope);
    void forgetStoredLogSecrets();
    void dropExpiredLogSession(const QString& message);
    bool handleLogAuthResult(const HttpResult& result);
    bool hasRefreshToken() const;
    QString logAccessSecret() const;
};

} // namespace tg
