#include "ModuleLinks.h"

namespace tg {

QString moduleIdForIntegration(const QString& integrationId)
{
    if (integrationId == QLatin1String("ticket-go-backend")) return QStringLiteral("tg-api");
    if (integrationId == QLatin1String("go-chat-api")) return QStringLiteral("gochat-api");
    if (integrationId == QLatin1String("ticket-go-ferry")) return QStringLiteral("tg-ferry");
    if (integrationId == QLatin1String("ticket-go-web")) return QStringLiteral("tg-web");
    if (integrationId == QLatin1String("ticket-go-admin")) return QStringLiteral("tg-admin");
    return QStringLiteral("custom-") + integrationId;
}

QString integrationIdForModule(const QString& moduleId, const QStringList& integrationIds)
{
    if (moduleId.startsWith(QLatin1String("gochat"))) return QStringLiteral("go-chat-api");
    if (moduleId == QLatin1String("tg-ferry")) return QStringLiteral("ticket-go-ferry");
    if (moduleId == QLatin1String("tg-web")) return QStringLiteral("ticket-go-web");
    if (moduleId == QLatin1String("tg-admin")) return QStringLiteral("ticket-go-admin");
    if (moduleId.startsWith(QLatin1String("tg-"))) return QStringLiteral("ticket-go-backend");
    if (!moduleId.startsWith(QLatin1String("custom-"))) return {};
    const QString rest = moduleId.mid(7);
    QString best;
    for (const QString& id : integrationIds) {
        if (rest == id || rest.startsWith(id + QLatin1Char('-'))) {
            if (id.size() > best.size()) best = id;
        }
    }
    return best;
}

QString logSourceForModule(const QString& moduleId)
{
    if (moduleId == QLatin1String("tg-api")) return QStringLiteral("system");
    if (moduleId == QLatin1String("tg-jwt")) return QStringLiteral("auth");
    if (moduleId == QLatin1String("tg-mfa")) return QStringLiteral("mail");
    if (moduleId == QLatin1String("tg-admin")) return QStringLiteral("api");
    if (moduleId == QLatin1String("tg-workers") || moduleId == QLatin1String("tg-queues")) return QStringLiteral("queue");
    if (moduleId == QLatin1String("tg-cron")) return QStringLiteral("scheduler");
    if (moduleId == QLatin1String("tg-iyzico")) return QStringLiteral("payments");
    if (moduleId == QLatin1String("tg-ferry") || moduleId == QLatin1String("tg-liknoss")) return QStringLiteral("liknoss");
    if (moduleId == QLatin1String("tg-accounting")) return QStringLiteral("accounting");
    if (moduleId == QLatin1String("tg-netgsm")) return QStringLiteral("sms");
    if (moduleId == QLatin1String("gochat-fcm")) return QStringLiteral("push");
    if (moduleId == QLatin1String("gochat-audit")) return QStringLiteral("audit");
    return {};
}

QStringList childModuleIds(const QString& moduleId)
{
    if (moduleId == QLatin1String("tg-api")) {
        return {
            QStringLiteral("tg-postgres"), QStringLiteral("tg-redis"), QStringLiteral("tg-workers"),
            QStringLiteral("tg-queues"), QStringLiteral("tg-cron"), QStringLiteral("tg-jwt"),
            QStringLiteral("tg-iyzico"), QStringLiteral("tg-netgsm"), QStringLiteral("tg-mfa"),
            QStringLiteral("tg-liknoss"), QStringLiteral("tg-accounting"), QStringLiteral("tg-nginx"),
            QStringLiteral("tg-docker"), QStringLiteral("tg-turmob"), QStringLiteral("tg-luca")
        };
    }
    if (moduleId == QLatin1String("gochat-api")) {
        return {
            QStringLiteral("gochat-otp"), QStringLiteral("gochat-ws"), QStringLiteral("gochat-fcm"),
            QStringLiteral("gochat-turn"), QStringLiteral("gochat-sos"), QStringLiteral("gochat-messaging"),
            QStringLiteral("gochat-voice"), QStringLiteral("gochat-video"), QStringLiteral("gochat-session")
        };
    }
    return {};
}

} // namespace tg
