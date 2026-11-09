#include "PathProbe.h"

#include <QElapsedTimer>
#include <QProcess>
#include <QRegularExpression>
#include <QTcpSocket>
#include <QTimer>

namespace tg {
namespace {
constexpr auto kHost = "api.ticket-go.net";
constexpr int kPort = 443;
} // namespace

PingRead readPingOutput(const QString& text)
{
    PingRead read;
    const QRegularExpression timeMatch(QStringLiteral("time[=<]\\s*([0-9]+(?:\\.[0-9]+)?)\\s*ms"));
    const QRegularExpression lossMatch(QStringLiteral("([0-9]+(?:\\.[0-9]+)?)%\\s+packet loss"));
    const QRegularExpressionMatch time = timeMatch.match(text);
    const QRegularExpressionMatch loss = lossMatch.match(text);
    if (loss.hasMatch()) read.lossPercent = qRound(loss.captured(1).toDouble());
    if (time.hasMatch() && read.lossPercent != 100) {
        read.replied = true;
        read.latencyMs = time.captured(1).toDouble();
    }
    return read;
}

PathProbe::PathProbe(QObject* parent)
    : QObject(parent)
{}

void PathProbe::finishOne()
{
    pending_ = qMax(0, pending_ - 1);
    if (pending_ == 0) busy_ = false;
}

void PathProbe::start(const std::function<void(PathResult)>& done)
{
    if (busy_) return;
    busy_ = true;
    done_ = done;
    pending_ = 2;

    auto* ping = new QProcess(this);
    ping->setProgram(QStringLiteral("/sbin/ping"));
    ping->setArguments({QStringLiteral("-c"), QStringLiteral("1"), QStringLiteral("-W"), QStringLiteral("2000"), QString::fromLatin1(kHost)});
    auto reportPing = [this, ping]() {
        if (ping->property("tg_done").toBool()) return;
        ping->setProperty("tg_done", true);
        const QString output = QString::fromUtf8(ping->readAllStandardOutput() + ping->readAllStandardError());
        ping->deleteLater();
        const PingRead read = readPingOutput(output);
        PathResult result;
        result.kind = QStringLiteral("icmp");
        result.lossPercent = read.lossPercent;
        if (read.replied && read.latencyMs >= 0) {
            result.latencyMs = qRound64(read.latencyMs);
            result.evidence = QStringLiteral("ICMP gecikmesi. Paket kaybı %1. Servis durumu değiştirilmedi.").arg(read.lossPercent < 0 ? 0 : read.lossPercent);
        } else {
            result.timeout = true;
            result.latencyMs = -1;
            result.evidence = QStringLiteral("ICMP yanıtı alınamadı");
        }
        if (done_) done_(result);
        finishOne();
    };
    connect(ping, &QProcess::finished, this, reportPing);
    connect(ping, &QProcess::errorOccurred, this, [reportPing](QProcess::ProcessError error) {
        if (error == QProcess::FailedToStart) reportPing();
    });
    ping->start();

    auto* socket = new QTcpSocket(this);
    auto* timer = new QTimer(socket);
    auto* clock = new QElapsedTimer();
    clock->start();
    timer->setSingleShot(true);
    auto finishTcp = [this, socket, clock](const PathResult& result) {
        if (socket->property("tg_done").toBool()) return;
        socket->setProperty("tg_done", true);
        socket->abort();
        socket->deleteLater();
        delete clock;
        if (done_) done_(result);
        finishOne();
    };
    connect(socket, &QTcpSocket::connected, this, [finishTcp, clock]() {
        PathResult result;
        result.kind = QStringLiteral("tcp");
        result.latencyMs = clock->elapsed();
        result.evidence = QStringLiteral("TCP 443 bağlantısı kuruldu. API sağlığı kanıtlanmaz.");
        finishTcp(result);
    });
    connect(socket, &QTcpSocket::errorOccurred, this, [finishTcp](QAbstractSocket::SocketError) {
        PathResult result;
        result.kind = QStringLiteral("tcp");
        result.timeout = true;
        result.latencyMs = -1;
        result.evidence = QStringLiteral("TCP bağlantısı kurulamadı");
        finishTcp(result);
    });
    connect(timer, &QTimer::timeout, this, [finishTcp]() {
        PathResult result;
        result.kind = QStringLiteral("tcp");
        result.timeout = true;
        result.latencyMs = -1;
        result.evidence = QStringLiteral("TCP bağlantısı kurulamadı");
        finishTcp(result);
    });
    timer->start(3000);
    socket->connectToHost(QString::fromLatin1(kHost), kPort);
}

} // namespace tg
