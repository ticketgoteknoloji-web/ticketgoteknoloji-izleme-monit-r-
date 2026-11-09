#include "ProbeClient.h"

#include <QHostInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSslCertificate>
#include <QSslKey>
#include <QTimer>
#include <QUrl>

namespace tg {

ProbeClient::ProbeClient(QObject* parent) : QObject(parent) {}

int ProbeClient::inFlight() const { return inFlight_; }

void ProbeClient::execute(ProbeRequest request, std::function<void(HttpResult)> done)
{
    HttpResult early;
    early.url = request.url;
    early.attempted = true;
    const QUrl url(request.url);
    const SsrfDecision decision = precheckUrl(url, request.extraHosts, request.allowPrivate);
    if (!decision.ok) {
        early.errorClass = QStringLiteral("ssrf");
        early.detail = decision.reason;
        done(early);
        return;
    }
    Pending pending;
    pending.request = request;
    pending.done = std::move(done);
    const int id = QHostInfo::lookupHost(url.host(), this, SLOT(onLookedUp(QHostInfo)));
    lookups_.insert(id, pending);
    ++inFlight_;
}

void ProbeClient::onLookedUp(const QHostInfo& info)
{
    if (!lookups_.contains(info.lookupId())) return;
    Pending pending = lookups_.take(info.lookupId());
    --inFlight_;
    if (info.error() != QHostInfo::NoError || info.addresses().isEmpty()) {
        HttpResult result;
        result.url = pending.request.url;
        result.attempted = true;
        result.errorClass = QStringLiteral("dns");
        result.detail = QStringLiteral("DNS çözümlenemedi.");
        pending.done(result);
        return;
    }
    for (const QHostAddress& address : info.addresses()) {
        if (addressBlocked(address, pending.request.allowPrivate)) {
            HttpResult result;
            result.url = pending.request.url;
            result.attempted = true;
            result.errorClass = QStringLiteral("ssrf");
            result.detail = QStringLiteral("Çözülen adres engellendi.");
            pending.done(result);
            return;
        }
    }
    ++inFlight_;
    send(pending);
}

void ProbeClient::send(Pending pending)
{
    QNetworkRequest request{QUrl(pending.request.url)};
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("TicketGoOps/0.1 (read-only)"));
    request.setTransferTimeout(pending.request.timeoutMs);
    const QJsonArray headers = QJsonDocument::fromJson(pending.request.headersJson.toUtf8()).array();
    for (const QJsonValue& value : headers) {
        const QString name = value.toObject().value(QStringLiteral("name")).toString().trimmed();
        const QString headerValue = value.toObject().value(QStringLiteral("value")).toString();
        const QString lower = name.toLower();
        if (name.isEmpty() || name.contains(QLatin1Char('\n')) || name.contains(QLatin1Char('\r')) || headerValue.contains(QLatin1Char('\n'))) continue;
        if (lower == QLatin1String("authorization") || lower == QLatin1String("cookie") || lower == QLatin1String("host") || lower.contains(QStringLiteral("api-key")) || lower.contains(QStringLiteral("token"))) continue;
        request.setRawHeader(name.toUtf8(), headerValue.toUtf8());
    }
    if (pending.request.authType == QLatin1String("bearer") && !pending.request.secret.isEmpty()) {
        request.setRawHeader("Authorization", QByteArray("Bearer ") + pending.request.secret.toUtf8());
        pending.authApplied = true;
    } else if (pending.request.authType == QLatin1String("basic") && !pending.request.secret.isEmpty()) {
        QString authError;
        const QByteArray header = basicAuthorizationValue(pending.request.secret, &authError);
        if (header.isEmpty()) {
            --inFlight_;
            HttpResult result;
            result.url = pending.request.url;
            result.attempted = true;
            result.errorClass = QStringLiteral("auth");
            result.detail = authError;
            pending.done(result);
            return;
        }
        request.setRawHeader("Authorization", header);
        pending.authApplied = true;
    } else if (pending.request.authType == QLatin1String("api_key") && !pending.request.secret.isEmpty()) {
        const QString header = pending.request.apiKeyHeader.trimmed().isEmpty() ? QStringLiteral("X-Api-Key") : pending.request.apiKeyHeader.trimmed();
        request.setRawHeader(header.toUtf8(), pending.request.secret.toUtf8());
        pending.authApplied = true;
    } else if (pending.request.authType == QLatin1String("mtls")) {
        const QList<QSslCertificate> certs = QSslCertificate::fromData(pending.request.certPem.toUtf8(), QSsl::Pem);
        QSslKey key(pending.request.keyPem.toUtf8(), QSsl::Rsa, QSsl::Pem);
        if (key.isNull()) key = QSslKey(pending.request.keyPem.toUtf8(), QSsl::Ec, QSsl::Pem);
        if (certs.isEmpty() || key.isNull()) {
            --inFlight_;
            HttpResult result;
            result.url = pending.request.url;
            result.attempted = true;
            result.errorClass = QStringLiteral("tls");
            result.detail = QStringLiteral("mTLS sertifikası veya anahtarı okunamadı.");
            pending.done(result);
            return;
        }
        QSslConfiguration ssl = QSslConfiguration::defaultConfiguration();
        ssl.setLocalCertificate(certs.first());
        ssl.setPrivateKey(key);
        ssl.setPeerVerifyMode(QSslSocket::VerifyPeer);
        request.setSslConfiguration(ssl);
    }
    QNetworkReply* reply = nullptr;
    const QString method = pending.request.method.toUpper();
    if (method == QLatin1String("HEAD")) reply = network_.head(request);
    else if (method == QLatin1String("POST")) {
        request.setHeader(QNetworkRequest::ContentTypeHeader, pending.request.contentType.isEmpty() ? QStringLiteral("application/x-www-form-urlencoded") : pending.request.contentType);
        reply = network_.post(request, pending.request.body);
        pending.request.body.fill('\0');
        pending.request.body.clear();
    } else reply = network_.get(request);
    reply->setProperty("tg_started", QDateTime::currentMSecsSinceEpoch());
    auto* timer = new QTimer(reply);
    timer->setSingleShot(true);
    timer->start(pending.request.timeoutMs + 250);
    QObject::connect(timer, &QTimer::timeout, reply, [reply]() { reply->abort(); });
    QObject::connect(reply, &QNetworkReply::finished, this, [this, reply, pending]() mutable {
        --inFlight_;
        const QVariant redirect = reply->attribute(QNetworkRequest::RedirectionTargetAttribute);
        if (redirect.isValid() && pending.redirects < 2 && reply->error() == QNetworkReply::NoError) {
            const QUrl next = reply->url().resolved(redirect.toUrl());
            const SsrfDecision decision = precheckUrl(next, pending.request.extraHosts, pending.request.allowPrivate);
            reply->deleteLater();
            if (!decision.ok) {
                HttpResult result;
                result.attempted = true;
                result.url = next.toString(QUrl::RemoveUserInfo);
                result.errorClass = QStringLiteral("ssrf");
                result.detail = decision.reason;
                pending.done(result);
                return;
            }
            if (next.host().compare(QUrl(pending.request.url).host(), Qt::CaseInsensitive) != 0) {
                pending.request.secret.clear();
                pending.request.authType = QStringLiteral("none");
            }
            pending.request.url = next.toString(QUrl::RemoveUserInfo);
            pending.redirects += 1;
            ++inFlight_;
            send(pending);
            return;
        }
        HttpResult result;
        result.attempted = true;
        result.url = reply->url().toString(QUrl::RemoveUserInfo | QUrl::RemoveQuery);
        result.latencyMs = reply->property("tg_started").toLongLong() > 0 ? QDateTime::currentMSecsSinceEpoch() - reply->property("tg_started").toLongLong() : -1;
        if (reply->property("tg_tls").toBool() || reply->error() == QNetworkReply::SslHandshakeFailedError) {
            result.errorClass = QStringLiteral("tls");
            result.detail = QStringLiteral("TLS doğrulaması başarısız. Sertifika denetimi kapatılmadı.");
        } else if (reply->error() == QNetworkReply::OperationCanceledError) {
            result.errorClass = QStringLiteral("timeout");
            result.detail = QStringLiteral("İstek zaman aşımına uğradı.");
        } else if (reply->error() != QNetworkReply::NoError && reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).isNull()) {
            result.errorClass = QStringLiteral("connection");
            result.detail = QStringLiteral("Bağlantı kurulamadı.");
        } else {
            result.transportOk = true;
            result.status = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
            const QByteArray body = reply->readAll().left(262144);
            const QJsonDocument document = QJsonDocument::fromJson(body);
            if (document.isObject()) {
                result.jsonOk = true;
                result.json = document.object();
            }
            if (result.status >= 400) result.detail = QStringLiteral("HTTP ") + QString::number(result.status);
        }
        reply->deleteLater();
        pending.request.secret.fill(QChar(0));
        pending.request.secret.clear();
        pending.request.keyPem.fill(QChar(0));
        pending.request.keyPem.clear();
        pending.done(result);
    });
}

} // namespace tg
