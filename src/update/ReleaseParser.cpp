#include "ReleaseParser.h"

#include "SemVer.h"
#include "UpdateTypes.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QRegularExpression>

namespace tg {
namespace {

const QString kOwner = officialRepoOwner();
const QString kRepo = officialRepoName();

bool hexSha(const QString& value)
{
    static const QRegularExpression pattern(QStringLiteral("^[0-9a-fA-F]{64}$"));
    return pattern.match(value).hasMatch();
}

bool sourceArchive(const QString& name, const QUrl& url)
{
    const QString lower = name.toLower();
    if (lower.contains(QStringLiteral("source code"))) return true;
    if (lower.contains(QStringLiteral("source-code"))) return true;
    const QString path = url.path().toLower();
    return path.contains(QStringLiteral("/archive/")) || path.contains(QStringLiteral("/zipball/")) || path.contains(QStringLiteral("/tarball/"));
}

} // namespace

QString normalizeArch(const QString& cpuArch)
{
    const QString arch = cpuArch.trimmed().toLower();
    if (arch == QLatin1String("arm64") || arch == QLatin1String("aarch64")) return QStringLiteral("arm64");
    if (arch == QLatin1String("x86_64") || arch == QLatin1String("x64") || arch == QLatin1String("amd64")) return QStringLiteral("x64");
    return {};
}

QString currentUpdatePlatform()
{
#if defined(Q_OS_WIN)
    return QStringLiteral("windows");
#elif defined(Q_OS_MACOS)
    return QStringLiteral("macos");
#else
    return {};
#endif
}

QString inferAssetPlatform(const QString& assetName, const QString& platformField)
{
    const QString explicitPlatform = platformField.trimmed().toLower();
    if (explicitPlatform == QLatin1String("macos") || explicitPlatform == QLatin1String("darwin") || explicitPlatform == QLatin1String("osx")) return QStringLiteral("macos");
    if (explicitPlatform == QLatin1String("windows") || explicitPlatform == QLatin1String("win") || explicitPlatform == QLatin1String("win32")) return QStringLiteral("windows");
    const QString lower = assetName.toLower();
    if (lower.contains(QStringLiteral("windows")) || lower.contains(QStringLiteral("-win-")) || lower.endsWith(QStringLiteral(".exe"))) return QStringLiteral("windows");
    if (lower.contains(QStringLiteral("macos")) || lower.contains(QStringLiteral("darwin")) || lower.contains(QStringLiteral(".app"))) return QStringLiteral("macos");
    return {};
}

QString assetNameForPlatformArch(const QString& platform, const QString& cpuArch)
{
    const QString arch = normalizeArch(cpuArch);
    const QString plat = platform.trimmed().toLower();
    if (plat == QLatin1String("macos")) {
        if (arch == QLatin1String("arm64")) return QStringLiteral("TicketGoIzleme-macOS-arm64.zip");
        if (arch == QLatin1String("x64")) return QStringLiteral("TicketGoIzleme-macOS-x64.zip");
    }
    if (plat == QLatin1String("windows") && arch == QLatin1String("x64")) return QStringLiteral("TicketGoIzleme-Windows-x64-Setup.exe");
    return {};
}

QString assetNameForArch(const QString& cpuArch)
{
    return assetNameForPlatformArch(QStringLiteral("macos"), cpuArch);
}

bool releaseUrlTrusted(const QUrl& url)
{
    if (!url.isValid() || url.scheme() != QLatin1String("https")) return false;
    if (url.host().compare(QStringLiteral("api.github.com"), Qt::CaseInsensitive) != 0) return false;
    const QString prefix = QStringLiteral("/repos/") + kOwner + QLatin1Char('/') + kRepo + QLatin1Char('/');
    return url.path().startsWith(prefix) && !url.path().contains(QStringLiteral(".."));
}

bool downloadHostTrusted(const QUrl& url)
{
    if (!url.isValid() || url.scheme() != QLatin1String("https")) return false;
    const QString host = url.host().toLower();
    return host == QLatin1String("github.com")
        || host == QLatin1String("api.github.com")
        || host == QLatin1String("objects.githubusercontent.com")
        || host == QLatin1String("release-assets.githubusercontent.com")
        || host == QLatin1String("github-releases.githubusercontent.com");
}

QString plainReleaseNotes(const QString& body)
{
    QString text = body;
    text.remove(QRegularExpression(QStringLiteral("(?is)<script[^>]*>.*?</script>")));
    text.remove(QRegularExpression(QStringLiteral("(?is)<style[^>]*>.*?</style>")));
    text.replace(QRegularExpression(QStringLiteral("(?i)<br\\s*/?>")), QStringLiteral("\n"));
    text.replace(QRegularExpression(QStringLiteral("(?i)</p>")), QStringLiteral("\n"));
    text.replace(QRegularExpression(QStringLiteral("(?i)</li>")), QStringLiteral("\n"));
    text.remove(QRegularExpression(QStringLiteral("(?is)<[^>]+>")));
    text.replace(QStringLiteral("&amp;"), QStringLiteral("&"));
    text.replace(QStringLiteral("&lt;"), QStringLiteral("<"));
    text.replace(QStringLiteral("&gt;"), QStringLiteral(">"));
    text.replace(QStringLiteral("&quot;"), QStringLiteral("\""));
    text.replace(QStringLiteral("&#39;"), QStringLiteral("'"));
    text.replace(QStringLiteral("&nbsp;"), QStringLiteral(" "));
    text.replace(QRegularExpression(QStringLiteral("[\\t\\x00-\\x08\\x0b\\x0c\\x0e-\\x1f]")), QString());
    if (text.size() > 20000) text = text.left(20000);
    return text.trimmed();
}

ParsedRelease parseLatestRelease(int httpStatus, const QByteArray& body, const QByteArray& rateRemaining)
{
    ParsedRelease parsed;
    if (httpStatus == 404) {
        parsed.kind = ReleaseKind::NoRelease;
        parsed.message = msgNoStableRelease();
        return parsed;
    }
    const QJsonDocument document = QJsonDocument::fromJson(body);
    const QString apiMessage = document.isObject() ? document.object().value(QStringLiteral("message")).toString() : QString();
    const bool rateMessage = apiMessage.contains(QStringLiteral("rate limit"), Qt::CaseInsensitive);
    const bool limited = httpStatus == 429 || rateMessage || (httpStatus == 403 && (rateRemaining == "0" || rateMessage));
    if (limited) {
        parsed.kind = ReleaseKind::RateLimited;
        parsed.message = msgRateLimit();
        return parsed;
    }
    if (httpStatus != 200 || !document.isObject()) {
        parsed.kind = ReleaseKind::ApiError;
        parsed.message = apiMessage.isEmpty() ? msgApiFailure() : apiMessage;
        return parsed;
    }
    const QJsonObject object = document.object();
    ReleaseInfo info;
    info.tag = object.value(QStringLiteral("tag_name")).toString();
    info.name = object.value(QStringLiteral("name")).toString();
    info.publishedAt = object.value(QStringLiteral("published_at")).toString();
    info.notes = plainReleaseNotes(object.value(QStringLiteral("body")).toString());
    info.draft = object.value(QStringLiteral("draft")).toBool();
    info.prerelease = object.value(QStringLiteral("prerelease")).toBool();
    const QJsonArray assets = object.value(QStringLiteral("assets")).toArray();
    for (const QJsonValue& value : assets) {
        const QJsonObject asset = value.toObject();
        ReleaseAsset item;
        item.name = asset.value(QStringLiteral("name")).toString();
        item.url = QUrl(asset.value(QStringLiteral("browser_download_url")).toString());
        item.size = static_cast<qint64>(asset.value(QStringLiteral("size")).toDouble(-1));
        item.contentType = asset.value(QStringLiteral("content_type")).toString();
        if (sourceArchive(item.name, item.url)) continue;
        info.assets.append(item);
    }
    parsed.release = info;
    if (info.draft || info.prerelease) {
        parsed.kind = ReleaseKind::NotStable;
        parsed.message = msgNoStableRelease();
        return parsed;
    }
    if (!parseSemVer(info.tag).valid) {
        parsed.kind = ReleaseKind::BadVersion;
        parsed.message = msgBadVersion();
        return parsed;
    }
    parsed.kind = ReleaseKind::Ok;
    return parsed;
}

AssetChoice selectReleasePackage(const ReleaseInfo& release, const QString& platform, const QString& cpuArch, const QByteArray& manifestJson)
{
    AssetChoice choice;
    const QString plat = platform.trimmed().toLower();
    const QString arch = normalizeArch(cpuArch);
    const QString expectedName = assetNameForPlatformArch(plat, cpuArch);
    if (plat.isEmpty()) {
        choice.message = msgWrongPlatform();
        return choice;
    }
    if (arch.isEmpty() || expectedName.isEmpty()) {
        choice.message = msgWrongArch();
        return choice;
    }
    const ReleaseAsset* package = nullptr;
    const ReleaseAsset* manifestAsset = nullptr;
    bool otherPlatform = false;
    bool otherArch = false;
    for (const ReleaseAsset& asset : release.assets) {
        if (asset.name == QLatin1String("TicketGoIzleme-update-manifest.json")) {
            manifestAsset = &asset;
            continue;
        }
        const QString assetPlatform = inferAssetPlatform(asset.name, asset.platform);
        if (asset.name == expectedName) package = &asset;
        else if (assetPlatform == plat) otherArch = true;
        else if (!assetPlatform.isEmpty()) otherPlatform = true;
    }
    if (!package) {
        if (otherArch) choice.message = msgWrongArch();
        else if (otherPlatform) choice.message = msgWrongPlatform();
        else choice.message = msgMissingAsset();
        return choice;
    }
    if (sourceArchive(package->name, package->url)) {
        choice.message = msgMissingAsset();
        return choice;
    }
    if (!manifestAsset || manifestJson.isEmpty()) {
        choice.message = QStringLiteral("Doğrulama manifesti yok. Kurulum başlatılmadı.");
        return choice;
    }
    const QJsonDocument document = QJsonDocument::fromJson(manifestJson);
    if (!document.isObject()) {
        choice.message = QStringLiteral("Doğrulama manifesti okunamadı.");
        return choice;
    }
    const QJsonObject root = document.object();
    if (root.value(QStringLiteral("channel")).toString().compare(QStringLiteral("stable"), Qt::CaseInsensitive) != 0) {
        choice.message = msgNoStableRelease();
        return choice;
    }
    const SemVer manifestVersion = parseSemVer(root.value(QStringLiteral("version")).toString());
    const SemVer releaseVersion = parseSemVer(release.tag);
    if (!manifestVersion.valid || compareSemVer(manifestVersion, releaseVersion) != 0) {
        choice.message = msgVersionMismatch();
        return choice;
    }
    const QJsonArray rows = root.value(QStringLiteral("assets")).toArray();
    QJsonObject row;
    bool found = false;
    for (const QJsonValue& value : rows) {
        const QJsonObject object = value.toObject();
        if (object.value(QStringLiteral("name")).toString() != expectedName) continue;
        const QString rowPlatform = inferAssetPlatform(object.value(QStringLiteral("name")).toString(), object.value(QStringLiteral("platform")).toString());
        if (!rowPlatform.isEmpty() && rowPlatform != plat) {
            choice.message = msgWrongPlatform();
            return choice;
        }
        // Eski macOS manifestlerinde platform alanı yoktur; eksik alan macos kabul edilir.
        if (rowPlatform.isEmpty() && plat != QLatin1String("macos")) {
            choice.message = msgWrongPlatform();
            return choice;
        }
        row = object;
        found = true;
        break;
    }
    if (!found) {
        choice.message = msgMissingAsset();
        return choice;
    }
    const QString rowArch = normalizeArch(row.value(QStringLiteral("arch")).toString());
    if (rowArch != arch) {
        choice.message = msgWrongArch();
        return choice;
    }
    const QString sha = row.value(QStringLiteral("sha256")).toString().toLower();
    if (!hexSha(sha)) {
        choice.message = QStringLiteral("Manifest içinde SHA-256 yok.");
        return choice;
    }
    if (row.value(QStringLiteral("bundleId")).toString() != QLatin1String("tr.ticketgoteknoloji.izleme")) {
        choice.message = msgBundleMismatch();
        return choice;
    }
    choice.ok = true;
    choice.package = *package;
    choice.package.sha256 = sha;
    choice.package.arch = arch;
    choice.package.platform = plat;
    choice.package.bundleId = QStringLiteral("tr.ticketgoteknoloji.izleme");
    const qint64 manifestSize = static_cast<qint64>(row.value(QStringLiteral("size")).toDouble(-1));
    if (manifestSize >= 0) choice.package.size = manifestSize;
    return choice;
}

AssetChoice selectMacPackage(const ReleaseInfo& release, const QString& cpuArch, const QByteArray& manifestJson)
{
    return selectReleasePackage(release, QStringLiteral("macos"), cpuArch, manifestJson);
}

} // namespace tg
