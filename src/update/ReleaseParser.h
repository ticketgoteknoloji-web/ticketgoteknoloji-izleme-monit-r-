#pragma once

#include <QString>
#include <QUrl>
#include <QVector>

namespace tg {

struct ReleaseAsset {
    QString name;
    QUrl url;
    qint64 size = -1;
    QString contentType;
    QString sha256;
    QString arch;
    QString platform;
    QString bundleId;
};

struct ReleaseInfo {
    QString tag;
    QString name;
    QString publishedAt;
    QString notes;
    bool draft = false;
    bool prerelease = false;
    QVector<ReleaseAsset> assets;
};

enum class ReleaseKind { Ok, NoRelease, RateLimited, ApiError, NotStable, BadVersion };

struct ParsedRelease {
    ReleaseKind kind = ReleaseKind::ApiError;
    QString message;
    ReleaseInfo release;
};

struct AssetChoice {
    bool ok = false;
    QString message;
    ReleaseAsset package;
};

bool releaseUrlTrusted(const QUrl& url);
bool downloadHostTrusted(const QUrl& url);
QString plainReleaseNotes(const QString& body);
ParsedRelease parseLatestRelease(int httpStatus, const QByteArray& body, const QByteArray& rateRemaining);
AssetChoice selectMacPackage(const ReleaseInfo& release, const QString& cpuArch, const QByteArray& manifestJson);
AssetChoice selectReleasePackage(const ReleaseInfo& release, const QString& platform, const QString& cpuArch, const QByteArray& manifestJson);
QString assetNameForArch(const QString& cpuArch);
QString assetNameForPlatformArch(const QString& platform, const QString& cpuArch);
QString normalizeArch(const QString& cpuArch);
QString currentUpdatePlatform();
QString inferAssetPlatform(const QString& assetName, const QString& platformField);

} // namespace tg
