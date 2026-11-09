#pragma once

#include <QString>

namespace tg {

struct PrefCheck {
    bool ok = false;
    QString error;
};

inline PrefCheck checkMonitorPrefs(int intervalSec, int timeoutSec, int retryCap, int latencyWarnMs, int retentionDays, const QString& quality)
{
    if (intervalSec < 15 || intervalSec > 300) {
        return {false, QStringLiteral("Kontrol aralığı 15–300 saniye olmalı. Kayıt yazılmadı.")};
    }
    if (timeoutSec < 2 || timeoutSec > 30) {
        return {false, QStringLiteral("Zaman aşımı 2–30 saniye olmalı. Kayıt yazılmadı.")};
    }
    if (retryCap < 0 || retryCap > 4) {
        return {false, QStringLiteral("Bağlantı bekleme katı 0–4 olmalı. Kayıt yazılmadı.")};
    }
    if (latencyWarnMs < 100 || latencyWarnMs > 60000) {
        return {false, QStringLiteral("Gecikme uyarısı 100–60000 ms olmalı. Kayıt yazılmadı.")};
    }
    if (retentionDays < 1 || retentionDays > 30) {
        return {false, QStringLiteral("Ölçüm saklama süresi 1–30 gün olmalı. Kayıt yazılmadı.")};
    }
    if (quality != QLatin1String("low") && quality != QLatin1String("medium") && quality != QLatin1String("high")) {
        return {false, QStringLiteral("Grafik kalitesi düşük, orta veya yüksek olmalı. Kayıt yazılmadı.")};
    }
    return {true, {}};
}

} // namespace tg
