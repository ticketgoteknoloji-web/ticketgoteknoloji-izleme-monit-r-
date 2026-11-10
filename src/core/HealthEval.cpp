#include "HealthEval.h"

#include <QJsonDocument>
#include <QJsonParseError>
#include <QUrl>

namespace tg {
namespace {

QString fieldText(const QJsonObject& object, const QString& key)
{
    const QJsonValue value = object.value(key);
    if (value.isString()) {
        return value.toString();
    }
    if (value.isObject()) {
        return value.toObject().value(QStringLiteral("status")).toString();
    }
    if (value.isBool()) {
        return value.toBool() ? QStringLiteral("true") : QStringLiteral("false");
    }
    if (value.isDouble()) {
        return QString::number(value.toDouble());
    }
    return {};
}

bool hasField(const QJsonObject& object, const QString& key)
{
    return object.contains(key) && !object.value(key).isNull() && !object.value(key).isUndefined();
}

ModuleStatus mapContractStatus(const QString& status)
{
    if (status == QLatin1String("healthy")) return ModuleStatus::Healthy;
    if (status == QLatin1String("degraded")) return ModuleStatus::Warning;
    if (status == QLatin1String("unhealthy")) return ModuleStatus::Critical;
    return ModuleStatus::Unknown;
}

ModuleStatus mapGuard(const QJsonObject& guard, bool* present)
{
    if (guard.isEmpty()) {
        *present = false;
        return ModuleStatus::NotConfigured;
    }
    *present = true;
    const QString status = guard.value(QStringLiteral("status")).toString();
    const bool enabled = guard.value(QStringLiteral("enabled")).toBool(true);
    if (!enabled || status == QLatin1String("disabled")) {
        return ModuleStatus::Disabled;
    }
    if (status == QLatin1String("missing_credentials") || guard.value(QStringLiteral("configured")).toBool(false) == false) {
        if (status == QLatin1String("ok") && guard.value(QStringLiteral("configured")).toBool(false)) {
            return ModuleStatus::Limited;
        }
        if (status == QLatin1String("missing_credentials") || !guard.value(QStringLiteral("configured")).toBool(false)) {
            return ModuleStatus::ConfigMissing;
        }
    }
    if (status == QLatin1String("ok")) {
        return ModuleStatus::Limited;
    }
    return ModuleStatus::Limited;
}

ModuleStatus mapNetgsm(const QJsonObject& guard, bool* present)
{
    if (guard.isEmpty()) {
        *present = false;
        return ModuleStatus::NotConfigured;
    }
    *present = true;
    const QString status = guard.value(QStringLiteral("status")).toString();
    const bool enabled = guard.value(QStringLiteral("enabled")).toBool(false);
    const bool configured = guard.value(QStringLiteral("configured")).toBool(false);
    if (status == QLatin1String("disabled") || (guard.contains(QStringLiteral("enabled")) && !enabled)) {
        return ModuleStatus::Disabled;
    }
    if (!configured || status == QLatin1String("missing_credentials")) {
        return ModuleStatus::ConfigMissing;
    }
    if (status == QLatin1String("ok") && enabled && configured) {
        return ModuleStatus::Healthy;
    }
    if (status == QLatin1String("error") || status == QLatin1String("down")) {
        return ModuleStatus::Critical;
    }
    return mapGuard(guard, present);
}

QString guardEvidence(const QString& title, const QJsonObject& guard)
{
    return title + QStringLiteral(" durumu: ") + (guard.value(QStringLiteral("status")).toString().isEmpty() ? QStringLiteral("alan var") : guard.value(QStringLiteral("status")).toString())
        + QStringLiteral(". Bu bir yapılandırma sinyalidir; işlem içeriği okunmadı.");
}

} // namespace

HealthClass classifyStatus(ModuleStatus status)
{
    switch (status) {
    case ModuleStatus::Healthy: return HealthClass::Healthy;
    case ModuleStatus::Warning:
    case ModuleStatus::NotReleased: return HealthClass::Warning;
    case ModuleStatus::Critical:
    case ModuleStatus::AuthRejected: return HealthClass::Critical;
    case ModuleStatus::ConnectionError: return HealthClass::Disconnected;
    case ModuleStatus::Disabled: return HealthClass::Disabled;
    case ModuleStatus::NotConfigured:
    case ModuleStatus::ConfigMissing: return HealthClass::NotConfigured;
    case ModuleStatus::Checking:
    case ModuleStatus::Limited:
    case ModuleStatus::Unknown:
    case ModuleStatus::Unsupported: return HealthClass::Unknown;
    }
    return HealthClass::Unknown;
}

QString healthClassKey(HealthClass healthClass)
{
    switch (healthClass) {
    case HealthClass::Healthy: return QStringLiteral("healthy");
    case HealthClass::Warning: return QStringLiteral("warning");
    case HealthClass::Critical: return QStringLiteral("critical");
    case HealthClass::Disconnected: return QStringLiteral("disconnected");
    case HealthClass::Disabled: return QStringLiteral("disabled");
    case HealthClass::NotConfigured: return QStringLiteral("not_configured");
    case HealthClass::Unknown: return QStringLiteral("unknown");
    }
    return QStringLiteral("unknown");
}

QString healthClassLabel(HealthClass healthClass)
{
    switch (healthClass) {
    case HealthClass::Healthy: return QStringLiteral("Sağlıklı");
    case HealthClass::Warning: return QStringLiteral("Uyarı");
    case HealthClass::Critical: return QStringLiteral("Kritik");
    case HealthClass::Disconnected: return QStringLiteral("Bağlantı Kesildi");
    case HealthClass::Disabled: return QStringLiteral("Devre dışı");
    case HealthClass::NotConfigured: return QStringLiteral("Yapılandırılmadı");
    case HealthClass::Unknown: return QStringLiteral("Bilinmiyor");
    }
    return QStringLiteral("Bilinmiyor");
}

bool isProblemClass(HealthClass healthClass)
{
    return healthClass == HealthClass::Warning || healthClass == HealthClass::Critical || healthClass == HealthClass::Disconnected;
}

bool shownOnOverview(ModuleStatus status)
{
    return classifyStatus(status) == HealthClass::Healthy;
}

QList<ModuleSnapshot> applyParentGuard(const QList<ModuleSnapshot>& modules)
{
    QList<ModuleSnapshot> guarded = modules;
    const QStringList parents = {QStringLiteral("tg-api"), QStringLiteral("gochat-api")};
    for (const QString& parentId : parents) {
        int index = -1;
        for (int i = 0; i < guarded.size(); ++i) {
            if (guarded.at(i).id == parentId) {
                index = i;
                break;
            }
        }
        if (index < 0) continue;
        const HealthClass parentClass = classifyStatus(guarded.at(index).status);
        if (parentClass != HealthClass::Healthy && parentClass != HealthClass::Warning) continue;
        QStringList criticalNames;
        QStringList lostNames;
        for (const ModuleSnapshot& child : guarded) {
            const bool dependency = parentId == QLatin1String("tg-api")
                ? (child.id == QLatin1String("tg-postgres") || child.id == QLatin1String("tg-redis") || child.id == QLatin1String("tg-workers")
                    || child.id == QLatin1String("tg-queues") || child.id == QLatin1String("tg-cron") || child.id == QLatin1String("tg-jwt")
                    || child.id == QLatin1String("tg-iyzico") || child.id == QLatin1String("tg-netgsm") || child.id == QLatin1String("tg-mfa")
                    || child.id == QLatin1String("tg-liknoss") || child.id == QLatin1String("tg-accounting"))
                : (child.id.startsWith(QLatin1String("gochat-")) && child.id != parentId);
            if (!dependency) continue;
            const HealthClass childClass = classifyStatus(child.status);
            if (childClass == HealthClass::Critical) criticalNames.push_back(child.name);
            else if (childClass == HealthClass::Disconnected) lostNames.push_back(child.name);
        }
        if (!criticalNames.isEmpty()) {
            guarded[index].status = ModuleStatus::Critical;
            guarded[index].evidence += QStringLiteral(" Alt servis kritik: ") + criticalNames.join(QStringLiteral(", ")) + QStringLiteral(".");
            guarded[index].lastError = guarded[index].evidence;
            guarded[index].connection = connectionLabel(guarded[index].status);
            guarded[index].causeLabel = QStringLiteral("Alt servis");
            guarded[index].causeText = criticalNames.join(QStringLiteral(", "));
        } else if (!lostNames.isEmpty() && parentClass == HealthClass::Healthy) {
            guarded[index].status = ModuleStatus::Warning;
            guarded[index].evidence += QStringLiteral(" Alt serviste bağlantı koptu: ") + lostNames.join(QStringLiteral(", ")) + QStringLiteral(".");
            guarded[index].lastError = guarded[index].evidence;
            guarded[index].connection = connectionLabel(guarded[index].status);
            guarded[index].causeLabel = QStringLiteral("Alt servis");
            guarded[index].causeText = lostNames.join(QStringLiteral(", "));
        }
    }
    return guarded;
}

ModuleSnapshot makeModule(const QString& id, const QString& group, const QString& name, ModuleStatus status, const QString& evidence, qint64 latencyMs)
{
    ModuleSnapshot module;
    module.id = id;
    module.group = group;
    module.name = name;
    module.status = status;
    module.evidence = evidence;
    module.latencyMs = latencyMs;
    module.connection = connectionLabel(status);
    module.lastCheck = QDateTime::currentDateTime();
    if (status == ModuleStatus::Critical || status == ModuleStatus::ConnectionError || status == ModuleStatus::Warning || status == ModuleStatus::AuthRejected) {
        module.lastError = evidence;
        module.causeLabel = status == ModuleStatus::ConnectionError ? QStringLiteral("Olası neden") : QStringLiteral("Kanıt");
        module.causeText = evidence;
        module.recommendation = status == ModuleStatus::ConnectionError
            ? QStringLiteral("Ağ, DNS ve TLS yolunu doğrulayın. Tek başarısızlık alarm üretmez.")
            : QStringLiteral("İlgili modülün sağlık alanını ve zaman damgasını inceleyin. Log kaynağı bağlı değilse kök neden kesinleştirilmez.");
    } else if (status == ModuleStatus::NotConfigured || status == ModuleStatus::Unsupported) {
        module.causeLabel = QStringLiteral("Kanıt yok");
        module.causeText = evidence;
        module.recommendation = QStringLiteral("Bu bileşen için doğrulanmış telemetry ucu tanımlanmadan sağlıklı kabul edilmez.");
    } else {
        module.causeLabel = QStringLiteral("Kanıt");
        module.causeText = evidence;
    }
    return module;
}

QList<ModuleSnapshot> evaluateTicketGo(const HttpResult& health, const HttpResult& live, const HttpResult& ready, const HttpResult& identity, const HttpResult& deps, bool depsAttempted)
{
    QList<ModuleSnapshot> modules;
    const bool anyTransport = health.transportOk || live.transportOk || ready.transportOk;
    const qint64 apiLatency = health.latencyMs >= 0 ? health.latencyMs : (live.latencyMs >= 0 ? live.latencyMs : ready.latencyMs);

    ModuleStatus apiStatus = ModuleStatus::ConnectionError;
    QString apiEvidence;
    if (!health.attempted && !live.attempted && !ready.attempted) {
        apiStatus = ModuleStatus::Checking;
        apiEvidence = QStringLiteral("Henüz ölçüm yok.");
    } else if (!health.transportOk && !live.transportOk && !ready.transportOk) {
        apiStatus = ModuleStatus::ConnectionError;
        apiEvidence = QStringLiteral("Ticket-Go sağlık uçlarına ulaşılamadı. ") + redactText(health.detail.isEmpty() ? ready.detail : health.detail);
    } else if (health.transportOk && health.status >= 500) {
        apiStatus = ModuleStatus::Critical;
        apiEvidence = QStringLiteral("GET /health HTTP ") + QString::number(health.status);
    } else if (ready.transportOk && ready.status == 200 && health.transportOk && health.status >= 200 && health.status < 300) {
        apiStatus = ModuleStatus::Healthy;
        apiEvidence = QStringLiteral("GET /health HTTP ") + QString::number(health.status) + QStringLiteral(", GET /health/ready HTTP 200. Yalnızca süreç ve ready yanıtı.");
    } else if (health.transportOk && health.status >= 200 && health.status < 300 && ready.transportOk && ready.status >= 500) {
        apiStatus = ModuleStatus::Warning;
        apiEvidence = QStringLiteral("Süreç yanıt verdi (HTTP ") + QString::number(health.status) + QStringLiteral(") ancak /health/ready HTTP ") + QString::number(ready.status) + QStringLiteral(".");
    } else if (anyTransport) {
        apiStatus = ModuleStatus::Warning;
        apiEvidence = QStringLiteral("Kısmi yanıt. /health=") + (health.attempted ? QString::number(health.status) : QStringLiteral("yok"))
            + QStringLiteral(" /live=") + (live.attempted ? QString::number(live.status) : QStringLiteral("yok"))
            + QStringLiteral(" /ready=") + (ready.attempted ? QString::number(ready.status) : QStringLiteral("yok"));
    }
    if (identity.jsonOk) {
        const QJsonObject body = unwrapPayload(identity.json);
        const QString version = fieldText(body, QStringLiteral("version"));
        if (!version.isEmpty()) {
            apiEvidence += QStringLiteral(" Sürüm alanı: ") + version.left(40) + QStringLiteral(".");
        }
    }
    modules.push_back(makeModule(QStringLiteral("tg-api"), QStringLiteral("ticket-go"), QStringLiteral("Backend API"), apiStatus, apiEvidence, apiLatency));

    const QJsonObject readyBody = ready.jsonOk ? unwrapPayload(ready.json) : QJsonObject();
    const bool readyUsable = ready.transportOk && ready.jsonOk && (ready.status == 200 || ready.status == 503);

    auto dependency = [&](const QString& id, const QString& name, const QString& key) {
        if (!ready.attempted) {
            modules.push_back(makeModule(id, QStringLiteral("ticket-go"), name, ModuleStatus::Checking, QStringLiteral("Ready ölçümü bekleniyor.")));
            return;
        }
        if (!ready.transportOk) {
            modules.push_back(makeModule(id, QStringLiteral("ticket-go"), name, ModuleStatus::Unknown, QStringLiteral("Ready uçuna ulaşılamadı. Bu bileşen sağlıklı sayılmadı.")));
            return;
        }
        if (!readyUsable || !hasField(readyBody, key)) {
            modules.push_back(makeModule(id, QStringLiteral("ticket-go"), name, ModuleStatus::NotConfigured, QStringLiteral("GET /health/ready bu bileşen için alan döndürmedi. Tek /health yanıtı yeterli kanıt değildir.")));
            return;
        }
        const QString value = fieldText(readyBody, key);
        ModuleStatus status = ModuleStatus::Unsupported;
        if (value == QLatin1String("up")) status = ModuleStatus::Healthy;
        else if (value == QLatin1String("down") || value == QLatin1String("error")) status = ModuleStatus::Critical;
        else if (value == QLatin1String("disabled")) status = ModuleStatus::Disabled;
        else status = ModuleStatus::Limited;
        modules.push_back(makeModule(id, QStringLiteral("ticket-go"), name, status, QStringLiteral("GET /health/ready ") + key + QStringLiteral("=") + value + QStringLiteral(".")));
    };
    dependency(QStringLiteral("tg-postgres"), QStringLiteral("PostgreSQL"), QStringLiteral("postgres"));
    if (modules.last().status == ModuleStatus::NotConfigured && readyUsable && hasField(readyBody, QStringLiteral("database"))) {
        modules.removeLast();
        dependency(QStringLiteral("tg-postgres"), QStringLiteral("PostgreSQL"), QStringLiteral("database"));
    }
    dependency(QStringLiteral("tg-redis"), QStringLiteral("Redis"), QStringLiteral("redis"));

    if (readyUsable && readyBody.value(QStringLiteral("aceAutomation")).isObject()) {
        const QJsonObject ace = readyBody.value(QStringLiteral("aceAutomation")).toObject();
        const bool running = ace.value(QStringLiteral("workerRunning")).toBool(false);
        const bool fresh = ace.value(QStringLiteral("heartbeatFresh")).toBool(false);
        const QString mode = ace.value(QStringLiteral("automationMode")).toString();
        ModuleStatus workerStatus = ModuleStatus::Limited;
        if (mode == QLatin1String("NOT_RUNNING") && !running) workerStatus = ModuleStatus::Disabled;
        else if (running && fresh) workerStatus = ModuleStatus::Healthy;
        else if (!running) workerStatus = ModuleStatus::Warning;
        modules.push_back(makeModule(QStringLiteral("tg-workers"), QStringLiteral("ticket-go"), QStringLiteral("Worker servisleri"), workerStatus,
            QStringLiteral("Yalnızca muhasebe otomasyon kalp atışı. mode=") + mode + QStringLiteral(", workerRunning=") + (running ? QStringLiteral("true") : QStringLiteral("false")) + QStringLiteral(". Diğer roller bu yanıtta yok.")));
        ModuleStatus acc = workerStatus == ModuleStatus::Healthy ? ModuleStatus::Healthy : (mode == QLatin1String("NOT_RUNNING") ? ModuleStatus::Disabled : ModuleStatus::Limited);
        modules.push_back(makeModule(QStringLiteral("tg-accounting"), QStringLiteral("ticket-go"), QStringLiteral("Go Muhasebe"), acc,
            QStringLiteral("aceAutomation. Muhasebe kaydı içeriği okunmadı. mode=") + mode + QStringLiteral(".")));
        if (ace.contains(QStringLiteral("schedulerRunning")) || ace.contains(QStringLiteral("schedulerEnabled"))) {
            const bool scheduler = ace.value(QStringLiteral("schedulerRunning")).toBool(false);
            modules.push_back(makeModule(QStringLiteral("tg-cron"), QStringLiteral("ticket-go"), QStringLiteral("Zamanlanmış işler"), scheduler ? ModuleStatus::Limited : ModuleStatus::Disabled,
                QStringLiteral("aceAutomation zamanlayıcı alanı. Ayrı iş listesi yok.")));
        } else {
            modules.push_back(makeModule(QStringLiteral("tg-cron"), QStringLiteral("ticket-go"), QStringLiteral("Zamanlanmış işler"), ModuleStatus::NotConfigured, QStringLiteral("Ready yanıtında zamanlayıcı alanı yok.")));
        }
    } else {
        modules.push_back(makeModule(QStringLiteral("tg-workers"), QStringLiteral("ticket-go"), QStringLiteral("Worker servisleri"), ready.transportOk ? ModuleStatus::NotConfigured : ModuleStatus::Unknown, QStringLiteral("Worker kalp atışı bu yanıtta yok.")));
        modules.push_back(makeModule(QStringLiteral("tg-accounting"), QStringLiteral("ticket-go"), QStringLiteral("Go Muhasebe"), ready.transportOk ? ModuleStatus::NotConfigured : ModuleStatus::Unknown, QStringLiteral("Go Muhasebe telemetry alanı yok. Kayıt içeriği istenmedi.")));
        modules.push_back(makeModule(QStringLiteral("tg-cron"), QStringLiteral("ticket-go"), QStringLiteral("Zamanlanmış işler"), ModuleStatus::NotConfigured, QStringLiteral("Zamanlayıcı alanı yok.")));
    }

    if (readyUsable && readyBody.value(QStringLiteral("integrations")).isObject()) {
        const QJsonObject integrations = readyBody.value(QStringLiteral("integrations")).toObject();
        auto addGuard = [&](const QString& id, const QString& name, const QString& key) {
            bool present = false;
            const QJsonObject guard = integrations.value(key).toObject();
            const ModuleStatus status = mapGuard(guard, &present);
            modules.push_back(makeModule(id, QStringLiteral("ticket-go"), name, present ? status : ModuleStatus::NotConfigured, present ? guardEvidence(name, guard) : QStringLiteral("Ready entegrasyon alanında bu modül yok.")));
        };
        addGuard(QStringLiteral("tg-mfa"), QStringLiteral("MFA / E-posta OTP"), QStringLiteral("smtp"));
        {
            bool present = false;
            const QJsonObject guard = integrations.value(QStringLiteral("netgsm")).toObject();
            const ModuleStatus status = mapNetgsm(guard, &present);
            const QString evidence = !present
                ? QStringLiteral("Ready entegrasyon alanında bu modül yok.")
                : (status == ModuleStatus::Healthy
                    ? QStringLiteral("NetGSM etkin ve yapılandırılmış. Backend durum sinyali: ok.")
                    : guardEvidence(QStringLiteral("NetGSM SMS"), guard));
            modules.push_back(makeModule(QStringLiteral("tg-netgsm"), QStringLiteral("ticket-go"), QStringLiteral("NetGSM SMS"), present ? status : ModuleStatus::NotConfigured, evidence));
        }
        addGuard(QStringLiteral("tg-liknoss"), QStringLiteral("Liknoss"), QStringLiteral("liknoss"));
    } else {
        modules.push_back(makeModule(QStringLiteral("tg-mfa"), QStringLiteral("ticket-go"), QStringLiteral("MFA / E-posta OTP"), ModuleStatus::NotConfigured, QStringLiteral("SMTP/OTP alanı ölçülmedi.")));
        ModuleStatus netgsmStatus = ModuleStatus::NotConfigured;
        QString netgsmEvidence = QStringLiteral("NetGSM alanı ölçülmedi.");
        if (!ready.attempted) {
            netgsmStatus = ModuleStatus::Checking;
            netgsmEvidence = QStringLiteral("Ready ölçümü bekleniyor.");
        } else if (!ready.transportOk) {
            netgsmStatus = ModuleStatus::Unknown;
            netgsmEvidence = QStringLiteral("Ready uçuna ulaşılamadı. Bu bileşen sağlıklı sayılmadı.");
        }
        modules.push_back(makeModule(QStringLiteral("tg-netgsm"), QStringLiteral("ticket-go"), QStringLiteral("NetGSM SMS"), netgsmStatus, netgsmEvidence));
        modules.push_back(makeModule(QStringLiteral("tg-liknoss"), QStringLiteral("ticket-go"), QStringLiteral("Liknoss"), ModuleStatus::NotConfigured, QStringLiteral("Liknoss alanı ölçülmedi.")));
    }

    if (readyUsable && readyBody.value(QStringLiteral("paymentEngine")).isObject()) {
        const QJsonObject pay = readyBody.value(QStringLiteral("paymentEngine")).toObject();
        const QString payStatus = pay.value(QStringLiteral("status")).toString();
        const bool readyFlag = pay.value(QStringLiteral("ready")).toBool(false);
        const bool iyzicoProvider = pay.value(QStringLiteral("provider")).toString() == QLatin1String("iyzico");
        ModuleStatus status = ModuleStatus::ConfigMissing;
        if (payStatus == QLatin1String("LIVE") && readyFlag && iyzicoProvider) status = ModuleStatus::Healthy;
        else if (payStatus == QLatin1String("LIVE") && readyFlag) status = ModuleStatus::Limited;
        else if (payStatus == QLatin1String("CONFIG_INCOMPLETE") || !readyFlag) status = ModuleStatus::ConfigMissing;
        const QString evidence = status == ModuleStatus::Healthy
            ? QStringLiteral("İyzico ödeme motoru canlı ve hazır.")
            : QStringLiteral("paymentEngine status=") + payStatus + QStringLiteral(". Ödeme veya BIN isteği atılmadı.");
        modules.push_back(makeModule(QStringLiteral("tg-iyzico"), QStringLiteral("ticket-go"), QStringLiteral("İyzico ödeme"), status, evidence));
    } else {
        modules.push_back(makeModule(QStringLiteral("tg-iyzico"), QStringLiteral("ticket-go"), QStringLiteral("İyzico ödeme"), ModuleStatus::NotConfigured, QStringLiteral("Ödeme hazır olma alanı yok. Ödeme isteği atılmadı.")));
    }

    if (readyUsable && (readyBody.contains(QStringLiteral("bull")) || readyBody.contains(QStringLiteral("queue")))) {
        const QString queue = fieldText(readyBody, QStringLiteral("queue"));
        const QString bull = fieldText(readyBody, QStringLiteral("bull"));
        modules.push_back(makeModule(QStringLiteral("tg-queues"), QStringLiteral("ticket-go"), QStringLiteral("Arka plan kuyrukları"), ModuleStatus::Limited,
            QStringLiteral("queue=") + queue + QStringLiteral(", bull=") + bull + QStringLiteral(". Derinlik ayrı ops ucu olmadan okunmadı.")));
    } else {
        modules.push_back(makeModule(QStringLiteral("tg-queues"), QStringLiteral("ticket-go"), QStringLiteral("Arka plan kuyrukları"), ModuleStatus::NotConfigured, QStringLiteral("Kuyruk derinliği ölçülmedi.")));
    }

    modules.push_back(makeModule(QStringLiteral("tg-nginx"), QStringLiteral("ticket-go"), QStringLiteral("Nginx"), ModuleStatus::NotConfigured, QStringLiteral("Ayrı nginx metriği veya log kaynağı yok. API yanıtı nginx sağlığını kanıtlamaz.")));
    modules.push_back(makeModule(QStringLiteral("tg-docker"), QStringLiteral("ticket-go"), QStringLiteral("Docker"), ModuleStatus::NotConfigured, QStringLiteral("Docker soketine veya daemon komutuna bağlanılmadı.")));
    modules.push_back(makeModule(QStringLiteral("tg-turmob"), QStringLiteral("ticket-go"), QStringLiteral("TÜRMOB Gelen e-Fatura"), ModuleStatus::NotConfigured, QStringLiteral("Herkese açık sağlık yanıtında TÜRMOB alanı yok. Fatura içeriği okunmadı.")));
    modules.push_back(makeModule(QStringLiteral("tg-luca"), QStringLiteral("ticket-go"), QStringLiteral("LUCA"), ModuleStatus::NotConfigured, QStringLiteral("Herkese açık sağlık yanıtında LUCA alanı yok.")));
    modules.push_back(makeModule(QStringLiteral("tg-ws"), QStringLiteral("ticket-go"), QStringLiteral("WebSocket"), ModuleStatus::NotConfigured, QStringLiteral("Ticket-Go WebSocket sayacı bu uçlarda yok. Soket açılmadı.")));
    modules.push_back(makeModule(QStringLiteral("tg-host"), QStringLiteral("ticket-go"), QStringLiteral("Sunucu kaynakları"), ModuleStatus::NotConfigured, QStringLiteral("Üretim sunucusunun CPU, RAM ve disk uçları tanımlı değil. Bu bilgisayarın kaynakları sunucu sayılmaz.")));

    if (!depsAttempted) {
        modules.push_back(makeModule(QStringLiteral("tg-jwt"), QStringLiteral("ticket-go"), QStringLiteral("Kimlik doğrulama ve JWT"), ModuleStatus::NotConfigured,
            QStringLiteral("GET /health/dependencies kimlik ister. OPS belirteci anahtar zincirinde yok. JWT sağlıklı sayılmadı.")));
    } else if (!deps.transportOk) {
        modules.push_back(makeModule(QStringLiteral("tg-jwt"), QStringLiteral("ticket-go"), QStringLiteral("Kimlik doğrulama ve JWT"), ModuleStatus::Unknown, QStringLiteral("Ops ucuna ulaşılamadı.")));
    } else if (deps.status == 401 || deps.status == 403) {
        modules.push_back(makeModule(QStringLiteral("tg-jwt"), QStringLiteral("ticket-go"), QStringLiteral("Kimlik doğrulama ve JWT"), ModuleStatus::AuthRejected,
            QStringLiteral("Ops ucu HTTP ") + QString::number(deps.status) + QStringLiteral(" döndü. Belirteç kabul edilmedi. Belirteç değeri gösterilmez.")));
    } else if (deps.jsonOk) {
        const QJsonObject body = unwrapPayload(deps.json);
        const QString jwt = fieldText(body, QStringLiteral("jwtConfig"));
        ModuleStatus jwtStatus = ModuleStatus::Limited;
        if (jwt == QLatin1String("degraded")) jwtStatus = ModuleStatus::Warning;
        else if (jwt.isEmpty()) jwtStatus = ModuleStatus::Unsupported;
        modules.push_back(makeModule(QStringLiteral("tg-jwt"), QStringLiteral("ticket-go"), QStringLiteral("Kimlik doğrulama ve JWT"), jwtStatus,
            QStringLiteral("Ops JWT alanı: ") + (jwt.isEmpty() ? QStringLiteral("yok") : jwt) + QStringLiteral(". Oturum içeriği okunmadı.")));
        if (body.contains(QStringLiteral("reservationQueueDepth")) || body.contains(QStringLiteral("checkoutQueueDepth"))) {
            for (int i = 0; i < modules.size(); ++i) {
                if (modules[i].id == QLatin1String("tg-queues")) {
                    modules[i].status = ModuleStatus::Limited;
                    modules[i].evidence = QStringLiteral("Ops kuyruk derinliği alındı. reservation=") + fieldText(body, QStringLiteral("reservationQueueDepth"))
                        + QStringLiteral(" checkout=") + fieldText(body, QStringLiteral("checkoutQueueDepth")) + QStringLiteral(".");
                    modules[i].connection = connectionLabel(modules[i].status);
                }
            }
        }
    } else {
        modules.push_back(makeModule(QStringLiteral("tg-jwt"), QStringLiteral("ticket-go"), QStringLiteral("Kimlik doğrulama ve JWT"), ModuleStatus::Unsupported, QStringLiteral("Ops yanıtı JSON değil.")));
    }
    return modules;
}

QList<ModuleSnapshot> evaluateGoChat(const HttpResult& gochat, const HttpResult& parentHealth)
{
    const QList<QPair<QString, QString>> catalog = {
        {QStringLiteral("gochat-api"), QStringLiteral("Go-Chat API")},
        {QStringLiteral("gochat-otp"), QStringLiteral("Telefon / OTP")},
        {QStringLiteral("gochat-session"), QStringLiteral("Oturum yönetimi")},
        {QStringLiteral("gochat-users"), QStringLiteral("Kullanıcı yönetimi")},
        {QStringLiteral("gochat-messaging"), QStringLiteral("Mesajlaşma API")},
        {QStringLiteral("gochat-ws"), QStringLiteral("WebSocket bağlantıları")},
        {QStringLiteral("gochat-send-errors"), QStringLiteral("Mesaj gönderim hataları")},
        {QStringLiteral("gochat-fcm"), QStringLiteral("Bildirim / FCM")},
        {QStringLiteral("gochat-voice"), QStringLiteral("Sesli arama")},
        {QStringLiteral("gochat-video"), QStringLiteral("Görüntülü arama")},
        {QStringLiteral("gochat-turn"), QStringLiteral("TURN / ICE")},
        {QStringLiteral("gochat-sos"), QStringLiteral("SOS acil durum")},
        {QStringLiteral("gochat-location"), QStringLiteral("Konum paylaşımı")},
        {QStringLiteral("gochat-discovery"), QStringLiteral("Kişi keşfi")},
        {QStringLiteral("gochat-chats"), QStringLiteral("Sohbet yönetimi")},
        {QStringLiteral("gochat-retention"), QStringLiteral("Saklama ve temizlik")},
        {QStringLiteral("gochat-deletion"), QStringLiteral("Hesap silme")},
        {QStringLiteral("gochat-audit"), QStringLiteral("Güvenlik ve audit")},
        {QStringLiteral("gochat-android"), QStringLiteral("Android API istekleri")}
    };
    QList<ModuleSnapshot> modules;
    auto pushAll = [&](ModuleStatus status, const QString& evidence) {
        for (const auto& item : catalog) {
            modules.push_back(makeModule(item.first, QStringLiteral("gochat"), item.second, status, evidence, item.first == QLatin1String("gochat-api") ? gochat.latencyMs : -1));
        }
    };
    if (!gochat.attempted) {
        pushAll(ModuleStatus::Checking, QStringLiteral("Go-Chat ölçümü bekleniyor."));
        return modules;
    }
    if (!gochat.transportOk) {
        pushAll(ModuleStatus::Unknown, QStringLiteral("Go-Chat sağlık ucuna ulaşılamadı. Alt özellikler sağlıklı sayılmadı. ") + redactText(gochat.detail));
        if (!modules.isEmpty()) {
            modules[0].status = ModuleStatus::ConnectionError;
            modules[0].connection = connectionLabel(ModuleStatus::ConnectionError);
            modules[0].lastError = modules[0].evidence;
        }
        return modules;
    }
    const bool parentUp = parentHealth.transportOk && parentHealth.status >= 200 && parentHealth.status < 500;
    if (gochat.status == 404 && parentUp) {
        pushAll(ModuleStatus::Disabled, QStringLiteral("GET /api/v1/gochat/health HTTP 404 ve Ticket-Go /health erişilebilir. Kaynak: GOCHAT_ENABLED kapalıyken router mount edilmez. Özellik aktif gösterilmedi."));
        return modules;
    }
    if (gochat.status == 404) {
        pushAll(ModuleStatus::ConnectionError, QStringLiteral("GET /api/v1/gochat/health HTTP 404. Üst API doğrulanamadığı için kapalı modül ile ayırt edilemedi."));
        return modules;
    }
    if (gochat.status >= 500) {
        pushAll(ModuleStatus::Critical, QStringLiteral("GET /api/v1/gochat/health HTTP ") + QString::number(gochat.status) + QStringLiteral("."));
        return modules;
    }
    if (!gochat.jsonOk) {
        modules.push_back(makeModule(QStringLiteral("gochat-api"), QStringLiteral("gochat"), QStringLiteral("Go-Chat API"), ModuleStatus::Warning,
            QStringLiteral("HTTP ") + QString::number(gochat.status) + QStringLiteral(" ancak gövde izleme sözleşmesi değil."), gochat.latencyMs));
        for (int i = 1; i < catalog.size(); ++i) {
            modules.push_back(makeModule(catalog[i].first, QStringLiteral("gochat"), catalog[i].second, ModuleStatus::Unsupported, QStringLiteral("Sağlık gövdesi ayrıştırılamadı.")));
        }
        return modules;
    }
    const QJsonObject body = unwrapPayload(gochat.json);
    const bool enabled = body.value(QStringLiteral("enabled")).toBool(false);
    modules.push_back(makeModule(QStringLiteral("gochat-api"), QStringLiteral("gochat"), QStringLiteral("Go-Chat API"), enabled ? ModuleStatus::Healthy : ModuleStatus::Disabled,
        QStringLiteral("GET /api/v1/gochat/health HTTP ") + QString::number(gochat.status) + QStringLiteral(", enabled=") + (enabled ? QStringLiteral("true") : QStringLiteral("false"))
            + QStringLiteral(", deployment=") + body.value(QStringLiteral("deployment")).toString() + QStringLiteral(". Mesaj içeriği okunmadı."),
        gochat.latencyMs));

    const QString sms = fieldText(body, QStringLiteral("sms"));
    const bool smsProven = body.value(QStringLiteral("smsDeliveryProven")).toBool(false);
    ModuleStatus smsStatus = ModuleStatus::Unsupported;
    QString smsEvidence = QStringLiteral("SMS alanı yok.");
    if (!sms.isEmpty()) {
        if (sms == QLatin1String("unconfigured")) {
            smsStatus = ModuleStatus::ConfigMissing;
            smsEvidence = QStringLiteral("SMS durumu unconfigured. OTP kodu istenmedi.");
        } else if (sms == QLatin1String("test-local")) {
            smsStatus = ModuleStatus::NotReleased;
            smsEvidence = QStringLiteral("SMS durumu test-local. Canlı teslimat gibi gösterilmedi.");
        } else if (smsProven) {
            smsStatus = ModuleStatus::Healthy;
            smsEvidence = QStringLiteral("smsDeliveryProven=true. Kod içeriği yok.");
        } else {
            smsStatus = ModuleStatus::Limited;
            smsEvidence = QStringLiteral("SMS kanalı bildirildi (") + sms + QStringLiteral(") ancak smsDeliveryProven true değil.");
        }
    }
    modules.push_back(makeModule(QStringLiteral("gochat-otp"), QStringLiteral("gochat"), QStringLiteral("Telefon / OTP"), smsStatus, smsEvidence));

    const auto metricGap = [&](const QString& id, const QString& name) {
        modules.push_back(makeModule(id, QStringLiteral("gochat"), name, ModuleStatus::Unsupported,
            QStringLiteral("Sağlık ucu bu alt modül için sayaç döndürmüyor. İçerik, OTP ve konum okunmadı.")));
    };
    metricGap(QStringLiteral("gochat-session"), QStringLiteral("Oturum yönetimi"));
    metricGap(QStringLiteral("gochat-users"), QStringLiteral("Kullanıcı yönetimi"));
    metricGap(QStringLiteral("gochat-messaging"), QStringLiteral("Mesajlaşma API"));
    if (body.value(QStringLiteral("realtime")).toString() == QLatin1String("websocket")) {
        modules.push_back(makeModule(QStringLiteral("gochat-ws"), QStringLiteral("gochat"), QStringLiteral("WebSocket bağlantıları"), ModuleStatus::Limited,
            QStringLiteral("Sağlık ucu realtime=websocket bildiriyor. Açık soket sayısı yok; bağlantı açılmadı.")));
    } else {
        metricGap(QStringLiteral("gochat-ws"), QStringLiteral("WebSocket bağlantıları"));
    }
    metricGap(QStringLiteral("gochat-send-errors"), QStringLiteral("Mesaj gönderim hataları"));

    const QString fcm = fieldText(body, QStringLiteral("fcm"));
    const bool fcmProven = body.value(QStringLiteral("fcmDeliveryProven")).toBool(false);
    if (fcm.isEmpty()) {
        metricGap(QStringLiteral("gochat-fcm"), QStringLiteral("Bildirim / FCM"));
    } else if (fcm == QLatin1String("unconfigured")) {
        modules.push_back(makeModule(QStringLiteral("gochat-fcm"), QStringLiteral("gochat"), QStringLiteral("Bildirim / FCM"), ModuleStatus::ConfigMissing, QStringLiteral("FCM unconfigured. TURN ve SOS ile karıştırılmadı.")));
    } else if (fcmProven) {
        modules.push_back(makeModule(QStringLiteral("gochat-fcm"), QStringLiteral("gochat"), QStringLiteral("Bildirim / FCM"), ModuleStatus::Healthy, QStringLiteral("fcmDeliveryProven=true.")));
    } else {
        modules.push_back(makeModule(QStringLiteral("gochat-fcm"), QStringLiteral("gochat"), QStringLiteral("Bildirim / FCM"), ModuleStatus::Limited,
            QStringLiteral("FCM durumu ") + fcm + QStringLiteral(". Teslimat kanıtı yok; sağlıklı sayılmadı.")));
    }
    metricGap(QStringLiteral("gochat-voice"), QStringLiteral("Sesli arama"));
    metricGap(QStringLiteral("gochat-video"), QStringLiteral("Görüntülü arama"));
    metricGap(QStringLiteral("gochat-turn"), QStringLiteral("TURN / ICE"));
    metricGap(QStringLiteral("gochat-sos"), QStringLiteral("SOS acil durum"));
    metricGap(QStringLiteral("gochat-location"), QStringLiteral("Konum paylaşımı"));
    metricGap(QStringLiteral("gochat-discovery"), QStringLiteral("Kişi keşfi"));
    metricGap(QStringLiteral("gochat-chats"), QStringLiteral("Sohbet yönetimi"));
    metricGap(QStringLiteral("gochat-retention"), QStringLiteral("Saklama ve temizlik"));
    metricGap(QStringLiteral("gochat-deletion"), QStringLiteral("Hesap silme"));
    metricGap(QStringLiteral("gochat-audit"), QStringLiteral("Güvenlik ve audit"));
    metricGap(QStringLiteral("gochat-android"), QStringLiteral("Android API istekleri"));
    return modules;
}

ModuleSnapshot evaluateReachability(const QString& id, const QString& name, const HttpResult& http)
{
    if (!http.attempted) {
        return makeModule(id, QStringLiteral("ticket-go"), name, ModuleStatus::Checking, QStringLiteral("Erişim ölçümü bekleniyor."));
    }
    if (!http.transportOk) {
        return makeModule(id, QStringLiteral("ticket-go"), name, ModuleStatus::ConnectionError, QStringLiteral("HTTP erişimi başarısız. ") + redactText(http.detail), http.latencyMs);
    }
    if (http.status >= 200 && http.status < 400) {
        return makeModule(id, QStringLiteral("ticket-go"), name, ModuleStatus::Limited,
            QStringLiteral("HTTP ") + QString::number(http.status) + QStringLiteral(". Bu yalnızca sayfa erişimidir; uygulamanın tamamı ölçülmedi."), http.latencyMs);
    }
    if (http.status >= 500) {
        return makeModule(id, QStringLiteral("ticket-go"), name, ModuleStatus::Critical, QStringLiteral("HTTP ") + QString::number(http.status) + QStringLiteral("."), http.latencyMs);
    }
    return makeModule(id, QStringLiteral("ticket-go"), name, ModuleStatus::Warning, QStringLiteral("HTTP ") + QString::number(http.status) + QStringLiteral("."), http.latencyMs);
}

QList<ModuleSnapshot> evaluateFerryApp(const HttpResult& http)
{
    const QString id = QStringLiteral("tg-ferry");
    const QString name = QStringLiteral("Feribot bileti satış");
    if (!http.attempted) {
        return {makeModule(id, QStringLiteral("ticket-go"), name, ModuleStatus::Checking, QStringLiteral("Public satış yapılandırması bekleniyor."))};
    }
    if (!http.transportOk) {
        return {makeModule(id, QStringLiteral("ticket-go"), name, ModuleStatus::ConnectionError, QStringLiteral("Satış API erişilemedi. ") + redactText(http.detail), http.latencyMs)};
    }
    if (http.status == 404) {
        return {makeModule(id, QStringLiteral("ticket-go"), name, ModuleStatus::Disabled, QStringLiteral("GET /api/v1/mobile-app/config yok. Satış uygulaması bu uçta bağlı değil."), http.latencyMs)};
    }
    if (http.status == 401 || http.status == 403) {
        return {makeModule(id, QStringLiteral("ticket-go"), name, ModuleStatus::AuthRejected, QStringLiteral("HTTP ") + QString::number(http.status) + QStringLiteral(". Public config kimlik istedi. Değer gösterilmez."), http.latencyMs)};
    }
    if (http.status >= 500) {
        return {makeModule(id, QStringLiteral("ticket-go"), name, ModuleStatus::Critical, QStringLiteral("HTTP ") + QString::number(http.status) + QStringLiteral("."), http.latencyMs)};
    }
    if (http.status < 200 || http.status >= 300 || !http.jsonOk) {
        return {makeModule(id, QStringLiteral("ticket-go"), name, ModuleStatus::Warning, QStringLiteral("HTTP ") + QString::number(http.status) + QStringLiteral(". Public config gövdesi okunamadı. Satış doğrulanmadı."), http.latencyMs)};
    }
    const QJsonObject body = unwrapPayload(http.json);
    const bool hasPublished = body.contains(QStringLiteral("published"));
    const bool published = body.value(QStringLiteral("published")).toBool(false);
    const bool active = !body.contains(QStringLiteral("isActive")) || body.value(QStringLiteral("isActive")).toBool(false);
    const bool maintenance = body.value(QStringLiteral("maintenanceMode")).toBool(false);
    const QString appName = body.value(QStringLiteral("appName")).toString().left(40);
    QString evidence = QStringLiteral("GET /api/v1/mobile-app/config HTTP ") + QString::number(http.status);
    if (!appName.isEmpty()) evidence += QStringLiteral(" · ") + appName;
    if (hasPublished) evidence += published ? QStringLiteral(" · published") : QStringLiteral(" · published=false");
    if (maintenance) evidence += QStringLiteral(" · bakım açık");
    evidence += QStringLiteral(". Bilet satışı ve ödeme çağrılmadı.");
    ModuleStatus status = ModuleStatus::Limited;
    if (!active) status = ModuleStatus::Disabled;
    else if (maintenance) status = ModuleStatus::Warning;
    else if (hasPublished && !published) status = ModuleStatus::NotReleased;
    ModuleSnapshot module = makeModule(id, QStringLiteral("ticket-go"), name, status, evidence, http.latencyMs);
    if (status == ModuleStatus::NotReleased) {
        module.recommendation = QStringLiteral("Public config erişildi ama published=false. Uygulama yayında sayılmadı.");
    } else if (status == ModuleStatus::Limited) {
        module.recommendation = QStringLiteral("Yalnız public config ölçüldü. Sefer arama, bilet ve ödeme sağlıklı kabul edilmez.");
    }
    return {module};
}

ModuleSnapshot evaluateHttpAdapter(const Integration& integration, const HttpResult& http)
{
    if (!http.attempted) {
        return makeModule(QStringLiteral("custom-") + integration.id, QStringLiteral("custom"), integration.name, ModuleStatus::Checking, QStringLiteral("Ölçüm bekleniyor."));
    }
    if (http.errorClass == QLatin1String("ssrf")) {
        return makeModule(QStringLiteral("custom-") + integration.id, QStringLiteral("custom"), integration.name, ModuleStatus::AuthRejected, http.detail);
    }
    if (!http.transportOk) {
        return makeModule(QStringLiteral("custom-") + integration.id, QStringLiteral("custom"), integration.name, ModuleStatus::ConnectionError, redactText(http.detail), http.latencyMs);
    }
    if (http.status == 401 || http.status == 403) {
        return makeModule(QStringLiteral("custom-") + integration.id, QStringLiteral("custom"), integration.name, ModuleStatus::AuthRejected,
            QStringLiteral("HTTP ") + QString::number(http.status) + QStringLiteral(". Kimlik bilgisi kabul edilmedi. Değer gösterilmez."), http.latencyMs);
    }
    if (http.status == integration.expectedStatus || (integration.expectedStatus == 200 && http.status >= 200 && http.status < 300)) {
        ModuleStatus status = ModuleStatus::Healthy;
        if (http.latencyMs > integration.latencyWarnMs) {
            status = ModuleStatus::Warning;
        }
        return makeModule(QStringLiteral("custom-") + integration.id, QStringLiteral("custom"), integration.name, status,
            QStringLiteral("HTTP ") + QString::number(http.status) + QStringLiteral(". Beklenen durum sağlandı. Gövde saklanmadı."), http.latencyMs);
    }
    if (http.status >= 500) {
        return makeModule(QStringLiteral("custom-") + integration.id, QStringLiteral("custom"), integration.name, ModuleStatus::Critical,
            QStringLiteral("HTTP ") + QString::number(http.status) + QStringLiteral("."), http.latencyMs);
    }
    return makeModule(QStringLiteral("custom-") + integration.id, QStringLiteral("custom"), integration.name, ModuleStatus::Warning,
        QStringLiteral("HTTP ") + QString::number(http.status) + QStringLiteral(", beklenen ") + QString::number(integration.expectedStatus) + QStringLiteral("."), http.latencyMs);
}

ContractCheck validateContract(const QJsonObject& object)
{
    ContractCheck check;
    if (object.value(QStringLiteral("schemaVersion")).toString() != QLatin1String("1.0")) {
        check.error = QStringLiteral("schemaVersion 1.0 değil.");
        return check;
    }
    const QString service = object.value(QStringLiteral("service")).toString().trimmed();
    const QString environment = object.value(QStringLiteral("environment")).toString();
    const QString status = object.value(QStringLiteral("status")).toString();
    const QString timestamp = object.value(QStringLiteral("timestamp")).toString();
    if (service.isEmpty() || service.size() > 120) {
        check.error = QStringLiteral("service geçersiz.");
        return check;
    }
    if (environment != QLatin1String("production") && environment != QLatin1String("staging") && environment != QLatin1String("development")) {
        check.error = QStringLiteral("environment geçersiz.");
        return check;
    }
    if (status != QLatin1String("healthy") && status != QLatin1String("degraded") && status != QLatin1String("unhealthy") && status != QLatin1String("unknown")) {
        check.error = QStringLiteral("status geçersiz.");
        return check;
    }
    if (timestamp.size() < 10 || timestamp.size() > 40) {
        check.error = QStringLiteral("timestamp geçersiz.");
        return check;
    }
    if (object.contains(QStringLiteral("metrics"))) {
        if (!object.value(QStringLiteral("metrics")).isObject()) {
            check.error = QStringLiteral("metrics nesne değil.");
            return check;
        }
        const QJsonObject metrics = object.value(QStringLiteral("metrics")).toObject();
        auto number = [&](const QString& key, std::optional<double>* target) {
            if (!metrics.contains(key)) return true;
            if (!metrics.value(key).isDouble()) return false;
            *target = metrics.value(key).toDouble();
            return true;
        };
        if (!number(QStringLiteral("responseTimeMs"), &check.responseMs) || !number(QStringLiteral("errorRate"), &check.errorRate) || !number(QStringLiteral("requestsPerMinute"), &check.rpm)) {
            check.error = QStringLiteral("metrics alan tipi geçersiz.");
            return check;
        }
        if (check.errorRate && (*check.errorRate < 0 || *check.errorRate > 1)) {
            check.error = QStringLiteral("errorRate aralık dışında.");
            return check;
        }
    }
    if (object.contains(QStringLiteral("components"))) {
        if (!object.value(QStringLiteral("components")).isArray()) {
            check.error = QStringLiteral("components dizi değil.");
            return check;
        }
        const QJsonArray components = object.value(QStringLiteral("components")).toArray();
        if (components.size() > 100) {
            check.error = QStringLiteral("components sınırı aşıldı.");
            return check;
        }
        for (const QJsonValue& value : components) {
            if (!value.isObject()) {
                check.error = QStringLiteral("component nesne değil.");
                return check;
            }
            const QJsonObject component = value.toObject();
            const QString name = component.value(QStringLiteral("name")).toString().trimmed();
            const QString componentStatus = component.value(QStringLiteral("status")).toString();
            if (name.isEmpty() || name.size() > 80) {
                check.error = QStringLiteral("component adı geçersiz.");
                return check;
            }
            if (componentStatus != QLatin1String("healthy") && componentStatus != QLatin1String("degraded") && componentStatus != QLatin1String("unhealthy") && componentStatus != QLatin1String("unknown")) {
                check.error = QStringLiteral("component status geçersiz.");
                return check;
            }
            check.components.push_back({name, mapContractStatus(componentStatus)});
        }
    }
    check.ok = true;
    check.overall = mapContractStatus(status);
    return check;
}

QList<ModuleSnapshot> evaluateContract(const Integration& integration, const HttpResult& http)
{
    QList<ModuleSnapshot> modules;
    if (!http.transportOk) {
        modules.push_back(evaluateHttpAdapter(integration, http));
        return modules;
    }
    if (http.status == 401 || http.status == 403) {
        modules.push_back(evaluateHttpAdapter(integration, http));
        return modules;
    }
    if (!http.jsonOk) {
        auto module = makeModule(QStringLiteral("custom-") + integration.id, QStringLiteral("custom"), integration.name, ModuleStatus::Warning,
            QStringLiteral("HTTP ") + QString::number(http.status) + QStringLiteral(" ancak gövde sözleşme JSON'u değil."), http.latencyMs);
        modules.push_back(module);
        return modules;
    }
    const ContractCheck check = validateContract(unwrapPayload(http.json).contains(QStringLiteral("schemaVersion")) ? unwrapPayload(http.json) : http.json);
    if (!check.ok) {
        modules.push_back(makeModule(QStringLiteral("custom-") + integration.id, QStringLiteral("custom"), integration.name, ModuleStatus::Warning,
            QStringLiteral("Sözleşme uyumsuz: ") + check.error, http.latencyMs));
        return modules;
    }
    auto root = makeModule(QStringLiteral("custom-") + integration.id, QStringLiteral("custom"), integration.name, check.overall,
        QStringLiteral("monitoring-contract 1.0 doğrulandı. Bu kayıt canlı örnek veri değildir; uçtan gelen gövdedir."), http.latencyMs);
    root.serviceRpm = check.rpm;
    root.errorRate = check.errorRate;
    if (check.responseMs) {
        root.latencyMs = qint64(*check.responseMs);
    }
    modules.push_back(root);
    for (const auto& component : check.components) {
        modules.push_back(makeModule(QStringLiteral("custom-") + integration.id + QStringLiteral("-") + component.first, QStringLiteral("custom"), integration.name + QStringLiteral(" / ") + component.first, component.second,
            QStringLiteral("Sözleşme bileşeni. İçerik alanı yok.")));
    }
    return modules;
}

QList<OpenApiHint> suggestOpenApi(const QByteArray& json, QString* error)
{
    QJsonParseError parseError;
    const QJsonDocument document = QJsonDocument::fromJson(json, &parseError);
    if (parseError.error != QJsonParseError::NoError || !document.isObject()) {
        if (error) *error = QStringLiteral("OpenAPI JSON okunamadı. YAML bu sürümde içe aktarılmaz ve dosya internetten çekilmez.");
        return {};
    }
    const QJsonObject paths = document.object().value(QStringLiteral("paths")).toObject();
    if (paths.isEmpty()) {
        if (error) *error = QStringLiteral("paths alanı yok. Rastgele adres taranmadı.");
        return {};
    }
    QList<OpenApiHint> hints;
    for (auto it = paths.begin(); it != paths.end(); ++it) {
        const QString path = it.key().toLower();
        if (!(path.contains(QStringLiteral("health")) || path.contains(QStringLiteral("ready")) || path.contains(QStringLiteral("live")) || path.contains(QStringLiteral("status")) || path.contains(QStringLiteral("telemetry")))) {
            continue;
        }
        if (!it.value().isObject()) continue;
        const QJsonObject methods = it.value().toObject();
        for (const QString& method : {QStringLiteral("get"), QStringLiteral("head")}) {
            if (methods.contains(method)) {
                hints.push_back({method.toUpper(), it.key()});
            }
        }
    }
    if (hints.isEmpty() && error) {
        *error = QStringLiteral("Sağlık benzeri uç bulunamadı. Diğer uçlar otomatik izlemeye alınmadı.");
    }
    return hints;
}

Overview summarize(const QList<ModuleSnapshot>& modules, bool hataVar)
{
    Overview overview;
    overview.hataVar = hataVar;
    overview.total = modules.size();
    double latencySum = 0;
    int latencyCount = 0;
    int ratioGood = 0;
    int ratioAll = 0;
    for (const ModuleSnapshot& module : modules) {
        const HealthClass bucket = classifyStatus(module.status);
        if (bucket == HealthClass::Healthy) ++overview.healthy;
        else if (bucket == HealthClass::Warning) ++overview.warning;
        else if (bucket == HealthClass::Critical) ++overview.critical;
        else if (bucket == HealthClass::Disconnected) ++overview.connectionErrors;
        else ++overview.unmonitored;
        if (bucket == HealthClass::NotConfigured) ++overview.notConfigured;
        if (module.status == ModuleStatus::Limited) ++overview.limited;
        if (bucket == HealthClass::Healthy || bucket == HealthClass::Warning || bucket == HealthClass::Critical || bucket == HealthClass::Disconnected) {
            ++ratioAll;
            if (bucket == HealthClass::Healthy) ++ratioGood;
        }
        if (module.latencyMs >= 0 && (module.status == ModuleStatus::Healthy || module.status == ModuleStatus::Warning || module.status == ModuleStatus::Limited || module.status == ModuleStatus::Critical)) {
            latencySum += double(module.latencyMs);
            ++latencyCount;
        }
        if (module.id == QLatin1String("tg-queues") && module.status != ModuleStatus::NotConfigured && module.status != ModuleStatus::Checking) {
            overview.queueText = module.evidence.left(120);
        }
    }
    if (ratioAll > 0) overview.healthRatio = double(ratioGood) / double(ratioAll);
    if (latencyCount > 0) overview.avgLatencyMs = latencySum / double(latencyCount);
    overview.openCritical = hataVar ? 1 : 0;
    return overview;
}

} // namespace tg
