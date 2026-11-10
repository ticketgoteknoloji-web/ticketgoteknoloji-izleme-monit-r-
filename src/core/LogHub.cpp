#include "LogHub.h"

#include "OpsTypes.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>
#include <QSet>
#include <QUrlQuery>

namespace tg {
namespace {

LogSource source(const QString& id, const QString& name, const QString& hubModule, const QString& hubType, bool hubBacked, const QString& reason)
{
    LogSource item;
    item.id = id;
    item.name = name;
    item.hubModule = hubModule;
    item.hubType = hubType;
    item.hubBacked = hubBacked;
    item.reason = reason;
    item.levels = hubBacked ? QStringLiteral("DEBUG INFO WARN ERROR CRITICAL") : QStringLiteral("—");
    if (!hubBacked) item.access = QStringLiteral("unavailable");
    return item;
}

QString field(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    if (value.isString()) return value.toString();
    if (value.isDouble()) return QString::number(value.toInt());
    return {};
}

} // namespace

QList<LogSource> discoveredLogSources()
{
    const QString hub = QStringLiteral("System Logs Hub kaydı. Okuma logs.read ister.");
    const QString missing = QStringLiteral("System Logs Hub kataloğunda ayrı kayıt yok. /health log döndürmez.");
    return {
        source(QStringLiteral("system"), QStringLiteral("Backend Ana Servis"), QStringLiteral("system"), {}, true, hub),
        source(QStringLiteral("auth"), QStringLiteral("Authentication / JWT"), QStringLiteral("auth"), {}, true, hub),
        source(QStringLiteral("mail"), QStringLiteral("MFA E-Posta OTP"), {}, QStringLiteral("mail"), true, hub),
        source(QStringLiteral("api"), QStringLiteral("Admin Panel API"), QStringLiteral("api"), {}, true, hub),
        source(QStringLiteral("postgres"), QStringLiteral("PostgreSQL"), {}, {}, false, missing),
        source(QStringLiteral("redis"), QStringLiteral("Redis"), {}, {}, false, missing),
        source(QStringLiteral("queue"), QStringLiteral("Worker Servisleri"), QStringLiteral("queue"), {}, true, hub),
        source(QStringLiteral("scheduler"), QStringLiteral("Zamanlanmış Görevler"), {}, QStringLiteral("scheduler"), true, hub),
        source(QStringLiteral("nginx"), QStringLiteral("Nginx"), {}, {}, false, missing),
        source(QStringLiteral("docker"), QStringLiteral("Docker"), {}, {}, false, missing),
        source(QStringLiteral("payments"), QStringLiteral("İyzico Ödeme"), QStringLiteral("payments"), QStringLiteral("payment"), true, hub),
        source(QStringLiteral("liknoss"), QStringLiteral("Feribot Bilet Satış"), QStringLiteral("liknoss"), QStringLiteral("liknoss"), true, hub),
        source(QStringLiteral("accounting"), QStringLiteral("Go Muhasebe"), QStringLiteral("accounting"), {}, true, hub),
        source(QStringLiteral("turmob"), QStringLiteral("TÜRMOB Gelen e-Fatura"), {}, {}, false, missing),
        source(QStringLiteral("luca"), QStringLiteral("LUCA"), {}, {}, false, missing),
        source(QStringLiteral("sms"), QStringLiteral("NetGSM SMS"), {}, QStringLiteral("sms"), true, hub),
        source(QStringLiteral("gochat"), QStringLiteral("Go-Chat API"), {}, {}, false, QStringLiteral("Mesaj içeriği okunmaz. Ayrı Go-Chat log ucu yok.")),
        source(QStringLiteral("gochat-ws"), QStringLiteral("Go-Chat WebSocket"), {}, {}, false, missing),
        source(QStringLiteral("push"), QStringLiteral("FCM Bildirimleri"), {}, QStringLiteral("push"), true, hub),
        source(QStringLiteral("turn"), QStringLiteral("TURN / Arama Servisleri"), {}, {}, false, missing),
        source(QStringLiteral("sos"), QStringLiteral("SOS Acil Durum"), {}, {}, false, missing),
        source(QStringLiteral("audit"), QStringLiteral("Audit ve Güvenlik Logları"), {}, QStringLiteral("audit"), true, hub),
        source(QStringLiteral("tickets"), QStringLiteral("Bilet kayıtları"), QStringLiteral("tickets"), {}, true, hub),
        source(QStringLiteral("voyages"), QStringLiteral("Seferler"), QStringLiteral("voyages"), {}, true, hub),
        source(QStringLiteral("mobile"), QStringLiteral("Mobil API"), QStringLiteral("mobile"), {}, true, hub),
        source(QStringLiteral("b2b"), QStringLiteral("B2B API"), QStringLiteral("b2b"), {}, true, hub),
        source(QStringLiteral("local-file"), QStringLiteral("Yerel dosya"), {}, {}, false, QStringLiteral("Seçilen yerel dosya yok."))
    };
}

QList<HubLogSection> hubLogSections()
{
    const auto section = [](const QString& code, const QString& label, const QString& logType, const QString& level) {
        HubLogSection item;
        item.code = code;
        item.label = label;
        item.logType = logType;
        item.level = level;
        return item;
    };
    return {
        section(QStringLiteral("all"), QStringLiteral("Sistem Logları"), {}, {}),
        section(QStringLiteral("audit"), QStringLiteral("Audit Log"), QStringLiteral("audit"), {}),
        section(QStringLiteral("api"), QStringLiteral("API Logları"), QStringLiteral("api"), {}),
        section(QStringLiteral("user"), QStringLiteral("Kullanıcı İşlem Logları"), QStringLiteral("login"), {}),
        section(QStringLiteral("security"), QStringLiteral("Güvenlik Logları"), QStringLiteral("rbac"), {}),
        section(QStringLiteral("notification"), QStringLiteral("Bildirim Logları"), QStringLiteral("notification"), {}),
        section(QStringLiteral("sms"), QStringLiteral("SMS Logları"), QStringLiteral("sms"), {}),
        section(QStringLiteral("mail"), QStringLiteral("E-Posta Logları"), QStringLiteral("mail"), {}),
        section(QStringLiteral("liknoss"), QStringLiteral("Liknoss Logları"), QStringLiteral("liknoss"), {}),
        section(QStringLiteral("mobile"), QStringLiteral("Mobil API Logları"), QStringLiteral("mobile_api"), {}),
        section(QStringLiteral("b2b"), QStringLiteral("B2B API Logları"), QStringLiteral("b2b_api"), {}),
        section(QStringLiteral("accounting"), QStringLiteral("Muhasebe Logları"), QStringLiteral("accounting"), {}),
        section(QStringLiteral("queue"), QStringLiteral("Arka Plan Görevleri"), QStringLiteral("queue"), {}),
        section(QStringLiteral("errors"), QStringLiteral("Hata Kayıtları"), {}, QStringLiteral("error"))
    };
}

int logPollIntervalMs(const QString& secondsText)
{
    bool ok = false;
    int seconds = secondsText.trimmed().toInt(&ok);
    if (!ok) seconds = 5;
    return qBound(5, seconds, 60) * 1000;
}

namespace {

QString safeToken(const QString& value, int limit)
{
    QString out;
    for (const QChar character : value.left(limit)) {
        if (character.isLetterOrNumber() || character == QLatin1Char('-') || character == QLatin1Char('_') || character == QLatin1Char('.') || character == QLatin1Char('@') || character == QLatin1Char('/') || character == QLatin1Char(':')) {
            out += character;
        }
    }
    return out;
}

bool isoDay(const QString& value)
{
    static const QRegularExpression pattern(QStringLiteral("^\\d{4}-\\d{2}-\\d{2}(?:[T ]\\d{2}:\\d{2}:\\d{2}(?:\\.\\d{1,3})?Z?)?$"));
    return pattern.match(value.trimmed()).hasMatch();
}

} // namespace

QUrl systemLogRequestUrl(const LogQuery& query, bool live, const QString& since)
{
    QUrl url(QStringLiteral("https://api.ticket-go.net/api/v1/system-logs"));
    QUrlQuery items;
    if (live) {
        url.setPath(QStringLiteral("/api/v1/system-logs/live"));
        if (isoDay(since)) items.addQueryItem(QStringLiteral("since"), since.trimmed());
        items.addQueryItem(QStringLiteral("limit"), QStringLiteral("20"));
        url.setQuery(items);
        return url;
    }
    items.addQueryItem(QStringLiteral("page"), QString::number(qMax(1, query.page)));
    items.addQueryItem(QStringLiteral("pageSize"), QStringLiteral("50"));
    QString level = query.level.trimmed().toLower();
    if (level == QLatin1String("warn")) level = QStringLiteral("warning");
    QString logType = query.logType.trimmed();
    for (const HubLogSection& section : hubLogSections()) {
        if (section.code != query.sourceId) continue;
        if (logType.isEmpty()) logType = section.logType;
        if (level.isEmpty()) level = section.level;
    }
    if (level == QLatin1String("debug") || level == QLatin1String("info") || level == QLatin1String("warning") || level == QLatin1String("error") || level == QLatin1String("critical")) {
        items.addQueryItem(QStringLiteral("level"), level);
    }
    if (!logType.isEmpty()) items.addQueryItem(QStringLiteral("logType"), safeToken(logType, 40));
    if (!query.sourceId.isEmpty() && query.sourceId != QLatin1String("all") && query.sourceId != QLatin1String("local-file")) {
        bool section = false;
        for (const HubLogSection& item : hubLogSections()) if (item.code == query.sourceId) section = true;
        if (!section) items.addQueryItem(QStringLiteral("module"), safeToken(query.sourceId, 40));
    }
    QString text = query.text.trimmed().left(80);
    text.remove(QRegularExpression(QStringLiteral("[\\r\\n&=]")));
    if (!text.isEmpty()) items.addQueryItem(QStringLiteral("q"), text);
    if (query.httpCode.size() >= 3 && query.httpCode.left(3).toInt() >= 100) items.addQueryItem(QStringLiteral("statusCode"), query.httpCode.left(3));
    const QString errorCode = safeToken(query.errorCode, 40);
    if (!errorCode.isEmpty()) items.addQueryItem(QStringLiteral("errorCode"), errorCode);
    const QString from = isoDay(query.dateFrom) ? query.dateFrom.trimmed() : logDateFrom(query.range, QDateTime::currentDateTimeUtc());
    if (!from.isEmpty()) items.addQueryItem(QStringLiteral("dateFrom"), from);
    if (isoDay(query.dateTo)) items.addQueryItem(QStringLiteral("dateTo"), query.dateTo.trimmed());
    const QString user = safeToken(query.user, 80);
    if (!user.isEmpty()) items.addQueryItem(QStringLiteral("userEmail"), user);
    const QString ip = safeToken(query.ip, 45);
    if (!ip.isEmpty()) items.addQueryItem(QStringLiteral("ipAddress"), ip);
    if (query.endpoint.startsWith(QLatin1Char('/')) && !query.endpoint.contains(QLatin1Char(' '))) {
        items.addQueryItem(QStringLiteral("endpoint"), query.endpoint.left(120));
    }
    const QString outcome = query.outcome.trimmed().toLower();
    if (outcome == QLatin1String("success") || outcome == QLatin1String("failed") || outcome == QLatin1String("pending")) {
        items.addQueryItem(QStringLiteral("status"), outcome);
    }
    const QString action = safeToken(query.action, 80);
    if (!action.isEmpty()) items.addQueryItem(QStringLiteral("action"), action);
    url.setQuery(items);
    return url;
}

QString normalizeLogLevel(const QString& raw)
{
    const QString level = raw.trimmed().toLower();
    if (level == QLatin1String("debug") || level == QLatin1String("trace") || level == QLatin1String("verbose")) return QStringLiteral("DEBUG");
    if (level == QLatin1String("info") || level == QLatin1String("information") || level == QLatin1String("notice")) return QStringLiteral("INFO");
    if (level == QLatin1String("warn") || level == QLatin1String("warning")) return QStringLiteral("WARN");
    if (level == QLatin1String("error") || level == QLatin1String("err")) return QStringLiteral("ERROR");
    if (level == QLatin1String("critical") || level == QLatin1String("fatal") || level == QLatin1String("crit")) return QStringLiteral("CRITICAL");
    return QStringLiteral("INFO");
}

QList<LogRecord> parseLocalLog(const QString& text)
{
    QList<LogRecord> records;
    const QStringList lines = text.split(QLatin1Char('\n'));
    int index = 0;
    for (const QString& line : lines) {
        const QString trimmed = line.trimmed();
        if (trimmed.isEmpty()) continue;
        LogRecord row;
        row.id = QStringLiteral("local-%1").arg(index++);
        row.application = QStringLiteral("Yerel dosya");
        row.sourceId = QStringLiteral("local-file");
        row.module = QStringLiteral("local-file");
        row.service = QStringLiteral("dosya");
        const QJsonDocument document = QJsonDocument::fromJson(trimmed.toUtf8());
        if (document.isObject()) {
            const QJsonObject object = document.object();
            row.level = normalizeLogLevel(field(object, QStringLiteral("level")));
            row.time = field(object, QStringLiteral("timestamp"));
            if (row.time.isEmpty()) row.time = field(object, QStringLiteral("time"));
            row.message = redactLog(field(object, QStringLiteral("message")).isEmpty() ? trimmed : field(object, QStringLiteral("message")));
            row.module = field(object, QStringLiteral("module"));
            if (!row.module.isEmpty()) row.sourceId = row.module;
            row.errorCode = field(object, QStringLiteral("errorCode"));
            row.httpCode = field(object, QStringLiteral("statusCode"));
            row.requestId = field(object, QStringLiteral("requestId"));
            row.correlationId = field(object, QStringLiteral("correlationId"));
            row.endpoint = field(object, QStringLiteral("endpoint"));
            row.stack = redactLog(field(object, QStringLiteral("stack")));
        } else {
            static const QRegularExpression levelRe(QStringLiteral("\\b(DEBUG|INFO|WARN(?:ING)?|ERROR|CRITICAL|FATAL)\\b"), QRegularExpression::CaseInsensitiveOption);
            const QRegularExpressionMatch match = levelRe.match(trimmed);
            row.level = match.hasMatch() ? normalizeLogLevel(match.captured(1)) : QStringLiteral("INFO");
            static const QRegularExpression timeRe(QStringLiteral("\\d{4}-\\d{2}-\\d{2}[T ]\\d{2}:\\d{2}:\\d{2}"));
            const QRegularExpressionMatch time = timeRe.match(trimmed);
            row.time = time.hasMatch() ? time.captured(0) : QString();
            row.message = redactLog(trimmed);
        }
        if (row.module.contains(QStringLiteral("gochat"), Qt::CaseInsensitive) || row.message.contains(QStringLiteral("gochat"), Qt::CaseInsensitive)) {
            row.message = QStringLiteral("[Go-Chat mesaj içeriği gösterilmez]");
            row.detail.clear();
            row.stack.clear();
        }
        records.push_back(row);
        if (records.size() >= 200) break;
    }
    return records;
}

QList<LogRecord> parseHubLogItems(const QJsonArray& items)
{
    QList<LogRecord> records;
    for (const QJsonValue& value : items) {
        if (!value.isObject()) continue;
        const QJsonObject object = value.toObject();
        LogRecord row;
        row.id = field(object, QStringLiteral("id"));
        row.eventId = field(object, QStringLiteral("logNo"));
        row.level = normalizeLogLevel(field(object, QStringLiteral("level")));
        row.module = field(object, QStringLiteral("module"));
        row.service = field(object, QStringLiteral("logType"));
        row.sourceId = row.module.isEmpty() ? row.service : row.module;
        row.application = QStringLiteral("Ticket-Go");
        row.time = field(object, QStringLiteral("createdAt"));
        row.date = field(object, QStringLiteral("date"));
        row.clock = field(object, QStringLiteral("time"));
        if (row.time.isEmpty()) row.time = row.date + QStringLiteral(" ") + row.clock;
        if (row.date.isEmpty() && row.time.size() >= 10) row.date = row.time.left(10);
        if (row.clock.isEmpty() && row.time.size() >= 19) row.clock = row.time.mid(11, 8);
        row.action = field(object, QStringLiteral("action"));
        row.message = redactLog(field(object, QStringLiteral("message")));
        row.httpCode = field(object, QStringLiteral("statusCode"));
        row.errorCode = field(object, QStringLiteral("errorCode"));
        row.endpoint = field(object, QStringLiteral("endpoint"));
        row.user = redactLog(field(object, QStringLiteral("userEmail")));
        row.ip = field(object, QStringLiteral("ipAddress")).left(45);
        row.duration = field(object, QStringLiteral("durationMs"));
        row.outcome = field(object, QStringLiteral("status"));
        row.method = field(object, QStringLiteral("httpMethod"));
        const QJsonObject meta = object.value(QStringLiteral("metaJson")).toObject();
        row.requestId = field(meta, QStringLiteral("requestId"));
        row.correlationId = field(meta, QStringLiteral("correlationId"));
        row.stack = redactLog(field(object, QStringLiteral("stackTrace")));
        row.detail = redactLog(field(object, QStringLiteral("exception")));
        if (row.service == QLatin1String("sms") || row.service == QLatin1String("mail") || row.service == QLatin1String("push") || row.service == QLatin1String("audit") || row.service == QLatin1String("scheduler") || row.service == QLatin1String("payment") || row.service == QLatin1String("liknoss")) {
            row.sourceId = row.service == QLatin1String("payment") ? QStringLiteral("payments") : row.service;
        }
        records.push_back(row);
        if (records.size() >= 80) break;
    }
    return records;
}

QString logDateFrom(const QString& range, const QDateTime& now)
{
    QDateTime start = now;
    if (range == QLatin1String("5m")) start = now.addSecs(-300);
    else if (range == QLatin1String("15m")) start = now.addSecs(-900);
    else if (range == QLatin1String("1h")) start = now.addSecs(-3600);
    else if (range == QLatin1String("24h")) start = now.addSecs(-86400);
    else if (range == QLatin1String("7d")) start = now.addDays(-7);
    else return {};
    return start.toUTC().toString(Qt::ISODate);
}

QList<LogRecord> filterLogRecords(const QList<LogRecord>& records, const LogQuery& query)
{
    QList<LogRecord> filtered;
    const QString level = query.level.trimmed().toUpper();
    for (const LogRecord& row : records) {
        if (!query.sourceId.isEmpty() && query.sourceId != QLatin1String("all") && row.sourceId != query.sourceId && row.module != query.sourceId && row.service != query.sourceId) continue;
        if (!level.isEmpty() && level != QLatin1String("TÜMÜ") && level != QLatin1String("ALL") && row.level != level && !(level == QLatin1String("WARN") && row.level == QLatin1String("WARN"))) continue;
        if (!query.httpCode.isEmpty() && row.httpCode != query.httpCode) continue;
        if (!query.errorCode.isEmpty() && !row.errorCode.contains(query.errorCode, Qt::CaseInsensitive)) continue;
        if (!query.requestId.isEmpty() && !row.requestId.contains(query.requestId, Qt::CaseInsensitive) && !row.correlationId.contains(query.requestId, Qt::CaseInsensitive)) continue;
        if (!query.service.isEmpty() && !row.service.contains(query.service, Qt::CaseInsensitive)) continue;
        if (!query.logType.isEmpty() && row.service != query.logType) continue;
        if (!query.user.isEmpty() && !row.user.contains(query.user, Qt::CaseInsensitive)) continue;
        if (!query.ip.isEmpty() && !row.ip.contains(query.ip)) continue;
        if (!query.endpoint.isEmpty() && !row.endpoint.contains(query.endpoint, Qt::CaseInsensitive)) continue;
        if (!query.outcome.isEmpty() && row.outcome.compare(query.outcome, Qt::CaseInsensitive) != 0) continue;
        if (!query.action.isEmpty() && !row.action.contains(query.action, Qt::CaseInsensitive)) continue;
        if (!query.text.isEmpty() && !row.message.contains(query.text, Qt::CaseInsensitive) && !row.endpoint.contains(query.text, Qt::CaseInsensitive) && !row.module.contains(query.text, Qt::CaseInsensitive)) continue;
        filtered.push_back(row);
    }
    return filtered;
}

QString analyzeLogRecord(const LogRecord& row, const QList<LogRecord>& page)
{
    if (row.level != QLatin1String("ERROR") && row.level != QLatin1String("CRITICAL")) {
        return QStringLiteral("Bu kayıt hata değil. Seviye ") + row.level + QStringLiteral(".");
    }
    int repeats = 0;
    int sameRequest = 0;
    QString first = row.time;
    for (const LogRecord& other : page) {
        if (other.module == row.module && (other.level == QLatin1String("ERROR") || other.level == QLatin1String("CRITICAL"))) {
            ++repeats;
            if (!other.time.isEmpty() && (first.isEmpty() || other.time < first)) first = other.time;
        }
        if (!row.requestId.isEmpty() && other.requestId == row.requestId && other.id != row.id) ++sameRequest;
    }
    QStringList lines;
    lines << QStringLiteral("Modül: ") + (row.module.isEmpty() ? QStringLiteral("bilinmiyor") : row.module);
    lines << QStringLiteral("İlk görülen (bu sayfa): ") + (first.isEmpty() ? QStringLiteral("zaman yok") : first);
    lines << QStringLiteral("Bu sayfada tekrar: ") + QString::number(repeats);
    lines << QStringLiteral("Endpoint: ") + (row.endpoint.isEmpty() ? QStringLiteral("kayıtta yok") : row.endpoint);
    lines << QStringLiteral("Aynı istek kimliğiyle başka kayıt: ") + QString::number(sameRequest);
    if (row.stack.isEmpty() && row.errorCode.isEmpty() && row.detail.isEmpty()) {
        lines << QStringLiteral("Kök neden henüz doğrulanamadı");
    } else {
        lines << QStringLiteral("Kanıt: ") + (row.errorCode.isEmpty() ? row.message.left(180) : row.errorCode);
        lines << QStringLiteral("Kontrol: ilgili modülün sağlık durumu ve bu kaydın HTTP kodu.");
    }
    return lines.join(QLatin1Char('\n'));
}

QString logReadFailureNote(bool tokenPresent, bool transportOk, int status, const QString& errorCode, const QString& errorClass)
{
    QString code;
    const QString trimmed = errorCode.trimmed();
    if (trimmed.size() >= 2 && trimmed.size() <= 40) {
        bool safe = true;
        for (const QChar character : trimmed) {
            if (!character.isLetterOrNumber() && character != QLatin1Char('_') && character != QLatin1Char('-')) safe = false;
        }
        if (safe) code = trimmed;
    }
    const QString suffix = code.isEmpty() ? QString() : QStringLiteral(" Kod: ") + code;
    if (!tokenPresent) {
        return QStringLiteral("Log veri kaynağına erişilemiyor. Güvenli oturum açın. Kimliksiz istek gönderilmedi.");
    }
    if (errorClass == QLatin1String("timeout")) return QStringLiteral("Log sunucusundan yanıt alınamadı.");
    if (!transportOk || status <= 0) return QStringLiteral("Log veri kaynağına erişilemiyor.");
    if (status == 401) return QStringLiteral("Oturum doğrulanamadı.") + suffix;
    if (status == 403) return QStringLiteral("İzleme hesabınızın Sistem Logları okuma yetkisi yok.") + suffix;
    if (status == 404) return QStringLiteral("Sistem Logları API bulunamadı.");
    if (status >= 500) return QStringLiteral("Backend log servisi hata verdi.") + suffix;
    return QStringLiteral("System Logs Hub yanıtı alınamadı. HTTP ") + QString::number(status) + suffix;
}

AuthEnvelope readAuthEnvelope(int status, const QJsonObject& json)
{
    AuthEnvelope envelope;
    const QJsonObject error = json.value(QStringLiteral("error")).toObject();
    envelope.code = error.value(QStringLiteral("code")).toString();
    if (envelope.code.size() > 40) envelope.code.clear();
    envelope.message = error.value(QStringLiteral("message")).toString().left(160);
    QJsonObject data = json.value(QStringLiteral("data")).toObject();
    if (data.value(QStringLiteral("data")).isObject() && !data.contains(QStringLiteral("accessToken"))) data = data.value(QStringLiteral("data")).toObject();
    envelope.mfaRequired = data.value(QStringLiteral("mfaRequired")).toBool() || data.value(QStringLiteral("mfaEnrollRequired")).toBool() || data.contains(QStringLiteral("mfaChallenge"));
    envelope.accessToken = data.value(QStringLiteral("accessToken")).toString();
    envelope.refreshToken = data.value(QStringLiteral("refreshToken")).toString();
    envelope.challengeToken = data.value(QStringLiteral("challengeToken")).toString();
    envelope.mfaMethod = data.value(QStringLiteral("mfaMethod")).toString().left(32);
    envelope.maskedEmail = data.value(QStringLiteral("maskedEmail")).toString().left(80);
    const QJsonObject user = data.value(QStringLiteral("user")).toObject();
    envelope.account = user.value(QStringLiteral("username")).toString();
    if (envelope.account.isEmpty()) envelope.account = user.value(QStringLiteral("email")).toString();
    envelope.account = envelope.account.left(320);
    if (status == 401 || status == 403 || envelope.mfaRequired) {
        envelope.accessToken.clear();
        envelope.refreshToken.clear();
    }
    return envelope;
}

SessionDecision decideAuthResult(bool transportOk, int status, const AuthEnvelope& envelope)
{
    SessionDecision decision;
    if (!transportOk || status <= 0) {
        decision.phase = QStringLiteral("error");
        decision.message = QStringLiteral("Sunucuya erişilemiyor.");
        return decision;
    }
    if (envelope.mfaRequired) {
        decision.phase = envelope.challengeToken.size() >= 20 ? QStringLiteral("mfa") : QStringLiteral("error");
        decision.message = decision.phase == QLatin1String("mfa")
            ? QStringLiteral("E-posta doğrulama kodu gerekli. Parola saklanmadı.")
            : QStringLiteral("Doğrulama adımı eksik. Oturum açılmadı.");
        return decision;
    }
    if (status == 200 && envelope.accessToken.size() >= 20) {
        decision.phase = QStringLiteral("open");
        decision.message = QStringLiteral("Oturum açıldı. Loglar isteniyor.");
        decision.storeSession = true;
        return decision;
    }
    decision.phase = QStringLiteral("error");
    if (status == 401 || status == 403) decision.message = QStringLiteral("Oturum açılmadı. Hesap doğrulanamadı.");
    else if (!envelope.message.isEmpty() && envelope.message.size() < 160 && !envelope.message.contains(QStringLiteral("eyJ"))) decision.message = envelope.message;
    else decision.message = QStringLiteral("Oturum açılmadı. HTTP ") + QString::number(status);
    return decision;
}

bool shouldRefreshLogSession(int status, bool hasRefresh, bool alreadyTried)
{
    return status == 401 && hasRefresh && !alreadyTried;
}

bool adoptPreviousLogSession()
{
    return false;
}

QString logSessionPhaseAfterResult(const QString& phase, int status, bool transportOk, bool refreshWillRun)
{
    if (phase != QLatin1String("open")) return phase;
    if (!transportOk || status <= 0) return phase;
    if (status == 401 && refreshWillRun) return phase;
    if (status == 401) return QStringLiteral("none");
    return phase;
}

QString emptyLogResultNote()
{
    return QStringLiteral("Kayıt bulunamadı");
}

void applySourceCounts(QList<LogSource>& sources, const QList<LogRecord>& records, bool hubConnected, const QString& unavailableReason)
{
    for (LogSource& source : sources) {
        if (!source.hubBacked) {
            if (source.id == QLatin1String("local-file")) continue;
            source.access = QStringLiteral("unavailable");
            source.total = -1;
            source.infos = -1;
            source.errors = -1;
            source.warnings = -1;
            source.criticals = -1;
            continue;
        }
        if (!hubConnected) {
            source.access = QStringLiteral("unavailable");
            source.reason = unavailableReason.isEmpty()
                ? QStringLiteral("Oturum doğrulanamadı.")
                : unavailableReason;
            source.total = -1;
            source.infos = -1;
            source.errors = -1;
            source.warnings = -1;
            source.criticals = -1;
            source.lastTime.clear();
            continue;
        }
        int total = 0;
        int infos = 0;
        int errors = 0;
        int warnings = 0;
        int criticals = 0;
        QString last;
        for (const LogRecord& row : records) {
            const bool match = row.sourceId == source.id || row.module == source.hubModule || (!source.hubType.isEmpty() && row.service == source.hubType);
            if (!match) continue;
            ++total;
            if (row.level == QLatin1String("INFO")) ++infos;
            if (row.level == QLatin1String("ERROR")) ++errors;
            if (row.level == QLatin1String("CRITICAL")) ++criticals;
            if (row.level == QLatin1String("WARN")) ++warnings;
            if (last.isEmpty() || row.time > last) last = row.time;
        }
        source.access = QStringLiteral("connected");
        source.total = total;
        source.infos = infos;
        source.errors = errors;
        source.warnings = warnings;
        source.criticals = criticals;
        source.lastTime = last;
        source.reason = total == 0 ? QStringLiteral("Bağlı. Bu sayfada kayıt yok.") : QStringLiteral("Bağlı. Sayılar getirilen sayfaya aittir.");
    }
}

QList<LogRecord> mergeLogRecords(const QList<LogRecord>& previous, const QList<LogRecord>& incoming, int cap, int* freshOut)
{
    QSet<QString> previousIds;
    for (const LogRecord& row : previous) previousIds.insert(row.id);
    QSet<QString> seen;
    QList<LogRecord> merged;
    int fresh = 0;
    const auto append = [&](LogRecord row) {
        if (row.id.isEmpty()) row.id = row.time + QLatin1Char('|') + row.message.left(80);
        if (seen.contains(row.id)) return;
        seen.insert(row.id);
        if (!previousIds.contains(row.id)) ++fresh;
        merged.push_back(row);
    };
    for (const LogRecord& row : incoming) {
        if (merged.size() >= cap) break;
        append(row);
    }
    for (const LogRecord& row : previous) {
        if (merged.size() >= cap) break;
        append(row);
    }
    if (freshOut) *freshOut = fresh;
    return merged;
}

QList<LogRecord> omitHiddenLogs(const QList<LogRecord>& rows, const QStringList& hiddenIds)
{
    if (hiddenIds.isEmpty()) return rows;
    QList<LogRecord> visible;
    for (const LogRecord& row : rows) {
        if (!hiddenIds.contains(row.id)) visible.push_back(row);
    }
    return visible;
}

} // namespace tg
