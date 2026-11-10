#include "ApiCatalog.h"

namespace tg {
namespace {

struct EndpointDef {
    const char* method;
    const char* path;
    const char* summary;
    const char* params;
    const char* responses;
    const char* auth;
    bool shareable;
};

const EndpointDef kBackend[] = {
    {"GET", "/health", "Süreç sağlık ucu. İzleyici bunu kimliksiz okur.", "Yok", "İzleyici 2xx ve 5xx yanıtlarını ayırır.", "Gerekmez", true},
    {"GET", "/health/live", "Canlılık ucu. İzleyici bunu kimliksiz okur.", "Yok", "İzleyici 2xx ve 5xx yanıtlarını ayırır.", "Gerekmez", true},
    {"GET", "/health/ready", "Hazır olma ucu. Alt servis alanları buradan okunur.", "Yok", "İzleyici HTTP 200 ve HTTP 503 gövdesini ayırır.", "Gerekmez", true},
    {"GET", "/api/v1/health", "Kimlik ucu. Sürüm alanı varsa kanıta yazılır.", "Yok", "İzleyici JSON gövdedeki version alanını okur. Alan yoksa sürüm gösterilmez.", "Gerekmez", true},
    {"GET", "/health/dependencies", "Bağımlılık ucu. Yalnız izleme jetonu tanımlıysa istenir.", "Yok", "Gövde saklanmaz. Başlık değeri gösterilmez.", "X-Ops-Token. Değer gösterilmez.", false},
    {"POST", "/api/v1/auth/login", "İzleme oturumu için kimlik girişi.", "JSON: username, password. Değerler gösterilmez.", "HTTP 200 tek başına başarı sayılmaz. Jeton gösterilmez.", "Gövde. Değerler gösterilmez.", false},
    {"POST", "/api/v1/auth/mfa/verify", "İzleme oturumunun ikinci adımı.", "JSON doğrulama kodu. Değer gösterilmez.", "HTTP 200 tek başına başarı sayılmaz. Jeton gösterilmez.", "Oturum. Değer gösterilmez.", false},
    {"POST", "/api/v1/auth/mfa/email-otp/resend", "E-posta doğrulama kodunu yeniden ister.", "Yok", "Kod ve alıcı gösterilmez.", "Oturum. Değer gösterilmez.", false},
    {"POST", "/api/v1/auth/refresh", "İzleme oturumunu yeniler.", "Kayıtlı yenileme jetonu. Değer gösterilmez.", "Jeton gösterilmez.", "Oturum. Değer gösterilmez.", false},
    {"POST", "/api/v1/auth/logout", "İzleme oturumunu kapatır.", "Yok", "Jeton gösterilmez.", "Oturum. Değer gösterilmez.", false},
    {"GET", "/api/v1/auth/me", "Açık izleme oturumunun kimliğini okur.", "Yok", "Hesap adı dışındaki profil alanları bu katalogda yok.", "Oturum. Değer gösterilmez.", false},
    {"GET", "/api/v1/system-logs", "Sistem loglarını sayfalar. logs.read gerekir.", "page, pageSize ve izleyicinin gönderdiği süzgeçler. Değerler müşteri kaydı değildir.", "Gövde kişisel veri olarak bu ekrana yazılmaz.", "Oturum ve logs.read. Jeton gösterilmez.", false},
    {"GET", "/api/v1/system-logs/live", "Sistem loglarının canlı penceresi.", "since, limit. Değerler gösterilmez.", "Gövde bu ekrana yazılmaz.", "Oturum ve logs.read. Jeton gösterilmez.", false}
};

const EndpointDef kGoChat[] = {
    {"GET", "/api/v1/gochat/health", "Go-Chat sağlık ucu. Mesaj ve kullanıcı içeriği okunmaz.", "Yok", "İzleyici enabled alanını ve HTTP kodunu ayırır. HTTP 404 üst API ile karşılaştırılır.", "Bu ölçümde gerekmez", true}
};

const EndpointDef kFerry[] = {
    {"GET", "/api/v1/mobile-app/config", "Public uygulama yapılandırması. Sefer, bilet ve ödeme çağrılmaz.", "platform, version. İzleyici platform=ios ve version=1.0.0 gönderir.", "İzleyici HTTP 200, 401, 403, 404 ve 5xx kodlarını ayırır. HTTP 200 satış kanıtı değildir.", "Bu ölçümde gerekmez", true}
};

const char* kSaleGaps[] = {
    "Sefer arama",
    "Güzergâhlar",
    "Tarih ve sefer saatleri",
    "Fiyat bilgileri",
    "Müsaitlik",
    "Rezervasyon",
    "Bilet oluşturma",
    "Bilet sorgulama",
    "İptal ve iade"
};

const ModuleSnapshot* findModule(const QList<ModuleSnapshot>& modules, const QString& id)
{
    for (const ModuleSnapshot& module : modules) {
        if (module.id == id) return &module;
    }
    return nullptr;
}

QString measuredVersion(const ModuleSnapshot* module)
{
    if (!module) return QStringLiteral("—");
    const QString marker = QStringLiteral("Sürüm alanı: ");
    const int at = module->evidence.indexOf(marker);
    if (at < 0) return QStringLiteral("—");
    QString value = module->evidence.mid(at + marker.size()).trimmed();
    if (value.endsWith(QLatin1Char('.'))) value.chop(1);
    value = value.trimmed();
    if (value.isEmpty() || value.size() > 40) return QStringLiteral("—");
    return value;
}

void paintStatus(ModuleStatus status, QString* label, QString* tone)
{
    switch (status) {
    case ModuleStatus::Healthy: *label = QStringLiteral("Çalışıyor"); *tone = QStringLiteral("green"); return;
    case ModuleStatus::Limited: *label = QStringLiteral("Kısmi sinyal"); *tone = QStringLiteral("amber"); return;
    case ModuleStatus::Warning: *label = QStringLiteral("Uyarı"); *tone = QStringLiteral("amber"); return;
    case ModuleStatus::Critical: *label = QStringLiteral("Çalışmıyor"); *tone = QStringLiteral("red"); return;
    case ModuleStatus::Disabled: *label = QStringLiteral("Devre dışı"); *tone = QStringLiteral("gray"); return;
    case ModuleStatus::ConnectionError: *label = QStringLiteral("Bağlantı hatası"); *tone = QStringLiteral("red"); return;
    case ModuleStatus::Checking: *label = QStringLiteral("Kontrol ediliyor"); *tone = QStringLiteral("gray"); return;
    case ModuleStatus::AuthRejected: *label = QStringLiteral("Kimlik reddedildi"); *tone = QStringLiteral("amber"); return;
    case ModuleStatus::NotReleased: *label = QStringLiteral("Henüz yayınlanmadı"); *tone = QStringLiteral("gray"); return;
    case ModuleStatus::ConfigMissing: *label = QStringLiteral("Yapılandırma eksik"); *tone = QStringLiteral("amber"); return;
    case ModuleStatus::Unknown:
    case ModuleStatus::NotConfigured:
    case ModuleStatus::Unsupported: *label = QStringLiteral("Ölçüm yok"); *tone = QStringLiteral("gray"); return;
    }
    *label = QStringLiteral("Ölçüm yok");
    *tone = QStringLiteral("gray");
}

QVariantMap endpointMap(const EndpointDef& def)
{
    QVariantMap map;
    map.insert(QStringLiteral("method"), QString::fromLatin1(def.method));
    map.insert(QStringLiteral("path"), QString::fromLatin1(def.path));
    map.insert(QStringLiteral("summary"), QString::fromUtf8(def.summary));
    map.insert(QStringLiteral("params"), QString::fromUtf8(def.params));
    map.insert(QStringLiteral("responses"), QString::fromUtf8(def.responses));
    map.insert(QStringLiteral("auth"), QString::fromUtf8(def.auth));
    map.insert(QStringLiteral("shareable"), def.shareable);
    map.insert(QStringLiteral("audienceLabel"), def.shareable ? QStringLiteral("Paylaşılabilir") : QStringLiteral("İç izleme"));
    map.insert(QStringLiteral("exampleRequest"), QStringLiteral("Bilgi mevcut değil"));
    map.insert(QStringLiteral("exampleResponse"), QStringLiteral("Bilgi mevcut değil"));
    return map;
}

QVariantList endpointsFrom(const EndpointDef* defs, int count, int* shareable, int* internal)
{
    QVariantList list;
    for (int i = 0; i < count; ++i) {
        list.push_back(endpointMap(defs[i]));
        if (defs[i].shareable) ++(*shareable);
        else ++(*internal);
    }
    return list;
}

QVariantMap product(const QString& id, const QString& name, const QString& initials, const QString& summary, const QString& logo, const QString& integrationId, const QString& moduleId, const ModuleSnapshot* module, const QString& environmentText, const QVariantList& endpoints, int endpointCount)
{
    QString label;
    QString tone;
    if (!module) {
        label = QStringLiteral("Ölçüm yok");
        tone = QStringLiteral("gray");
    } else if (id == QLatin1String("ferry")) {
        if (module->status == ModuleStatus::ConnectionError) {
            label = QStringLiteral("Bağlantı hatası");
            tone = QStringLiteral("red");
        } else if (module->status == ModuleStatus::Checking) {
            label = QStringLiteral("Kontrol ediliyor");
            tone = QStringLiteral("gray");
        } else {
            label = QStringLiteral("Ölçüm yok");
            tone = QStringLiteral("gray");
        }
    } else {
        paintStatus(module->status, &label, &tone);
    }
    QString connection = QStringLiteral("Ölçüm yok");
    if (module) {
        if (module->status == ModuleStatus::ConnectionError) connection = QStringLiteral("Bağlantı hatası");
        else if (module->status == ModuleStatus::Checking) connection = QStringLiteral("Kontrol ediliyor");
        else if (module->status == ModuleStatus::Unknown || module->status == ModuleStatus::NotConfigured) connection = connectionLabel(module->status);
        else if (id == QLatin1String("ferry")) connection = QStringLiteral("Bağlı");
        else connection = module->connection.isEmpty() ? connectionLabel(module->status) : module->connection;
    }
    QVariantMap map;
    map.insert(QStringLiteral("id"), id);
    map.insert(QStringLiteral("name"), name);
    map.insert(QStringLiteral("initials"), initials);
    map.insert(QStringLiteral("summary"), summary);
    map.insert(QStringLiteral("baseUrl"), QStringLiteral("https://api.ticket-go.net"));
    map.insert(QStringLiteral("logo"), logo);
    map.insert(QStringLiteral("integrationId"), integrationId);
    map.insert(QStringLiteral("moduleId"), moduleId);
    map.insert(QStringLiteral("version"), id == QLatin1String("ticketgo") ? measuredVersion(module) : QStringLiteral("—"));
    map.insert(QStringLiteral("statusLabel"), label);
    map.insert(QStringLiteral("tone"), tone);
    map.insert(QStringLiteral("verifiedHealthy"), label == QStringLiteral("Çalışıyor"));
    map.insert(QStringLiteral("connection"), connection);
    map.insert(QStringLiteral("lastCheck"), module && module->lastCheck.isValid() ? module->lastCheck.toString(QStringLiteral("yyyy-MM-dd HH:mm:ss")) : QStringLiteral("—"));
    map.insert(QStringLiteral("evidence"), module ? module->evidence : QStringLiteral("Güncel ölçüm yok. Eski kayıt sağlık kanıtı sayılmadı."));
    map.insert(QStringLiteral("configLabel"), module ? statusLabel(module->status) : QStringLiteral("Ölçüm yok"));
    map.insert(QStringLiteral("environment"), environmentText.trimmed().isEmpty() ? QStringLiteral("Ölçülmedi") : environmentText);
    map.insert(QStringLiteral("docAvailable"), false);
    map.insert(QStringLiteral("docReason"), QStringLiteral("Bu projede paylaşılabilir OpenAPI veya dış doküman adresi yok."));
    map.insert(QStringLiteral("endpoints"), endpoints);
    map.insert(QStringLiteral("endpointCount"), endpointCount);
    return map;
}

} // namespace

QVariantMap buildApiCenter(const QList<ModuleSnapshot>& modules, const QString& environmentText)
{
    int shareable = 0;
    int internal = 0;
    const QVariantList backendEndpoints = endpointsFrom(kBackend, int(sizeof kBackend / sizeof kBackend[0]), &shareable, &internal);
    const QVariantList goChatEndpoints = endpointsFrom(kGoChat, int(sizeof kGoChat / sizeof kGoChat[0]), &shareable, &internal);
    const QVariantList ferryEndpoints = endpointsFrom(kFerry, int(sizeof kFerry / sizeof kFerry[0]), &shareable, &internal);
    const QString root = QStringLiteral("qrc:/qt/qml/TicketGo/Ops/qml/assets/");
    QVariantMap ticketGo = product(QStringLiteral("ticketgo"), QStringLiteral("Ticket-Go Backend API"), QStringLiteral("TG"),
        QStringLiteral("Ticket-Go üretim API'sinin ölçülen sağlık ve iç izleme uçları. Sağlık yanıtı ticari işlemin başarısı değildir."),
        root + QStringLiteral("logo-ticketgo.png"), QStringLiteral("ticket-go-backend"), QStringLiteral("tg-api"), findModule(modules, QStringLiteral("tg-api")), environmentText, backendEndpoints, backendEndpoints.size());
    ticketGo.insert(QStringLiteral("auth"), QStringLiteral("Sağlık uçları kimliksizdir. Log ve oturum uçları izleme oturumu ister. Jeton gösterilmez."));
    QVariantMap goChat = product(QStringLiteral("gochat"), QStringLiteral("Go-Chat API"), QStringLiteral("GC"),
        QStringLiteral("Go-Chat sağlık ucu. Kullanıcı, mesaj ve özel görüşme içeriği bu ekranda yoktur."),
        root + QStringLiteral("logo-gochat-mark.png"), QStringLiteral("go-chat-api"), QStringLiteral("gochat-api"), findModule(modules, QStringLiteral("gochat-api")), environmentText, goChatEndpoints, goChatEndpoints.size());
    goChat.insert(QStringLiteral("auth"), QStringLiteral("Bu sağlık ölçümü kimliksizdir. İçerik uçları katalogda yoktur."));
    QVariantMap ferry = product(QStringLiteral("ferry"), QStringLiteral("Ticket-Go Bilet Satış API"), QStringLiteral("FB"),
        QStringLiteral("Feribot bilet satışı. Ölçülen uç public uygulama yapılandırmasıdır. Satış işlemi doğrulanmadı."),
        root + QStringLiteral("logo-ferry.png"), QStringLiteral("ticket-go-ferry"), QStringLiteral("tg-ferry"), findModule(modules, QStringLiteral("tg-ferry")), environmentText, ferryEndpoints, ferryEndpoints.size());
    ferry.insert(QStringLiteral("auth"), QStringLiteral("Yapılandırma ölçümü kimliksizdir. HTTP 401 veya 403 kimlik istediğini gösterir; değer yazılmaz."));
    ferry.insert(QStringLiteral("providerNote"), QStringLiteral("Dentur aktif satış sağlayıcısı olarak bu katalogda yazılıdır. Ayrı bir Dentur sağlık ölçümü yoktur."));
    const ModuleSnapshot* liknoss = findModule(modules, QStringLiteral("tg-liknoss"));
    ferry.insert(QStringLiteral("liknossLabel"), liknoss ? statusLabel(liknoss->status) : QStringLiteral("Ölçüm yok"));
    ferry.insert(QStringLiteral("liknossNote"), QStringLiteral("Liknoss API bilgisi bu izlemede satış ucu olarak yok. Liknoss devre dışı olsa da feribot satış API'sinin tamamı çalışmıyor sayılmaz."));
    QVariantList gaps;
    for (const char* name : kSaleGaps) {
        QVariantMap gap;
        gap.insert(QStringLiteral("name"), QString::fromUtf8(name));
        gap.insert(QStringLiteral("state"), QStringLiteral("Doğrulanmış uç yok"));
        gaps.push_back(gap);
    }
    ferry.insert(QStringLiteral("capabilities"), gaps);

    QVariantList apis;
    apis << ticketGo << goChat << ferry;
    int active = 0;
    for (const QVariant& item : apis) {
        if (item.toMap().value(QStringLiteral("verifiedHealthy")).toBool()) ++active;
    }
    QVariantMap center;
    center.insert(QStringLiteral("groupCount"), apis.size());
    center.insert(QStringLiteral("activeCount"), active);
    center.insert(QStringLiteral("partnerText"), QStringLiteral("—"));
    center.insert(QStringLiteral("partnerState"), QStringLiteral("Henüz yapılandırılmadı"));
    center.insert(QStringLiteral("partnerNote"), QStringLiteral("İş ortağı kaydı, erişim talebi, anahtar, kota ve onay için ayrı bir yönetim API'si gerekir. Bu uygulamada üretilmez."));
    center.insert(QStringLiteral("shareableCount"), shareable);
    center.insert(QStringLiteral("internalCount"), internal);
    center.insert(QStringLiteral("apis"), apis);
    return center;
}

bool apiShareableText(const QString& text)
{
    const QString value = text.trimmed();
    if (value.isEmpty() || value.size() > 180) return false;
    if (value.contains(QStringLiteral("Bearer"), Qt::CaseInsensitive) || value.contains(QStringLiteral("token"), Qt::CaseInsensitive) || value.contains(QStringLiteral("password"), Qt::CaseInsensitive) || value.contains(QStringLiteral("secret"), Qt::CaseInsensitive)) return false;
    const QVariantMap center = buildApiCenter({}, QString());
    if (value == QLatin1String("https://api.ticket-go.net")) return true;
    const QVariantList apis = center.value(QStringLiteral("apis")).toList();
    for (const QVariant& apiValue : apis) {
        const QVariantList endpoints = apiValue.toMap().value(QStringLiteral("endpoints")).toList();
        for (const QVariant& endpointValue : endpoints) {
            const QVariantMap endpoint = endpointValue.toMap();
            if (!endpoint.value(QStringLiteral("shareable")).toBool()) continue;
            if (endpoint.value(QStringLiteral("path")).toString() == value) return true;
        }
    }
    return false;
}

} // namespace tg
