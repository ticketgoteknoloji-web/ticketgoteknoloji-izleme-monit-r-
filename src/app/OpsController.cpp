#include "OpsController.h"
#include "ReportStats.h"
#include "ModuleLinks.h"
#include "ReportWriter.h"

#include "HealthEval.h"
#include "ApiCatalog.h"
#include "ModuleLinks.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileDialog>
#include <QJsonArray>
#include <QJsonDocument>
#include <QClipboard>
#include <QProcess>
#include <QDesktopServices>
#include <QFileInfo>
#include <QGuiApplication>
#include <QStandardPaths>
#include <QSysInfo>
#include <QSystemTrayIcon>
#include <QUrl>

#if defined(Q_OS_MACOS)
#include <AudioToolbox/AudioToolbox.h>
#endif

namespace tg {
namespace {
QString pct(double value) { return QString::number(value, 'f', 0) + QStringLiteral("%"); }

QString logoForModule(const QString& id, const QString& group)
{
    const QString root = QStringLiteral("qrc:/qt/qml/TicketGo/Ops/qml/assets/");
    if (id == QLatin1String("tg-ferry")) return root + QStringLiteral("logo-ferry.png");
    if (id.startsWith(QLatin1String("gochat")) || group == QLatin1String("gochat")) return root + QStringLiteral("logo-gochat-mark.png");
    if (id.startsWith(QLatin1String("tg-")) || group == QLatin1String("ticket-go")) return root + QStringLiteral("logo-ticketgo.png");
    return {};
}
}

OpsController::OpsController(Engine* engine, QObject* parent) : QObject(parent), engine_(engine)
{
    connect(engine_, &Engine::updated, this, &OpsController::changed);
    connect(engine_, &Engine::testFinished, this, &OpsController::testFinished);
    connect(engine_, &Engine::criticalOpened, this, [this](const QString& title, const QString& body) {
        emit notice(title + QStringLiteral(" — ") + body);
    });
}

int OpsController::revision() const { return engine_->revision(); }
QString OpsController::healthRatioText() const
{
    const Overview overview = engine_->overview();
    const int measured = overview.healthy + overview.warning + overview.critical + overview.connectionErrors;
    if (!overview.healthRatio || measured <= 0) return QStringLiteral("Hesaplanamadı · ölçülen servis yok");
    return QString::number(*overview.healthRatio * 100.0, 'f', 1) + QStringLiteral("% · ")
        + QString::number(overview.healthy) + QStringLiteral("/") + QString::number(measured)
        + QStringLiteral(" ölçülen · izlenemeyenler paydaya girmez");
}
int OpsController::healthyCount() const { return engine_->overview().healthy; }
int OpsController::warningCount() const { return engine_->overview().warning; }
int OpsController::criticalCount() const { return engine_->overview().critical; }
int OpsController::disconnectedCount() const { return engine_->overview().connectionErrors; }
int OpsController::unmonitoredCount() const { return engine_->overview().unmonitored; }
int OpsController::totalCount() const { return engine_->overview().total; }
int OpsController::notConfiguredCount() const { return engine_->overview().notConfigured; }
QString OpsController::overviewFilter() const { return engine_->overviewFilter(); }
void OpsController::setOverviewFilter(const QString& filter) { engine_->setOverviewFilter(filter); }
bool OpsController::hataVar() const { return engine_->overview().hataVar; }
QString OpsController::avgLatencyText() const
{
    const auto value = engine_->overview().avgLatencyMs;
    if (!value) return QStringLiteral("—");
    return QString::number(*value, 'f', 0) + QStringLiteral(" ms");
}
QString OpsController::queueText() const { return engine_->queueText(); }
QString OpsController::stationCpu() const { return engine_->host().cpuValid ? pct(engine_->host().cpuPercent) : QStringLiteral("ölçülüyor"); }
QString OpsController::stationMem() const { return engine_->host().memValid ? pct(engine_->host().memPercent) : QStringLiteral("—"); }
QString OpsController::stationDisk() const { return engine_->host().diskValid ? pct(engine_->host().diskPercent) : QStringLiteral("—"); }
QString OpsController::environmentText() const { return engine_->environmentText(); }
QString OpsController::regionText() const { return engine_->regionText(); }
QString OpsController::uptimeText() const { return engine_->uptimeText(); }
QString OpsController::logBanner() const { return engine_->logBanner(); }
QString OpsController::keychainNote() const { return engine_->keychainNote(); }
QString OpsController::dataDirectory() const { return engine_->dataDirectory(); }
bool OpsController::soundEnabled() const { return engine_->soundEnabled(); }
void OpsController::setSoundEnabled(bool on) { engine_->setSoundEnabled(on); }
bool OpsController::clock24h() const { return engine_->setting(QStringLiteral("clock_24h"), QStringLiteral("1")) != QLatin1String("0"); }
bool OpsController::animationsEnabled() const
{
    return engine_->setting(QStringLiteral("animations"), QStringLiteral("1")) != QLatin1String("0")
        && engine_->setting(QStringLiteral("low_power"), QStringLiteral("0")) != QLatin1String("1");
}
QString OpsController::graphicsQuality() const
{
    const QString value = engine_->setting(QStringLiteral("graphics_quality"), QStringLiteral("high"));
    if (value == QLatin1String("low") || value == QLatin1String("medium")) return value;
    return QStringLiteral("high");
}
bool OpsController::monitorEnabled() const { return engine_->setting(QStringLiteral("monitor_enabled"), QStringLiteral("1")) != QLatin1String("0"); }
bool OpsController::quietMode() const { return engine_->setting(QStringLiteral("quiet_mode"), QStringLiteral("0")) == QLatin1String("1"); }
bool OpsController::desktopNotify() const { return engine_->setting(QStringLiteral("desktop_notify"), QStringLiteral("1")) != QLatin1String("0"); }

QVariantMap OpsController::preferences() const
{
    auto on = [this](const QString& key, bool fallback) { return engine_->setting(key, fallback ? QStringLiteral("1") : QStringLiteral("0")) != QLatin1String("0"); };
    return {
        {QStringLiteral("intervalSec"), engine_->setting(QStringLiteral("probe_interval_sec"), QStringLiteral("15")).toInt()},
        {QStringLiteral("timeoutSec"), engine_->setting(QStringLiteral("probe_timeout_sec"), QStringLiteral("8")).toInt()},
        {QStringLiteral("retryCap"), engine_->setting(QStringLiteral("retry_cap"), QStringLiteral("4")).toInt()},
        {QStringLiteral("latencyWarnMs"), engine_->setting(QStringLiteral("latency_warn_ms"), QStringLiteral("1500")).toInt()},
        {QStringLiteral("retentionDays"), engine_->setting(QStringLiteral("retention_days"), QStringLiteral("1")).toInt()},
        {QStringLiteral("monitorEnabled"), on(QStringLiteral("monitor_enabled"), true)},
        {QStringLiteral("rememberPage"), on(QStringLiteral("remember_page"), true)},
        {QStringLiteral("clock24h"), on(QStringLiteral("clock_24h"), true)},
        {QStringLiteral("sound"), on(QStringLiteral("sound"), true)},
        {QStringLiteral("desktopNotify"), on(QStringLiteral("desktop_notify"), true)},
        {QStringLiteral("quiet"), on(QStringLiteral("quiet_mode"), false)},
        {QStringLiteral("animations"), on(QStringLiteral("animations"), true)},
        {QStringLiteral("lowPower"), engine_->setting(QStringLiteral("low_power"), QStringLiteral("0")) == QLatin1String("1")},
        {QStringLiteral("startFullscreen"), engine_->setting(QStringLiteral("start_fullscreen"), QStringLiteral("0")) == QLatin1String("1")},
        {QStringLiteral("quality"), graphicsQuality()}
    };
}

QVariantMap OpsController::defaultPreferences() const
{
    return {
        {QStringLiteral("intervalSec"), 15},
        {QStringLiteral("timeoutSec"), 8},
        {QStringLiteral("retryCap"), 4},
        {QStringLiteral("latencyWarnMs"), 1500},
        {QStringLiteral("retentionDays"), 1},
        {QStringLiteral("monitorEnabled"), true},
        {QStringLiteral("rememberPage"), true},
        {QStringLiteral("clock24h"), true},
        {QStringLiteral("sound"), true},
        {QStringLiteral("desktopNotify"), true},
        {QStringLiteral("quiet"), false},
        {QStringLiteral("animations"), true},
        {QStringLiteral("lowPower"), false},
        {QStringLiteral("startFullscreen"), false},
        {QStringLiteral("quality"), QStringLiteral("high")}
    };
}

QString OpsController::savePreferences(const QVariantMap& values)
{
    int customUpdated = 0;
    const QString error = engine_->savePreferences(
        values.value(QStringLiteral("intervalSec")).toInt(),
        values.value(QStringLiteral("timeoutSec")).toInt(),
        values.value(QStringLiteral("retryCap")).toInt(),
        values.value(QStringLiteral("latencyWarnMs")).toInt(),
        values.value(QStringLiteral("retentionDays")).toInt(),
        values.value(QStringLiteral("monitorEnabled")).toBool(),
        values.value(QStringLiteral("rememberPage")).toBool(),
        values.value(QStringLiteral("clock24h")).toBool(),
        values.value(QStringLiteral("sound")).toBool(),
        values.value(QStringLiteral("desktopNotify")).toBool(),
        values.value(QStringLiteral("quiet")).toBool(),
        values.value(QStringLiteral("animations")).toBool(),
        values.value(QStringLiteral("lowPower")).toBool(),
        values.value(QStringLiteral("startFullscreen")).toBool(),
        values.value(QStringLiteral("quality")).toString(),
        &customUpdated);
    if (!error.isEmpty()) return error;
    return {};
}

QString OpsController::openDataFolder() const
{
    const QString dir = engine_->dataDirectory();
    if (dir.isEmpty() || !QFileInfo(dir).isDir()) return QStringLiteral("Veri klasörü bulunamadı.");
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(dir))) return QStringLiteral("Finder isteği kabul edilmedi.");
    return {};
}

QString OpsController::systemInfoText() const
{
    return QStringList{
        QStringLiteral("Program: TICKET GO TEKNOLOJİ AŞ. Sistem İzleme ve Takip Görüntüleme Sistemi"),
        QStringLiteral("Sürüm: ") + QGuiApplication::applicationVersion(),
        QStringLiteral("Qt: ") + QString::fromLatin1(qVersion()),
        QStringLiteral("macOS: ") + QSysInfo::prettyProductName(),
        QStringLiteral("Mimari: ") + QSysInfo::currentCpuArchitecture(),
        QStringLiteral("Veri klasörü: ") + engine_->dataDirectory(),
        QStringLiteral("Veritabanı: ") + databaseStatus(),
        QStringLiteral("İzleme: ") + QString(monitorEnabled() ? QStringLiteral("aktif") : QStringLiteral("durduruldu")),
        QStringLiteral("TLS: doğrulama açık"),
        QStringLiteral("Kimlik saklama: ") + engine_->keychainNote()
    }.join(QLatin1Char('\n'));
}

QString OpsController::copySystemInfo() const
{
    if (QClipboard* clipboard = QGuiApplication::clipboard()) clipboard->setText(systemInfoText());
    else return QStringLiteral("Pano kullanılamıyor.");
    return {};
}

QString OpsController::testNotification() const
{
    if (!QSystemTrayIcon::isSystemTrayAvailable() || !QSystemTrayIcon::supportsMessages()) {
        return QStringLiteral("Masaüstü bildirimi kullanılamıyor. Sistem tepsisi veya bildirim desteği yok.");
    }
    QSystemTrayIcon tray;
    tray.setIcon(QIcon(QStringLiteral(":/qt/qml/TicketGo/Ops/qml/assets/logo-program.png")));
    tray.show();
    tray.showMessage(QStringLiteral("TICKET GO"), QStringLiteral("Yerel bildirim denemesi. Bu bir alarm değildir."), QSystemTrayIcon::Information, 4000);
#if defined(Q_OS_MACOS)
    if (soundEnabled() && !quietMode()) AudioServicesPlayAlertSound(kSystemSoundID_UserPreferredAlert);
#endif
    return {};
}

QString OpsController::clearSampleCache()
{
    return engine_->clearOldSamples();
}

QString OpsController::databaseStatus() const
{
    const qint64 bytes = engine_->databaseBytes();
    if (bytes < 0) return QStringLiteral("bulunamadı");
    return QString::number(bytes / 1024) + QStringLiteral(" KB");
}

void OpsController::setLastPage(int index) { engine_->setSetting(QStringLiteral("last_page"), QString::number(index)); }
bool OpsController::prefOn(const QString& key, bool fallback) const
{
    return engine_->setting(key, fallback ? QStringLiteral("1") : QStringLiteral("0")) != QLatin1String("0");
}
int OpsController::prefInt(const QString& key, int fallback) const
{
    const QString value = engine_->setting(key);
    return value.isEmpty() ? fallback : value.toInt();
}

QString OpsController::goChatLogo() const
{
    return logoForModule(QStringLiteral("gochat-api"), QStringLiteral("gochat"));
}

QString OpsController::programLogo() const
{
    return QStringLiteral("qrc:/qt/qml/TicketGo/Ops/qml/assets/logo-program.png");
}

QString OpsController::goChatLine() const
{
    const QVariantMap api = moduleMap(QStringLiteral("gochat-api"));
    const QVariantMap fcm = moduleMap(QStringLiteral("gochat-fcm"));
    const QVariantMap turn = moduleMap(QStringLiteral("gochat-turn"));
    const QVariantMap sos = moduleMap(QStringLiteral("gochat-sos"));
    return api.value(QStringLiteral("statusLabel")).toString() + QStringLiteral(" · FCM ") + fcm.value(QStringLiteral("statusLabel")).toString()
        + QStringLiteral(" · TURN ") + turn.value(QStringLiteral("statusLabel")).toString()
        + QStringLiteral(" · SOS ") + sos.value(QStringLiteral("statusLabel")).toString();
}

QString OpsController::ticketGoLine() const
{
    const QVariantMap api = moduleMap(QStringLiteral("tg-api"));
    return api.value(QStringLiteral("statusLabel")).toString() + QStringLiteral(" · ") + api.value(QStringLiteral("latency")).toString();
}

QVariantMap OpsController::toMap(const ModuleSnapshot& module) const
{
    QVariantList spark;
    for (double point : module.spark) spark.push_back(point);
    QVariantMap map;
    map.insert(QStringLiteral("moduleId"), module.id);
    map.insert(QStringLiteral("name"), module.name);
    map.insert(QStringLiteral("group"), module.group);
    map.insert(QStringLiteral("logo"), logoForModule(module.id, module.group));
    const HealthClass bucket = classifyStatus(module.status);
    map.insert(QStringLiteral("status"), statusKey(module.status));
    const QString shownLabel = moduleStatusLabel(module.id, module.status);
    map.insert(QStringLiteral("statusLabel"), shownLabel);
    map.insert(QStringLiteral("healthClass"), healthClassKey(bucket));
    map.insert(QStringLiteral("classLabel"), (module.id == QLatin1String("tg-netgsm") || module.id == QLatin1String("tg-iyzico")) && module.status == ModuleStatus::Healthy ? shownLabel : healthClassLabel(bucket));
    map.insert(QStringLiteral("problem"), isProblemClass(bucket));
    ModuleStatus toneStatus = module.status;
    if (bucket == HealthClass::Warning) toneStatus = ModuleStatus::Warning;
    else if (bucket == HealthClass::Critical) toneStatus = ModuleStatus::Critical;
    else if (bucket == HealthClass::Disconnected) toneStatus = ModuleStatus::ConnectionError;
    else if (bucket == HealthClass::Healthy) toneStatus = ModuleStatus::Healthy;
    map.insert(QStringLiteral("tone"), toneFor(toneStatus));
    map.insert(QStringLiteral("logHint"), engine_->logBanner());
    map.insert(QStringLiteral("latency"), latencyText(module.latencyMs));
    map.insert(QStringLiteral("lastCheck"), module.lastCheck.isValid() ? module.lastCheck.toString(QStringLiteral("HH:mm:ss")) : QStringLiteral("—"));
    map.insert(QStringLiteral("evidence"), module.evidence);
    map.insert(QStringLiteral("lastError"), module.lastError.isEmpty() ? QStringLiteral("—") : module.lastError);
    map.insert(QStringLiteral("connection"), module.connection);
    map.insert(QStringLiteral("availability"), module.availability ? QString::number(*module.availability, 'f', 1) + QStringLiteral("%") : QStringLiteral("—"));
    map.insert(QStringLiteral("errorRate"), module.errorRate ? QString::number(*module.errorRate * 100.0, 'f', 1) + QStringLiteral("%") : QStringLiteral("—"));
    map.insert(QStringLiteral("rpm"), module.serviceRpm ? QString::number(*module.serviceRpm, 'f', 0) : QStringLiteral("—"));
    map.insert(QStringLiteral("probes"), module.probeCount > 0 ? QString::number(module.probeCount) : QStringLiteral("—"));
    map.insert(QStringLiteral("causeLabel"), module.causeLabel);
    map.insert(QStringLiteral("causeText"), module.causeText);
    map.insert(QStringLiteral("recommendation"), module.recommendation);
    map.insert(QStringLiteral("spark"), spark);
    map.insert(QStringLiteral("httpCode"), module.lastHttp > 0 ? QString::number(module.lastHttp) : QStringLiteral("—"));
    return map;
}

QVariantList OpsController::modules() const
{
    QVariantList list;
    for (const ModuleSnapshot& module : engine_->modules()) list.push_back(toMap(module));
    return list;
}

QVariantMap OpsController::moduleMap(const QString& id) const
{
    for (const ModuleSnapshot& module : engine_->modules()) {
        if (module.id == id) return toMap(module);
    }
    return {};
}

QVariantList OpsController::incidents() const
{
    QVariantList list;
    for (const Incident& incident : engine_->incidents()) {
        QVariantMap map;
        map.insert(QStringLiteral("id"), incident.id);
        map.insert(QStringLiteral("moduleId"), incident.moduleId);
        map.insert(QStringLiteral("application"), incident.application);
        map.insert(QStringLiteral("moduleName"), incident.moduleName);
        map.insert(QStringLiteral("service"), incident.service);
        map.insert(QStringLiteral("kind"), incident.kind);
        map.insert(QStringLiteral("httpStatus"), incident.httpStatus);
        map.insert(QStringLiteral("message"), incident.message);
        map.insert(QStringLiteral("firstSeen"), incident.firstSeen.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        map.insert(QStringLiteral("lastSeen"), incident.lastSeen.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")));
        map.insert(QStringLiteral("repeatCount"), incident.repeatCount);
        map.insert(QStringLiteral("severity"), incident.severity);
        map.insert(QStringLiteral("logSource"), incident.logSource);
        map.insert(QStringLiteral("causeLabel"), incident.causeLabel);
        map.insert(QStringLiteral("causeText"), incident.causeText);
        map.insert(QStringLiteral("recommendation"), incident.recommendation);
        map.insert(QStringLiteral("state"), incident.state);
        map.insert(QStringLiteral("level"), incident.severity == QLatin1String("critical") ? QStringLiteral("CRITICAL") : (incident.severity == QLatin1String("warning") ? QStringLiteral("WARNING") : incident.severity.toUpper()));
        map.insert(QStringLiteral("flow"), incident.state == QLatin1String("open") ? QStringLiteral("ACTIVE") : (incident.state == QLatin1String("acknowledged") ? QStringLiteral("ACKNOWLEDGED") : (incident.state == QLatin1String("resolved") ? QStringLiteral("RESOLVED") : incident.state)));
        map.insert(QStringLiteral("firstMs"), incident.firstSeen.toMSecsSinceEpoch());
        map.insert(QStringLiteral("lastMs"), incident.lastSeen.toMSecsSinceEpoch());
        map.insert(QStringLiteral("resolvedAt"), incident.resolvedAt.isValid() ? incident.resolvedAt.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QString());
        QString ackAt;
        const QString prefix = incident.id + QLatin1Char('|');
        for (const QString& line : engine_->setting(QStringLiteral("incident_ack_at")).split(QLatin1Char('\n'), Qt::SkipEmptyParts)) {
            if (line.startsWith(prefix)) ackAt = line.mid(prefix.size());
        }
        map.insert(QStringLiteral("ackAt"), ackAt);
        map.insert(QStringLiteral("hidden"), engine_->hiddenIncidentIds().contains(incident.id));
        map.insert(QStringLiteral("silenced"), engine_->incidentSilenced(incident.id));
        map.insert(QStringLiteral("logo"), logoForModule(incident.moduleId, incident.application == QLatin1String("Go-Chat") ? QStringLiteral("gochat") : QString()));
        list.push_back(map);
    }
    return list;
}

QVariantList OpsController::integrations() const
{
    QVariantList list;
    for (const Integration& integration : engine_->integrations()) {
        QVariantMap map;
        map.insert(QStringLiteral("id"), integration.id);
        map.insert(QStringLiteral("name"), integration.name);
        map.insert(QStringLiteral("description"), integration.description);
        map.insert(QStringLiteral("category"), integration.category);
        map.insert(QStringLiteral("environment"), integration.environment);
        map.insert(QStringLiteral("kind"), integration.kind);
        map.insert(QStringLiteral("baseUrl"), integration.baseUrl);
        map.insert(QStringLiteral("healthPath"), integration.healthPath);
        map.insert(QStringLiteral("method"), integration.method);
        map.insert(QStringLiteral("authType"), integration.authType);
        map.insert(QStringLiteral("apiKeyHeader"), integration.apiKeyHeader);
        map.insert(QStringLiteral("oauthTokenUrl"), integration.oauthTokenUrl);
        map.insert(QStringLiteral("oauthClientId"), integration.oauthClientId);
        map.insert(QStringLiteral("intervalMs"), integration.intervalMs);
        map.insert(QStringLiteral("timeoutMs"), integration.timeoutMs);
        map.insert(QStringLiteral("expectedStatus"), integration.expectedStatus);
        map.insert(QStringLiteral("latencyWarnMs"), integration.latencyWarnMs);
        map.insert(QStringLiteral("adapter"), integration.adapter);
        map.insert(QStringLiteral("paused"), integration.paused);
        map.insert(QStringLiteral("locked"), integration.locked);
        map.insert(QStringLiteral("allowPrivate"), integration.allowPrivate);
        map.insert(QStringLiteral("wsUrl"), integration.wsUrl);
        map.insert(QStringLiteral("successNote"), integration.successNote);
        map.insert(QStringLiteral("credential"), engine_->hasCredential(integration.id));
        map.insert(QStringLiteral("paused"), integration.paused);
        const QString moduleId = moduleIdForIntegration(integration.id);
        const QVariantMap module = moduleMap(moduleId);
        const ModuleStatus status = module.isEmpty() ? ModuleStatus::Checking : statusFromKey(module.value(QStringLiteral("status")).toString());
        const QString bucket = integrationBucket(status, integration.paused);
        map.insert(QStringLiteral("moduleId"), moduleId);
        map.insert(QStringLiteral("status"), statusKey(status));
        map.insert(QStringLiteral("bucket"), bucket);
        const QString measuredLabel = module.value(QStringLiteral("statusLabel")).toString();
        map.insert(QStringLiteral("statusLabel"), bucket == QLatin1String("unconfigured") && !integration.locked ? QStringLiteral("Yapılandırıldı / Doğrulanmadı") : (measuredLabel.isEmpty() ? statusLabel(status) : measuredLabel));
        map.insert(QStringLiteral("tone"), bucket == QLatin1String("healthy") ? QStringLiteral("green") : (bucket == QLatin1String("warning") ? QStringLiteral("amber") : (bucket == QLatin1String("critical") || bucket == QLatin1String("connection") || bucket == QLatin1String("auth") ? QStringLiteral("red") : QStringLiteral("gray"))));
        map.insert(QStringLiteral("latency"), bucket == QLatin1String("unconfigured") ? QStringLiteral("Ölçüm alınamadı") : module.value(QStringLiteral("latency")).toString());
        const QString seen = module.value(QStringLiteral("lastCheck")).toString();
        map.insert(QStringLiteral("lastCheck"), bucket == QLatin1String("unconfigured") || seen.isEmpty() || seen == QLatin1String("—") ? QStringLiteral("Ölçüm alınamadı") : seen);
        map.insert(QStringLiteral("httpCode"), module.value(QStringLiteral("httpCode")).toString().isEmpty() ? QStringLiteral("—") : module.value(QStringLiteral("httpCode")).toString());
        map.insert(QStringLiteral("evidence"), module.value(QStringLiteral("evidence")).toString());
        auto stamp = [](const QVariantMap& sample) {
            const qint64 ts = sample.value(QStringLiteral("ts")).toLongLong();
            if (!sample.value(QStringLiteral("found")).toBool() || ts <= 0) return QStringLiteral("Ölçüm alınamadı");
            return QDateTime::fromMSecsSinceEpoch(ts).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        };
        const QVariantMap success = engine_->latestHealthySample(moduleId);
        const QVariantMap failed = engine_->latestFailedSample(moduleId);
        map.insert(QStringLiteral("lastSuccess"), stamp(success));
        map.insert(QStringLiteral("lastFailure"), stamp(failed));
        map.insert(QStringLiteral("lastFailureText"), failed.value(QStringLiteral("evidence")).toString());
        map.insert(QStringLiteral("apiType"), QStringLiteral("REST / HTTPS"));
        QString authLabel = QStringLiteral("Kimlik doğrulamasız");
        if (integration.authType == QLatin1String("api_key")) authLabel = QStringLiteral("API Key");
        else if (integration.authType == QLatin1String("bearer")) authLabel = QStringLiteral("Bearer Token");
        else if (integration.authType == QLatin1String("basic")) authLabel = QStringLiteral("Basic Authentication");
        else if (integration.authType == QLatin1String("oauth2")) authLabel = QStringLiteral("OAuth 2.0");
        else if (integration.authType == QLatin1String("mtls")) authLabel = QStringLiteral("mTLS");
        map.insert(QStringLiteral("authLabel"), authLabel);
        map.insert(QStringLiteral("failThreshold"), integration.failThreshold > 0 ? integration.failThreshold : 3);
        list.push_back(map);
    }
    return list;
}

QString OpsController::logBody(const QString& level) const { return engine_->logBody(level); }
QVariantList OpsController::latencySeries(const QString& moduleId, int minutes) const
{
    QVariantList list;
    for (double value : engine_->latencySeries(moduleId, minutes)) list.push_back(value);
    return list;
}

QVariantList OpsController::latencyTimeline(const QString& moduleId, int minutes) const
{
    QVariantList list;
    for (const auto& point : engine_->latencyPoints(moduleId, minutes)) {
        list.push_back(QVariantMap{
            {QStringLiteral("ts"), point.first},
            {QStringLiteral("latency"), point.second},
            {QStringLiteral("at"), QDateTime::fromMSecsSinceEpoch(point.first).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"))}
        });
    }
    return list;
}

QVariantMap OpsController::performanceMetric(const QString& id, int minutes) const
{
    const QVariantMap raw = engine_->performanceMetric(id, minutes);
    const bool found = raw.value(QStringLiteral("found")).toBool();
    const qint64 latency = raw.value(QStringLiteral("latencyMs")).toLongLong();
    const qint64 stamp = raw.value(QStringLiteral("ts")).toLongLong();
    QString value = QStringLiteral("Ölçüm alınamadı");
    if (found && latency >= 0) value = QString::number(latency) + QStringLiteral(" ms");
    else if (found) value = raw.value(QStringLiteral("evidence")).toString().isEmpty() ? QStringLiteral("Ölçüm alınamadı") : raw.value(QStringLiteral("evidence")).toString();
    const int http = raw.value(QStringLiteral("httpStatus")).toInt();
    return {
        {QStringLiteral("value"), value},
        {QStringLiteral("latencyMs"), latency},
        {QStringLiteral("found"), found},
        {QStringLiteral("http"), http > 0 ? QString::number(http) : QStringLiteral("Veri yok")},
        {QStringLiteral("lastAt"), stamp > 0 ? QDateTime::fromMSecsSinceEpoch(stamp).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("Ölçüm alınamadı")},
        {QStringLiteral("successRate"), raw.contains(QStringLiteral("successRate")) ? raw.value(QStringLiteral("successRate")).toString() : QStringLiteral("Ölçüm alınamadı")},
        {QStringLiteral("errorRate"), raw.contains(QStringLiteral("errorRate")) ? raw.value(QStringLiteral("errorRate")).toString() : QStringLiteral("Ölçüm alınamadı")},
        {QStringLiteral("evidence"), raw.value(QStringLiteral("evidence")).toString()}
    };
}

QString OpsController::measurePerformance(const QString& kind)
{
    return engine_->measurePerformance(kind);
}

QVariantMap OpsController::latencyStats(const QString& moduleId, int minutes) const
{
    QList<double> values;
    for (double value : engine_->latencySeries(moduleId, minutes)) values.push_back(value);
    const SeriesStats stats = summarizeSeries(values);
    auto text = [](const std::optional<double>& value) { return value ? QString::number(*value, 'f', 0) + QStringLiteral(" ms") : QStringLiteral("Ölçüm alınamadı"); };
    const qint64 lastMs = engine_->lastLatencyMs(moduleId, minutes);
    QString delta = QStringLiteral("karşılaştırma yok");
    if (values.size() >= 2) delta = QString::number(values.last() - values.first(), 'f', 0) + QStringLiteral(" ms");
    return {
        {QStringLiteral("count"), stats.count},
        {QStringLiteral("min"), text(stats.minimum)},
        {QStringLiteral("max"), text(stats.maximum)},
        {QStringLiteral("avg"), text(stats.average)},
        {QStringLiteral("last"), text(stats.last)},
        {QStringLiteral("p95"), stats.p95 ? text(stats.p95) : QStringLiteral("yeterli ham veri yok")},
        {QStringLiteral("lastValue"), stats.last ? *stats.last : -1},
        {QStringLiteral("delta"), delta},
        {QStringLiteral("lastAt"), lastMs > 0 ? QDateTime::fromMSecsSinceEpoch(lastMs).toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("Ölçüm alınamadı")}
    };
}
void OpsController::setPaused(const QString& id, bool paused) { engine_->setPaused(id, paused); }
QString OpsController::probeNow(const QString& id) { return engine_->probeNow(id); }

QString OpsController::logoFor(const QString& moduleId, const QString& group) const
{
    return logoForModule(moduleId, group);
}

QVariantMap OpsController::apiCenter() const
{
    return buildApiCenter(engine_->modules(), engine_->environmentText());
}

QString OpsController::copyApiText(const QString& text)
{
    const QString value = text.trimmed();
    QString message;
    if (value.isEmpty() || value == QLatin1String("—")) message = QStringLiteral("API adresi bulunamadı");
    else if (!apiShareableText(value)) message = QStringLiteral("Bu metin paylaşılabilir katalogda yok. Kopyalanmadı.");
    else if (QClipboard* clipboard = QGuiApplication::clipboard()) {
        clipboard->setText(value);
        message = value.startsWith(QLatin1String("https://")) ? QStringLiteral("Adres kopyalandı") : QStringLiteral("Yol kopyalandı");
    } else {
        message = QStringLiteral("Pano kullanılamıyor.");
    }
    emit notice(message);
    return message;
}

QString OpsController::refreshApi(const QString& integrationId)
{
    QString message;
    if (integrationId != QLatin1String("ticket-go-backend") && integrationId != QLatin1String("go-chat-api") && integrationId != QLatin1String("ticket-go-ferry")) {
        message = QStringLiteral("API durumu alınamadı");
    } else {
        message = engine_->probeNow(integrationId);
    }
    if (message != QLatin1String("Test gönderildi.")) emit notice(message);
    return message;
}

QString OpsController::openApiDoc(const QString& productId)
{
    Q_UNUSED(productId);
    const QString message = QStringLiteral("Dokümantasyon henüz tanımlanmadı.");
    emit notice(message);
    return message;
}

void OpsController::postNotice(const QString& text)
{
    const QString message = text.trimmed();
    if (!message.isEmpty()) emit notice(message);
}

QString OpsController::probeModule(const QString& moduleId)
{
    QStringList ids;
    for (const Integration& integration : engine_->integrations()) ids.push_back(integration.id);
    const QString integrationId = integrationIdForModule(moduleId, ids);
    if (integrationId.isEmpty()) return QStringLiteral("Bu modülün kendi bağlayıcısı yok.");
    return engine_->probeNow(integrationId);
}

QVariantMap OpsController::moduleProbe(const QString& moduleId) const
{
    QStringList ids;
    for (const Integration& integration : engine_->integrations()) ids.push_back(integration.id);
    const QString integrationId = integrationIdForModule(moduleId, ids);
    QVariantMap map;
    map.insert(QStringLiteral("integrationId"), integrationId);
    for (const Integration& integration : engine_->integrations()) {
        if (integration.id != integrationId) continue;
        map.insert(QStringLiteral("endpoint"), integration.baseUrl + integration.healthPath);
        map.insert(QStringLiteral("method"), integration.method);
        map.insert(QStringLiteral("timeoutMs"), integration.timeoutMs);
        map.insert(QStringLiteral("intervalMs"), integration.intervalMs);
        map.insert(QStringLiteral("paused"), integration.paused);
        break;
    }
    if (!map.contains(QStringLiteral("endpoint"))) {
        map.insert(QStringLiteral("endpoint"), QStringLiteral("—"));
        map.insert(QStringLiteral("method"), QStringLiteral("—"));
        map.insert(QStringLiteral("note"), QStringLiteral("Bu modül ayrı bir HTTP ucu ile ölçülmüyor."));
    } else if (moduleId != QLatin1String("tg-api") && moduleId != QLatin1String("gochat-api") && moduleId != QLatin1String("tg-ferry") && moduleId != QLatin1String("tg-web") && moduleId != QLatin1String("tg-admin") && !moduleId.startsWith(QLatin1String("custom-"))) {
        map.insert(QStringLiteral("note"), QStringLiteral("Bu kart üst sağlık yanıtındaki alandır. Ayrı istek atılmaz."));
    } else {
        map.insert(QStringLiteral("note"), QStringLiteral("Salt okunur sağlık kontrolü."));
    }
    return map;
}

QVariantList OpsController::relatedModules(const QString& moduleId) const
{
    QVariantList list;
    for (const QString& id : childModuleIds(moduleId)) {
        const QVariantMap child = moduleMap(id);
        if (!child.isEmpty()) list.push_back(child);
    }
    return list;
}

QString OpsController::moduleLogNote(const QString& moduleId) const
{
    if (logSourceForModule(moduleId).isEmpty()) return QStringLiteral("Bu modüle ait log kaynağına erişilemiyor");
    if (!engine_->logTokenConfigured() && engine_->logRows().isEmpty()) return QStringLiteral("Bu modüle ait log kaynağına erişilemiyor");
    return QStringLiteral("Kayıtlar seçilen modülün kaynağından süzülür. Başka modülün satırı gösterilmez.");
}

QVariantList OpsController::moduleLogs(const QString& moduleId, const QString& level, const QString& text) const
{
    QVariantList list;
    const QString source = logSourceForModule(moduleId);
    if (source.isEmpty()) return list;
    const QString wanted = level.trimmed().toUpper();
    for (const LogRecord& row : engine_->logRows()) {
        if (row.sourceId != source && row.module != source && row.service != source) continue;
        if (!wanted.isEmpty() && wanted != QLatin1String("TÜMÜ") && row.level != wanted) continue;
        if (!text.isEmpty() && !row.message.contains(text, Qt::CaseInsensitive) && !row.errorCode.contains(text, Qt::CaseInsensitive) && !row.requestId.contains(text, Qt::CaseInsensitive)) continue;
        list.push_back(logMap(row));
        if (list.size() >= 40) break;
    }
    return list;
}

QString OpsController::exportModuleLogs(const QString& moduleId, const QString& level, const QString& text)
{
    const QVariantList rows = moduleLogs(moduleId, level, text);
    if (rows.isEmpty()) return QStringLiteral("Dışa aktarılacak gerçek kayıt yok. Dosya oluşturulmadı.");
    const QString path = QFileDialog::getSaveFileName(nullptr, QStringLiteral("Maskelenmiş logları kaydet"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/modul-log.txt"), QStringLiteral("Metin (*.txt)"));
    if (path.isEmpty()) return QStringLiteral("Kayıt iptal edildi.");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return QStringLiteral("Dosya yazılamadı.");
    for (const QVariant& value : rows) {
        const QVariantMap row = value.toMap();
        const QString line = row.value(QStringLiteral("time")).toString() + QLatin1Char('\t') + row.value(QStringLiteral("level")).toString() + QLatin1Char('\t') + row.value(QStringLiteral("message")).toString() + QLatin1Char('\n');
        file.write(line.toUtf8());
    }
    return path;
}

QString OpsController::exportVisibleLogs()
{
    const QList<LogRecord> rows = engine_->logRows();
    if (rows.isEmpty()) return QStringLiteral("Dışa aktarılacak gerçek kayıt yok. Dosya oluşturulmadı.");
    const QString path = QFileDialog::getSaveFileName(nullptr, QStringLiteral("Maskelenmiş logları kaydet"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + QStringLiteral("/log-analizi.txt"), QStringLiteral("Metin (*.txt)"));
    if (path.isEmpty()) return QStringLiteral("Kayıt iptal edildi.");
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return QStringLiteral("Dosya yazılamadı.");
    for (const LogRecord& row : rows) {
        file.write((row.time + QLatin1Char('\t') + row.level + QLatin1Char('\t') + row.message + QLatin1Char('\n')).toUtf8());
    }
    return path;
}
QString OpsController::acknowledge(const QString& id) { return engine_->acknowledge(id); }
QString OpsController::silenceIncident(const QString& id, int minutes) { return engine_->silenceIncident(id, minutes); }
QString OpsController::hideIncident(const QString& id) { return engine_->hideIncident(id); }
QString OpsController::restoreHiddenIncidents() { return engine_->restoreHiddenIncidents(); }

QString OpsController::logSourceFor(const QString& moduleId) const { return logSourceForModule(moduleId); }

QString OpsController::copyIncident(const QString& id) const
{
    for (const QVariant& value : incidents()) {
        const QVariantMap row = value.toMap();
        if (row.value(QStringLiteral("id")).toString() != id) continue;
        const QString text = QStringList{
            QStringLiteral("Olay ID: ") + row.value(QStringLiteral("id")).toString(),
            QStringLiteral("Uygulama: ") + row.value(QStringLiteral("application")).toString(),
            QStringLiteral("Modül: ") + row.value(QStringLiteral("moduleName")).toString(),
            QStringLiteral("Seviye: ") + row.value(QStringLiteral("level")).toString(),
            QStringLiteral("Durum: ") + row.value(QStringLiteral("flow")).toString(),
            QStringLiteral("Açıklama: ") + row.value(QStringLiteral("message")).toString()
        }.join(QLatin1Char('\n'));
        if (QClipboard* clipboard = QGuiApplication::clipboard()) clipboard->setText(text);
        else return QStringLiteral("Pano kullanılamıyor.");
        return {};
    }
    return QStringLiteral("Olay yok.");
}

QString OpsController::exportIncidents(const QString& format, const QString& ids) const
{
    const QStringList wanted = ids.split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QStringList lines;
    lines << QStringLiteral("Kaynak: yerel olay kayıtları. Gizli alan yok.");
    int count = 0;
    for (const QVariant& value : incidents()) {
        const QVariantMap row = value.toMap();
        if (row.value(QStringLiteral("hidden")).toBool()) continue;
        if (!wanted.contains(row.value(QStringLiteral("id")).toString())) continue;
        lines << row.value(QStringLiteral("firstSeen")).toString() + QStringLiteral(" | ")
            + row.value(QStringLiteral("level")).toString() + QStringLiteral(" | ")
            + row.value(QStringLiteral("flow")).toString() + QStringLiteral(" | ")
            + row.value(QStringLiteral("application")).toString() + QStringLiteral(" | ")
            + row.value(QStringLiteral("moduleName")).toString() + QStringLiteral(" | ")
            + row.value(QStringLiteral("message")).toString();
        ++count;
    }
    if (count == 0) lines << QStringLiteral("Bu görünümde olay yok.");
    const bool pdf = format == QLatin1String("pdf");
    const QString path = QFileDialog::getSaveFileName(nullptr, QStringLiteral("Olayları kaydet"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + (pdf ? QStringLiteral("/olay-gecmisi.pdf") : QStringLiteral("/olay-gecmisi.csv")), pdf ? QStringLiteral("PDF (*.pdf)") : QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) return QStringLiteral("Dışa aktarma iptal edildi.");
    QString error;
    const bool ok = pdf
        ? writePdfDocument(path, QStringLiteral("SİSTEM OLAYLARI VE HATA GEÇMİŞİ"), QString::number(count) + QStringLiteral(" kayıt"), lines, &error)
        : writeCsvDocument(path, QStringLiteral("SİSTEM OLAYLARI VE HATA GEÇMİŞİ"), lines, &error);
    if (!ok) return error.isEmpty() ? QStringLiteral("Dosya yazılamadı.") : error;
    return path;
}
void OpsController::clearLogFile() { engine_->clearLogFile(); }
void OpsController::clearCredential(const QString& id) { engine_->clearCredential(id); }
bool OpsController::credentialConfigured(const QString& id) const { return engine_->hasCredential(id); }
QString OpsController::setCredential(const QString& id, const QString& secret) { return engine_->setCredential(id, secret); }

Integration OpsController::fromMap(const QVariantMap& draft) const
{
    Integration integration;
    integration.id = draft.value(QStringLiteral("id")).toString();
    integration.name = draft.value(QStringLiteral("name")).toString().trimmed();
    integration.description = draft.value(QStringLiteral("description")).toString().trimmed();
    integration.category = draft.value(QStringLiteral("category")).toString().trimmed();
    integration.environment = draft.value(QStringLiteral("environment")).toString();
    integration.baseUrl = draft.value(QStringLiteral("baseUrl")).toString().trimmed();
    integration.healthPath = draft.value(QStringLiteral("healthPath")).toString().trimmed();
    integration.method = draft.value(QStringLiteral("method")).toString();
    integration.authType = draft.value(QStringLiteral("authType")).toString();
    integration.apiKeyHeader = draft.value(QStringLiteral("apiKeyHeader")).toString().trimmed();
    integration.oauthTokenUrl = draft.value(QStringLiteral("oauthTokenUrl")).toString().trimmed();
    integration.oauthClientId = draft.value(QStringLiteral("oauthClientId")).toString().trimmed();
    integration.intervalMs = draft.value(QStringLiteral("intervalMs")).toInt();
    integration.timeoutMs = draft.value(QStringLiteral("timeoutMs")).toInt();
    integration.expectedStatus = draft.value(QStringLiteral("expectedStatus")).toInt();
    integration.latencyWarnMs = draft.value(QStringLiteral("latencyWarnMs")).toInt();
    integration.failThreshold = draft.value(QStringLiteral("failThreshold")).toInt();
    if (integration.failThreshold == 0) integration.failThreshold = 3;
    integration.paused = draft.value(QStringLiteral("paused")).toBool();
    integration.adapter = draft.value(QStringLiteral("adapter")).toString();
    if (integration.adapter.isEmpty()) integration.adapter = QStringLiteral("http");
    integration.allowPrivate = draft.value(QStringLiteral("allowPrivate")).toBool();
    integration.wsUrl = draft.value(QStringLiteral("wsUrl")).toString().trimmed();
    integration.successNote = draft.value(QStringLiteral("successNote")).toString().trimmed();
    integration.headersJson = QString::fromUtf8(QJsonDocument(QJsonArray::fromVariantList(draft.value(QStringLiteral("headers")).toList())).toJson(QJsonDocument::Compact));
    return integration;
}

QString OpsController::saveIntegration(const QVariantMap& draft)
{
    return engine_->saveIntegration(fromMap(draft));
}

QString OpsController::removeIntegration(const QString& id) { return engine_->removeIntegration(id); }

QVariantMap OpsController::integrationIssues(const QVariantMap& draft) const
{
    return integrationFieldErrors(fromMap(draft), engine_->integrations());
}

void OpsController::testDraft(const QVariantMap& draft, const QString& secret)
{
    engine_->testUnsaved(fromMap(draft), secret.isEmpty() ? QString() : secret);
}

QString OpsController::pickLogFile()
{
    const QString path = QFileDialog::getOpenFileName(nullptr, QStringLiteral("Log dosyası seç"), QDir::homePath(), QStringLiteral("Log (*.log *.txt *.json);;Tüm dosyalar (*)"));
    if (path.isEmpty()) return {};
    return engine_->setLogFile(path);
}

QString OpsController::exportReport(const QString& kind, const QString& format)
{
    return exportReportDocument(kind, format, QString(), QStringLiteral("24h"), QString(), QString());
}

QVariantMap OpsController::reportDocument(const QString& kind, const QString& moduleId, const QString& rangeKey, const QString& fromIso, const QString& toIso) const
{
    return engine_->reportDocument(kind, moduleId, rangeKey, fromIso, toIso);
}

QVariantList OpsController::savedReports() const
{
    QVariantList rows;
    for (const QVariantMap& row : engine_->savedReports()) rows.push_back(row);
    return rows;
}

QString OpsController::saveReportRecord(const QString& name, const QString& kind, const QString& period, const QString& system, const QString& format, const QString& summary)
{
    return engine_->saveReportRecord(name, kind, period, system, format, QString(), summary);
}

QString OpsController::exportReportDocument(const QString& kind, const QString& format, const QString& moduleId, const QString& rangeKey, const QString& fromIso, const QString& toIso)
{
    const QVariantMap document = engine_->reportDocument(kind, moduleId, rangeKey, fromIso, toIso);
    if (!document.value(QStringLiteral("ok")).toBool()) return document.value(QStringLiteral("error")).toString();
    const bool pdf = format == QLatin1String("pdf");
    const QString path = QFileDialog::getSaveFileName(nullptr, QStringLiteral("Raporu kaydet"), QStandardPaths::writableLocation(QStandardPaths::DocumentsLocation) + (pdf ? QStringLiteral("/ticketgo-rapor.pdf") : QStringLiteral("/ticketgo-rapor.csv")), pdf ? QStringLiteral("PDF (*.pdf)") : QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty()) return QStringLiteral("Dışa aktarma iptal edildi.");
    QStringList lines = document.value(QStringLiteral("lines")).toStringList();
    lines << document.value(QStringLiteral("incidentLines")).toStringList();
    QString error;
    const bool ok = pdf
        ? writePdfDocument(path, document.value(QStringLiteral("title")).toString(), document.value(QStringLiteral("period")).toString(), lines, &error)
        : writeCsvDocument(path, document.value(QStringLiteral("title")).toString(), lines, &error);
    if (!ok) return error.isEmpty() ? QStringLiteral("Dosya yazılamadı.") : error;
    const QString saved = engine_->saveReportRecord(kind, kind, document.value(QStringLiteral("period")).toString(), document.value(QStringLiteral("system")).toString(), pdf ? QStringLiteral("pdf") : QStringLiteral("csv"), path, document.value(QStringLiteral("summary")).toString());
    return saved.isEmpty() ? path : saved;
}

QString OpsController::deleteSavedReport(const QString& id) { return engine_->deleteSavedReport(id); }

QString OpsController::openSavedReport(const QString& id) const
{
    const QString path = engine_->savedReport(id).value(QStringLiteral("path")).toString();
    if (path.isEmpty() || !QFileInfo::exists(path)) return QStringLiteral("Dosya yok. Önce dışa aktarın.");
    if (!QDesktopServices::openUrl(QUrl::fromLocalFile(path))) return QStringLiteral("Dosya açma isteği kabul edilmedi.");
    return {};
}

QString OpsController::revealSavedReport(const QString& id) const
{
    const QString path = engine_->savedReport(id).value(QStringLiteral("path")).toString();
    if (path.isEmpty() || !QFileInfo::exists(path)) return QStringLiteral("Dosya yok. Finder açılmadı.");
    if (!QProcess::startDetached(QStringLiteral("/usr/bin/open"), {QStringLiteral("-R"), path})) return QStringLiteral("Finder isteği kabul edilmedi.");
    return {};
}

QString OpsController::importOpenApi()
{
    const QString path = QFileDialog::getOpenFileName(nullptr, QStringLiteral("OpenAPI JSON seç"), QDir::homePath(), QStringLiteral("JSON (*.json)"));
    if (path.isEmpty()) return QStringLiteral("İçe aktarma iptal edildi. İnternetten dosya çekilmedi.");
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly) || file.size() > 5LL * 1024 * 1024) return QStringLiteral("Dosya okunamadı veya 5 MB sınırını aşıyor.");
    QString error;
    const auto hints = suggestOpenApi(file.readAll(), &error);
    if (hints.isEmpty()) return error.isEmpty() ? QStringLiteral("Öneri yok.") : error;
    QStringList lines;
    for (const OpenApiHint& hint : hints) lines << hint.method + QLatin1Char(' ') + hint.path;
    return QStringLiteral("Önerilen uçlar (otomatik kaydedilmedi):\n") + lines.join(QLatin1Char('\n'));
}

QVariantMap OpsController::logMap(const LogRecord& row) const
{
    return {
        {QStringLiteral("id"), row.id},
        {QStringLiteral("time"), row.time.isEmpty() ? QStringLiteral("—") : row.time},
        {QStringLiteral("level"), row.level},
        {QStringLiteral("application"), row.application},
        {QStringLiteral("module"), row.module},
        {QStringLiteral("service"), row.service},
        {QStringLiteral("eventId"), row.eventId},
        {QStringLiteral("requestId"), row.requestId},
        {QStringLiteral("httpCode"), row.httpCode},
        {QStringLiteral("errorCode"), row.errorCode},
        {QStringLiteral("message"), row.message},
        {QStringLiteral("action"), row.action.isEmpty() ? QStringLiteral("Veri yok") : row.action},
        {QStringLiteral("date"), row.date.isEmpty() ? QStringLiteral("Veri yok") : row.date},
        {QStringLiteral("clock"), row.clock.isEmpty() ? QStringLiteral("Veri yok") : row.clock},
        {QStringLiteral("detail"), row.detail},
        {QStringLiteral("stack"), row.stack},
        {QStringLiteral("correlationId"), row.correlationId},
        {QStringLiteral("endpoint"), row.endpoint},
        {QStringLiteral("user"), row.user.isEmpty() ? QStringLiteral("Veri yok") : row.user},
        {QStringLiteral("ip"), row.ip.isEmpty() ? QStringLiteral("Veri yok") : row.ip},
        {QStringLiteral("duration"), row.duration.isEmpty() ? QStringLiteral("Veri yok") : row.duration + QStringLiteral(" ms")},
        {QStringLiteral("outcome"), row.outcome.isEmpty() ? QStringLiteral("Veri yok") : row.outcome},
        {QStringLiteral("method"), row.method.isEmpty() ? QStringLiteral("Veri yok") : row.method},
        {QStringLiteral("analysis"), analyzeLogRecord(row, engine_->logRows())}
    };
}

QVariantList OpsController::logSources() const
{
    QVariantList list;
    for (const LogSource& source : engine_->logSources()) {
        list.push_back(QVariantMap{
            {QStringLiteral("id"), source.id},
            {QStringLiteral("name"), source.name},
            {QStringLiteral("access"), source.access},
            {QStringLiteral("accessLabel"), source.access == QLatin1String("connected") ? QStringLiteral("Bağlı") : (source.reason.isEmpty() ? QStringLiteral("Bağlantı yok") : source.reason.section(QLatin1Char('.'), 0, 0))},
            {QStringLiteral("reason"), source.reason},
            {QStringLiteral("lastTime"), source.lastTime.isEmpty() ? QStringLiteral("—") : source.lastTime},
            {QStringLiteral("total"), source.total < 0 ? QStringLiteral("—") : QString::number(source.total)},
            {QStringLiteral("errors"), source.errors < 0 ? QStringLiteral("—") : QString::number(source.errors)},
            {QStringLiteral("infos"), source.infos < 0 ? QStringLiteral("—") : QString::number(source.infos)},
            {QStringLiteral("warnings"), source.warnings < 0 ? QStringLiteral("—") : QString::number(source.warnings)},
            {QStringLiteral("criticals"), source.criticals < 0 ? QStringLiteral("—") : QString::number(source.criticals)},
            {QStringLiteral("levels"), source.levels}
        });
    }
    return list;
}

QVariantList OpsController::logRows() const
{
    QVariantList list;
    for (const LogRecord& row : engine_->logRows()) list.push_back(logMap(row));
    return list;
}

QVariantMap OpsController::logDetail() const { return logMap(engine_->selectedLog()); }
QString OpsController::logAccessNote() const { return engine_->logAccessNote(); }
int OpsController::logHubTotal() const { return engine_->logHubTotal(); }
QString OpsController::logRefreshedAt() const { return engine_->logRefreshedAt(); }
bool OpsController::logHubConnected() const { return engine_->logHubConnected(); }
bool OpsController::logStale() const { return engine_->logStale(); }
QString OpsController::logLastRecordAt() const { return engine_->logLastRecordAt(); }
int OpsController::logPollSeconds() const { return engine_->logPollSeconds(); }
bool OpsController::logLive() const { return engine_->logLive(); }

int OpsController::countLevel(const QString& level) const
{
    int count = 0;
    for (const LogRecord& row : engine_->logRows()) if (row.level == level) ++count;
    return count;
}

int OpsController::logInfoCount() const { return countLevel(QStringLiteral("INFO")); }
int OpsController::logWarnCount() const { return countLevel(QStringLiteral("WARN")); }
int OpsController::logErrorCount() const { return countLevel(QStringLiteral("ERROR")); }
int OpsController::logCriticalCount() const { return countLevel(QStringLiteral("CRITICAL")); }
int OpsController::logTotalCount() const { return engine_->logRows().size(); }

int OpsController::logConnectedSources() const
{
    int count = 0;
    for (const LogSource& source : engine_->logSources()) if (source.access == QLatin1String("connected")) ++count;
    return count;
}

void OpsController::refreshLogs() { engine_->refreshLogs(); }
void OpsController::setLogPage(int page) { engine_->setLogPage(page); }
int OpsController::logPage() const { return engine_->logPage(); }
void OpsController::hideHubLog(const QString& id) { engine_->hideHubLog(id); }
void OpsController::restoreHiddenHubLogs() { engine_->restoreHiddenHubLogs(); }

QString OpsController::copySelectedLog()
{
    const LogRecord row = engine_->selectedLog();
    if (row.id.isEmpty()) return QStringLiteral("Kopyalanacak kayıt yok.");
    const QString text = QStringList{
        QStringLiteral("Log ID: ") + row.id,
        QStringLiteral("Tarih: ") + row.date + QLatin1Char(' ') + row.clock,
        QStringLiteral("Seviye: ") + row.level,
        QStringLiteral("Modül: ") + row.module,
        QStringLiteral("Servis: ") + row.service,
        QStringLiteral("İşlem: ") + row.action,
        QStringLiteral("Mesaj: ") + row.message,
        QStringLiteral("Hata: ") + row.errorCode,
        QStringLiteral("HTTP: ") + row.httpCode,
        QStringLiteral("Endpoint: ") + row.endpoint,
        QStringLiteral("Kullanıcı: ") + row.user,
        QStringLiteral("IP: ") + row.ip,
        QStringLiteral("Süre: ") + row.duration,
        QStringLiteral("Sonuç: ") + row.outcome
    }.join(QLatin1Char('\n'));
    if (QClipboard* clipboard = QGuiApplication::clipboard()) clipboard->setText(text);
    return QStringLiteral("Maskelenmiş ayrıntı kopyalandı.");
}

void OpsController::applyLogFilter(const QString& source, const QString& level, const QString& range, const QString& text, const QString& httpCode, const QString& errorCode, const QString& requestId, const QString& service, const QString& logType, const QString& dateFrom, const QString& dateTo, const QString& user, const QString& ip, const QString& endpoint, const QString& outcome, const QString& action)
{
    LogQuery query;
    query.sourceId = source;
    query.level = level;
    query.range = range;
    query.text = text;
    query.httpCode = httpCode;
    query.errorCode = errorCode;
    query.requestId = requestId;
    query.service = service;
    query.logType = logType;
    query.dateFrom = dateFrom;
    query.dateTo = dateTo;
    query.user = user;
    query.ip = ip;
    query.endpoint = endpoint;
    query.outcome = outcome;
    query.action = action;
    engine_->applyLogQuery(query);
}

QVariantList OpsController::logSections() const
{
    QVariantList list;
    for (const HubLogSection& section : hubLogSections()) {
        list.push_back(QVariantMap{{QStringLiteral("code"), section.code}, {QStringLiteral("label"), section.label}, {QStringLiteral("logType"), section.logType}});
    }
    return list;
}

void OpsController::setLogPollSeconds(int seconds) { engine_->setLogPollSeconds(seconds); }

int OpsController::logNewCount() const { return engine_->logNewCount(); }

QVariantList OpsController::logAlerts() const
{
    const QStringList hidden = engine_->setting(QStringLiteral("dismissed_log_ids")).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    QVariantList list;
    for (const LogRecord& row : engine_->recentLogs()) {
        if (row.level != QLatin1String("ERROR") && row.level != QLatin1String("CRITICAL")) continue;
        if (hidden.contains(row.id)) continue;
        QVariantMap map = logMap(row);
        map.insert(QStringLiteral("note"), QStringLiteral("Log gözlemi. Kapatmak backend hatasını çözmez."));
        list.push_back(map);
        if (list.size() >= 30) break;
    }
    return list;
}

void OpsController::dismissLogAlert(const QString& id)
{
    if (id.trimmed().isEmpty()) return;
    QStringList hidden = engine_->setting(QStringLiteral("dismissed_log_ids")).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (!hidden.contains(id)) hidden.push_back(id.left(80));
    engine_->setSetting(QStringLiteral("dismissed_log_ids"), hidden.join(QLatin1Char('\n')));
    engine_->audit(QStringLiteral("log-bildirimi-kapatildi"), id.left(80), QStringLiteral("Backend kaydı silinmedi ve çözülmedi."));
}

QString OpsController::removeCustomModule(const QString& moduleId)
{
    if (!moduleId.startsWith(QLatin1String("custom-"))) return QStringLiteral("Zorunlu modül silinmez. İzlemeden çıkarılabilir.");
    QStringList ids;
    for (const Integration& integration : engine_->integrations()) ids.push_back(integration.id);
    const QString integrationId = integrationIdForModule(moduleId, ids);
    if (integrationId.isEmpty()) return QStringLiteral("Silinecek yerel entegrasyon bulunamadı. Kayıt duruyor.");
    return engine_->removeIntegration(integrationId);
}

void OpsController::setLogLive(bool on) { engine_->setLogLive(on); }
void OpsController::selectLog(const QString& id) { engine_->selectLog(id); }

QString OpsController::logSessionText() const { return engine_->logSessionText(); }
QString OpsController::logSessionPhase() const { return engine_->logSessionPhase(); }
bool OpsController::logSessionBusy() const { return engine_->logSessionBusy(); }
QString OpsController::logSessionMessage() const { return engine_->logSessionMessage(); }
QString OpsController::logSessionAccount() const { return engine_->logSessionAccount(); }
QString OpsController::logSessionVerifiedAt() const { return engine_->logSessionVerifiedAt(); }
QString OpsController::logMfaHint() const { return engine_->logMfaHint(); }
QString OpsController::openLogSession(const QString& username, const QString& password) { return engine_->openLogSession(username, password); }
QString OpsController::submitLogMfa(const QString& code) { return engine_->submitLogMfa(code); }
QString OpsController::resendLogMfa() { return engine_->resendLogMfa(); }
QString OpsController::refreshLogSession() { return engine_->refreshLogSession(); }
QString OpsController::closeLogSession() { return engine_->closeLogSession(); }
QString OpsController::validateLogSession() { return engine_->validateLogSession(); }
QString OpsController::setLogToken(const QString& token)
{
    QString error;
    return engine_->setLogToken(token, &error) ? QStringLiteral("Oturum aktif. Jeton ekranda tutulmaz.") : error;
}

void OpsController::clearLogToken() { engine_->clearLogToken(); }
bool OpsController::logTokenConfigured() const { return engine_->logTokenConfigured(); }

} // namespace tg
