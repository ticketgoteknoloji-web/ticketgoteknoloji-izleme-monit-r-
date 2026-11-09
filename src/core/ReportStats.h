#pragma once

#include <algorithm>
#include <QDateTime>
#include <QList>
#include <QPair>
#include <QString>
#include <optional>

namespace tg {

struct SeriesStats {
    int count = 0;
    std::optional<double> minimum;
    std::optional<double> maximum;
    std::optional<double> average;
    std::optional<double> p95;
    std::optional<double> last;
};

inline QList<QPair<qint64, double>> thinSeries(const QList<QPair<qint64, double>>& series, int maxPoints)
{
    if (series.size() <= qMax(2, maxPoints)) return series;
    QList<QPair<qint64, double>> out;
    const int step = qMax(1, series.size() / (maxPoints - 1));
    for (int index = 0; index < series.size(); index += step) out.push_back(series.at(index));
    if (out.last().first != series.last().first) out.push_back(series.last());
    return out;
}

inline SeriesStats summarizeSeries(const QList<double>& values)
{
    SeriesStats stats;
    stats.count = values.size();
    if (values.isEmpty()) return stats;
    double minimum = values.first();
    double maximum = values.first();
    double sum = 0;
    for (double value : values) {
        minimum = qMin(minimum, value);
        maximum = qMax(maximum, value);
        sum += value;
    }
    stats.minimum = minimum;
    stats.maximum = maximum;
    stats.average = sum / double(values.size());
    stats.last = values.last();
    if (values.size() >= 20) {
        QList<double> sorted = values;
        std::sort(sorted.begin(), sorted.end());
        stats.p95 = sorted.at(qMin(sorted.size() - 1, int(0.95 * (sorted.size() - 1))));
    }
    return stats;
}

struct ProbeSample {
    QString moduleId;
    QString status;
    qint64 latencyMs = -1;
};

struct PeriodStats {
    int measured = 0;
    int healthy = 0;
    int warning = 0;
    int failed = 0;
    int excluded = 0;
    int latencyCount = 0;
    std::optional<double> availability;
    std::optional<double> avgLatencyMs;
    std::optional<double> p95LatencyMs;
};

inline bool countsForAvailability(const QString& status)
{
    return status == QLatin1String("healthy") || status == QLatin1String("warning") || status == QLatin1String("critical") || status == QLatin1String("connection_error");
}

inline PeriodStats summarizeProbes(const QList<ProbeSample>& rows)
{
    PeriodStats stats;
    QList<qint64> latencies;
    for (const ProbeSample& row : rows) {
        if (row.status == QLatin1String("healthy")) {
            ++stats.healthy;
            ++stats.measured;
        } else if (row.status == QLatin1String("warning")) {
            ++stats.warning;
            ++stats.measured;
        } else if (row.status == QLatin1String("critical") || row.status == QLatin1String("connection_error")) {
            ++stats.failed;
            ++stats.measured;
        } else {
            ++stats.excluded;
        }
        if (countsForAvailability(row.status) && row.latencyMs >= 0) latencies.push_back(row.latencyMs);
    }
    if (stats.measured > 0) stats.availability = double(stats.healthy + stats.warning) / double(stats.measured);
    stats.latencyCount = latencies.size();
    if (!latencies.isEmpty()) {
        qint64 sum = 0;
        for (qint64 value : latencies) sum += value;
        stats.avgLatencyMs = double(sum) / double(latencies.size());
    }
    if (latencies.size() >= 20) {
        std::sort(latencies.begin(), latencies.end());
        const int index = qMin(latencies.size() - 1, int(0.95 * (latencies.size() - 1)));
        stats.p95LatencyMs = double(latencies.at(index));
    }
    return stats;
}

inline QString availabilityText(const PeriodStats& stats)
{
    if (!stats.availability) return QStringLiteral("Hesaplanamadı");
    return QString::number(*stats.availability * 100.0, 'f', 1) + QStringLiteral("%");
}

inline QString latencyText(const std::optional<double>& value)
{
    if (!value) return QStringLiteral("ölçüm yok");
    return QString::number(*value, 'f', 0) + QStringLiteral(" ms");
}

inline bool resolveReportRange(const QString& key, const QString& fromIso, const QString& toIso, const QDateTime& now, QDateTime* from, QDateTime* to, QString* error)
{
    QDateTime end = now;
    QDateTime start;
    if (key == QLatin1String("1h")) start = now.addSecs(-3600);
    else if (key == QLatin1String("24h")) start = now.addSecs(-86400);
    else if (key == QLatin1String("7d")) start = now.addDays(-7);
    else if (key == QLatin1String("30d")) start = now.addDays(-30);
    else if (key == QLatin1String("custom")) {
        start = QDateTime::fromString(fromIso, Qt::ISODate);
        end = QDateTime::fromString(toIso, Qt::ISODate);
        if (!start.isValid() || !end.isValid()) {
            if (error) *error = QStringLiteral("Tarih okunamadı. Rapor üretilmedi.");
            return false;
        }
    } else {
        if (error) *error = QStringLiteral("Tarih aralığı geçersiz. Rapor üretilmedi.");
        return false;
    }
    if (!start.isValid() || !end.isValid() || end <= start) {
        if (error) *error = QStringLiteral("Bitiş, başlangıçtan sonra olmalı. Rapor üretilmedi.");
        return false;
    }
    if (from) *from = start;
    if (to) *to = end;
    return true;
}

inline QString csvCell(const QString& raw)
{
    QString value = raw;
    value.replace(QLatin1Char('"'), QStringLiteral("''"));
    value.replace(QLatin1Char('\n'), QLatin1Char(' '));
    value.replace(QLatin1Char('\r'), QLatin1Char(' '));
    if (value.startsWith(QLatin1Char('=')) || value.startsWith(QLatin1Char('+')) || value.startsWith(QLatin1Char('-')) || value.startsWith(QLatin1Char('@'))) {
        value.prepend(QLatin1Char('\''));
    }
    return QLatin1Char('"') + value + QLatin1Char('"');
}

} // namespace tg
