#include "Engine.h"

#include "AppPrefs.h"
#include "HealthEval.h"
#include "ModuleLinks.h"
#include "ReportStats.h"
#include "ReportWriter.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QUuid>
#include <algorithm>
#include <QJsonArray>
#include <QSet>
#include <QJsonDocument>
#include <QStandardPaths>
#include <QUrl>
#include <QUrlQuery>
#include <QWebSocket>
#include <QtGlobal>

namespace tg {
namespace {

QString validate(const Integration& integration, const QList<Integration>& existing, QString* normalizedPath)
{
    const QVariantMap errors = integrationFieldErrors(integration, existing);
    if (errors.contains(QStringLiteral("summary"))) return errors.value(QStringLiteral("summary")).toString();
    if (normalizedPath) *normalizedPath = errors.value(QStringLiteral("healthPathValue")).toString();
    return {};
}

} // namespace

Engine::Engine(QObject* parent)
    : QObject(parent)
    , credentials_(std::make_unique<KeychainCredentialStore>())
    , client_(this)
{
    timer_.setInterval(1000);
    connect(&timer_, &QTimer::timeout, this, &Engine::tick);
}

bool Engine::start(QString* error)
{
    dataDir_ = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    QDir().mkpath(dataDir_);
    if (!store_.open(dataDir_ + QStringLiteral("/monitor.sqlite"), error)) return false;
    integrations_ = store_.integrations();
    if (store_.setting(QStringLiteral("probe_interval_sec")).isEmpty()) {
        for (Integration& item : integrations_) {
            if (item.paused) continue;
            item.intervalMs = 15000;
            QString ignored;
            store_.saveIntegration(item, &ignored);
        }
        store_.setSetting(QStringLiteral("probe_interval_sec"), QStringLiteral("15"));
        integrations_ = store_.integrations();
    }
    incidents_.restore(store_.incidents());
    seedModules();
    ensureCustomModules();
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    for (const Integration& integration : integrations_) {
        nextDue_.insert(integration.id, integration.paused ? now + 86400000 : now + 400);
    }
    timer_.start();
    syncSockets();
    if (!adoptPreviousLogSession()) forgetStoredLogSecrets();
    emit updated();
    return true;
}

void Engine::seedModules()
{
    HttpResult empty;
    modules_ = evaluateTicketGo(empty, empty, empty, empty, empty, false);
    modules_ += evaluateGoChat(empty, empty);
    modules_ += evaluateFerryApp(empty);
    modules_ += evaluateReachability(QStringLiteral("tg-web"), QStringLiteral("Web uygulaması"), empty);
    modules_ += evaluateReachability(QStringLiteral("tg-admin"), QStringLiteral("Admin panel"), empty);
}

QList<ModuleSnapshot> Engine::modules() const { return applyParentGuard(modules_); }
QList<Incident> Engine::incidents() const { return incidents_.all(); }
QList<Integration> Engine::integrations() const { return integrations_; }
HostSample Engine::host() const { return host_; }
int Engine::revision() const { return revision_; }
QString Engine::dataDirectory() const { return dataDir_; }
QString Engine::keychainNote() const { return credentials_->backendName() + QStringLiteral(". Gizler kaynak dosyasına ve SQLite'a yazılmaz."); }
QString Engine::environmentText() const { return environment_; }
QString Engine::regionText() const { return region_; }
QString Engine::uptimeText() const { return uptime_; }
QString Engine::queueText() const
{
    for (const ModuleSnapshot& module : modules_) {
        if (module.id == QLatin1String("tg-queues") && module.status != ModuleStatus::NotConfigured && module.status != ModuleStatus::Checking) return module.evidence.left(140);
    }
    return QStringLiteral("Bağlantı yapılandırılmadı");
}

Overview Engine::overview() const
{
    Overview overview = summarize(applyParentGuard(modules_), incidents_.openCriticalCount() > 0);
    overview.openCritical = incidents_.openCriticalCount();
    overview.hataVar = overview.openCritical > 0;
    overview.environmentText = environment_;
    overview.regionText = region_;
    overview.uptimeText = uptime_;
    overview.queueText = queueText();
    return overview;
}

QString Engine::logBanner() const
{
    const QString path = store_.setting(QStringLiteral("log_file"));
    if (path.isEmpty()) return QStringLiteral("Log erişimi yapılandırılmadı. Uzak komut çalıştırılmaz. Yalnızca seçilen yerel dosya okunur.");
    if (!QFileInfo(path).isFile()) return QStringLiteral("Seçilen log dosyası bulunamadı.");
    return QStringLiteral("Yerel dosya okunuyor. Satırlar maskelenir.");
}

QString Engine::logBody(const QString& level) const
{
    const QString path = store_.setting(QStringLiteral("log_file"));
    if (path.isEmpty() || !QFileInfo(path).isFile()) return logBanner();
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return QStringLiteral("Dosya okunamadı.");
    const qint64 size = file.size();
    if (size > 65536) file.seek(size - 65536);
    const QString text = redactText(QString::fromUtf8(file.readAll()));
    QStringList lines;
    for (const QString& line : text.split(QLatin1Char('\n'))) {
        const QString upper = line.toUpper();
        if (!level.isEmpty() && level != QLatin1String("Tümü") && !upper.contains(level.toUpper())) continue;
        if (!line.trimmed().isEmpty()) lines.push_back(line.left(400));
    }
    if (lines.size() > 200) lines = lines.mid(lines.size() - 200);
    if (lines.isEmpty()) return QStringLiteral("Seçilen dosyada bu filtreye uyan satır yok.");
    return lines.join(QLatin1Char('\n'));
}

bool Engine::soundEnabled() const { return store_.setting(QStringLiteral("sound"), QStringLiteral("1")) != QLatin1String("0"); }
void Engine::setSoundEnabled(bool on) { store_.setSetting(QStringLiteral("sound"), on ? QStringLiteral("1") : QStringLiteral("0")); emit updated(); }

QString Engine::overviewFilter() const
{
    const QString value = store_.setting(QStringLiteral("overview_filter"), QStringLiteral("problems"));
    if (value == QLatin1String("all") || value == QLatin1String("healthy") || value == QLatin1String("critical")
        || value == QLatin1String("warning") || value == QLatin1String("unmonitored") || value == QLatin1String("problems")) {
        return value;
    }
    return QStringLiteral("problems");
}

void Engine::setOverviewFilter(const QString& filter)
{
    if (filter != QLatin1String("all") && filter != QLatin1String("healthy") && filter != QLatin1String("critical")
        && filter != QLatin1String("warning") && filter != QLatin1String("unmonitored") && filter != QLatin1String("problems")) {
        return;
    }
    store_.setSetting(QStringLiteral("overview_filter"), filter);
    emit updated();
}

void Engine::setPaused(const QString& id, bool paused)
{
    store_.setPaused(id, paused);
    integrations_ = store_.integrations();
    if (paused) nextDue_[id] = QDateTime::currentMSecsSinceEpoch() + 86400000;
    else nextDue_[id] = QDateTime::currentMSecsSinceEpoch() + 300;
    applyPauseState(id, paused);
    syncSockets();
    emit updated();
}

QString Engine::saveIntegration(Integration integration)
{
    if (integration.id.isEmpty()) integration.id = QUuid::createUuid().toString(QUuid::Id128);
    Integration* current = findIntegration(integration.id);
    if (current && current->locked) {
        current->intervalMs = qMax(15000, integration.intervalMs);
        current->timeoutMs = integration.timeoutMs;
        current->paused = integration.paused;
        current->latencyWarnMs = integration.latencyWarnMs;
        current->failThreshold = integration.failThreshold;
        integration = *current;
    } else {
        integration.locked = false;
        integration.kind = QStringLiteral("custom");
        if (integration.adapter != QLatin1String("contract-1.0")) integration.adapter = QStringLiteral("http");
        if (current) {
            if (integration.wsUrl.isEmpty()) integration.wsUrl = current->wsUrl;
            if (integration.headersJson.isEmpty() || integration.headersJson == QLatin1String("[]")) integration.headersJson = current->headersJson;
            if (integration.successNote.isEmpty()) integration.successNote = current->successNote;
            if (integration.logSource.isEmpty()) integration.logSource = current->logSource;
        }
        QString path;
        const QString problem = validate(integration, integrations_, &path);
        if (!problem.isEmpty()) return problem;
        integration.healthPath = path;
    }
    QString error;
    if (!store_.saveIntegration(integration, &error)) return error.isEmpty() ? QStringLiteral("Kayıt başarısız.") : error;
    integrations_ = store_.integrations();
    nextDue_[integration.id] = QDateTime::currentMSecsSinceEpoch() + (integration.paused ? 86400000 : 500);
    ensureCustomModules();
    ++revision_;
    emit updated();
    return QStringLiteral("id:") + integration.id;
}

QString Engine::removeIntegration(const QString& id)
{
    Integration* current = findIntegration(id);
    if (current && current->locked) {
        store_.appendAudit(QStringLiteral("silme-reddedildi"), id, QStringLiteral("Yerleşik bağlayıcı silinmedi."));
        return QStringLiteral("Yerleşik bağlayıcı silinmez. İzleme durdurulabilir.");
    }
    QString error;
    if (!store_.removeIntegration(id, &error)) {
        store_.appendAudit(QStringLiteral("silme-reddedildi"), id, error.isEmpty() ? QStringLiteral("Kayıt duruyor.") : error);
        return error.isEmpty() ? QStringLiteral("Kayıt silinemedi. Kayıt duruyor.") : error;
    }
    credentials_->removeSecret(id, nullptr);
    nextDue_.remove(id);
    transportFails_.remove(id);
    store_.appendAudit(QStringLiteral("yerel-entegrasyon-silindi"), id, QStringLiteral("Uzak servis kapatılmadı."));
    integrations_ = store_.integrations();
    modules_.erase(std::remove_if(modules_.begin(), modules_.end(), [&](const ModuleSnapshot& module) { return module.id.startsWith(QStringLiteral("custom-") + id); }), modules_.end());
    ++revision_;
    emit updated();
    return {};
}

int Engine::pathIntervalMs() const
{
    int interval = 60000;
    for (const Integration& integration : integrations_) {
        if (integration.id == QLatin1String("ticket-go-backend")) interval = integration.intervalMs;
    }
    return qBound(15000, interval, 300000);
}

void Engine::storePath(const PathResult& result)
{
    const QString id = result.kind == QLatin1String("icmp")
        ? QStringLiteral("path:icmp:api.ticket-go.net")
        : QStringLiteral("path:tcp:api.ticket-go.net:443");
    store_.addSample(id, ModuleStatus::Unsupported, result.latencyMs, 0, result.evidence);
    ++revision_;
    emit updated();
}

QString Engine::measurePerformance(const QString& kind)
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (now - lastManualMs_.value(kind) < 15000) return QStringLiteral("Ölçüm çok sık. Son ölçüm kullanılıyor.");
    lastManualMs_.insert(kind, now);
    if (kind == QLatin1String("https-backend")) {
        if (chains_.contains(QStringLiteral("ticket-go-backend"))) return QStringLiteral("Ölçüm sürüyor.");
        return probeNow(QStringLiteral("ticket-go-backend"));
    }
    if (kind == QLatin1String("https-gochat")) {
        if (chains_.contains(QStringLiteral("go-chat-api"))) return QStringLiteral("Ölçüm sürüyor.");
        return probeNow(QStringLiteral("go-chat-api"));
    }
    if (kind == QLatin1String("path")) {
        if (pathProbe_.busy()) return QStringLiteral("Ölçüm sürüyor.");
        lastPathMs_ = now;
        pathProbe_.start([this](PathResult result) { storePath(result); });
        return QStringLiteral("Ping ve TCP ölçümü başladı. Ayrı HTTPS isteği açılmadı.");
    }
    return QStringLiteral("Bu kartın ölçüm kaynağı yapılandırılmamış.");
}

QVariantMap Engine::performanceMetric(const QString& id, int minutes) const
{
    const QDateTime since = QDateTime::currentDateTime().addSecs(-qMax(1, minutes) * 60);
    const QVariantMap latest = store_.latestSample(id);
    const QList<ProbeSample> rows = store_.probeSamples(since, QDateTime::currentDateTime().addSecs(1), id);
    int measured = 0;
    int failed = 0;
    for (const ProbeSample& row : rows) {
        if (!countsForAvailability(row.status)) continue;
        ++measured;
        if (row.status == QLatin1String("critical") || row.status == QLatin1String("connection_error") || row.status == QLatin1String("auth_rejected")) ++failed;
    }
    QVariantMap map = latest;
    map.insert(QStringLiteral("measured"), measured);
    map.insert(QStringLiteral("failed"), failed);
    if (measured > 0) {
        map.insert(QStringLiteral("successRate"), QString::number(100.0 * double(measured - failed) / double(measured), 'f', 0) + QStringLiteral("%"));
        map.insert(QStringLiteral("errorRate"), QString::number(100.0 * double(failed) / double(measured), 'f', 0) + QStringLiteral("%"));
    }
    return map;
}

QString Engine::probeNow(const QString& id)
{
    Integration* integration = findIntegration(id);
    if (!integration) return QStringLiteral("Bağlayıcı bulunamadı.");
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    if (chains_.contains(id)) return QStringLiteral("Ölçüm sürüyor.");
    if (now - lastManualMs_.value(id) < 15000) return QStringLiteral("Test çok sık. Son sonuç duruyor.");
    lastManualMs_.insert(id, now);
    begin(*integration, true, false, {});
    if (integration->paused) return QStringLiteral("İzleme kapalı. Tek seferlik test gönderildi. API kapatılmadı.");
    return QStringLiteral("Test gönderildi.");
}

void Engine::testUnsaved(const Integration& integration, const QString& secret)
{
    Integration copy = integration;
    if (copy.id.isEmpty()) copy.id = QStringLiteral("draft");
    QString path;
    const QString problem = validate(copy, integrations_, &path);
    if (!problem.isEmpty()) {
        emit testFinished(problem);
        return;
    }
    if (!path.isEmpty()) copy.healthPath = path;
    begin(copy, true, true, secret.isNull() ? QString() : secret);
}

QString Engine::setCredential(const QString& id, const QString& secret)
{
    QString error;
    if (!credentials_->setSecret(id, secret, &error)) return error;
    return {};
}

bool Engine::hasCredential(const QString& id) const { return credentials_->contains(id); }

void Engine::clearCredential(const QString& id)
{
    credentials_->removeSecret(id, nullptr);
    emit updated();
}

QString Engine::acknowledge(const QString& id)
{
    bool found = false;
    bool open = false;
    for (const Incident& incident : incidents_.all()) {
        if (incident.id != id) continue;
        found = true;
        open = incident.state == QLatin1String("open");
    }
    if (!found) return QStringLiteral("Olay yok.");
    if (!open) return QStringLiteral("Yalnızca ACTIVE olay onaylanır. Çözüldü sayılmadı.");
    incidents_.acknowledge(id);
    for (const Incident& incident : incidents_.all()) store_.saveIncident(incident);
    QStringList lines = store_.setting(QStringLiteral("incident_ack_at")).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    lines.erase(std::remove_if(lines.begin(), lines.end(), [&](const QString& line) { return line.startsWith(id + QLatin1Char('|')); }), lines.end());
    lines.push_back(id + QLatin1Char('|') + QDateTime::currentDateTime().toString(Qt::ISODate));
    store_.setSetting(QStringLiteral("incident_ack_at"), lines.join(QLatin1Char('\n')));
    store_.appendAudit(QStringLiteral("olay-onaylandi"), id.left(80), QStringLiteral("Cozulmedi."));
    ++revision_;
    emit updated();
    return {};
}

QString Engine::silenceIncident(const QString& id, int minutes)
{
    if (minutes < 5 || minutes > 240) return QStringLiteral("Susturma süresi 5 ile 240 dakika arasında olmalı. Olay değişmedi.");
    bool found = false;
    bool resolved = false;
    for (const Incident& incident : incidents_.all()) {
        if (incident.id != id) continue;
        found = true;
        resolved = incident.state == QLatin1String("resolved");
    }
    if (!found) return QStringLiteral("Olay yok. Susturma yazılmadı.");
    if (resolved) return QStringLiteral("Çözülmüş olay susturulmadı.");
    const QString until = QDateTime::currentDateTime().addSecs(minutes * 60).toString(Qt::ISODate);
    QStringList lines = store_.setting(QStringLiteral("incident_silence_until")).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    lines.erase(std::remove_if(lines.begin(), lines.end(), [&](const QString& line) { return line.startsWith(id + QLatin1Char('|')); }), lines.end());
    lines.push_back(id + QLatin1Char('|') + until);
    store_.setSetting(QStringLiteral("incident_silence_until"), lines.join(QLatin1Char('\n')));
    store_.appendAudit(QStringLiteral("olay-susturuldu"), id.left(80), QStringLiteral("Cozulmedi. Bildirim tekrari durdu."));
    ++revision_;
    emit updated();
    return {};
}

bool Engine::incidentSilenced(const QString& id) const
{
    return incidentSilenceActive(store_.setting(QStringLiteral("incident_silence_until")), id, QDateTime::currentDateTime());
}

QString Engine::hideIncident(const QString& id)
{
    bool found = false;
    for (const Incident& incident : incidents_.all()) {
        if (incident.id == id) found = true;
    }
    if (!found) return QStringLiteral("Olay yok. Görünüm değişmedi.");
    QStringList hidden = store_.setting(QStringLiteral("hidden_incident_ids")).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (!hidden.contains(id)) hidden.push_back(id);
    store_.setSetting(QStringLiteral("hidden_incident_ids"), hidden.join(QLatin1Char('\n')));
    store_.appendAudit(QStringLiteral("olay-gorunumu-gizlendi"), id.left(80), QStringLiteral("Kaynak log ve olay kaydi silinmedi."));
    ++revision_;
    emit updated();
    return {};
}

QString Engine::restoreHiddenIncidents()
{
    store_.setSetting(QStringLiteral("hidden_incident_ids"), QString());
    store_.appendAudit(QStringLiteral("olay-gorunumu-geri-yuklendi"), QStringLiteral("hidden_incident_ids"), QStringLiteral("Yerel gizleme kalkti. Kaynak degismedi."));
    ++revision_;
    emit updated();
    return {};
}

QStringList Engine::hiddenIncidentIds() const
{
    return store_.setting(QStringLiteral("hidden_incident_ids")).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
}

QString Engine::setting(const QString& key) const { return store_.setting(key); }
QString Engine::setting(const QString& key, const QString& fallback) const
{
    const QString value = store_.setting(key);
    return value.isEmpty() ? fallback : value;
}
void Engine::setSetting(const QString& key, const QString& value) { store_.setSetting(key, value); emit updated(); }
void Engine::audit(const QString& action, const QString& target, const QString& result) { store_.appendAudit(action, target, result); }

QString Engine::setLogFile(const QString& path)
{
    QFileInfo info(path);
    if (!info.isFile() || info.size() > 50LL * 1024 * 1024) return QStringLiteral("Okunabilir bir dosya seçin (en fazla 50 MB).");
    store_.setSetting(QStringLiteral("log_file"), info.absoluteFilePath());
    emit updated();
    return {};
}

void Engine::clearLogFile()
{
    store_.setSetting(QStringLiteral("log_file"), QString());
    localLogs_.clear();
    store_.appendAudit(QStringLiteral("yerel-log-bagi-kaldirildi"), QStringLiteral("log_file"), QStringLiteral("Dosya ve backend logu silinmedi."));
    emit updated();
}

QList<double> Engine::latencySeries(const QString& moduleId, int minutes) const
{
    const auto series = store_.latencySeries(moduleId, QDateTime::currentDateTime().addSecs(-minutes * 60));
    QList<double> values;
    for (const auto& point : series) values.push_back(point.second);
    if (values.size() > 120) values = values.mid(values.size() - 120);
    return values;
}

QList<QPair<qint64, double>> Engine::latencyPoints(const QString& moduleId, int minutes) const
{
    const auto series = store_.latencySeries(moduleId, QDateTime::currentDateTime().addSecs(-qMax(1, minutes) * 60));
    return thinSeries(series, 48);
}

qint64 Engine::lastLatencyMs(const QString& moduleId, int minutes) const
{
    const auto series = store_.latencySeries(moduleId, QDateTime::currentDateTime().addSecs(-minutes * 60));
    if (series.isEmpty()) return 0;
    return series.last().first;
}

QString Engine::exportReport(const QString& path, const QString& kind, const QString& format)
{
    QString error;
    const bool ok = format == QLatin1String("pdf")
        ? writePdfReport(path, kind, overview(), modules_, incidents_.all(), &error)
        : writeCsvReport(path, kind, overview(), modules_, incidents_.all(), &error);
    return ok ? QString() : error;
}

QString Engine::savePreferences(int intervalSec, int timeoutSec, int retryCap, int latencyWarnMs, int retentionDays, bool monitorEnabled, bool rememberPage, bool clock24h, bool sound, bool desktopNotify, bool quiet, bool animations, bool lowPower, bool startFullscreen, const QString& quality, int* customUpdated)
{
    const PrefCheck check = checkMonitorPrefs(intervalSec, timeoutSec, retryCap, latencyWarnMs, retentionDays, quality);
    if (!check.ok) return check.error;
    int customCount = 0;
    for (Integration& item : integrations_) {
        if (item.locked) {
            item.intervalMs = intervalSec * 1000;
            item.timeoutMs = timeoutSec * 1000;
        } else {
            item.latencyWarnMs = latencyWarnMs;
            ++customCount;
        }
        QString error;
        if (!store_.saveIntegration(item, &error)) {
            return error.isEmpty() ? QStringLiteral("Ayar yazılamadı. Değişiklik tamamlanmadı.") : error;
        }
    }
    integrations_ = store_.integrations();
    store_.setSetting(QStringLiteral("probe_interval_sec"), QString::number(intervalSec));
    store_.setSetting(QStringLiteral("probe_timeout_sec"), QString::number(timeoutSec));
    store_.setSetting(QStringLiteral("retry_cap"), QString::number(retryCap));
    store_.setSetting(QStringLiteral("latency_warn_ms"), QString::number(latencyWarnMs));
    store_.setSetting(QStringLiteral("retention_days"), QString::number(retentionDays));
    store_.setSetting(QStringLiteral("monitor_enabled"), monitorEnabled ? QStringLiteral("1") : QStringLiteral("0"));
    store_.setSetting(QStringLiteral("remember_page"), rememberPage ? QStringLiteral("1") : QStringLiteral("0"));
    store_.setSetting(QStringLiteral("clock_24h"), clock24h ? QStringLiteral("1") : QStringLiteral("0"));
    store_.setSetting(QStringLiteral("sound"), sound ? QStringLiteral("1") : QStringLiteral("0"));
    store_.setSetting(QStringLiteral("desktop_notify"), desktopNotify ? QStringLiteral("1") : QStringLiteral("0"));
    store_.setSetting(QStringLiteral("quiet_mode"), quiet ? QStringLiteral("1") : QStringLiteral("0"));
    store_.setSetting(QStringLiteral("animations"), animations ? QStringLiteral("1") : QStringLiteral("0"));
    store_.setSetting(QStringLiteral("low_power"), lowPower ? QStringLiteral("1") : QStringLiteral("0"));
    store_.setSetting(QStringLiteral("start_fullscreen"), startFullscreen ? QStringLiteral("1") : QStringLiteral("0"));
    store_.setSetting(QStringLiteral("graphics_quality"), quality);
    if (customUpdated) *customUpdated = customCount;
    store_.appendAudit(QStringLiteral("ayar-kaydedildi"), QStringLiteral("preferences"), QStringLiteral("Kimlik ve olay kayıtları silinmedi."));
    ++revision_;
    emit updated();
    return {};
}

QString Engine::clearOldSamples()
{
    const int days = qBound(1, store_.setting(QStringLiteral("retention_days"), QStringLiteral("1")).toInt(), 30);
    store_.pruneSamples(QDateTime::currentDateTime().addDays(-days));
    store_.appendAudit(QStringLiteral("ornek-onbellek-temizlendi"), QString::number(days), QStringLiteral("Olay, audit ve entegrasyon silinmedi."));
    return QStringLiteral("Saklama süresinden eski ölçüm örnekleri silindi. Olay geçmişi, audit ve entegrasyon kayıtları duruyor.");
}

qint64 Engine::databaseBytes() const
{
    return QFileInfo(store_.path()).size();
}

QVariantMap Engine::reportDocument(const QString& kind, const QString& moduleId, const QString& rangeKey, const QString& fromIso, const QString& toIso) const
{
    QVariantMap map;
    QDateTime from;
    QDateTime to;
    QString error;
    if (!resolveReportRange(rangeKey, fromIso, toIso, QDateTime::currentDateTime(), &from, &to, &error)) {
        map.insert(QStringLiteral("ok"), false);
        map.insert(QStringLiteral("error"), error);
        return map;
    }
    QSet<QString> allowed;
    QStringList names;
    int healthy = 0;
    int warning = 0;
    int critical = 0;
    int unmonitored = 0;
    for (const ModuleSnapshot& module : modules_) {
        if (!moduleId.isEmpty() && module.id != moduleId) continue;
        if (kind == QLatin1String("ticketgo") && module.group != QLatin1String("ticket-go")) continue;
        if (kind == QLatin1String("gochat") && module.group != QLatin1String("gochat")) continue;
        if (kind == QLatin1String("integrations") && module.group != QLatin1String("custom")) continue;
        allowed.insert(module.id);
        names << module.name;
        const QString key = statusKey(module.status);
        if (key == QLatin1String("healthy")) ++healthy;
        else if (key == QLatin1String("warning") || key == QLatin1String("not_released")) ++warning;
        else if (key == QLatin1String("critical") || key == QLatin1String("connection_error") || key == QLatin1String("auth_rejected")) ++critical;
        else ++unmonitored;
    }
    QList<ProbeSample> probes;
    for (const ProbeSample& row : store_.probeSamples(from, to, moduleId)) {
        if (!moduleId.isEmpty() || allowed.isEmpty() || allowed.contains(row.moduleId)) probes.push_back(row);
    }
    const PeriodStats stats = summarizeProbes(probes);
    int incidentsInRange = 0;
    int openIncidents = 0;
    QStringList incidentLines;
    for (const Incident& incident : incidents_.all()) {
        if (!moduleId.isEmpty() && incident.moduleId != moduleId) continue;
        if (!allowed.isEmpty() && !allowed.contains(incident.moduleId) && kind != QLatin1String("overview") && kind != QLatin1String("incidents") && kind != QLatin1String("errors") && kind != QLatin1String("availability") && kind != QLatin1String("performance")) continue;
        if (incident.lastSeen < from || incident.firstSeen > to) continue;
        ++incidentsInRange;
        if (incident.state != QLatin1String("resolved")) ++openIncidents;
        if (incidentLines.size() < 12) {
            incidentLines << incident.moduleName + QStringLiteral(" · ") + incident.severity + QStringLiteral(" · ") + incident.state
                + QStringLiteral(" · ") + incident.firstSeen.toString(QStringLiteral("dd.MM.yyyy HH:mm"))
                + QStringLiteral(" · Olası neden: ") + redactText(incident.causeText).left(160);
        }
    }
    const QString period = from.toString(QStringLiteral("dd.MM.yyyy HH:mm")) + QStringLiteral(" — ") + to.toString(QStringLiteral("dd.MM.yyyy HH:mm"));
    QStringList lines;
    lines << QStringLiteral("Dönem: ") + period;
    lines << QStringLiteral("Kaynak: yerel ölçüm örnekleri ve olay kayıtları. Uzak veritabanına yazılmadı.");
    lines << QStringLiteral("İzlenen modül: ") + QString::number(names.size());
    lines << QStringLiteral("Anlık sağlıklı: ") + QString::number(healthy) + QStringLiteral(" · sorunlu: ") + QString::number(critical) + QStringLiteral(" · uyarı: ") + QString::number(warning) + QStringLiteral(" · izlenemeyen: ") + QString::number(unmonitored);
    lines << QStringLiteral("Dönem erişilebilirliği: ") + availabilityText(stats) + QStringLiteral(". Hesap: (sağlıklı + uyarı) / ölçülen sağlık kontrolü. İzlenemeyen aralık kesinti sayılmaz.");
    lines << QStringLiteral("Ölçülen kontrol: ") + QString::number(stats.measured) + QStringLiteral(" · kapsam dışı örnek: ") + QString::number(stats.excluded);
    lines << QStringLiteral("Ortalama yanıt: ") + latencyText(stats.avgLatencyMs) + QStringLiteral(" · P95: ") + (stats.p95LatencyMs ? latencyText(stats.p95LatencyMs) : QStringLiteral("yeterli ham veri yok"));
    lines << QStringLiteral("Dönem olayı: ") + QString::number(incidentsInRange) + QStringLiteral(" · kapanmamış: ") + QString::number(openIncidents);
    lines << QStringLiteral("Log satırı bu rapora alınmaz. Mesaj içeriği gösterilmez.");
    if (names.isEmpty()) lines << QStringLiteral("Bu kapsamdaki modül kaydı yok.");
    if (stats.measured == 0) lines << QStringLiteral("Bu dönemde ölçülmüş sağlık kontrolü yok. Erişilebilirlik üretilmedi.");
    map.insert(QStringLiteral("ok"), true);
    map.insert(QStringLiteral("title"), QStringLiteral("SİSTEM RAPORLARI VE ANALİZ MERKEZİ"));
    map.insert(QStringLiteral("period"), period);
    map.insert(QStringLiteral("system"), moduleId.isEmpty() ? kind : moduleId);
    map.insert(QStringLiteral("summary"), lines.join(QLatin1Char('\n')));
    map.insert(QStringLiteral("lines"), lines);
    map.insert(QStringLiteral("incidentLines"), incidentLines);
    map.insert(QStringLiteral("measured"), stats.measured);
    map.insert(QStringLiteral("healthyNow"), healthy);
    map.insert(QStringLiteral("warningNow"), warning);
    map.insert(QStringLiteral("criticalNow"), critical);
    map.insert(QStringLiteral("unmonitoredNow"), unmonitored);
    map.insert(QStringLiteral("availability"), availabilityText(stats));
    map.insert(QStringLiteral("avgLatency"), latencyText(stats.avgLatencyMs));
    map.insert(QStringLiteral("p95"), stats.p95LatencyMs ? latencyText(stats.p95LatencyMs) : QStringLiteral("yeterli ham veri yok"));
    map.insert(QStringLiteral("errors"), stats.failed);
    map.insert(QStringLiteral("incidents"), incidentsInRange);
    map.insert(QStringLiteral("excluded"), stats.excluded);
    QVariantList chart;
    int taken = 0;
    for (const ProbeSample& row : probes) {
        if (row.latencyMs < 0 || !countsForAvailability(row.status)) continue;
        chart << row.latencyMs;
        if (++taken >= 120) break;
    }
    map.insert(QStringLiteral("latency"), chart);
    return map;
}

QString Engine::saveReportRecord(const QString& name, const QString& kind, const QString& period, const QString& system, const QString& format, const QString& path, const QString& summary)
{
    const QString id = QUuid::createUuid().toString(QUuid::WithoutBraces);
    QString error;
    if (!store_.insertSavedReport(id, name.isEmpty() ? kind : name, kind, period, system, format, path.isEmpty() ? QStringLiteral("kaydedildi") : QStringLiteral("dosyaya yazıldı"), path, redactText(summary), &error)) {
        return error.isEmpty() ? QStringLiteral("Rapor kaydı yazılamadı.") : error;
    }
    store_.appendAudit(QStringLiteral("yerel-rapor-kaydedildi"), id, QStringLiteral("Monitoring kaynağı silinmedi."));
    ++revision_;
    emit updated();
    return {};
}

QList<QVariantMap> Engine::savedReports() const { return store_.savedReports(); }

QVariantMap Engine::savedReport(const QString& id) const { return store_.savedReport(id); }

QString Engine::deleteSavedReport(const QString& id)
{
    const QVariantMap row = store_.savedReport(id);
    if (row.value(QStringLiteral("id")).toString().isEmpty()) return QStringLiteral("Rapor kaydı yok. Silinmedi.");
    const QString path = row.value(QStringLiteral("path")).toString();
    if (!path.isEmpty() && QFileInfo::exists(path)) {
        const QFileInfo info(path);
        const QString suffix = info.suffix().toLower();
        if (!info.isFile() || (suffix != QLatin1String("pdf") && suffix != QLatin1String("csv"))) {
            return QStringLiteral("Dosya yolu rapor dosyası değil. Kayıt duruyor.");
        }
        if (!QFile::remove(path)) return QStringLiteral("Dosya silinemedi. Kayıt duruyor.");
    }
    QString error;
    if (!store_.removeSavedReport(id, &error)) return error.isEmpty() ? QStringLiteral("Kayıt silinemedi.") : error;
    store_.appendAudit(QStringLiteral("yerel-rapor-silindi"), id, QStringLiteral("Ölçüm, olay ve audit duruyor."));
    ++revision_;
    emit updated();
    return {};
}

void Engine::tick()
{
    const qint64 now = QDateTime::currentMSecsSinceEpoch();
    const int hostEvery = store_.setting(QStringLiteral("low_power"), QStringLiteral("0")) == QLatin1String("1") ? 30000 : 5000;
    if (now - lastHostMs_ > hostEvery) {
        host_ = hostMetrics_.sample();
        lastHostMs_ = now;
        ++revision_;
        emit updated();
    }
    if (logLive_) {
        const int wait = logBackoffMs_ > 0 ? logBackoffMs_ : logPollIntervalMs(store_.setting(QStringLiteral("log_poll_sec"), QStringLiteral("5")));
        if (now - lastLogPollMs_ >= wait) {
            lastLogPollMs_ = now;
            pollSystemLogs();
        }
    }
    if (now - lastPruneMs_ > 600000) {
        const int days = qBound(1, store_.setting(QStringLiteral("retention_days"), QStringLiteral("1")).toInt(), 30);
        store_.pruneSamples(QDateTime::currentDateTime().addDays(-days));
        lastPruneMs_ = now;
    }
    if (store_.setting(QStringLiteral("monitor_enabled"), QStringLiteral("1")) != QLatin1String("0") && !pathProbe_.busy() && now - lastPathMs_ >= pathIntervalMs()) {
        lastPathMs_ = now;
        pathProbe_.start([this](PathResult result) { storePath(result); });
    }
    if (store_.setting(QStringLiteral("monitor_enabled"), QStringLiteral("1")) == QLatin1String("0")) return;
    if (chains_.size() >= 3) return;
    for (const Integration& integration : integrations_) {
        if (integration.paused || chains_.contains(integration.id)) continue;
        if (nextDue_.value(integration.id, now) > now) continue;
        if (chains_.size() >= 3) break;
        begin(integration, false, false, {});
    }
}

void Engine::ensureCustomModules()
{
    for (const Integration& integration : integrations_) {
        if (integration.locked) continue;
        const QString moduleId = moduleIdForIntegration(integration.id);
        bool found = false;
        for (const ModuleSnapshot& module : modules_) {
            if (module.id == moduleId) found = true;
        }
        if (found) continue;
        HttpResult empty;
        ModuleSnapshot module = evaluateHttpAdapter(integration, empty);
        if (integration.paused) {
            module.status = ModuleStatus::Disabled;
            module.connection = connectionLabel(ModuleStatus::Disabled);
            module.evidence = QStringLiteral("İzleme durduruldu. Geçmiş ölçüm duruyor. Uzak API kapatılmadı.");
        } else {
            module.status = ModuleStatus::Checking;
            module.evidence = QStringLiteral("Yapılandırıldı / Doğrulanmadı. Ölçüm bekleniyor.");
        }
        module.lastCheck = QDateTime();
        modules_.push_back(module);
    }
}

void Engine::applyPauseState(const QString& id, bool paused)
{
    QStringList ids;
    for (const Integration& item : integrations_) ids.push_back(item.id);
    for (ModuleSnapshot& module : modules_) {
        const bool mine = module.id == moduleIdForIntegration(id) || integrationIdForModule(module.id, ids) == id;
        if (!mine) continue;
        if (paused) {
            module.status = ModuleStatus::Disabled;
            module.connection = connectionLabel(ModuleStatus::Disabled);
            module.evidence = QStringLiteral("İzleme durduruldu. Geçmiş ölçüm duruyor. Uzak API kapatılmadı.");
        } else if (module.status == ModuleStatus::Disabled) {
            module.status = ModuleStatus::Checking;
            module.connection = connectionLabel(ModuleStatus::Checking);
            module.evidence = QStringLiteral("İzleme açıldı. Yeni ölçüm bekleniyor.");
        }
    }
}

QVariantMap Engine::latestSample(const QString& moduleId) const { return store_.latestSample(moduleId); }
QVariantMap Engine::latestHealthySample(const QString& moduleId) const { return store_.latestSampleMatching(moduleId, {QStringLiteral("healthy")}); }
QVariantMap Engine::latestFailedSample(const QString& moduleId) const
{
    return store_.latestSampleMatching(moduleId, {QStringLiteral("critical"), QStringLiteral("connection_error"), QStringLiteral("auth_rejected")});
}

Integration* Engine::findIntegration(const QString& id)
{
    for (Integration& integration : integrations_) {
        if (integration.id == id) return &integration;
    }
    return nullptr;
}

QString Engine::joinUrl(const QString& base, const QString& path) const
{
    QString root = base;
    while (root.endsWith(QLatin1Char('/'))) root.chop(1);
    const QString suffix = path.startsWith(QLatin1Char('/')) ? path : QStringLiteral("/") + path;
    return root + suffix;
}

ProbeRequest Engine::baseRequest(const Chain& chain, const QString& url, bool withAuth) const
{
    ProbeRequest request;
    request.url = url;
    request.method = chain.integration.method;
    request.timeoutMs = chain.integration.timeoutMs;
    request.allowPrivate = chain.integration.allowPrivate;
    request.headersJson = chain.integration.headersJson;
    if (chain.integration.allowPrivate) request.extraHosts << QUrl(chain.integration.baseUrl).host();
    if (withAuth) {
        request.authType = chain.bearer.isEmpty() ? chain.integration.authType : QStringLiteral("bearer");
        request.secret = chain.bearer.isEmpty() ? chain.secret : chain.bearer;
        request.apiKeyHeader = chain.integration.apiKeyHeader;
        request.certPem = chain.certPem;
        request.keyPem = chain.keyPem;
    }
    return request;
}

void Engine::begin(const Integration& integration, bool manual, bool draft, const QString& secretOverride)
{
    if (chains_.contains(integration.id)) return;
    Chain chain;
    chain.integration = integration;
    chain.manual = manual;
    chain.draft = draft;
    if (!secretOverride.isNull()) chain.secret = secretOverride;
    else if (credentials_->contains(integration.id)) {
        bool ok = false;
        chain.secret = credentials_->secret(integration.id, &ok, nullptr);
        if (!ok) chain.secret.clear();
    }
    if (integration.authType == QLatin1String("mtls")) {
        bool ok = false;
        chain.certPem = credentials_->secret(integration.id + QStringLiteral("|cert"), &ok, nullptr);
        chain.keyPem = credentials_->secret(integration.id + QStringLiteral("|key"), &ok, nullptr);
    }
    chains_.insert(integration.id, chain);
    advance(integration.id);
}

void Engine::advance(const QString& id)
{
    if (!chains_.contains(id)) return;
    Chain chain = chains_.value(id);
    const Integration& integration = chain.integration;
    QString url;
    bool withAuth = false;
    bool postToken = false;
    if (integration.kind == QLatin1String("ticketgo")) {
        const QStringList paths = {QStringLiteral("/health"), QStringLiteral("/health/live"), QStringLiteral("/health/ready"), QStringLiteral("/api/v1/health")};
        if (chain.step < paths.size()) url = joinUrl(integration.baseUrl, paths.at(chain.step));
        else if (chain.step == 4 && !chain.secret.isEmpty()) {
            url = joinUrl(integration.baseUrl, QStringLiteral("/health/dependencies"));
            withAuth = true;
            chain.depsAttempted = true;
            chain.integration.authType = QStringLiteral("api_key");
            chain.integration.apiKeyHeader = QStringLiteral("X-Ops-Token");
        } else {
            chains_.insert(id, chain);
            finish(id);
            return;
        }
    } else if (integration.kind == QLatin1String("gochat")) {
        if (chain.step == 0) url = joinUrl(integration.baseUrl, integration.healthPath);
        else if (chain.step == 1 && chain.single.status == 404) url = joinUrl(integration.baseUrl, QStringLiteral("/health"));
        else {
            chains_.insert(id, chain);
            finish(id);
            return;
        }
    } else if (integration.authType == QLatin1String("oauth2") && chain.bearer.isEmpty()) {
        postToken = true;
        url = integration.oauthTokenUrl;
    } else {
        if (chain.step > 0) {
            chains_.insert(id, chain);
            finish(id);
            return;
        }
        url = joinUrl(integration.baseUrl, integration.healthPath);
        withAuth = integration.authType != QLatin1String("none");
    }
    chains_.insert(id, chain);
    ProbeRequest request = baseRequest(chains_.value(id), url, withAuth);
    if (postToken) {
        request.method = QStringLiteral("POST");
        request.authType = QStringLiteral("none");
        QUrlQuery query;
        query.addQueryItem(QStringLiteral("grant_type"), QStringLiteral("client_credentials"));
        query.addQueryItem(QStringLiteral("client_id"), integration.oauthClientId);
        query.addQueryItem(QStringLiteral("client_secret"), chain.secret);
        request.body = query.query(QUrl::FullyEncoded).toUtf8();
        request.allowPrivate = integration.allowPrivate;
        if (integration.allowPrivate) request.extraHosts << QUrl(url).host();
    }
    client_.execute(request, [this, id](HttpResult result) { onResult(id, result); });
}

void Engine::onResult(const QString& id, const HttpResult& result)
{
    if (!chains_.contains(id)) return;
    Chain chain = chains_.value(id);
    if (chain.integration.authType == QLatin1String("oauth2") && chain.bearer.isEmpty() && chain.integration.kind == QLatin1String("custom")) {
        const QString token = result.json.value(QStringLiteral("access_token")).toString();
        if (!result.transportOk || token.isEmpty()) {
            chain.single = result;
            chain.single.detail = QStringLiteral("OAuth belirteci alınamadı. Gizli değer gösterilmez.");
            chain.step = 99;
            chains_.insert(id, chain);
            finish(id);
            return;
        }
        chain.bearer = token;
        chains_.insert(id, chain);
        advance(id);
        return;
    }
    if (chain.integration.kind == QLatin1String("ticketgo")) {
        if (chain.step == 0) chain.health = result;
        else if (chain.step == 1) chain.live = result;
        else if (chain.step == 2) chain.ready = result;
        else if (chain.step == 3) chain.identity = result;
        else chain.deps = result;
    } else if (chain.integration.kind == QLatin1String("gochat")) {
        if (chain.step == 0) chain.single = result;
        else chain.parent = result;
    } else chain.single = result;
    chain.step += 1;
    chains_.insert(id, chain);
    advance(id);
}

void Engine::finish(const QString& id)
{
    if (!chains_.contains(id)) return;
    Chain chain = chains_.take(id);
    chain.secret.fill(QChar(0));
    chain.keyPem.fill(QChar(0));
    chain.bearer.fill(QChar(0));
    const bool transportOk = chain.integration.kind == QLatin1String("ticketgo") ? (chain.health.transportOk || chain.live.transportOk || chain.ready.transportOk)
        : (chain.integration.kind == QLatin1String("gochat") ? chain.single.transportOk : chain.single.transportOk);
    int& fails = transportFails_[id];
    if (!chain.draft) {
        if (transportOk) fails = 0;
        else ++fails;
        const int base = qMax(15000, chain.integration.intervalMs);
        const int cap = qBound(0, store_.setting(QStringLiteral("retry_cap"), QStringLiteral("4")).toInt(), 4);
        const int delay = transportOk || cap == 0 ? base : qMin(300000, base * (1 << qMin(fails, cap)));
        nextDue_[id] = QDateTime::currentMSecsSinceEpoch() + delay;
    }
    QList<ModuleSnapshot> produced;
    int httpStatus = 0;
    if (chain.integration.kind == QLatin1String("ticketgo")) {
        produced = evaluateTicketGo(chain.health, chain.live, chain.ready, chain.identity, chain.deps, chain.depsAttempted);
        httpStatus = chain.health.status;
        const QJsonObject identity = chain.identity.jsonOk ? unwrapPayload(chain.identity.json) : QJsonObject();
        if (identity.contains(QStringLiteral("env"))) environment_ = identity.value(QStringLiteral("env")).toString();
        else if (identity.contains(QStringLiteral("environment"))) environment_ = identity.value(QStringLiteral("environment")).toString();
        if (identity.contains(QStringLiteral("region"))) region_ = identity.value(QStringLiteral("region")).toString();
        if (identity.contains(QStringLiteral("uptime"))) uptime_ = identity.value(QStringLiteral("uptime")).toString();
    } else if (chain.integration.kind == QLatin1String("gochat")) {
        produced = evaluateGoChat(chain.single, chain.parent);
        httpStatus = chain.single.status;
    } else if (chain.integration.kind == QLatin1String("ferry")) {
        produced = evaluateFerryApp(chain.single);
        httpStatus = chain.single.status;
    } else if (chain.integration.kind == QLatin1String("reachability")) {
        const QString moduleId = chain.integration.id == QLatin1String("ticket-go-web") ? QStringLiteral("tg-web") : QStringLiteral("tg-admin");
        const QString name = chain.integration.id == QLatin1String("ticket-go-web") ? QStringLiteral("Web uygulaması") : QStringLiteral("Admin panel");
        produced = {evaluateReachability(moduleId, name, chain.single)};
        httpStatus = chain.single.status;
    } else if (chain.integration.adapter == QLatin1String("contract-1.0")) {
        produced = evaluateContract(chain.integration, chain.single);
        httpStatus = chain.single.status;
    } else {
        produced = {evaluateHttpAdapter(chain.integration, chain.single)};
        httpStatus = chain.single.status;
    }
    const ModuleSnapshot first = produced.isEmpty() ? ModuleSnapshot{} : produced.first();
    if (chain.draft || chain.manual) {
        const HttpResult shown = chain.integration.kind == QLatin1String("ticketgo") ? chain.health : chain.single;
        QStringList parts;
        parts << statusLabel(first.status);
        parts << (shown.transportOk ? QStringLiteral("Bağlantı kuruldu.") : QStringLiteral("Bağlantı kurulamadı."));
        parts << (shown.status > 0 ? QStringLiteral("HTTP ") + QString::number(shown.status) : QStringLiteral("HTTP kodu yok."));
        parts << (shown.latencyMs >= 0 ? QString::number(shown.latencyMs) + QStringLiteral(" ms") : QStringLiteral("Yanıt süresi ölçülemedi."));
        parts << QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        if (shown.errorClass == QLatin1String("timeout")) parts << QStringLiteral("İstek zaman aşımına uğradı.");
        if (shown.errorClass == QLatin1String("tls")) parts << QStringLiteral("TLS doğrulaması başarısız. Sertifika denetimi kapatılmadı.");
        if (first.status == ModuleStatus::AuthRejected) parts << QStringLiteral("Kimlik doğrulama hatası.");
        if (!first.evidence.isEmpty()) parts << first.evidence;
        emit testFinished(parts.join(QStringLiteral(" ")));
    }
    if (chain.draft) return;
    if (!chain.integration.locked && chain.integration.kind == QLatin1String("custom")) {
        const int after = qBound(2, chain.integration.failThreshold <= 0 ? 3 : chain.integration.failThreshold, 10);
        for (ModuleSnapshot& module : produced) module.alertAfter = after;
    }
    applyModules(produced, httpStatus);
    const auto opened = incidents_.newlyOpened(produced, store_.setting(QStringLiteral("log_file")).isEmpty() ? QStringLiteral("Log kaynağı bağlı değil") : QStringLiteral("Yerel dosya"));
    for (const Incident& incident : incidents_.all()) store_.saveIncident(incident);
    for (const Incident& incident : opened) {
        if (incident.severity == QLatin1String("critical") && !incidentSilenced(incident.id)) {
            emit criticalOpened(QStringLiteral("HATA VAR"), incident.moduleName + QStringLiteral(": ") + incident.message.left(180));
        }
    }
    ++revision_;
    emit updated();
}

void Engine::decorate(ModuleSnapshot& module) const
{
    int bad = 0;
    const int total = store_.sampleCount(module.id, QDateTime::currentDateTime().addSecs(-3600), &bad);
    module.probeCount = total;
    if (total > 0) {
        module.availability = 100.0 * double(total - bad) / double(total);
        module.errorRate = double(bad) / double(total);
    }
    const auto series = store_.latencySeries(module.id, QDateTime::currentDateTime().addSecs(-3600));
    for (int i = qMax(0, series.size() - 24); i < series.size(); ++i) module.spark.push_back(series.at(i).second);
}

void Engine::applyModules(const QList<ModuleSnapshot>& list, int httpStatus)
{
    for (ModuleSnapshot incoming : list) {
        incoming.lastHttp = httpStatus;
        if (incoming.status != ModuleStatus::Checking && incoming.status != ModuleStatus::NotConfigured) {
            store_.addSample(incoming.id, incoming.status, incoming.latencyMs, httpStatus, incoming.evidence);
        }
        decorate(incoming);
        bool replaced = false;
        for (ModuleSnapshot& current : modules_) {
            if (current.id == incoming.id) {
                current = incoming;
                replaced = true;
                break;
            }
        }
        if (!replaced) modules_.push_back(incoming);
    }
}

void Engine::syncSockets()
{
    for (auto it = sockets_.begin(); it != sockets_.end(); ++it) {
        if (it.value()) it.value()->deleteLater();
    }
    sockets_.clear();
    for (const Integration& integration : integrations_) {
        if (integration.paused || integration.wsUrl.trimmed().isEmpty()) continue;
        const QUrl url(integration.wsUrl);
        if (url.scheme() != QLatin1String("wss")) continue;
        const SsrfDecision decision = precheckUrl(QUrl(QStringLiteral("https://") + url.host()), integration.allowPrivate ? QStringList{url.host()} : QStringList{}, integration.allowPrivate);
        if (!decision.ok) continue;
        auto* socket = new QWebSocket(QString(), QWebSocketProtocol::VersionLatest, this);
        sockets_.insert(integration.id, socket);
        connect(socket, &QWebSocket::textMessageReceived, this, [this, integration](const QString& message) {
            const QJsonDocument document = QJsonDocument::fromJson(message.left(65536).toUtf8());
            if (!document.isObject()) return;
            const ContractCheck check = validateContract(document.object());
            if (!check.ok) return;
            for (ModuleSnapshot& module : modules_) {
                if (module.id == QStringLiteral("custom-") + integration.id) {
                    module.evidence = QStringLiteral("WSS sözleşme mesajı alındı. status=") + statusLabel(check.overall);
                    module.status = check.overall;
                    module.connection = connectionLabel(check.overall);
                }
            }
            ++revision_;
            emit updated();
        });
        connect(socket, &QWebSocket::disconnected, this, [this, id = integration.id]() {
            QTimer::singleShot(qMin(300000, 5000 * (1 + transportFails_.value(id))), this, [this]() { syncSockets(); });
        });
        socket->open(url);
    }
}

QList<LogSource> Engine::logSources() const
{
    QList<LogSource> sources = discoveredLogSources();
    QList<LogRecord> combined = hubLogs_ + localLogs_;
    applySourceCounts(sources, combined, hubLogConnected_, hubDenyNote_);
    for (LogSource& source : sources) {
        if (source.id != QLatin1String("local-file")) continue;
        const QString path = store_.setting(QStringLiteral("log_file"));
        if (path.isEmpty() || !QFileInfo(path).isFile()) {
            source.access = QStringLiteral("unavailable");
            source.reason = QStringLiteral("Yerel dosya seçilmedi.");
            source.total = -1;
            continue;
        }
        source.access = QStringLiteral("connected");
        source.hubBacked = true;
        int infos = 0;
        int errors = 0;
        int warnings = 0;
        int criticals = 0;
        QString last;
        for (const LogRecord& row : localLogs_) {
            if (row.level == QLatin1String("INFO")) ++infos;
            if (row.level == QLatin1String("ERROR")) ++errors;
            if (row.level == QLatin1String("CRITICAL")) ++criticals;
            if (row.level == QLatin1String("WARN")) ++warnings;
            if (last.isEmpty() || row.time > last) last = row.time;
        }
        source.total = localLogs_.size();
        source.infos = infos;
        source.errors = errors;
        source.warnings = warnings;
        source.criticals = criticals;
        source.lastTime = last;
        source.reason = QStringLiteral("Yerel dosya okunuyor. Satırlar maskelenir.");
        source.levels = QStringLiteral("DEBUG INFO WARN ERROR CRITICAL");
    }
    return sources;
}

QList<LogRecord> Engine::logRows() const
{
    const QStringList hidden = store_.setting(QStringLiteral("hidden_hub_log_ids")).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    return omitHiddenLogs(filterLogRecords(hubLogs_ + localLogs_, logQuery_), hidden);
}

QList<LogRecord> Engine::recentLogs() const
{
    return hubLogs_ + localLogs_;
}

LogRecord Engine::selectedLog() const
{
    for (const LogRecord& row : logRows()) {
        if (row.id == selectedLogId_) return row;
    }
    return {};
}

QString Engine::logAccessNote() const { return logAccessNote_; }
int Engine::logHubTotal() const { return logHubTotal_; }
QString Engine::logRefreshedAt() const { return logRefreshedAt_.isEmpty() ? QStringLiteral("Henüz yenilenmedi") : logRefreshedAt_; }
bool Engine::logLive() const { return logLive_; }
bool Engine::logHubConnected() const { return hubLogConnected_; }
bool Engine::logStale() const { return logStale_; }
QString Engine::logLastRecordAt() const
{
    QString last;
    for (const LogRecord& row : hubLogs_) if (last.isEmpty() || row.time > last) last = row.time;
    return last.isEmpty() ? QStringLiteral("Veri yok") : last;
}
int Engine::logPollSeconds() const
{
    return logPollIntervalMs(store_.setting(QStringLiteral("log_poll_sec"), QStringLiteral("5"))) / 1000;
}
void Engine::setLogPollSeconds(int seconds)
{
    store_.setSetting(QStringLiteral("log_poll_sec"), QString::number(qBound(5, seconds, 60)));
    logBackoffMs_ = 0;
}
int Engine::logNewCount() const { return logNewCount_; }

void Engine::applyLogQuery(const LogQuery& query)
{
    logQuery_ = query;
    refreshLogs();
}

void Engine::setLogLive(bool on)
{
    logLive_ = on;
    logBackoffMs_ = 0;
    if (on) {
        lastLogPollMs_ = QDateTime::currentMSecsSinceEpoch();
        pollSystemLogs();
    }
    ++revision_;
    emit updated();
}

void Engine::selectLog(const QString& id)
{
    selectedLogId_ = id;
    ++revision_;
    emit updated();
}

bool Engine::setLogToken(const QString& token, QString* error)
{
    if (token.trimmed().size() < 20) {
        if (error) *error = QStringLiteral("Jeton çok kısa. Değer saklanmadı.");
        return false;
    }
    logAccessToken_ = token.trimmed();
    logSessionPhase_ = QStringLiteral("open");
    logRefreshUsed_ = false;
    logSessionVerifiedAt_ = QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy HH:mm"));
    QString keychainError;
    credentials_->setSecret(QStringLiteral("system-logs"), logAccessToken_, &keychainError);
    refreshLogs();
    return true;
}

bool Engine::logTokenConfigured() const { return logAccessSecret().size() >= 20; }

QString Engine::logSessionText() const
{
    if (logSessionPhase_ == QLatin1String("mfa")) return QStringLiteral("Çok adımlı doğrulama bekleniyor. Kod henüz doğrulanmadı.");
    if (logSessionPhase_ != QLatin1String("open") || logAccessSecret().size() < 20) return QStringLiteral("Oturum yok. İç yönetim hesabıyla oturum açın.");
    const QString account = logSessionAccount();
    return QStringLiteral("Oturum aktif. Hesap: ") + (account.isEmpty() ? QStringLiteral("doğrulandı") : account) + QStringLiteral(". Jeton gösterilmez.");
}

QString Engine::logSessionPhase() const { return logSessionPhase_; }
bool Engine::logSessionBusy() const { return logSessionBusy_; }
QString Engine::logSessionMessage() const { return logSessionMessage_; }
QString Engine::logSessionAccount() const
{
    if (!logSessionAccount_.isEmpty()) return logSessionAccount_;
    return store_.setting(QStringLiteral("log_session_user"));
}
QString Engine::logSessionVerifiedAt() const { return logSessionVerifiedAt_; }
QString Engine::logMfaHint() const { return logMfaHint_; }

void Engine::storeLogSession(const AuthEnvelope& envelope)
{
    if (envelope.accessToken.size() < 20) {
        logSessionPhase_ = QStringLiteral("error");
        logSessionMessage_ = QStringLiteral("Oturum jetonu alınamadı. Oturum açılmadı.");
        logSessionBusy_ = false;
        return;
    }
    logAccessToken_ = envelope.accessToken;
    if (envelope.refreshToken.size() >= 10) logRefreshToken_ = envelope.refreshToken;
    QString error;
    credentials_->setSecret(QStringLiteral("system-logs"), logAccessToken_, &error);
    if (logRefreshToken_.size() >= 10) credentials_->setSecret(QStringLiteral("system-logs-refresh"), logRefreshToken_, &error);
    if (!envelope.account.isEmpty()) {
        logSessionAccount_ = envelope.account;
        store_.setSetting(QStringLiteral("log_session_user"), envelope.account);
    }
    mfaChallenge_.clear();
    logMfaHint_.clear();
    logSessionPhase_ = QStringLiteral("open");
    logSessionVerifiedAt_ = QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy HH:mm"));
    store_.setSetting(QStringLiteral("log_session_verified_at"), logSessionVerifiedAt_);
    logSessionBusy_ = false;
    store_.appendAudit(QStringLiteral("log-oturumu-yazildi"), QStringLiteral("system-logs"), QStringLiteral("Jeton degeri yazilmadi."));
}

QString Engine::openLogSession(const QString& username, const QString& password)
{
    const QString login = username.trimmed().left(320);
    if (login.isEmpty() || password.isEmpty()) {
        logSessionPhase_ = QStringLiteral("error");
        logSessionMessage_ = QStringLiteral("Kullanıcı adı ve parola gerekli.");
        ++revision_;
        emit updated();
        return logSessionMessage_;
    }
    if (logSessionBusy_) return QStringLiteral("İstek sürüyor.");
    QJsonObject body;
    body.insert(QStringLiteral("username"), login);
    body.insert(QStringLiteral("password"), password);
    ProbeRequest request;
    request.url = QStringLiteral("https://api.ticket-go.net/api/v1/auth/login");
    request.method = QStringLiteral("POST");
    request.contentType = QStringLiteral("application/json");
    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    request.timeoutMs = 8000;
    logSessionBusy_ = true;
    logSessionPhase_ = QStringLiteral("busy");
    logSessionMessage_.clear();
    client_.execute(request, [this, login](HttpResult result) {
        logSessionBusy_ = false;
        const AuthEnvelope envelope = readAuthEnvelope(result.status, result.json);
        const SessionDecision decision = decideAuthResult(result.transportOk, result.status, envelope);
        logSessionPhase_ = decision.phase;
        logSessionMessage_ = decision.message;
        logAccessNote_ = decision.message;
        if (decision.phase == QLatin1String("mfa")) {
            mfaChallenge_ = envelope.challengeToken;
            logMfaHint_ = envelope.maskedEmail;
            store_.setSetting(QStringLiteral("log_session_user"), login);
        } else if (decision.storeSession) {
            AuthEnvelope stored = envelope;
            if (stored.account.isEmpty()) stored.account = login;
            logRefreshUsed_ = false;
            storeLogSession(stored);
            if (logSessionPhase_ == QLatin1String("open")) refreshLogs();
        }
        ++revision_;
        emit updated();
    });
    return QStringLiteral("Oturum isteği gönderildi. Parola saklanmaz.");
}

QString Engine::submitLogMfa(const QString& code)
{
    const QString trimmed = code.trimmed();
    if (mfaChallenge_.size() < 20) return QStringLiteral("Doğrulama adımı yok.");
    if (trimmed.size() < 6 || trimmed.size() > 12) {
        logSessionPhase_ = QStringLiteral("mfa");
        logSessionMessage_ = QStringLiteral("Kod eksik. Oturum açılmadı.");
        ++revision_;
        emit updated();
        return logSessionMessage_;
    }
    if (logSessionBusy_) return QStringLiteral("İstek sürüyor.");
    QJsonObject body;
    body.insert(QStringLiteral("challengeToken"), mfaChallenge_);
    body.insert(QStringLiteral("code"), trimmed);
    ProbeRequest request;
    request.url = QStringLiteral("https://api.ticket-go.net/api/v1/auth/mfa/verify");
    request.method = QStringLiteral("POST");
    request.contentType = QStringLiteral("application/json");
    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    request.timeoutMs = 8000;
    logSessionBusy_ = true;
    client_.execute(request, [this](HttpResult result) {
        logSessionBusy_ = false;
        const AuthEnvelope envelope = readAuthEnvelope(result.status, result.json);
        const SessionDecision decision = decideAuthResult(result.transportOk, result.status, envelope);
        if (decision.storeSession) {
            logRefreshUsed_ = false;
            storeLogSession(envelope);
            if (logSessionPhase_ == QLatin1String("open")) {
                logSessionMessage_ = QStringLiteral("Oturum doğrulandı. Loglar isteniyor.");
                logAccessNote_ = logSessionMessage_;
                refreshLogs();
            }
        } else {
            logSessionPhase_ = QStringLiteral("mfa");
            logSessionMessage_ = decision.phase == QLatin1String("error") ? decision.message : QStringLiteral("Çok adımlı doğrulama kabul edilmedi.");
            logAccessNote_ = logSessionMessage_;
        }
        ++revision_;
        emit updated();
    });
    return QStringLiteral("Doğrulama isteği gönderildi.");
}

QString Engine::resendLogMfa()
{
    if (mfaChallenge_.size() < 20) return QStringLiteral("Doğrulama adımı yok.");
    if (logSessionBusy_) return QStringLiteral("İstek sürüyor.");
    QJsonObject body;
    body.insert(QStringLiteral("challengeToken"), mfaChallenge_);
    ProbeRequest request;
    request.url = QStringLiteral("https://api.ticket-go.net/api/v1/auth/mfa/email-otp/resend");
    request.method = QStringLiteral("POST");
    request.contentType = QStringLiteral("application/json");
    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    request.timeoutMs = 8000;
    logSessionBusy_ = true;
    client_.execute(request, [this](HttpResult result) {
        logSessionBusy_ = false;
        logSessionPhase_ = QStringLiteral("mfa");
        if (result.transportOk && result.status == 200) logSessionMessage_ = QStringLiteral("Yeni doğrulama kodu gönderildi. Kod saklanmaz.");
        else logSessionMessage_ = QStringLiteral("Doğrulama kodu yeniden gönderilemedi.");
        logAccessNote_ = logSessionMessage_;
        ++revision_;
        emit updated();
    });
    return QStringLiteral("Yeniden gönderme isteği iletildi.");
}

QString Engine::refreshLogSession()
{
    if (logRefreshInFlight_) return QStringLiteral("Oturum yenileme sürüyor.");
    if (logRefreshUsed_) return QStringLiteral("Oturum yenileme zaten denendi. Yeniden oturum açın.");
    QString refresh = logRefreshToken_;
    if (refresh.size() < 10) {
        bool ok = false;
        refresh = credentials_->secret(QStringLiteral("system-logs-refresh"), &ok, nullptr);
        if (ok && refresh.size() >= 10) logRefreshToken_ = refresh;
    }
    if (refresh.size() < 10) return QStringLiteral("Yenileme kaydı yok. Yeniden oturum açın.");
    logRefreshUsed_ = true;
    logRefreshInFlight_ = true;
    QJsonObject body;
    body.insert(QStringLiteral("refreshToken"), refresh);
    ProbeRequest request;
    request.url = QStringLiteral("https://api.ticket-go.net/api/v1/auth/refresh");
    request.method = QStringLiteral("POST");
    request.contentType = QStringLiteral("application/json");
    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    request.timeoutMs = 8000;
    client_.execute(request, [this](HttpResult result) {
        logRefreshInFlight_ = false;
        const AuthEnvelope envelope = readAuthEnvelope(result.status, result.json);
        if (envelope.accessToken.size() >= 20) {
            storeLogSession(envelope);
            if (logSessionPhase_ == QLatin1String("open")) {
                logSessionMessage_ = QStringLiteral("Oturum yenilendi. Loglar isteniyor.");
                logAccessNote_ = logSessionMessage_;
                refreshLogs();
            }
            ++revision_;
            emit updated();
            return;
        }
        dropExpiredLogSession(QStringLiteral("Oturum süresi doldu. Yeniden giriş yapın."));
    });
    return QStringLiteral("Oturum yenileme denendi.");
}

QString Engine::closeLogSession()
{
    QString token = logAccessSecret();
    QString refresh = logRefreshToken_;
    if (refresh.size() < 10) {
        bool refreshOk = false;
        refresh = credentials_->secret(QStringLiteral("system-logs-refresh"), &refreshOk, nullptr);
    }
    const bool hadToken = token.size() >= 20;
    logAccessToken_.clear();
    logRefreshToken_.clear();
    logSessionAccount_.clear();
    QString ignored;
    credentials_->removeSecret(QStringLiteral("system-logs"), &ignored);
    credentials_->removeSecret(QStringLiteral("system-logs-refresh"), &ignored);
    mfaChallenge_.clear();
    logMfaHint_.clear();
    logRefreshUsed_ = false;
    logRefreshInFlight_ = false;
    logSessionBusy_ = false;
    logSessionPhase_ = QStringLiteral("none");
    store_.setSetting(QStringLiteral("log_session_user"), QString());
    store_.setSetting(QStringLiteral("log_session_verified_at"), QString());
    logSessionVerifiedAt_.clear();
    hubLogs_.clear();
    hubLogConnected_ = false;
    hubDenyNote_ = logReadFailureNote(false, true, 0, QString());
    logAccessNote_ = hubDenyNote_;
    store_.appendAudit(QStringLiteral("log-oturumu-kapatildi"), QStringLiteral("system-logs"), QStringLiteral("Jeton degeri yazilmadi. Backend logu silinmedi."));
    ++revision_;
    emit updated();
    if (!hadToken) {
        logSessionMessage_ = QStringLiteral("Oturum bu bilgisayardan kapatıldı.");
        return logSessionMessage_;
    }
    QJsonObject body;
    if (refresh.size() >= 10) body.insert(QStringLiteral("refreshToken"), refresh);
    ProbeRequest request;
    request.url = QStringLiteral("https://api.ticket-go.net/api/v1/auth/logout");
    request.method = QStringLiteral("POST");
    request.contentType = QStringLiteral("application/json");
    request.body = QJsonDocument(body).toJson(QJsonDocument::Compact);
    request.authType = QStringLiteral("bearer");
    request.secret = token;
    request.timeoutMs = 8000;
    client_.execute(request, [this](HttpResult result) {
        const bool confirmed = result.transportOk && (result.status == 200 || result.status == 204);
        logSessionMessage_ = confirmed
            ? QStringLiteral("Oturum kapatıldı.")
            : QStringLiteral("Backend oturumu kapatıldığı doğrulanamadı. Yerel oturum silindi.");
        logAccessNote_ = logSessionMessage_;
        ++revision_;
        emit updated();
    });
    logSessionMessage_ = QStringLiteral("Oturum kapatılıyor.");
    return logSessionMessage_;
}

QString Engine::validateLogSession()
{
    const QString token = logAccessSecret();
    if (token.isEmpty()) return QStringLiteral("Oturum yok.");
    ProbeRequest request;
    request.url = QStringLiteral("https://api.ticket-go.net/api/v1/auth/me");
    request.method = QStringLiteral("GET");
    request.authType = QStringLiteral("bearer");
    request.secret = token;
    request.timeoutMs = 8000;
    client_.execute(request, [this](HttpResult result) {
        if (handleLogAuthResult(result)) return;
        const AuthEnvelope envelope = readAuthEnvelope(result.status, result.json);
        if (!result.transportOk) logAccessNote_ = QStringLiteral("Sunucuya erişilemiyor.");
        else if (result.status == 200) {
            if (!envelope.account.isEmpty()) {
                logSessionAccount_ = envelope.account;
                store_.setSetting(QStringLiteral("log_session_user"), envelope.account);
            }
            logAccessNote_ = QStringLiteral("Oturum doğrulandı. Log okuma ayrıca logs.read ister.");
        } else if (result.status == 403) logAccessNote_ = QStringLiteral("İzleme hesabınızın Sistem Logları okuma yetkisi yok.");
        else logAccessNote_ = QStringLiteral("Oturum doğrulanamadı. HTTP ") + QString::number(result.status);
        ++revision_;
        emit updated();
    });
    return QStringLiteral("Oturum doğrulaması gönderildi.");
}

void Engine::hideHubLog(const QString& id)
{
    const QString key = id.trimmed().left(80);
    if (key.isEmpty()) return;
    QStringList hidden = store_.setting(QStringLiteral("hidden_hub_log_ids")).split(QLatin1Char('\n'), Qt::SkipEmptyParts);
    if (!hidden.contains(key)) hidden.push_back(key);
    store_.setSetting(QStringLiteral("hidden_hub_log_ids"), hidden.join(QLatin1Char('\n')));
    store_.appendAudit(QStringLiteral("log-gorunumu-gizlendi"), key, QStringLiteral("Backend sistem logu silinmedi."));
    ++revision_;
    emit updated();
}

void Engine::restoreHiddenHubLogs()
{
    store_.setSetting(QStringLiteral("hidden_hub_log_ids"), QString());
    store_.appendAudit(QStringLiteral("log-gorunumu-geri-yuklendi"), QStringLiteral("hidden_hub_log_ids"), QStringLiteral("Yerel gizleme kalktı. Backend değişmedi."));
    ++revision_;
    emit updated();
}

int Engine::logPage() const { return qMax(1, logQuery_.page); }

void Engine::setLogPage(int page)
{
    logQuery_.page = qMax(1, page);
    refreshLogs();
}

void Engine::clearLogToken()
{
    logAccessToken_.clear();
    logRefreshToken_.clear();
    logSessionPhase_ = QStringLiteral("none");
    logSessionAccount_.clear();
    QString ignored;
    credentials_->removeSecret(QStringLiteral("system-logs"), &ignored);
    credentials_->removeSecret(QStringLiteral("system-logs-refresh"), &ignored);
    store_.setSetting(QStringLiteral("log_session_user"), QString());
    store_.setSetting(QStringLiteral("log_session_verified_at"), QString());
    store_.appendAudit(QStringLiteral("log-jetonu-silindi"), QStringLiteral("system-logs"), QStringLiteral("Jeton değeri yazılmadı. Backend logu silinmedi."));
    hubLogs_.clear();
    hubLogConnected_ = false;
    hubDenyNote_ = logReadFailureNote(false, true, 0, QString());
    logAccessNote_ = hubDenyNote_;
    ++revision_;
    emit updated();
}

void Engine::refreshLogs()
{
    const QString path = store_.setting(QStringLiteral("log_file"));
    localLogs_.clear();
    if (!path.isEmpty() && QFileInfo(path).isFile()) {
        QFile file(path);
        if (file.open(QIODevice::ReadOnly)) {
            const qint64 size = file.size();
            if (size > 131072) file.seek(size - 131072);
            localLogs_ = parseLocalLog(QString::fromUtf8(file.readAll()));
        }
    }
    const QString token = logAccessSecret();
    if (token.isEmpty()) {
        hubLogConnected_ = false;
        hubLogs_.clear();
        logStale_ = false;
        logNewCount_ = 0;
        logLiveCursor_.clear();
        logHubTotal_ = -1;
        logRefreshedAt_ = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
        hubDenyNote_ = logReadFailureNote(false, true, 0, QString());
        logAccessNote_ = localLogs_.isEmpty()
            ? hubDenyNote_
            : QStringLiteral("Yalnız seçilen yerel dosya okunuyor. ") + hubDenyNote_;
        ++revision_;
        emit updated();
        return;
    }
    if (logFetchBusy_) {
        logAccessNote_ = QStringLiteral("Önceki log isteği sürüyor.");
        ++revision_;
        emit updated();
        return;
    }
    startLogList();
}

void Engine::startLogList()
{
    const QString token = logAccessSecret();
    if (token.isEmpty() || logFetchBusy_) return;
    logFetchBusy_ = true;
    ProbeRequest request;
    request.url = systemLogRequestUrl(logQuery_, false, QString()).toString(QUrl::FullyEncoded);
    request.method = QStringLiteral("GET");
    request.timeoutMs = 8000;
    request.authType = QStringLiteral("bearer");
    request.secret = token;
    client_.execute(request, [this](HttpResult result) {
        logFetchBusy_ = false;
        ingestHubLogs(result);
    });
}

void Engine::pollSystemLogs()
{
    if (!logLive_ || logFetchBusy_) return;
    const QString token = logAccessSecret();
    if (token.isEmpty()) return;
    logFetchBusy_ = true;
    ProbeRequest request;
    request.url = systemLogRequestUrl(logQuery_, true, logLiveCursor_).toString(QUrl::FullyEncoded);
    request.method = QStringLiteral("GET");
    request.timeoutMs = 8000;
    request.authType = QStringLiteral("bearer");
    request.secret = token;
    client_.execute(request, [this](HttpResult result) {
        logFetchBusy_ = false;
        ingestLiveLogs(result);
    });
}

void Engine::ingestHubLogs(const HttpResult& result)
{
    if (handleLogAuthResult(result)) return;
    if (!result.transportOk || result.status < 200 || result.status >= 300 || !result.jsonOk) {
        noteLogFailure(result);
        return;
    }
    const QJsonObject body = unwrapPayload(result.json);
    QJsonArray items = body.value(QStringLiteral("items")).toArray();
    if (items.isEmpty() && result.json.value(QStringLiteral("data")).isArray()) items = result.json.value(QStringLiteral("data")).toArray();
    const QList<LogRecord> incoming = parseHubLogItems(items);
    logHubTotal_ = body.contains(QStringLiteral("total")) ? body.value(QStringLiteral("total")).toInt() : incoming.size();
    logRefreshedAt_ = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    int fresh = 0;
    hubLogs_ = mergeLogRecords(hubLogs_, incoming, 80, &fresh);
    if (!logLive_) {
        hubLogs_ = mergeLogRecords({}, incoming, 80, &fresh);
        fresh = 0;
    }
    logNewCount_ = fresh;
    hubLogConnected_ = true;
    logStale_ = false;
    logBackoffMs_ = 0;
    hubDenyNote_.clear();
    logAccessNote_ = incoming.isEmpty()
        ? emptyLogResultNote()
        : QStringLiteral("System Logs Hub bağlı. Bu sayfa ") + QString::number(hubLogs_.size()) + QStringLiteral(" kayıt. Kaynak toplamı ") + QString::number(logHubTotal_) + QStringLiteral(". Mesaj içeriği maskelenir.");
    ++revision_;
    emit updated();
}

void Engine::noteLogFailure(const HttpResult& result)
{
    const QString errorCode = result.json.value(QStringLiteral("error")).toObject().value(QStringLiteral("code")).toString();
    const int interval = logPollIntervalMs(store_.setting(QStringLiteral("log_poll_sec"), QStringLiteral("5")));
    logBackoffMs_ = qMin(60000, qMax(interval * 2, logBackoffMs_ > 0 ? logBackoffMs_ * 2 : interval * 2));
    hubLogConnected_ = false;
    logNewCount_ = 0;
    logStale_ = !hubLogs_.isEmpty();
    if (!logStale_) logHubTotal_ = -1;
    hubDenyNote_ = logReadFailureNote(true, result.transportOk, result.status, errorCode, result.errorClass);
    logAccessNote_ = logStale_
        ? QStringLiteral("Son başarılı kayıtlar duruyor. Yeni kayıt gelmedi. ") + hubDenyNote_
        : hubDenyNote_;
    logRefreshedAt_ = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    ++revision_;
    emit updated();
}

void Engine::ingestLiveLogs(const HttpResult& result)
{
    if (handleLogAuthResult(result)) return;
    if (!result.transportOk || result.status < 200 || result.status >= 300 || !result.jsonOk) {
        noteLogFailure(result);
        return;
    }
    const QJsonObject body = unwrapPayload(result.json);
    const QString cursor = body.value(QStringLiteral("cursor")).toString();
    if (!cursor.isEmpty()) logLiveCursor_ = cursor;
    const QJsonArray items = body.value(QStringLiteral("items")).toArray();
    logBackoffMs_ = 0;
    hubLogConnected_ = true;
    logRefreshedAt_ = QDateTime::currentDateTime().toString(QStringLiteral("yyyy-MM-dd HH:mm:ss"));
    if (items.isEmpty()) {
        logNewCount_ = 0;
        logStale_ = false;
        hubDenyNote_.clear();
        logAccessNote_ = QStringLiteral("Canlı bağlantı açık. Bu aralıkta yeni kayıt yok.");
        ++revision_;
        emit updated();
        return;
    }
    if (logQuery_.page > 1) {
        logNewCount_ = items.size();
        logAccessNote_ = QStringLiteral("Yeni kayıt var. İlk sayfada görünür. Açık sayfa değiştirilmedi.");
        ++revision_;
        emit updated();
        return;
    }
    startLogList();
}

QString Engine::logAccessSecret() const
{
    if (logAccessToken_.size() >= 20) return logAccessToken_;
    return {};
}

bool Engine::hasRefreshToken() const
{
    if (logRefreshToken_.size() >= 10) return true;
    bool ok = false;
    const QString stored = credentials_->secret(QStringLiteral("system-logs-refresh"), &ok, nullptr);
    return ok && stored.size() >= 10;
}

void Engine::forgetStoredLogSecrets()
{
    QString ignored;
    credentials_->removeSecret(QStringLiteral("system-logs"), &ignored);
    credentials_->removeSecret(QStringLiteral("system-logs-refresh"), &ignored);
    store_.setSetting(QStringLiteral("log_session_user"), QString());
    store_.setSetting(QStringLiteral("log_session_verified_at"), QString());
    logSessionVerifiedAt_.clear();
    logSessionAccount_.clear();
    logAccessToken_.clear();
    logRefreshToken_.clear();
    logSessionPhase_ = QStringLiteral("none");
}

void Engine::discardLogSession()
{
    forgetStoredLogSecrets();
    mfaChallenge_.clear();
    logMfaHint_.clear();
    logRefreshUsed_ = false;
    logRefreshInFlight_ = false;
    logSessionBusy_ = false;
    logSessionMessage_.clear();
    hubLogs_.clear();
    hubLogConnected_ = false;
    store_.appendAudit(QStringLiteral("log-oturumu-kapatildi"), QStringLiteral("system-logs"), QStringLiteral("Uygulama kapandi. Jeton degeri yazilmadi."));
}

void Engine::dropExpiredLogSession(const QString& message)
{
    if (logSessionPhase_ == QLatin1String("none") && logAccessToken_.isEmpty()) return;
    logAccessToken_.clear();
    logRefreshToken_.clear();
    logSessionAccount_.clear();
    QString ignored;
    credentials_->removeSecret(QStringLiteral("system-logs"), &ignored);
    credentials_->removeSecret(QStringLiteral("system-logs-refresh"), &ignored);
    logRefreshInFlight_ = false;
    logSessionBusy_ = false;
    logSessionPhase_ = QStringLiteral("none");
    logSessionMessage_ = message;
    logAccessNote_ = message;
    hubDenyNote_ = message;
    hubLogConnected_ = false;
    hubLogs_.clear();
    logHubTotal_ = -1;
    store_.setSetting(QStringLiteral("log_session_user"), QString());
    store_.setSetting(QStringLiteral("log_session_verified_at"), QString());
    logSessionVerifiedAt_.clear();
    store_.appendAudit(QStringLiteral("log-oturumu-reddedildi"), QStringLiteral("system-logs"), QStringLiteral("Jeton degeri yazilmadi."));
    ++revision_;
    emit updated();
}

bool Engine::handleLogAuthResult(const HttpResult& result)
{
    if (result.status != 401) return false;
    if (logSessionPhase_ != QLatin1String("open")) return true;
    if (logRefreshInFlight_) return true;
    const bool refreshWillRun = shouldRefreshLogSession(result.status, hasRefreshToken(), logRefreshUsed_);
    const QString next = logSessionPhaseAfterResult(logSessionPhase_, result.status, result.transportOk, refreshWillRun);
    if (refreshWillRun && next == QLatin1String("open")) {
        refreshLogSession();
        return true;
    }
    if (next == QLatin1String("none")) {
        dropExpiredLogSession(QStringLiteral("Oturum süresi doldu. Yeniden giriş yapın."));
        return true;
    }
    return false;
}

} // namespace tg
