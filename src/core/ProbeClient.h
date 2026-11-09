#pragma once

#include "OpsTypes.h"

#include <QHash>
#include <QHostInfo>
#include <QNetworkAccessManager>
#include <functional>

namespace tg {

struct ProbeRequest {
    QString url;
    QString method = QStringLiteral("GET");
    QByteArray body;
    int timeoutMs = 8000;
    QString authType = QStringLiteral("none");
    QString secret;
    QString apiKeyHeader = QStringLiteral("X-Api-Key");
    QString certPem;
    QString keyPem;
    QString headersJson = QStringLiteral("[]");
    QString contentType;
    bool allowPrivate = false;
    QStringList extraHosts;
};

class ProbeClient : public QObject {
    Q_OBJECT
public:
    explicit ProbeClient(QObject* parent = nullptr);
    void execute(ProbeRequest request, std::function<void(HttpResult)> done);
    int inFlight() const;

private slots:
    void onLookedUp(const QHostInfo& info);

private:
    struct Pending {
        ProbeRequest request;
        std::function<void(HttpResult)> done;
        int redirects = 0;
        bool authApplied = false;
    };
    void send(Pending pending);
    QNetworkAccessManager network_;
    QHash<int, Pending> lookups_;
    int inFlight_ = 0;
};

} // namespace tg
