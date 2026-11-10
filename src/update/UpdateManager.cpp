#include "UpdateManager.h"

#include "InstallPlan.h"
#include "ReleaseParser.h"
#include "SemVer.h"
#include "UpdateTypes.h"
#include "ZipSafety.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLocale>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QProcess>
#include <QStandardPaths>
#include <QSysInfo>
#include <QTimer>

namespace tg {
namespace {

QString semverText(const QString& tag)
{
    const SemVer version = parseSemVer(tag);
    if (!version.valid) return {};
    return QString::number(version.major) + QLatin1Char('.') + QString::number(version.minor) + QLatin1Char('.') + QString::number(version.patch);
}

} // namespace

UpdateManager::UpdateManager(QObject* parent)
    : QObject(parent)
    , network_(new QNetworkAccessManager(this))
    , releaseUrl_(officialReleaseApi())
    , currentVersion_(QCoreApplication::applicationVersion().isEmpty() ? appVersionLiteral() : QCoreApplication::applicationVersion())
    , platformText_((currentUpdatePlatform() == QLatin1String("windows") ? QStringLiteral("Windows ") : QStringLiteral("macOS ")) + normalizeArch(QSysInfo::currentCpuArchitecture()))
    , arch_(QSysInfo::currentCpuArchitecture())
    , updatePlatform_(currentUpdatePlatform())
    , executablePath_(QCoreApplication::applicationFilePath())
    , dataDirectory_(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation))
    , probe_(probeMacSignature)
{
    loadState();
}

void UpdateManager::setReleaseUrl(const QUrl& url)
{
    const bool loopback = url.scheme() == QLatin1String("http") && (url.host() == QLatin1String("127.0.0.1") || url.host() == QLatin1String("localhost"));
    if (loopback) {
        releaseUrl_ = url;
        loopbackTests_ = true;
        return;
    }
    if (url == officialReleaseApi()) {
        releaseUrl_ = officialReleaseApi();
        loopbackTests_ = false;
    }
}

void UpdateManager::setArchForTesting(const QString& arch) { arch_ = arch; }
void UpdateManager::setPlatformForTesting(const QString& platform) { updatePlatform_ = platform.trimmed().toLower(); platformText_ = (updatePlatform_ == QLatin1String("windows") ? QStringLiteral("Windows ") : QStringLiteral("macOS ")) + normalizeArch(arch_); }
void UpdateManager::setSignatureProbe(SignatureProbe probe) { probe_ = std::move(probe); }
void UpdateManager::setExecutablePath(const QString& path) { executablePath_ = path; }
void UpdateManager::setInstallLauncher(const std::function<bool(const QString& planPath)>& launcher) { launcher_ = launcher; }
void UpdateManager::setAllowSystemAuthorization(bool enabled) { allowAuthorization_ = enabled; }
void UpdateManager::setExpectedAuthorityForTesting(const QString& authority) { authorityOverride_ = authority.trimmed(); }

QString effectiveAuthority(const QString& overrideAuthority)
{
    return overrideAuthority.isEmpty() ? configuredDeveloperAuthority() : overrideAuthority;
}

void UpdateManager::setDataDirectory(const QString& path)
{
    dataDirectory_ = path;
    loadState();
}

void UpdateManager::loadState()
{
    QFile file(dataDirectory_ + QStringLiteral("/update-state.json"));
    if (!file.open(QIODevice::ReadOnly)) return;
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    lastCheckedText_ = object.value(QStringLiteral("lastCheckedAt")).toString();
    lastStatusText_ = object.value(QStringLiteral("lastStatus")).toString();
}

void UpdateManager::persist() const
{
    if (dataDirectory_.isEmpty()) return;
    QDir().mkpath(dataDirectory_);
    QJsonObject object;
    object.insert(QStringLiteral("lastCheckedAt"), lastCheckedText_);
    object.insert(QStringLiteral("lastStatus"), lastStatusText_);
    object.insert(QStringLiteral("channel"), QStringLiteral("Stable"));
    QFile file(dataDirectory_ + QStringLiteral("/update-state.json"));
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return;
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

void UpdateManager::setPhase(const QString& phase, const QString& status)
{
    phase_ = phase;
    statusText_ = status;
    if (phase != QLatin1String("IDLE") && phase != QLatin1String("CHECKING") && phase != QLatin1String("DOWNLOADING") && phase != QLatin1String("VERIFYING") && phase != QLatin1String("INSTALLING")) {
        lastStatusText_ = status;
        lastCheckedText_ = QDateTime::currentDateTime().toString(QStringLiteral("dd.MM.yyyy HH:mm"));
        persist();
    }
    emit changed();
}

void UpdateManager::fail(const QString& message, const QString& guidance)
{
    errorMessage_ = message;
    guidance_ = guidance;
    progressText_.clear();
    setPhase(QStringLiteral("ERROR"), message);
}

QString UpdateManager::formatBytes(qint64 bytes) const
{
    if (bytes < 0) return QStringLiteral("Veri yok");
    QLocale locale(QLocale::Turkish);
    const double value = static_cast<double>(bytes);
    if (value < 1024.0) return locale.toString(bytes) + QStringLiteral(" B");
    if (value < 1024.0 * 1024.0) return locale.toString(value / 1024.0, 'f', 1) + QStringLiteral(" KB");
    return locale.toString(value / (1024.0 * 1024.0), 'f', 1) + QStringLiteral(" MB");
}

bool UpdateManager::urlAllowed(const QUrl& url) const
{
    if (!url.isValid()) return false;
    if (loopbackTests_) return url.scheme() == QLatin1String("http") && url.host() == releaseUrl_.host() && url.port() == releaseUrl_.port();
    if (url == releaseUrl_) return releaseUrlTrusted(url);
    if (declaredDownloads_.contains(url)) return downloadHostTrusted(url);
    return false;
}

void UpdateManager::activateButton()
{
    if (busy()) return;
    if (phase_ == QLatin1String("UPDATE_AVAILABLE")) {
        dialogOpen_ = true;
        emit changed();
        return;
    }
    if (phase_ == QLatin1String("READY_TO_INSTALL")) {
        confirmInstall();
        return;
    }
    checkForUpdates();
}

void UpdateManager::dismissDialog()
{
    if (phase_ == QLatin1String("DOWNLOADING")) {
        cancelDownload();
        return;
    }
    dialogOpen_ = false;
    emit changed();
}

void UpdateManager::checkForUpdates()
{
    if (busy()) return;
    if (normalizeArch(arch_).isEmpty()) {
        setPhase(QStringLiteral("UNSUPPORTED_PLATFORM"), QStringLiteral("Bu işlemci mimarisi için paket seçilmedi."));
        return;
    }
    errorMessage_.clear();
    guidance_.clear();
    dialogOpen_ = false;
    setPhase(QStringLiteral("CHECKING"), QStringLiteral("Denetleniyor"));
    declaredDownloads_.clear();
    startRequest(releaseUrl_, Fetch::Release, {});
}

void UpdateManager::confirmDownload()
{
    if (phase_ != QLatin1String("UPDATE_AVAILABLE") || !urlAllowed(packageUrl_)) return;
    cancelRequested_ = false;
    receivedBytes_ = 0;
    progressPercent_ = 0;
    progressText_ = QStringLiteral("İndiriliyor: %0");
    speedText_.clear();
    elapsed_.start();
    const QString staging = dataDirectory_ + QStringLiteral("/updates/staging");
    QDir().mkpath(staging);
    QString fileName = QFileInfo(packageUrl_.path()).fileName();
    if (updatePlatform_ == QLatin1String("windows")) {
        if (!fileName.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive))
            fileName = assetNameForPlatformArch(QStringLiteral("windows"), arch_);
    } else if (!fileName.endsWith(QStringLiteral(".zip"), Qt::CaseInsensitive)) {
        fileName = QStringLiteral("package.zip");
    }
    if (fileName.isEmpty()) fileName = QStringLiteral("package.bin");
    packagePath_ = staging + QLatin1Char('/') + fileName;
    QFile::remove(packagePath_);
    QFile::remove(packagePath_ + QStringLiteral(".partial"));
    setPhase(QStringLiteral("DOWNLOADING"), QStringLiteral("İndiriliyor"));
    startRequest(packageUrl_, Fetch::Package, packagePath_ + QStringLiteral(".partial"));
}

void UpdateManager::cancelDownload()
{
    if (fetch_ != Fetch::Package || !reply_) return;
    cancelRequested_ = true;
    reply_->abort();
}

void UpdateManager::startRequest(const QUrl& url, Fetch kind, const QString& filePath)
{
    if (!urlAllowed(url) && !(kind != Fetch::Release && downloadHostTrusted(url) && declaredDownloads_.contains(url))) {
        fail(msgApiFailure());
        return;
    }
    if (loopbackTests_ && !urlAllowed(url)) {
        fail(msgApiFailure());
        return;
    }
    if (!loopbackTests_ && kind == Fetch::Release && !releaseUrlTrusted(url)) {
        fail(msgApiFailure());
        return;
    }
    if (!loopbackTests_ && kind != Fetch::Release && !downloadHostTrusted(url) && !declaredDownloads_.contains(url)) {
        fail(msgPathRejected());
        return;
    }
    delete file_;
    file_ = nullptr;
    if (!filePath.isEmpty()) {
        file_ = new QFile(filePath, this);
        if (!file_->open(QIODevice::WriteOnly | QIODevice::Truncate)) {
            fail(QStringLiteral("Geçici indirme dosyası açılamadı."));
            return;
        }
    }
    QNetworkRequest request(url);
    request.setHeader(QNetworkRequest::UserAgentHeader, QStringLiteral("TicketGoIzleme/") + currentVersion_ + QStringLiteral(" (+") + officialRepositoryUrl() + QLatin1Char(')'));
    request.setRawHeader("Accept", kind == Fetch::Release || kind == Fetch::Manifest ? "application/vnd.github+json" : "application/octet-stream");
    request.setRawHeader("X-GitHub-Api-Version", "2022-11-28");
    request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::ManualRedirectPolicy);
    request.setTransferTimeout(kind == Fetch::Package ? 120000 : 20000);
    fetch_ = kind;
    reply_ = network_->get(request);
    connect(reply_, &QNetworkReply::sslErrors, reply_, [reply = reply_](const QList<QSslError>&) { reply->abort(); });
    connect(reply_, &QNetworkReply::readyRead, this, [this]() {
        if (!reply_ || !file_) return;
        file_->write(reply_->readAll());
        receivedBytes_ = file_->size();
        noteProgress();
    });
    connect(reply_, &QNetworkReply::downloadProgress, this, [this](qint64 received, qint64 total) {
        if (file_) return;
        receivedBytes_ = received;
        if (total > 0) packageBytes_ = total;
        noteProgress();
    });
    connect(reply_, &QNetworkReply::finished, this, &UpdateManager::finishReply);
}

void UpdateManager::noteProgress()
{
    const qint64 total = packageBytes_ > 0 ? packageBytes_ : -1;
    progressPercent_ = total > 0 ? static_cast<int>(qMin<qint64>(100, receivedBytes_ * 100 / total)) : -1;
    progressText_ = progressPercent_ >= 0 ? QStringLiteral("İndiriliyor: %") + QString::number(progressPercent_) : QStringLiteral("İndiriliyor...");
    transferText_ = formatBytes(receivedBytes_) + QStringLiteral(" / ") + formatBytes(total);
    if (elapsed_.isValid() && elapsed_.elapsed() > 500 && receivedBytes_ > 0) speedText_ = formatBytes(receivedBytes_ * 1000 / elapsed_.elapsed()) + QStringLiteral("/s");
    emit changed();
}

void UpdateManager::followRedirect(const QUrl& url)
{
    QUrl next = url;
    if (reply_) {
        const QUrl redirected = reply_->attribute(QNetworkRequest::RedirectionTargetAttribute).toUrl();
        if (redirected.isValid()) next = reply_->url().resolved(redirected);
    }
    const Fetch kind = fetch_;
    const QString path = file_ ? file_->fileName() : QString();
    reply_->deleteLater();
    reply_ = nullptr;
    if (file_) {
        file_->close();
        delete file_;
        file_ = nullptr;
    }
    const bool releaseRedirect = kind == Fetch::Release && (loopbackTests_ ? urlAllowed(next) : releaseUrlTrusted(next));
    const bool downloadRedirect = kind != Fetch::Release && (loopbackTests_ ? urlAllowed(next) : downloadHostTrusted(next));
    if (!releaseRedirect && !downloadRedirect) {
        fail(msgPathRejected());
        return;
    }
    if (downloadRedirect && !declaredDownloads_.contains(next)) declaredDownloads_.append(next);
    startRequest(next, kind, path);
}

void UpdateManager::finishReply()
{
    if (!reply_) return;
    const int status = reply_->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt();
    const auto error = reply_->error();
    if (cancelRequested_ && fetch_ == Fetch::Package) {
        if (file_) {
            const QString path = file_->fileName();
            file_->close();
            delete file_;
            file_ = nullptr;
            QFile::remove(path);
        }
        reply_->deleteLater();
        reply_ = nullptr;
        fetch_ = Fetch::None;
        cancelRequested_ = false;
        progressText_.clear();
        errorMessage_ = msgCancelled();
        setPhase(QStringLiteral("UPDATE_AVAILABLE"), msgCancelled());
        dialogOpen_ = true;
        return;
    }
    if (status >= 300 && status < 400 && error != QNetworkReply::OperationCanceledError) {
        followRedirect(QUrl());
        return;
    }
    QByteArray body;
    const QByteArray rate = reply_->rawHeader("X-RateLimit-Remaining");
    if (!file_) body = reply_->readAll();
    else {
        file_->write(reply_->readAll());
        file_->flush();
        receivedBytes_ = file_->size();
        file_->close();
    }
    const Fetch kind = fetch_;
    reply_->deleteLater();
    reply_ = nullptr;
    fetch_ = Fetch::None;
    const bool transportFailed = status == 0 || error == QNetworkReply::TimeoutError || error == QNetworkReply::OperationCanceledError;
    if (transportFailed || (kind != Fetch::Release && status != 200)) {
        if (file_) {
            const QString path = file_->fileName();
            delete file_;
            file_ = nullptr;
            QFile::remove(path);
        }
        fail(kind == Fetch::Release ? msgApiFailure() : msgDownloadFailed());
        return;
    }
    if (kind == Fetch::Release) takeRelease(status, body, rate);
    else if (kind == Fetch::Manifest) takeManifest(body);
    else if (kind == Fetch::Package) takePackage();
}

void UpdateManager::takeRelease(int status, const QByteArray& body, const QByteArray& rateRemaining)
{
    const ParsedRelease parsed = parseLatestRelease(status, body, rateRemaining);
    if (parsed.kind == ReleaseKind::NoRelease || parsed.kind == ReleaseKind::NotStable) {
        setPhase(QStringLiteral("NO_RELEASE"), parsed.message);
        return;
    }
    if (parsed.kind != ReleaseKind::Ok) {
        fail(parsed.message);
        return;
    }
    const VersionOrder order = compareVersions(currentVersion_, parsed.release.tag);
    if (order == VersionOrder::Invalid) {
        fail(msgBadVersion());
        return;
    }
    if (order != VersionOrder::Newer) {
        latestVersion_ = semverText(parsed.release.tag);
        publishedText_.clear();
        releaseNotes_.clear();
        setPhase(QStringLiteral("UP_TO_DATE"), QStringLiteral("Program güncel"));
        return;
    }
    const QString expectedName = assetNameForPlatformArch(updatePlatform_, arch_);
    packageUrl_.clear();
    manifestUrl_.clear();
    packageBytes_ = -1;
    bool otherArch = false;
    bool otherPlatform = false;
    for (const ReleaseAsset& asset : parsed.release.assets) {
        if (asset.name == QLatin1String("TicketGoIzleme-update-manifest.json")) {
            manifestUrl_ = asset.url;
            continue;
        }
        const QString assetPlatform = inferAssetPlatform(asset.name, asset.platform);
        if (asset.name == expectedName) {
            packageUrl_ = asset.url;
            packageBytes_ = asset.size;
        } else if (assetPlatform == updatePlatform_) {
            otherArch = true;
        } else if (!assetPlatform.isEmpty()) {
            otherPlatform = true;
        }
    }
    if (packageUrl_.isEmpty()) {
        if (otherArch) fail(msgWrongArch());
        else if (otherPlatform) fail(msgWrongPlatform());
        else fail(msgMissingAsset());
        return;
    }
    if (manifestUrl_.isEmpty()) {
        fail(QStringLiteral("Doğrulama manifesti yok. Kurulum başlatılmadı."));
        return;
    }
    if ((!loopbackTests_ && !downloadHostTrusted(packageUrl_)) || (loopbackTests_ && !urlAllowed(packageUrl_))) {
        fail(msgPathRejected());
        return;
    }
    latestTag_ = parsed.release.tag;
    expectedVersion_ = semverText(parsed.release.tag);
    releaseNotes_ = parsed.release.notes;
    releasePublished_ = parsed.release.publishedAt;
    declaredDownloads_ = {manifestUrl_, packageUrl_};
    startRequest(manifestUrl_, Fetch::Manifest, {});
}

void UpdateManager::takeManifest(const QByteArray& body)
{
    ReleaseInfo info;
    info.tag = latestTag_;
    ReleaseAsset package;
    package.name = assetNameForPlatformArch(updatePlatform_, arch_);
    package.url = packageUrl_;
    package.size = packageBytes_;
    package.platform = updatePlatform_;
    ReleaseAsset manifest;
    manifest.name = QStringLiteral("TicketGoIzleme-update-manifest.json");
    manifest.url = manifestUrl_;
    info.assets = {package, manifest};
    const AssetChoice choice = selectReleasePackage(info, updatePlatform_, arch_, body);
    if (!choice.ok) {
        fail(choice.message);
        return;
    }
    expectedSha_ = choice.package.sha256;
    packageBytes_ = choice.package.size;
    packageBytesText_ = formatBytes(packageBytes_);
    publishAvailable();
}

void UpdateManager::publishAvailable()
{
    latestVersion_ = expectedVersion_;
    const QDateTime published = QDateTime::fromString(releasePublished_, Qt::ISODate);
    publishedText_ = published.isValid() ? published.toLocalTime().toString(QStringLiteral("dd.MM.yyyy HH:mm")) : releasePublished_;
    errorMessage_.clear();
    dialogOpen_ = false;
    setPhase(QStringLiteral("UPDATE_AVAILABLE"), QStringLiteral("Yeni sürüm var"));
}

void UpdateManager::takePackage()
{
    if (!file_) {
        fail(msgDownloadFailed());
        return;
    }
    const QString partial = file_->fileName();
    delete file_;
    file_ = nullptr;
    if (packageBytes_ > 0 && receivedBytes_ != packageBytes_) {
        QFile::remove(partial);
        fail(QStringLiteral("İndirilen boyut yayın boyutuyla uyuşmuyor."));
        return;
    }
    const QString finalPath = packagePath_;
    QFile::remove(finalPath);
    if (!QFile::rename(partial, finalPath)) {
        QFile::remove(partial);
        fail(msgDownloadFailed());
        return;
    }
    setPhase(QStringLiteral("VERIFYING"), QStringLiteral("Doğrulanıyor"));
    const QString actual = fileSha256(finalPath);
    if (actual.compare(expectedSha_, Qt::CaseInsensitive) != 0) {
        QFile::remove(finalPath);
        fail(msgHashMismatch());
        return;
    }
    if (updatePlatform_ == QLatin1String("windows") && finalPath.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive)) {
        windowsSetupPath_ = finalPath;
        extractedApp_.clear();
        const InstallLocation location = inspectLocation(executablePath_);
        if (!location.installed) {
            fail(location.message.isEmpty() ? msgNotInstalled() : location.message);
            return;
        }
        errorMessage_.clear();
        guidance_ = msgWindowsSmartScreen();
        dialogOpen_ = true;
        setPhase(QStringLiteral("READY_TO_INSTALL"), QStringLiteral("Kurulum hazır"));
        return;
    }
    const QString extractRoot = dataDirectory_ + QStringLiteral("/updates/extracted");
    QDir(extractRoot).removeRecursively();
    QDir().mkpath(extractRoot);
    QString error;
    if (!extractZip(finalPath, extractRoot, &error)) {
        QDir(extractRoot).removeRecursively();
        fail(error.isEmpty() ? msgPathRejected() : error);
        return;
    }
    const QString appPath = findSingleApp(extractRoot, &error);
    if (appPath.isEmpty()) {
        fail(error.isEmpty() ? msgPathRejected() : error);
        return;
    }
    const VerifyResult verified = verifyExtractedApp(appPath, expectedSha_, actual, QStringLiteral("tr.ticketgoteknoloji.izleme"), expectedVersion_, effectiveAuthority(authorityOverride_), probe_);
    extractedApp_ = appPath;
    windowsSetupPath_.clear();
    if (!verified.hashOk || !verified.bundleOk || !verified.versionOk) {
        fail(verified.message);
        return;
    }
    if (!verified.autoInstallAllowed) {
        guidance_ = verified.guidance.isEmpty() ? msgManualOnly() : verified.guidance;
        fail(verified.message.isEmpty() ? msgManualOnly() : verified.message, guidance_);
        return;
    }
    const InstallLocation location = inspectLocation(executablePath_);
    if (!location.installed) {
        fail(location.message.isEmpty() ? msgNotInstalled() : location.message);
        return;
    }
    const bool writable = QFileInfo(QFileInfo(location.bundlePath).absolutePath()).isWritable();
    if (!writable && !allowAuthorization_) {
        fail(msgNoPermission());
        return;
    }
    errorMessage_.clear();
    guidance_.clear();
    dialogOpen_ = true;
    setPhase(QStringLiteral("READY_TO_INSTALL"), QStringLiteral("Kurulum hazır"));
}

void UpdateManager::confirmInstall()
{
    if (phase_ != QLatin1String("READY_TO_INSTALL")) return;
    const InstallLocation location = inspectLocation(executablePath_);
    if (!location.installed) {
        fail(location.message.isEmpty() ? msgNotInstalled() : location.message);
        return;
    }
    const bool windowsSetup = updatePlatform_ == QLatin1String("windows") && !windowsSetupPath_.isEmpty();
    if (!windowsSetup && effectiveAuthority(authorityOverride_).isEmpty()) {
        fail(msgManualOnly(), msgManualOnly());
        return;
    }
    const QString backup = dataDirectory_ + QStringLiteral("/updates/backup/") + QFileInfo(location.bundlePath).fileName();
    HelperPlan plan;
    plan.parentPid = QCoreApplication::applicationPid();
    plan.sourceApp = windowsSetup ? windowsSetupPath_ : extractedApp_;
    plan.targetApp = location.bundlePath;
    plan.backupApp = backup;
    plan.dataDirectory = dataDirectory_;
    plan.bundleId = QStringLiteral("tr.ticketgoteknoloji.izleme");
    plan.version = expectedVersion_;
    plan.authority = windowsSetup ? QStringLiteral("windows-setup-hash-verified") : effectiveAuthority(authorityOverride_);
    plan.kind = windowsSetup ? QStringLiteral("windows-setup") : QStringLiteral("macos-app");
    QDir().mkpath(dataDirectory_ + QStringLiteral("/updates"));
    const QString planPath = dataDirectory_ + QStringLiteral("/updates/install-plan.json");
    QString error;
    if (!writeHelperPlan(planPath, plan, &error)) {
        fail(error.isEmpty() ? msgPathRejected() : error);
        return;
    }
    setPhase(QStringLiteral("INSTALLING"), QStringLiteral("Yükleniyor"));
    bool started = false;
    if (launcher_) started = launcher_(planPath);
    else {
#if defined(Q_OS_WIN)
        const QString helper = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("ticketgo-update-helper.exe"));
#else
        const QString helper = QDir(QCoreApplication::applicationDirPath()).filePath(QStringLiteral("../Helpers/ticketgo-update-helper"));
#endif
        if (!QFileInfo::exists(helper) || !shellPathSafe(helper) || !shellPathSafe(planPath)) {
            fail(QStringLiteral("Güncelleme yardımcı uygulaması hazır değil. Kurulum başlatılmadı."));
            return;
        }
        if (!windowsSetup) {
            const bool writable = QFileInfo(QFileInfo(location.bundlePath).absolutePath()).isWritable();
            if (!writable && allowAuthorization_) {
                fail(msgNoPermission());
                return;
            }
        }
        started = QProcess::startDetached(helper, {planPath});
    }
    if (!started) {
        fail(QStringLiteral("Güncelleme yardımcı uygulaması başlatılamadı. Kurulu sürüm duruyor."));
        return;
    }
    dialogOpen_ = false;
    setPhase(QStringLiteral("RESTART_REQUIRED"), QStringLiteral("Yeniden açılıyor"));
    emit restartRequested();
}

} // namespace tg
