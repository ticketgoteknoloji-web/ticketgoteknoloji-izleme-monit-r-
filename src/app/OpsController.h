#pragma once

#include "Engine.h"

#include <QObject>
#include <QVariantList>

namespace tg {

class OpsController : public QObject {
    Q_OBJECT
    Q_PROPERTY(int revision READ revision NOTIFY changed)
    Q_PROPERTY(QVariantList modules READ modules NOTIFY changed)
    Q_PROPERTY(QVariantList incidents READ incidents NOTIFY changed)
    Q_PROPERTY(QVariantList integrations READ integrations NOTIFY changed)
    Q_PROPERTY(QString healthRatioText READ healthRatioText NOTIFY changed)
    Q_PROPERTY(int healthyCount READ healthyCount NOTIFY changed)
    Q_PROPERTY(int warningCount READ warningCount NOTIFY changed)
    Q_PROPERTY(int criticalCount READ criticalCount NOTIFY changed)
    Q_PROPERTY(int disconnectedCount READ disconnectedCount NOTIFY changed)
    Q_PROPERTY(int unmonitoredCount READ unmonitoredCount NOTIFY changed)
    Q_PROPERTY(int totalCount READ totalCount NOTIFY changed)
    Q_PROPERTY(int notConfiguredCount READ notConfiguredCount NOTIFY changed)
    Q_PROPERTY(QString overviewFilter READ overviewFilter WRITE setOverviewFilter NOTIFY changed)
    Q_PROPERTY(bool hataVar READ hataVar NOTIFY changed)
    Q_PROPERTY(QString avgLatencyText READ avgLatencyText NOTIFY changed)
    Q_PROPERTY(QString queueText READ queueText NOTIFY changed)
    Q_PROPERTY(QString stationCpu READ stationCpu NOTIFY changed)
    Q_PROPERTY(QString stationMem READ stationMem NOTIFY changed)
    Q_PROPERTY(QString stationDisk READ stationDisk NOTIFY changed)
    Q_PROPERTY(QString environmentText READ environmentText NOTIFY changed)
    Q_PROPERTY(QString regionText READ regionText NOTIFY changed)
    Q_PROPERTY(QString uptimeText READ uptimeText NOTIFY changed)
    Q_PROPERTY(QString logBanner READ logBanner NOTIFY changed)
    Q_PROPERTY(QString keychainNote READ keychainNote NOTIFY changed)
    Q_PROPERTY(QString dataDirectory READ dataDirectory NOTIFY changed)
    Q_PROPERTY(bool soundEnabled READ soundEnabled WRITE setSoundEnabled NOTIFY changed)
    Q_PROPERTY(bool clock24h READ clock24h NOTIFY changed)
    Q_PROPERTY(bool animationsEnabled READ animationsEnabled NOTIFY changed)
    Q_PROPERTY(QString graphicsQuality READ graphicsQuality NOTIFY changed)
    Q_PROPERTY(bool monitorEnabled READ monitorEnabled NOTIFY changed)
    Q_PROPERTY(bool quietMode READ quietMode NOTIFY changed)
    Q_PROPERTY(bool desktopNotify READ desktopNotify NOTIFY changed)
    Q_PROPERTY(QString goChatLogo READ goChatLogo CONSTANT)
    Q_PROPERTY(QString programLogo READ programLogo CONSTANT)
    Q_PROPERTY(QString goChatLine READ goChatLine NOTIFY changed)
    Q_PROPERTY(QVariantList logSources READ logSources NOTIFY changed)
    Q_PROPERTY(QVariantList logRows READ logRows NOTIFY changed)
    Q_PROPERTY(QVariantMap logDetail READ logDetail NOTIFY changed)
    Q_PROPERTY(QString logAccessNote READ logAccessNote NOTIFY changed)
    Q_PROPERTY(int logHubTotal READ logHubTotal NOTIFY changed)
    Q_PROPERTY(QString logRefreshedAt READ logRefreshedAt NOTIFY changed)
    Q_PROPERTY(bool logHubConnected READ logHubConnected NOTIFY changed)
    Q_PROPERTY(bool logStale READ logStale NOTIFY changed)
    Q_PROPERTY(QString logLastRecordAt READ logLastRecordAt NOTIFY changed)
    Q_PROPERTY(int logPollSeconds READ logPollSeconds NOTIFY changed)
    Q_PROPERTY(bool logLive READ logLive NOTIFY changed)
    Q_PROPERTY(int logInfoCount READ logInfoCount NOTIFY changed)
    Q_PROPERTY(int logWarnCount READ logWarnCount NOTIFY changed)
    Q_PROPERTY(int logErrorCount READ logErrorCount NOTIFY changed)
    Q_PROPERTY(int logCriticalCount READ logCriticalCount NOTIFY changed)
    Q_PROPERTY(int logTotalCount READ logTotalCount NOTIFY changed)
    Q_PROPERTY(int logConnectedSources READ logConnectedSources NOTIFY changed)
    Q_PROPERTY(int logNewCount READ logNewCount NOTIFY changed)
    Q_PROPERTY(QString ticketGoLine READ ticketGoLine NOTIFY changed)
public:
    explicit OpsController(Engine* engine, QObject* parent = nullptr);
    int revision() const;
    QVariantList modules() const;
    QVariantList incidents() const;
    QVariantList integrations() const;
    QString healthRatioText() const;
    int healthyCount() const;
    int warningCount() const;
    int criticalCount() const;
    int disconnectedCount() const;
    int unmonitoredCount() const;
    int totalCount() const;
    int notConfiguredCount() const;
    QString overviewFilter() const;
    void setOverviewFilter(const QString& filter);
    bool hataVar() const;
    QString avgLatencyText() const;
    QString queueText() const;
    QString stationCpu() const;
    QString stationMem() const;
    QString stationDisk() const;
    QString environmentText() const;
    QString regionText() const;
    QString uptimeText() const;
    QString logBanner() const;
    QString keychainNote() const;
    QString dataDirectory() const;
    bool soundEnabled() const;
    void setSoundEnabled(bool on);
    bool clock24h() const;
    bool animationsEnabled() const;
    QString graphicsQuality() const;
    bool monitorEnabled() const;
    bool quietMode() const;
    bool desktopNotify() const;
    QString goChatLogo() const;
    QString programLogo() const;
    QString goChatLine() const;
    QString ticketGoLine() const;

    Q_INVOKABLE QVariantMap moduleMap(const QString& id) const;
    Q_INVOKABLE QString logBody(const QString& level) const;
    Q_INVOKABLE void refreshLogs();
    Q_INVOKABLE void setLogPage(int page);
    Q_INVOKABLE int logPage() const;
    Q_INVOKABLE void hideHubLog(const QString& id);
    Q_INVOKABLE void restoreHiddenHubLogs();
    Q_INVOKABLE QString copySelectedLog();
    Q_INVOKABLE void applyLogFilter(const QString& source, const QString& level, const QString& range, const QString& text, const QString& httpCode, const QString& errorCode, const QString& requestId, const QString& service, const QString& logType, const QString& dateFrom, const QString& dateTo, const QString& user, const QString& ip, const QString& endpoint, const QString& outcome, const QString& action);
    Q_INVOKABLE QVariantList logSections() const;
    Q_INVOKABLE void setLogPollSeconds(int seconds);
    Q_INVOKABLE QVariantList logAlerts() const;
    Q_INVOKABLE void dismissLogAlert(const QString& id);
    Q_INVOKABLE QString removeCustomModule(const QString& moduleId);
    Q_INVOKABLE void setLogLive(bool on);
    Q_INVOKABLE void selectLog(const QString& id);
    Q_INVOKABLE QString setLogToken(const QString& token);
    Q_PROPERTY(QString logSessionPhase READ logSessionPhase NOTIFY changed)
    Q_PROPERTY(bool logSessionBusy READ logSessionBusy NOTIFY changed)
    Q_PROPERTY(QString logSessionMessage READ logSessionMessage NOTIFY changed)
    Q_PROPERTY(QString logSessionAccount READ logSessionAccount NOTIFY changed)
    Q_PROPERTY(QString logSessionVerifiedAt READ logSessionVerifiedAt NOTIFY changed)
    Q_PROPERTY(QString logMfaHint READ logMfaHint NOTIFY changed)
    Q_INVOKABLE QString logSessionText() const;
    Q_INVOKABLE QString openLogSession(const QString& username, const QString& password);
    Q_INVOKABLE QString submitLogMfa(const QString& code);
    Q_INVOKABLE QString resendLogMfa();
    QString logSessionPhase() const;
    bool logSessionBusy() const;
    QString logSessionMessage() const;
    QString logSessionAccount() const;
    QString logSessionVerifiedAt() const;
    QString logMfaHint() const;
    Q_INVOKABLE QString refreshLogSession();
    Q_INVOKABLE QString closeLogSession();
    Q_INVOKABLE QString validateLogSession();
    Q_INVOKABLE void clearLogToken();
    Q_INVOKABLE bool logTokenConfigured() const;
    Q_INVOKABLE QVariantList latencySeries(const QString& moduleId, int minutes) const;
    Q_INVOKABLE QVariantList latencyTimeline(const QString& moduleId, int minutes) const;
    Q_INVOKABLE QVariantMap latencyStats(const QString& moduleId, int minutes) const;
    Q_INVOKABLE QVariantMap performanceMetric(const QString& id, int minutes) const;
    Q_INVOKABLE QString measurePerformance(const QString& kind);
    Q_INVOKABLE void setPaused(const QString& id, bool paused);
    Q_INVOKABLE QString probeNow(const QString& id);
    Q_INVOKABLE QString probeModule(const QString& moduleId);
    Q_INVOKABLE QVariantMap moduleProbe(const QString& moduleId) const;
    Q_INVOKABLE QVariantList relatedModules(const QString& moduleId) const;
    Q_INVOKABLE QVariantList moduleLogs(const QString& moduleId, const QString& level, const QString& text) const;
    Q_INVOKABLE QString moduleLogNote(const QString& moduleId) const;
    Q_INVOKABLE QString exportModuleLogs(const QString& moduleId, const QString& level, const QString& text);
    Q_INVOKABLE QString exportVisibleLogs();
    Q_INVOKABLE QString logoFor(const QString& moduleId, const QString& group) const;
    Q_INVOKABLE QVariantMap apiCenter() const;
    Q_INVOKABLE QString copyApiText(const QString& text);
    Q_INVOKABLE QString refreshApi(const QString& integrationId);
    Q_INVOKABLE QString openApiDoc(const QString& productId);
    Q_INVOKABLE void postNotice(const QString& text);
    Q_INVOKABLE QVariantMap integrationIssues(const QVariantMap& draft) const;
    Q_INVOKABLE QString saveIntegration(const QVariantMap& draft);
    Q_INVOKABLE QString removeIntegration(const QString& id);
    Q_INVOKABLE void testDraft(const QVariantMap& draft, const QString& secret);
    Q_INVOKABLE QString setCredential(const QString& id, const QString& secret);
    Q_INVOKABLE bool credentialConfigured(const QString& id) const;
    Q_INVOKABLE void clearCredential(const QString& id);
    Q_INVOKABLE QString acknowledge(const QString& id);
    Q_INVOKABLE QString silenceIncident(const QString& id, int minutes);
    Q_INVOKABLE QString hideIncident(const QString& id);
    Q_INVOKABLE QString restoreHiddenIncidents();
    Q_INVOKABLE QString copyIncident(const QString& id) const;
    Q_INVOKABLE QString logSourceFor(const QString& moduleId) const;
    Q_INVOKABLE QString exportIncidents(const QString& format, const QString& ids) const;
    Q_INVOKABLE QString pickLogFile();
    Q_INVOKABLE void clearLogFile();
    Q_INVOKABLE QString exportReport(const QString& kind, const QString& format);
    Q_INVOKABLE QVariantMap reportDocument(const QString& kind, const QString& moduleId, const QString& rangeKey, const QString& fromIso, const QString& toIso) const;
    Q_INVOKABLE QVariantList savedReports() const;
    Q_INVOKABLE QString saveReportRecord(const QString& name, const QString& kind, const QString& period, const QString& system, const QString& format, const QString& summary);
    Q_INVOKABLE QString exportReportDocument(const QString& kind, const QString& format, const QString& moduleId, const QString& rangeKey, const QString& fromIso, const QString& toIso);
    Q_INVOKABLE QString deleteSavedReport(const QString& id);
    Q_INVOKABLE QString openSavedReport(const QString& id) const;
    Q_INVOKABLE QString revealSavedReport(const QString& id) const;
    Q_INVOKABLE QString importOpenApi();
    Q_INVOKABLE QVariantMap preferences() const;
    Q_INVOKABLE QVariantMap defaultPreferences() const;
    Q_INVOKABLE QString savePreferences(const QVariantMap& values);
    Q_INVOKABLE QString openDataFolder() const;
    Q_INVOKABLE QString copySystemInfo() const;
    Q_INVOKABLE QString systemInfoText() const;
    Q_INVOKABLE QString testNotification() const;
    Q_INVOKABLE QString clearSampleCache();
    Q_INVOKABLE QString databaseStatus() const;
    Q_INVOKABLE void setLastPage(int index);
    Q_INVOKABLE bool prefOn(const QString& key, bool fallback) const;
    Q_INVOKABLE int prefInt(const QString& key, int fallback) const;

signals:
    void changed();
    void testFinished(const QString& summary);
    void notice(const QString& text);

private:
    QVariantList logSources() const;
    QVariantList logRows() const;
    QVariantMap logDetail() const;
    QString logAccessNote() const;
    int logHubTotal() const;
    QString logRefreshedAt() const;
    bool logHubConnected() const;
    bool logStale() const;
    QString logLastRecordAt() const;
    int logPollSeconds() const;
    bool logLive() const;
    int logInfoCount() const;
    int logWarnCount() const;
    int logErrorCount() const;
    int logCriticalCount() const;
    int logTotalCount() const;
    int logConnectedSources() const;
    int logNewCount() const;
    QVariantMap toMap(const ModuleSnapshot& module) const;
    QVariantMap logMap(const LogRecord& row) const;
    int countLevel(const QString& level) const;
    Integration fromMap(const QVariantMap& draft) const;
    Engine* engine_;
};

} // namespace tg
