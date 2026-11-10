#include "OpsTypes.h"

#include <QHostAddress>
#include <QJsonValue>
#include <QRegularExpression>
#include <QUrl>

namespace tg {
namespace {

bool isMetadata(const QHostAddress& address)
{
    if (address.protocol() == QAbstractSocket::IPv4Protocol) {
        const quint32 ip = address.toIPv4Address();
        if (ip == 0xA9FEA9FE) {
            return true;
        }
    }
    const QString text = address.toString().toLower();
    return text == QStringLiteral("169.254.169.254") || text.contains(QStringLiteral("metadata.google"));
}

bool isPrivateAddress(const QHostAddress& address)
{
    if (address.isLoopback() || address.isLinkLocal() || address.isMulticast() || address.isBroadcast()) {
        return true;
    }
    if (address.protocol() == QAbstractSocket::IPv4Protocol) {
        const quint32 ip = address.toIPv4Address();
        const quint32 a = (ip >> 24) & 0xff;
        const quint32 b = (ip >> 16) & 0xff;
        if (a == 10 || a == 127 || a == 0) {
            return true;
        }
        if (a == 172 && b >= 16 && b <= 31) {
            return true;
        }
        if (a == 192 && b == 168) {
            return true;
        }
        if (a == 169 && b == 254) {
            return true;
        }
        if (a == 100 && b >= 64 && b <= 127) {
            return true;
        }
    }
    if (address.protocol() == QAbstractSocket::IPv6Protocol) {
        const QString t = address.toString().toLower();
        if (t.startsWith(QStringLiteral("fc")) || t.startsWith(QStringLiteral("fd")) || t.startsWith(QStringLiteral("fe80")) || t == QStringLiteral("::") || t == QStringLiteral("::1")) {
            return true;
        }
    }
    return false;
}

void maskE164(QString& text)
{
    for (int i = 0; i + 12 < text.size(); ++i) {
        if (text.at(i) != QLatin1Char('+')) {
            continue;
        }
        int j = i + 1;
        int digits = 0;
        while (j < text.size() && text.at(j).isDigit()) {
            ++digits;
            ++j;
        }
        if (digits >= 10 && digits <= 15) {
            for (int k = i + 4; k < j - 2; ++k) {
                text[k] = QLatin1Char('*');
            }
            i = j;
        }
    }
}

} // namespace

QString statusKey(ModuleStatus status)
{
    switch (status) {
    case ModuleStatus::Checking: return QStringLiteral("checking");
    case ModuleStatus::Healthy: return QStringLiteral("healthy");
    case ModuleStatus::Warning: return QStringLiteral("warning");
    case ModuleStatus::Critical: return QStringLiteral("critical");
    case ModuleStatus::Limited: return QStringLiteral("limited");
    case ModuleStatus::Unknown: return QStringLiteral("unknown");
    case ModuleStatus::NotConfigured: return QStringLiteral("not_configured");
    case ModuleStatus::Disabled: return QStringLiteral("disabled");
    case ModuleStatus::NotReleased: return QStringLiteral("not_released");
    case ModuleStatus::ConfigMissing: return QStringLiteral("config_missing");
    case ModuleStatus::ConnectionError: return QStringLiteral("connection_error");
    case ModuleStatus::Unsupported: return QStringLiteral("unsupported");
    case ModuleStatus::AuthRejected: return QStringLiteral("auth_rejected");
    }
    return QStringLiteral("unknown");
}

ModuleStatus statusFromKey(const QString& key)
{
    if (key == QLatin1String("checking")) return ModuleStatus::Checking;
    if (key == QLatin1String("healthy")) return ModuleStatus::Healthy;
    if (key == QLatin1String("warning")) return ModuleStatus::Warning;
    if (key == QLatin1String("critical")) return ModuleStatus::Critical;
    if (key == QLatin1String("limited")) return ModuleStatus::Limited;
    if (key == QLatin1String("not_configured")) return ModuleStatus::NotConfigured;
    if (key == QLatin1String("disabled")) return ModuleStatus::Disabled;
    if (key == QLatin1String("not_released")) return ModuleStatus::NotReleased;
    if (key == QLatin1String("config_missing")) return ModuleStatus::ConfigMissing;
    if (key == QLatin1String("connection_error")) return ModuleStatus::ConnectionError;
    if (key == QLatin1String("unsupported")) return ModuleStatus::Unsupported;
    if (key == QLatin1String("auth_rejected")) return ModuleStatus::AuthRejected;
    return ModuleStatus::Unknown;
}

QString statusLabel(ModuleStatus status)
{
    switch (status) {
    case ModuleStatus::Checking: return QStringLiteral("Kontrol ediliyor");
    case ModuleStatus::Healthy: return QStringLiteral("Sağlıklı");
    case ModuleStatus::Warning: return QStringLiteral("Uyarı");
    case ModuleStatus::Critical: return QStringLiteral("Kritik");
    case ModuleStatus::Limited: return QStringLiteral("Kısmi sinyal");
    case ModuleStatus::Unknown: return QStringLiteral("Bilinmiyor");
    case ModuleStatus::NotConfigured: return QStringLiteral("Bağlantı yapılandırılmadı");
    case ModuleStatus::Disabled: return QStringLiteral("Devre dışı");
    case ModuleStatus::NotReleased: return QStringLiteral("Henüz yayınlanmadı");
    case ModuleStatus::ConfigMissing: return QStringLiteral("Yapılandırma eksik");
    case ModuleStatus::ConnectionError: return QStringLiteral("Bağlantı Kesildi");
    case ModuleStatus::Unsupported: return QStringLiteral("Ölçüm yok");
    case ModuleStatus::AuthRejected: return QStringLiteral("Kimlik reddedildi");
    }
    return QStringLiteral("Bilinmiyor");
}

QString moduleStatusLabel(const QString& moduleId, ModuleStatus status)
{
    if ((moduleId == QLatin1String("tg-netgsm") || moduleId == QLatin1String("tg-iyzico")) && status == ModuleStatus::Healthy) return QStringLiteral("Çalışıyor");
    return statusLabel(status);
}

QString connectionLabel(ModuleStatus status)
{
    switch (status) {
    case ModuleStatus::Healthy:
    case ModuleStatus::Warning:
    case ModuleStatus::Critical:
    case ModuleStatus::Limited:
        return QStringLiteral("Bağlı");
    case ModuleStatus::ConnectionError:
        return QStringLiteral("Bağlantı Kesildi");
    case ModuleStatus::Disabled:
        return QStringLiteral("Devre dışı");
    case ModuleStatus::NotReleased:
        return QStringLiteral("Henüz yayınlanmadı");
    case ModuleStatus::ConfigMissing:
        return QStringLiteral("Yapılandırma eksik");
    case ModuleStatus::AuthRejected:
        return QStringLiteral("Kimlik reddedildi");
    case ModuleStatus::Checking:
        return QStringLiteral("Kontrol ediliyor");
    case ModuleStatus::Unsupported:
        return QStringLiteral("Ölçüm yok");
    case ModuleStatus::NotConfigured:
        return QStringLiteral("Bağlantı yapılandırılmadı");
    case ModuleStatus::Unknown:
        return QStringLiteral("Bilinmiyor");
    }
    return QStringLiteral("Bilinmiyor");
}

QString toneFor(ModuleStatus status)
{
    switch (status) {
    case ModuleStatus::Healthy: return QStringLiteral("green");
    case ModuleStatus::Warning:
    case ModuleStatus::Limited:
    case ModuleStatus::AuthRejected: return QStringLiteral("amber");
    case ModuleStatus::Critical:
    case ModuleStatus::ConnectionError: return QStringLiteral("red");
    default: return QStringLiteral("gray");
    }
}

bool countsInHealthRatio(ModuleStatus status)
{
    return status == ModuleStatus::Healthy || status == ModuleStatus::Warning || status == ModuleStatus::Critical || status == ModuleStatus::ConnectionError;
}

bool isBadStatus(ModuleStatus status)
{
    return status == ModuleStatus::Warning || status == ModuleStatus::Critical || status == ModuleStatus::ConnectionError || status == ModuleStatus::AuthRejected;
}

QString redactText(const QString& input)
{
    QString text = input;
    const QStringList keys = {
        QStringLiteral("password"), QStringLiteral("secret"), QStringLiteral("token"),
        QStringLiteral("apiKey"), QStringLiteral("api_key"), QStringLiteral("authorization"),
        QStringLiteral("otp"), QStringLiteral("access_token"), QStringLiteral("refreshToken"),
        QStringLiteral("refresh_token")
    };
    for (const QString& key : keys) {
        const QString needle = QStringLiteral("\"") + key + QStringLiteral("\"");
        int from = 0;
        while ((from = text.indexOf(needle, from, Qt::CaseInsensitive)) >= 0) {
            const int colon = text.indexOf(QLatin1Char(':'), from + needle.size());
            if (colon < 0 || colon - from > 8 + needle.size()) {
                break;
            }
            int valueStart = colon + 1;
            while (valueStart < text.size() && text.at(valueStart).isSpace()) {
                ++valueStart;
            }
            if (valueStart >= text.size()) {
                break;
            }
            if (text.at(valueStart) == QLatin1Char('"')) {
                const int end = text.indexOf(QLatin1Char('"'), valueStart + 1);
                if (end > valueStart) {
                    text.replace(valueStart + 1, end - valueStart - 1, QStringLiteral("[gizli]"));
                    from = valueStart + 10;
                    continue;
                }
            }
            from = valueStart + 1;
        }
    }
    int bearer = 0;
    const QString bearerWord = QStringLiteral("Bearer ");
    while ((bearer = text.indexOf(bearerWord, bearer, Qt::CaseInsensitive)) >= 0) {
        int end = bearer + bearerWord.size();
        while (end < text.size() && !text.at(end).isSpace() && text.at(end) != QLatin1Char('"') && text.at(end) != QLatin1Char('\n')) {
            ++end;
        }
        text.replace(bearer, end - bearer, QStringLiteral("Bearer [gizli]"));
        bearer += 16;
    }
    const int pem = text.indexOf(QStringLiteral("-----BEGIN"));
    if (pem >= 0) {
        text.replace(pem, text.size() - pem, QStringLiteral("[sertifika gizlendi]"));
    }
    maskE164(text);
    if (text.size() > 360) {
        text.truncate(360);
        text += QStringLiteral("…");
    }
    return text;
}

QString redactLog(const QString& input)
{
    QString text = redactText(input);
    text.replace(QRegularExpression(QStringLiteral("\\b(?:\\d[ -]?){13,19}\\b")), QStringLiteral("[kart gizlendi]"));
    text.replace(QRegularExpression(QStringLiteral("eyJ[A-Za-z0-9_-]{10,}\\.[A-Za-z0-9_-]{10,}\\.[A-Za-z0-9_-]{8,}")), QStringLiteral("[jwt gizlendi]"));
    text.replace(QRegularExpression(QStringLiteral("\\b[A-Z0-9._%+-]{1,40}@[A-Z0-9.-]+\\.[A-Z]{2,}\\b"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("[e-posta gizlendi]"));
    if (text.size() > 2000) {
        text.truncate(2000);
        text += QStringLiteral("…");
    }
    return text;
}

QJsonObject unwrapPayload(const QJsonObject& object)
{
    const QJsonValue data = object.value(QStringLiteral("data"));
    if (data.isObject()) {
        return data.toObject();
    }
    return object;
}

QString optNumber(const std::optional<double>& value, int digits)
{
    if (!value) {
        return QStringLiteral("—");
    }
    return QString::number(*value, 'f', digits);
}

QString latencyText(qint64 ms)
{
    if (ms < 0) {
        return QStringLiteral("—");
    }
    return QString::number(ms) + QStringLiteral(" ms");
}

QStringList defaultAllowHosts()
{
    return {
        QStringLiteral("api.ticket-go.net"),
        QStringLiteral("admin.ticket-go.net"),
        QStringLiteral("www.ticket-go.net"),
        QStringLiteral("ticket-go.net"),
        QStringLiteral("www.ticketgoteknoloji.com"),
        QStringLiteral("ticketgoteknoloji.com")
    };
}

SsrfDecision precheckUrl(const QUrl& url, const QStringList& extraHosts, bool allowPrivate)
{
    SsrfDecision decision;
    if (!url.isValid() || url.scheme().isEmpty() || url.host().isEmpty()) {
        decision.reason = QStringLiteral("Adres çözümlenemedi.");
        return decision;
    }
    if (!url.userInfo().isEmpty()) {
        decision.reason = QStringLiteral("Adres içinde kullanıcı bilgisi kabul edilmez.");
        return decision;
    }
    const QString scheme = url.scheme().toLower();
    if (scheme != QLatin1String("https") && !(allowPrivate && scheme == QLatin1String("http"))) {
        decision.reason = QStringLiteral("Yalnızca HTTPS kabul edilir. Özel ağ izni olmadan HTTP kapalıdır.");
        return decision;
    }
    const QString host = url.host().toLower();
    if (host == QLatin1String("metadata.google.internal") || host.endsWith(QStringLiteral(".metadata.google.internal"))) {
        decision.reason = QStringLiteral("Bulut metadata adresi engellendi.");
        return decision;
    }
    QStringList allowed = defaultAllowHosts();
    allowed += extraHosts;
    bool listed = false;
    for (QString item : allowed) {
        item = item.trimmed().toLower();
        if (!item.isEmpty() && item == host) {
            listed = true;
            break;
        }
    }
    if (!listed && !allowPrivate) {
        decision.reason = QStringLiteral("Hedef izin listesinde değil. Kaydetmeden önce bu alan adına açık izin verin.");
        return decision;
    }
    QHostAddress literal(host);
    if (!literal.isNull()) {
        if (isMetadata(literal) || addressBlocked(literal, allowPrivate)) {
            decision.reason = QStringLiteral("Bu IP adresine istek gönderilmez.");
            return decision;
        }
    }
    decision.ok = true;
    return decision;
}

bool addressBlocked(const QHostAddress& address, bool allowPrivate)
{
    if (isMetadata(address)) {
        return true;
    }
    if (allowPrivate) {
        return false;
    }
    return isPrivateAddress(address);
}

QString integrationBucket(ModuleStatus status, bool paused)
{
    if (paused || status == ModuleStatus::Disabled) return QStringLiteral("disabled");
    if (status == ModuleStatus::Checking || status == ModuleStatus::NotConfigured || status == ModuleStatus::Unknown || status == ModuleStatus::ConfigMissing) return QStringLiteral("unconfigured");
    if (status == ModuleStatus::Healthy) return QStringLiteral("healthy");
    if (status == ModuleStatus::Warning || status == ModuleStatus::NotReleased) return QStringLiteral("warning");
    if (status == ModuleStatus::ConnectionError) return QStringLiteral("connection");
    if (status == ModuleStatus::Critical) return QStringLiteral("critical");
    if (status == ModuleStatus::AuthRejected) return QStringLiteral("auth");
    return QStringLiteral("other");
}

QByteArray basicAuthorizationValue(const QString& secret, QString* error)
{
    const int split = secret.indexOf(QLatin1Char(':'));
    if (split <= 0 || split >= secret.size() - 1 || secret.contains(QLatin1Char('\n')) || secret.contains(QLatin1Char('\r'))) {
        if (error) *error = QStringLiteral("Basic kimlik biçimi geçersiz. Değer gösterilmez.");
        return {};
    }
    return QByteArray("Basic ") + secret.toUtf8().toBase64();
}

QVariantMap integrationFieldErrors(const Integration& integration, const QList<Integration>& existing)
{
    QVariantMap errors;
    auto reject = [&](const QString& key, const QString& text) {
        if (!errors.contains(key)) errors.insert(key, text);
    };
    if (integration.name.trimmed().isEmpty() || integration.name.trimmed().size() > 120) reject(QStringLiteral("name"), QStringLiteral("Entegrasyon adı gerekli."));
    if (integration.category.trimmed().isEmpty() || integration.category.trimmed().size() > 120) reject(QStringLiteral("application"), QStringLiteral("Uygulama adı gerekli."));
    if (integration.description.size() > 400) reject(QStringLiteral("description"), QStringLiteral("Açıklama 400 karakteri aşamaz."));
    QString healthPath = integration.healthPath.trimmed();
    const QUrl base(integration.baseUrl.trimmed());
    const SsrfDecision baseDecision = precheckUrl(base, {}, integration.allowPrivate);
    if (!baseDecision.ok) reject(QStringLiteral("baseUrl"), baseDecision.reason);
    if (healthPath.startsWith(QStringLiteral("https://"), Qt::CaseInsensitive) || healthPath.startsWith(QStringLiteral("http://"), Qt::CaseInsensitive)) {
        const QUrl full(healthPath);
        const SsrfDecision pathDecision = precheckUrl(full, {}, integration.allowPrivate);
        if (!pathDecision.ok) reject(QStringLiteral("healthPath"), pathDecision.reason);
        else if (full.host().compare(base.host(), Qt::CaseInsensitive) != 0) reject(QStringLiteral("healthPath"), QStringLiteral("Health adresi base URL ile aynı host olmalı."));
        else {
            healthPath = full.path(QUrl::FullyDecoded);
            if (full.hasQuery()) healthPath += QStringLiteral("?") + full.query();
        }
    }
    if (!healthPath.startsWith(QLatin1Char('/')) || healthPath.contains(QStringLiteral("..")) || healthPath.contains(QLatin1Char(' ')) || healthPath.contains(QLatin1Char('\\')) || healthPath.size() > 240) {
        reject(QStringLiteral("healthPath"), QStringLiteral("Health yolu geçersiz. / ile başlamalı."));
    }
    const QString method = integration.method.trimmed().toUpper();
    if (method != QLatin1String("GET") && method != QLatin1String("HEAD")) reject(QStringLiteral("method"), QStringLiteral("Yalnızca GET veya HEAD."));
    if (integration.intervalMs < 15000 || integration.intervalMs > 300000) reject(QStringLiteral("intervalMs"), QStringLiteral("Kontrol aralığı 15–300 saniye olmalı."));
    if (integration.timeoutMs < 2000 || integration.timeoutMs > 30000) reject(QStringLiteral("timeoutMs"), QStringLiteral("Zaman aşımı 2–30 saniye aralığında olmalı."));
    if (integration.expectedStatus < 100 || integration.expectedStatus > 599) reject(QStringLiteral("expectedStatus"), QStringLiteral("Beklenen HTTP durumu geçersiz."));
    if (integration.latencyWarnMs < 1 || integration.latencyWarnMs > 120000) reject(QStringLiteral("latencyWarnMs"), QStringLiteral("Kritik gecikme eşiği geçersiz."));
    if (integration.failThreshold < 2 || integration.failThreshold > 10) reject(QStringLiteral("failThreshold"), QStringLiteral("Hata uyarı eşiği 2 ile 10 ölçüm arasında olmalı."));
    const QString auth = integration.authType.trimmed();
    if (auth != QLatin1String("none") && auth != QLatin1String("api_key") && auth != QLatin1String("bearer") && auth != QLatin1String("basic") && auth != QLatin1String("oauth2")) {
        reject(QStringLiteral("authType"), QStringLiteral("Bu kimlik doğrulama yöntemi uygulanmıyor."));
    }
    if (auth == QLatin1String("api_key")) {
        const QString header = integration.apiKeyHeader.trimmed();
        const QString lower = header.toLower();
        const QRegularExpression safe(QStringLiteral("^[A-Za-z0-9-]{1,40}$"));
        if (!safe.match(header).hasMatch() || lower == QLatin1String("authorization") || lower == QLatin1String("cookie") || lower == QLatin1String("host")) {
            reject(QStringLiteral("apiKeyHeader"), QStringLiteral("API anahtarı başlığı geçersiz."));
        }
    }
    if (auth == QLatin1String("oauth2")) {
        if (integration.oauthClientId.trimmed().isEmpty() || integration.oauthClientId.size() > 120) reject(QStringLiteral("oauthClientId"), QStringLiteral("OAuth istemci kimliği gerekli."));
        const SsrfDecision tokenDecision = precheckUrl(QUrl(integration.oauthTokenUrl.trimmed()), {}, integration.allowPrivate);
        if (!tokenDecision.ok) reject(QStringLiteral("oauthTokenUrl"), tokenDecision.reason);
    }
    if (!integration.wsUrl.trimmed().isEmpty()) {
        const QUrl socket(integration.wsUrl.trimmed());
        if (socket.scheme() != QLatin1String("wss")) reject(QStringLiteral("wsUrl"), QStringLiteral("Ek akış yalnızca wss olabilir. Sağlık kontrolünün yerine geçmez."));
    }
    if (integration.adapter != QLatin1String("http") && integration.adapter != QLatin1String("contract-1.0")) reject(QStringLiteral("adapter"), QStringLiteral("API türü REST / HTTPS dışında desteklenmiyor."));
    const QString key = base.scheme().toLower() + QStringLiteral("://") + base.host().toLower() + QStringLiteral("|") + healthPath;
    for (const Integration& item : existing) {
        if (!integration.id.isEmpty() && item.id == integration.id) continue;
        const QUrl other(item.baseUrl.trimmed());
        const QString otherKey = other.scheme().toLower() + QStringLiteral("://") + other.host().toLower() + QStringLiteral("|") + item.healthPath;
        if (otherKey == key) reject(QStringLiteral("duplicate"), QStringLiteral("Bu base URL ve health yolu başka bir kayıtta var."));
    }
    QString summary;
    const QStringList order = {QStringLiteral("name"), QStringLiteral("application"), QStringLiteral("description"), QStringLiteral("baseUrl"), QStringLiteral("healthPath"), QStringLiteral("method"), QStringLiteral("intervalMs"), QStringLiteral("timeoutMs"), QStringLiteral("expectedStatus"), QStringLiteral("latencyWarnMs"), QStringLiteral("failThreshold"), QStringLiteral("authType"), QStringLiteral("apiKeyHeader"), QStringLiteral("oauthClientId"), QStringLiteral("oauthTokenUrl"), QStringLiteral("wsUrl"), QStringLiteral("duplicate"), QStringLiteral("adapter")};
    for (const QString& item : order) {
        if (errors.contains(item)) {
            summary = errors.value(item).toString();
            break;
        }
    }
    if (summary.isEmpty()) {
        errors.clear();
        errors.insert(QStringLiteral("healthPathValue"), healthPath);
    } else {
        errors.insert(QStringLiteral("summary"), summary);
    }
    return errors;
}

} // namespace tg
