#pragma once

#include <QObject>
#include <QString>
#include <functional>

namespace tg {

struct PingRead {
    bool replied = false;
    double latencyMs = -1;
    int lossPercent = -1;
};

struct PathResult {
    QString kind;
    qint64 latencyMs = -1;
    bool timeout = false;
    int lossPercent = -1;
    QString evidence;
};

PingRead readPingOutput(const QString& text);

class PathProbe : public QObject {
    Q_OBJECT
public:
    explicit PathProbe(QObject* parent = nullptr);
    bool busy() const { return busy_; }
    void start(const std::function<void(PathResult)>& done);

private:
    bool busy_ = false;
    int pending_ = 0;
    std::function<void(PathResult)> done_;
    void finishOne();
};

} // namespace tg
