#pragma once

#include <QDateTime>
#include <QHostAddress>
#include <QJsonObject>
#include <QVariantMap>
#include <QList>
#include <QString>
#include <optional>

namespace tg {

enum class ModuleStatus {
    Checking,
    Healthy,
    Warning,
    Critical,
    Limited,
    Unknown,
    NotConfigured,
    Disabled,
    NotReleased,
    ConfigMissing,
    ConnectionError,
    Unsupported,
    AuthRejected
};

struct HttpResult {
    QString url;
    bool attempted = false;
    bool transportOk = false;
    int status = 0;
    qint64 latencyMs = -1;
    QJsonObject json;
    bool jsonOk = false;
    QString errorClass;
    QString detail;
};

struct ModuleSnapshot {
    QString id;
    QString group;
    QString name;
    ModuleStatus status = ModuleStatus::NotConfigured;
    QString evidence;
    QString lastError;
    QString connection;
    qint64 latencyMs = -1;
    QDateTime lastCheck;
    std::optional<double> availability;
    std::optional<double> errorRate;
    std::optional<double> serviceRpm;
    int probeCount = 0;
    QString causeLabel;
    QString causeText;
    QString recommendation;
    QList<double> spark;
    int lastHttp = 0;
    int alertAfter = 0;
};

struct Integration {
    QString id;
    QString name;
    QString description;
    QString category;
    QString environment = QStringLiteral("production");
    QString kind = QStringLiteral("custom");
    QString baseUrl;
    QString healthPath = QStringLiteral("/health");
    QString method = QStringLiteral("GET");
    QString authType = QStringLiteral("none");
    QString apiKeyHeader = QStringLiteral("X-Api-Key");
    QString oauthTokenUrl;
    QString oauthClientId;
    int intervalMs = 60000;
    int timeoutMs = 8000;
    int expectedStatus = 200;
    int latencyWarnMs = 1500;
    int failThreshold = 3;
    QString adapter = QStringLiteral("http");
    bool paused = false;
    bool locked = false;
    bool allowPrivate = false;
    QString wsUrl;
    QString successNote;
    QString headersJson = QStringLiteral("[]");
    QString logSource;
};

struct Incident {
    QString id;
    QString moduleId;
    QString application;
    QString moduleName;
    QString service;
    QString kind;
    int httpStatus = 0;
    QString message;
    QDateTime firstSeen;
    QDateTime lastSeen;
    int repeatCount = 1;
    QString severity;
    QString logSource;
    QString causeLabel;
    QString causeText;
    QString recommendation;
    QString state = QStringLiteral("open");
    QDateTime resolvedAt;
};

struct Overview {
    int healthy = 0;
    int warning = 0;
    int critical = 0;
    int limited = 0;
    int notConfigured = 0;
    int connectionErrors = 0;
    int unmonitored = 0;
    int total = 0;
    std::optional<double> healthRatio;
    bool hataVar = false;
    std::optional<double> avgLatencyMs;
    int openCritical = 0;
    QString queueText = QStringLiteral("Bağlantı yapılandırılmadı");
    QString environmentText = QStringLiteral("Ölçülmedi");
    QString regionText = QStringLiteral("Ölçülmedi");
    QString uptimeText = QStringLiteral("Ölçülmedi");
};

struct HostSample {
    bool cpuValid = false;
    double cpuPercent = -1;
    bool memValid = false;
    double memPercent = -1;
    bool diskValid = false;
    double diskPercent = -1;
    QString label = QStringLiteral("Bu bilgisayar");
};

QString statusKey(ModuleStatus status);
QString statusLabel(ModuleStatus status);
QString moduleStatusLabel(const QString& moduleId, ModuleStatus status);
QString connectionLabel(ModuleStatus status);
QString toneFor(ModuleStatus status);
bool countsInHealthRatio(ModuleStatus status);
bool isBadStatus(ModuleStatus status);
ModuleStatus statusFromKey(const QString& key);

QString redactText(const QString& input);
QString redactLog(const QString& input);
QJsonObject unwrapPayload(const QJsonObject& object);
QString optNumber(const std::optional<double>& value, int digits = 1);
QString latencyText(qint64 ms);

struct SsrfDecision {
    bool ok = false;
    QString reason;
};

SsrfDecision precheckUrl(const QUrl& url, const QStringList& extraHosts, bool allowPrivate);
QVariantMap integrationFieldErrors(const Integration& integration, const QList<Integration>& existing);
QString integrationBucket(ModuleStatus status, bool paused);
QByteArray basicAuthorizationValue(const QString& secret, QString* error);
bool addressBlocked(const QHostAddress& address, bool allowPrivate);
QStringList defaultAllowHosts();

} // namespace tg
