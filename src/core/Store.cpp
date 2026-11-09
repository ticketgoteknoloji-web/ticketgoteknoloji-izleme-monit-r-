#include "Store.h"

#include <QDateTime>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>
#include <QVariant>

namespace tg {
namespace {

Integration builtin(const QString& id, const QString& name, const QString& kind, const QString& base, const QString& path, const QString& adapter, int interval)
{
    Integration item;
    item.id = id;
    item.name = name;
    item.kind = kind;
    item.category = kind;
    item.environment = QStringLiteral("production");
    item.baseUrl = base;
    item.healthPath = path;
    item.method = QStringLiteral("GET");
    item.adapter = adapter;
    item.intervalMs = interval;
    item.timeoutMs = 8000;
    item.expectedStatus = 200;
    item.locked = true;
    item.paused = false;
    item.authType = QStringLiteral("none");
    item.description = QStringLiteral("Yerleşik salt okunur bağlayıcı.");
    return item;
}

QList<Integration> builtins()
{
    return {
        builtin(QStringLiteral("ticket-go-backend"), QStringLiteral("Ticket-Go Backend"), QStringLiteral("ticketgo"), QStringLiteral("https://api.ticket-go.net"), QStringLiteral("/health"), QStringLiteral("ticketgo"), 15000),
        builtin(QStringLiteral("go-chat-api"), QStringLiteral("Go-Chat API"), QStringLiteral("gochat"), QStringLiteral("https://api.ticket-go.net"), QStringLiteral("/api/v1/gochat/health"), QStringLiteral("gochat"), 15000),
        builtin(QStringLiteral("ticket-go-ferry"), QStringLiteral("Feribot bileti satış"), QStringLiteral("ferry"), QStringLiteral("https://api.ticket-go.net"), QStringLiteral("/api/v1/mobile-app/config?platform=ios&version=1.0.0"), QStringLiteral("ferry"), 15000),
        builtin(QStringLiteral("ticket-go-web"), QStringLiteral("Web uygulaması"), QStringLiteral("reachability"), QStringLiteral("https://www.ticket-go.net"), QStringLiteral("/"), QStringLiteral("http"), 15000),
        builtin(QStringLiteral("ticket-go-admin"), QStringLiteral("Admin panel"), QStringLiteral("reachability"), QStringLiteral("https://admin.ticket-go.net"), QStringLiteral("/"), QStringLiteral("http"), 15000)
    };
}

} // namespace

Store::~Store()
{
    const QString name = connectionName_;
    if (db_.isValid()) {
        db_.close();
        db_ = QSqlDatabase();
    }
    if (!name.isEmpty()) QSqlDatabase::removeDatabase(name);
}

bool Store::open(const QString& filePath, QString* error)
{
    path_ = filePath;
    connectionName_ = QStringLiteral("tgops-") + QUuid::createUuid().toString(QUuid::Id128);
    db_ = QSqlDatabase::addDatabase(QStringLiteral("QSQLITE"), connectionName_);
    db_.setDatabaseName(filePath);
    if (!db_.open()) {
        if (error) *error = db_.lastError().text();
        return false;
    }
    QSqlQuery query(db_);
    const QStringList statements = {
        QStringLiteral("CREATE TABLE IF NOT EXISTS integrations (id TEXT PRIMARY KEY, name TEXT, description TEXT, category TEXT, environment TEXT, kind TEXT, base_url TEXT, health_path TEXT, method TEXT, auth_type TEXT, api_key_header TEXT, oauth_token_url TEXT, oauth_client_id TEXT, interval_ms INTEGER, timeout_ms INTEGER, expected_status INTEGER, latency_warn_ms INTEGER, fail_threshold INTEGER, adapter TEXT, paused INTEGER, locked INTEGER, allow_private INTEGER, ws_url TEXT, success_note TEXT, headers_json TEXT, log_source TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS samples (id INTEGER PRIMARY KEY AUTOINCREMENT, module_id TEXT, ts INTEGER, status TEXT, latency_ms INTEGER, http_status INTEGER, evidence TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS incidents (id TEXT PRIMARY KEY, module_id TEXT, application TEXT, module_name TEXT, service TEXT, kind TEXT, http_status INTEGER, message TEXT, first_seen INTEGER, last_seen INTEGER, repeat_count INTEGER, severity TEXT, log_source TEXT, cause_label TEXT, cause_text TEXT, recommendation TEXT, state TEXT, resolved_at INTEGER)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS settings (key TEXT PRIMARY KEY, value TEXT)"),
        QStringLiteral("CREATE INDEX IF NOT EXISTS idx_samples_module ON samples(module_id, ts)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS audit_log (id INTEGER PRIMARY KEY AUTOINCREMENT, ts INTEGER, action TEXT, target TEXT, result TEXT)"),
        QStringLiteral("CREATE TABLE IF NOT EXISTS saved_reports (id TEXT PRIMARY KEY, name TEXT, kind TEXT, created_at INTEGER, period TEXT, system TEXT, format TEXT, status TEXT, path TEXT, summary TEXT)")
    };
    for (const QString& sql : statements) {
        if (!query.exec(sql)) {
            if (error) *error = query.lastError().text();
            return false;
        }
    }
    QSet<QString> present;
    for (const Integration& item : integrations()) present.insert(item.id);
    for (const Integration& item : builtins()) {
        if (present.contains(item.id)) continue;
        QString ignored;
        saveIntegration(item, &ignored);
    }
    return true;
}

QString Store::path() const { return path_; }

Integration Store::readIntegration(QSqlQuery& query) const
{
    Integration item;
    item.id = query.value(0).toString();
    item.name = query.value(1).toString();
    item.description = query.value(2).toString();
    item.category = query.value(3).toString();
    item.environment = query.value(4).toString();
    item.kind = query.value(5).toString();
    item.baseUrl = query.value(6).toString();
    item.healthPath = query.value(7).toString();
    item.method = query.value(8).toString();
    item.authType = query.value(9).toString();
    item.apiKeyHeader = query.value(10).toString();
    item.oauthTokenUrl = query.value(11).toString();
    item.oauthClientId = query.value(12).toString();
    item.intervalMs = query.value(13).toInt();
    item.timeoutMs = query.value(14).toInt();
    item.expectedStatus = query.value(15).toInt();
    item.latencyWarnMs = query.value(16).toInt();
    item.failThreshold = query.value(17).toInt();
    item.adapter = query.value(18).toString();
    item.paused = query.value(19).toInt() != 0;
    item.locked = query.value(20).toInt() != 0;
    item.allowPrivate = query.value(21).toInt() != 0;
    item.wsUrl = query.value(22).toString();
    item.successNote = query.value(23).toString();
    item.headersJson = query.value(24).toString();
    item.logSource = query.value(25).toString();
    return item;
}

QList<Integration> Store::integrations() const
{
    QList<Integration> list;
    QSqlQuery query(db_);
    query.exec(QStringLiteral("SELECT id,name,description,category,environment,kind,base_url,health_path,method,auth_type,api_key_header,oauth_token_url,oauth_client_id,interval_ms,timeout_ms,expected_status,latency_warn_ms,fail_threshold,adapter,paused,locked,allow_private,ws_url,success_note,headers_json,log_source FROM integrations ORDER BY locked DESC, name"));
    while (query.next()) list.push_back(readIntegration(query));
    return list;
}

bool Store::saveIntegration(const Integration& integration, QString* error)
{
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("INSERT INTO integrations (id,name,description,category,environment,kind,base_url,health_path,method,auth_type,api_key_header,oauth_token_url,oauth_client_id,interval_ms,timeout_ms,expected_status,latency_warn_ms,fail_threshold,adapter,paused,locked,allow_private,ws_url,success_note,headers_json,log_source) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET name=excluded.name, description=excluded.description, category=excluded.category, environment=excluded.environment, kind=excluded.kind, base_url=excluded.base_url, health_path=excluded.health_path, method=excluded.method, auth_type=excluded.auth_type, api_key_header=excluded.api_key_header, oauth_token_url=excluded.oauth_token_url, oauth_client_id=excluded.oauth_client_id, interval_ms=excluded.interval_ms, timeout_ms=excluded.timeout_ms, expected_status=excluded.expected_status, latency_warn_ms=excluded.latency_warn_ms, fail_threshold=excluded.fail_threshold, adapter=excluded.adapter, paused=excluded.paused, locked=excluded.locked, allow_private=excluded.allow_private, ws_url=excluded.ws_url, success_note=excluded.success_note, headers_json=excluded.headers_json, log_source=excluded.log_source"));
    query.addBindValue(integration.id);
    query.addBindValue(integration.name);
    query.addBindValue(integration.description);
    query.addBindValue(integration.category);
    query.addBindValue(integration.environment);
    query.addBindValue(integration.kind);
    query.addBindValue(integration.baseUrl);
    query.addBindValue(integration.healthPath);
    query.addBindValue(integration.method);
    query.addBindValue(integration.authType);
    query.addBindValue(integration.apiKeyHeader);
    query.addBindValue(integration.oauthTokenUrl);
    query.addBindValue(integration.oauthClientId);
    query.addBindValue(integration.intervalMs);
    query.addBindValue(integration.timeoutMs);
    query.addBindValue(integration.expectedStatus);
    query.addBindValue(integration.latencyWarnMs);
    query.addBindValue(integration.failThreshold);
    query.addBindValue(integration.adapter);
    query.addBindValue(integration.paused ? 1 : 0);
    query.addBindValue(integration.locked ? 1 : 0);
    query.addBindValue(integration.allowPrivate ? 1 : 0);
    query.addBindValue(integration.wsUrl);
    query.addBindValue(integration.successNote);
    query.addBindValue(integration.headersJson);
    query.addBindValue(integration.logSource);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

bool Store::removeIntegration(const QString& id, QString* error)
{
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("DELETE FROM integrations WHERE id=? AND locked=0"));
    query.addBindValue(id);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

void Store::setPaused(const QString& id, bool paused)
{
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("UPDATE integrations SET paused=? WHERE id=?"));
    query.addBindValue(paused ? 1 : 0);
    query.addBindValue(id);
    query.exec();
}

void Store::addSample(const QString& moduleId, ModuleStatus status, qint64 latencyMs, int httpStatus, const QString& evidence)
{
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("INSERT INTO samples (module_id, ts, status, latency_ms, http_status, evidence) VALUES (?,?,?,?,?,?)"));
    query.addBindValue(moduleId);
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    query.addBindValue(statusKey(status));
    query.addBindValue(latencyMs);
    query.addBindValue(httpStatus);
    query.addBindValue(redactText(evidence));
    query.exec();
}

QVariantMap Store::latestSample(const QString& moduleId) const
{
    QVariantMap map;
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("SELECT ts, latency_ms, http_status, status, evidence FROM samples WHERE module_id=? ORDER BY ts DESC LIMIT 1"));
    query.addBindValue(moduleId);
    if (!query.exec() || !query.next()) return map;
    map.insert(QStringLiteral("found"), true);
    map.insert(QStringLiteral("ts"), query.value(0).toLongLong());
    map.insert(QStringLiteral("latencyMs"), query.value(1).toLongLong());
    map.insert(QStringLiteral("httpStatus"), query.value(2).toInt());
    map.insert(QStringLiteral("status"), query.value(3).toString());
    map.insert(QStringLiteral("evidence"), query.value(4).toString());
    return map;
}

QVariantMap Store::latestSampleMatching(const QString& moduleId, const QStringList& statuses) const
{
    QVariantMap map;
    if (statuses.isEmpty()) return map;
    QStringList marks;
    for (int index = 0; index < statuses.size(); ++index) marks << QStringLiteral("?");
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("SELECT ts, latency_ms, http_status, status, evidence FROM samples WHERE module_id=? AND status IN (") + marks.join(QLatin1Char(',')) + QStringLiteral(") ORDER BY ts DESC LIMIT 1"));
    query.addBindValue(moduleId);
    for (const QString& status : statuses) query.addBindValue(status);
    if (!query.exec() || !query.next()) return map;
    map.insert(QStringLiteral("found"), true);
    map.insert(QStringLiteral("ts"), query.value(0).toLongLong());
    map.insert(QStringLiteral("latencyMs"), query.value(1).toLongLong());
    map.insert(QStringLiteral("httpStatus"), query.value(2).toInt());
    map.insert(QStringLiteral("status"), query.value(3).toString());
    map.insert(QStringLiteral("evidence"), query.value(4).toString());
    return map;
}

QList<QPair<qint64, double>> Store::latencySeries(const QString& moduleId, const QDateTime& since) const
{
    QList<QPair<qint64, double>> series;
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("SELECT ts, latency_ms FROM samples WHERE module_id=? AND ts>=? AND latency_ms>=0 ORDER BY ts ASC"));
    query.addBindValue(moduleId);
    query.addBindValue(since.toMSecsSinceEpoch());
    if (!query.exec()) return series;
    while (query.next()) series.push_back({query.value(0).toLongLong(), query.value(1).toDouble()});
    return series;
}

int Store::sampleCount(const QString& moduleId, const QDateTime& since, int* badOut) const
{
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("SELECT status FROM samples WHERE module_id=? AND ts>=?"));
    query.addBindValue(moduleId);
    query.addBindValue(since.toMSecsSinceEpoch());
    int total = 0;
    int bad = 0;
    if (query.exec()) {
        while (query.next()) {
            const ModuleStatus status = statusFromKey(query.value(0).toString());
            if (!countsInHealthRatio(status)) continue;
            ++total;
            if (status != ModuleStatus::Healthy) ++bad;
        }
    }
    if (badOut) *badOut = bad;
    return total;
}

void Store::pruneSamples(const QDateTime& cutoff)
{
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("DELETE FROM samples WHERE ts<?"));
    query.addBindValue(cutoff.toMSecsSinceEpoch());
    query.exec();
}

void Store::saveIncident(const Incident& incident)
{
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("INSERT INTO incidents (id,module_id,application,module_name,service,kind,http_status,message,first_seen,last_seen,repeat_count,severity,log_source,cause_label,cause_text,recommendation,state,resolved_at) VALUES (?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?,?) ON CONFLICT(id) DO UPDATE SET message=excluded.message, last_seen=excluded.last_seen, repeat_count=excluded.repeat_count, severity=excluded.severity, cause_label=excluded.cause_label, cause_text=excluded.cause_text, state=excluded.state, resolved_at=excluded.resolved_at"));
    query.addBindValue(incident.id);
    query.addBindValue(incident.moduleId);
    query.addBindValue(incident.application);
    query.addBindValue(incident.moduleName);
    query.addBindValue(incident.service);
    query.addBindValue(incident.kind);
    query.addBindValue(incident.httpStatus);
    query.addBindValue(redactText(incident.message));
    query.addBindValue(incident.firstSeen.toMSecsSinceEpoch());
    query.addBindValue(incident.lastSeen.toMSecsSinceEpoch());
    query.addBindValue(incident.repeatCount);
    query.addBindValue(incident.severity);
    query.addBindValue(incident.logSource);
    query.addBindValue(incident.causeLabel);
    query.addBindValue(redactText(incident.causeText));
    query.addBindValue(incident.recommendation);
    query.addBindValue(incident.state);
    query.addBindValue(incident.resolvedAt.isValid() ? incident.resolvedAt.toMSecsSinceEpoch() : QVariant());
    query.exec();
}

QList<Incident> Store::incidents() const
{
    QList<Incident> list;
    QSqlQuery query(db_);
    query.exec(QStringLiteral("SELECT id,module_id,application,module_name,service,kind,http_status,message,first_seen,last_seen,repeat_count,severity,log_source,cause_label,cause_text,recommendation,state,resolved_at FROM incidents ORDER BY last_seen DESC"));
    while (query.next()) {
        Incident incident;
        incident.id = query.value(0).toString();
        incident.moduleId = query.value(1).toString();
        incident.application = query.value(2).toString();
        incident.moduleName = query.value(3).toString();
        incident.service = query.value(4).toString();
        incident.kind = query.value(5).toString();
        incident.httpStatus = query.value(6).toInt();
        incident.message = query.value(7).toString();
        incident.firstSeen = QDateTime::fromMSecsSinceEpoch(query.value(8).toLongLong());
        incident.lastSeen = QDateTime::fromMSecsSinceEpoch(query.value(9).toLongLong());
        incident.repeatCount = query.value(10).toInt();
        incident.severity = query.value(11).toString();
        incident.logSource = query.value(12).toString();
        incident.causeLabel = query.value(13).toString();
        incident.causeText = query.value(14).toString();
        incident.recommendation = query.value(15).toString();
        incident.state = query.value(16).toString();
        if (!query.value(17).isNull()) incident.resolvedAt = QDateTime::fromMSecsSinceEpoch(query.value(17).toLongLong());
        list.push_back(incident);
    }
    return list;
}

void Store::setSetting(const QString& key, const QString& value)
{
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("INSERT INTO settings (key,value) VALUES (?,?) ON CONFLICT(key) DO UPDATE SET value=excluded.value"));
    query.addBindValue(key);
    query.addBindValue(value);
    query.exec();
}

QString Store::setting(const QString& key, const QString& fallback) const
{
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("SELECT value FROM settings WHERE key=?"));
    query.addBindValue(key);
    if (query.exec() && query.next()) return query.value(0).toString();
    return fallback;
}

void Store::appendAudit(const QString& action, const QString& target, const QString& result)
{
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("INSERT INTO audit_log (ts, action, target, result) VALUES (?,?,?,?)"));
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    query.addBindValue(action.left(80));
    query.addBindValue(target.left(120));
    query.addBindValue(result.left(240));
    query.exec();
}

int Store::auditCount() const
{
    QSqlQuery query(db_);
    if (query.exec(QStringLiteral("SELECT COUNT(*) FROM audit_log")) && query.next()) return query.value(0).toInt();
    return 0;
}

QList<ProbeSample> Store::probeSamples(const QDateTime& from, const QDateTime& to, const QString& moduleId) const
{
    QList<ProbeSample> rows;
    QSqlQuery query(db_);
    if (moduleId.isEmpty()) {
        query.prepare(QStringLiteral("SELECT module_id, status, latency_ms FROM samples WHERE ts>=? AND ts<? ORDER BY ts ASC"));
        query.addBindValue(from.toMSecsSinceEpoch());
        query.addBindValue(to.toMSecsSinceEpoch());
    } else {
        query.prepare(QStringLiteral("SELECT module_id, status, latency_ms FROM samples WHERE module_id=? AND ts>=? AND ts<? ORDER BY ts ASC"));
        query.addBindValue(moduleId);
        query.addBindValue(from.toMSecsSinceEpoch());
        query.addBindValue(to.toMSecsSinceEpoch());
    }
    if (!query.exec()) return rows;
    while (query.next()) {
        ProbeSample row;
        row.moduleId = query.value(0).toString();
        row.status = query.value(1).toString();
        row.latencyMs = query.value(2).toLongLong();
        rows.push_back(row);
    }
    return rows;
}

bool Store::insertSavedReport(const QString& id, const QString& name, const QString& kind, const QString& period, const QString& system, const QString& format, const QString& status, const QString& path, const QString& summary, QString* error)
{
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("INSERT INTO saved_reports (id,name,kind,created_at,period,system,format,status,path,summary) VALUES (?,?,?,?,?,?,?,?,?,?)"));
    query.addBindValue(id);
    query.addBindValue(name.left(120));
    query.addBindValue(kind.left(40));
    query.addBindValue(QDateTime::currentMSecsSinceEpoch());
    query.addBindValue(period.left(80));
    query.addBindValue(system.left(80));
    query.addBindValue(format.left(12));
    query.addBindValue(status.left(40));
    query.addBindValue(path.left(500));
    query.addBindValue(summary.left(4000));
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return true;
}

namespace {
QVariantMap readSavedRow(QSqlQuery& query)
{
    return {
        {QStringLiteral("id"), query.value(0).toString()},
        {QStringLiteral("name"), query.value(1).toString()},
        {QStringLiteral("kind"), query.value(2).toString()},
        {QStringLiteral("created"), QDateTime::fromMSecsSinceEpoch(query.value(3).toLongLong()).toString(QStringLiteral("dd.MM.yyyy HH:mm"))},
        {QStringLiteral("period"), query.value(4).toString()},
        {QStringLiteral("system"), query.value(5).toString()},
        {QStringLiteral("format"), query.value(6).toString()},
        {QStringLiteral("status"), query.value(7).toString()},
        {QStringLiteral("path"), query.value(8).toString()},
        {QStringLiteral("summary"), query.value(9).toString()}
    };
}
}

QList<QVariantMap> Store::savedReports() const
{
    QList<QVariantMap> rows;
    QSqlQuery query(db_);
    if (!query.exec(QStringLiteral("SELECT id,name,kind,created_at,period,system,format,status,path,summary FROM saved_reports ORDER BY created_at DESC"))) return rows;
    while (query.next()) rows.push_back(readSavedRow(query));
    return rows;
}

QVariantMap Store::savedReport(const QString& id) const
{
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("SELECT id,name,kind,created_at,period,system,format,status,path,summary FROM saved_reports WHERE id=?"));
    query.addBindValue(id);
    if (query.exec() && query.next()) return readSavedRow(query);
    return {};
}

bool Store::removeSavedReport(const QString& id, QString* error)
{
    QSqlQuery query(db_);
    query.prepare(QStringLiteral("DELETE FROM saved_reports WHERE id=?"));
    query.addBindValue(id);
    if (!query.exec()) {
        if (error) *error = query.lastError().text();
        return false;
    }
    return query.numRowsAffected() > 0;
}

} // namespace tg
