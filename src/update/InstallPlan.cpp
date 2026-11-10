#include "InstallPlan.h"

#include "UpdateTypes.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QThread>

#if defined(Q_OS_UNIX)
#include <signal.h>
#include <unistd.h>
#endif
#if defined(Q_OS_WIN)
#include <windows.h>
#endif

namespace tg {
namespace {

bool copyTree(const QString& from, const QString& to, QString* error)
{
    QDir().mkpath(QFileInfo(to).absolutePath());
    if (QFileInfo::exists(to) && !QDir(to).removeRecursively()) {
        if (error) *error = QStringLiteral("Eski kopya temizlenemedi.");
        return false;
    }
#if defined(Q_OS_MACOS)
    QProcess process;
    process.start(QStringLiteral("/usr/bin/ditto"), {from, to});
    if (!process.waitForFinished(180000) || process.exitCode() != 0) {
        if (error) *error = QStringLiteral("Uygulama kopyalanamadı.");
        return false;
    }
    return true;
#else
    Q_UNUSED(from);
    if (error) *error = QStringLiteral("Bu platformda paket kopyalama desteklenmiyor.");
    return false;
#endif
}

bool sameOrInside(const QString& path, const QString& root)
{
    if (root.isEmpty() || path.isEmpty()) return false;
    return path == root || path.startsWith(root + QLatin1Char('/')) || path.startsWith(root + QLatin1Char('\\'));
}

bool protectedPath(const QString& path)
{
    const QString absolute = QFileInfo(path).absoluteFilePath();
#if defined(Q_OS_WIN)
    if (!absolute.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive) && !absolute.endsWith(QStringLiteral(".app"), Qt::CaseInsensitive)) return true;
#else
    if (!absolute.endsWith(QStringLiteral(".app"))) return true;
#endif
    if (absolute.contains(QStringLiteral("/ticket-go-izleme-qt/")) || absolute.contains(QStringLiteral("\\ticket-go-izleme-qt\\"))) return true;
    if (absolute.contains(QStringLiteral("/build/")) || absolute.contains(QStringLiteral("\\build\\"))
        || absolute.contains(QStringLiteral("/build-release/")) || absolute.contains(QStringLiteral("\\build-release\\"))) return true;
    if (absolute.contains(QStringLiteral(".."))) return true;
    return false;
}

bool parentStillRunning(qint64 pid)
{
    if (pid <= 0) return false;
#if defined(Q_OS_WIN)
    HANDLE process = OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION, FALSE, static_cast<DWORD>(pid));
    if (!process) return false;
    DWORD code = STILL_ACTIVE;
    const BOOL ok = GetExitCodeProcess(process, &code);
    CloseHandle(process);
    return ok && code == STILL_ACTIVE;
#elif defined(Q_OS_UNIX)
    return kill(static_cast<pid_t>(pid), 0) == 0;
#else
    Q_UNUSED(pid);
    return false;
#endif
}

} // namespace

bool shellPathSafe(const QString& path)
{
    if (path.isEmpty()) return false;
#if defined(Q_OS_WIN)
    const QString forbidden = QStringLiteral("\"$`!\n\r;&|<>");
#else
    const QString forbidden = QStringLiteral("\"\\$`!\n\r;&|<>");
#endif
    for (const QChar ch : path) {
        if (forbidden.contains(ch)) return false;
    }
    return !path.contains(QStringLiteral(".."));
}

InstallLocation inspectLocation(const QString& executablePath)
{
    InstallLocation location;
    const QString normalized = QDir::fromNativeSeparators(executablePath.trimmed());
    const bool windowsAbsolute = normalized.size() >= 3
        && normalized.at(0).isLetter()
        && normalized.at(1) == QLatin1Char(':')
        && normalized.at(2) == QLatin1Char('/');
    // Keep Windows drive paths intact so cross-platform update tests do not resolve them under the macOS source tree.
    const QString absolute = windowsAbsolute ? normalized : QFileInfo(executablePath).absoluteFilePath();
    const QString lower = absolute.toLower();
    // Path shape decides the install layout so Windows update flows stay testable on macOS helpers.
    if (lower.endsWith(QStringLiteral("ticketgoizleme.exe"))) {
        location.bundlePath = absolute;
        if (lower.contains(QStringLiteral("ticket-go-izleme-qt")) || lower.contains(QStringLiteral("/build/")) || lower.contains(QStringLiteral("\\build\\"))
            || lower.contains(QStringLiteral("/build-release/")) || lower.contains(QStringLiteral("\\build-release\\"))
            || lower.contains(QStringLiteral("/build-v")) || lower.contains(QStringLiteral("\\build-v"))) {
            location.developerBuild = true;
            location.message = msgDeveloperBuild();
            return location;
        }
        location.installed = true;
        return location;
    }
    const bool insideBundle = absolute.contains(QStringLiteral(".app/Contents/MacOS/"));
    if (!insideBundle) {
        location.message = msgNotInstalled();
        return location;
    }
    const int marker = absolute.indexOf(QStringLiteral(".app/Contents/MacOS/"));
    location.bundlePath = absolute.left(marker + 4);
    if (absolute.contains(QStringLiteral("/ticket-go-izleme-qt/")) || absolute.contains(QStringLiteral("/build/")) || absolute.contains(QStringLiteral("/build-release/"))) {
        location.developerBuild = true;
        location.message = msgDeveloperBuild();
        return location;
    }
    location.installed = true;
    return location;
}

ApplyResult applyUpdate(const ApplyRequest& request, const std::function<bool(const QString& appPath)>& accept)
{
    ApplyResult result;
    result.backupApp = request.backupApp;
#if !defined(Q_OS_MACOS)
    Q_UNUSED(accept);
    result.message = QStringLiteral("Bu kurulum yolu yalnızca macOS uygulama paketleri içindir.");
    return result;
#else
    if (!request.writable) {
        result.message = msgNoPermission();
        return result;
    }
    if (protectedPath(request.targetApp) || protectedPath(request.sourceApp)) {
        result.message = msgDeveloperBuild();
        return result;
    }
    if (!shellPathSafe(request.targetApp) || !shellPathSafe(request.sourceApp) || !shellPathSafe(request.backupApp)) {
        result.message = msgPathRejected();
        return result;
    }
    if (!request.dataDirectory.isEmpty() && (sameOrInside(request.targetApp, request.dataDirectory) || sameOrInside(request.dataDirectory, request.targetApp))) {
        result.message = msgPathRejected();
        return result;
    }
    if (!QFileInfo(request.sourceApp + QStringLiteral("/Contents/Info.plist")).exists()) {
        result.message = QStringLiteral("Yeni paket hazır değil.");
        return result;
    }
    const bool hadTarget = QFileInfo(request.targetApp + QStringLiteral("/Contents/Info.plist")).exists();
    if (hadTarget && !copyTree(request.targetApp, request.backupApp, &result.message)) return result;
    const QString incoming = request.targetApp + QStringLiteral(".incoming");
    QDir(incoming).removeRecursively();
    if (!copyTree(request.sourceApp, incoming, &result.message)) return result;
    const QString previous = request.targetApp + QStringLiteral(".previous");
    QDir(previous).removeRecursively();
    if (hadTarget && !QDir().rename(request.targetApp, previous)) {
        QDir(incoming).removeRecursively();
        result.message = QStringLiteral("Kurulu uygulama taşınamadı. Eski sürüm duruyor.");
        return result;
    }
    if (!QDir().rename(incoming, request.targetApp)) {
        if (hadTarget) QDir().rename(previous, request.targetApp);
        QDir(incoming).removeRecursively();
        result.rolledBack = hadTarget;
        result.message = QStringLiteral("Yeni uygulama yerine konamadı. Önceki sürüm duruyor.");
        return result;
    }
    if (!accept || !accept(request.targetApp)) {
        QDir(request.targetApp).removeRecursively();
        if (hadTarget && QDir().rename(previous, request.targetApp)) result.rolledBack = true;
        else if (hadTarget && copyTree(request.backupApp, request.targetApp, &result.message)) result.rolledBack = true;
        if (result.message.isEmpty()) result.message = QStringLiteral("Yeni sürüm doğrulanamadı. Önceki sürüm geri yüklendi.");
        return result;
    }
    QDir(previous).removeRecursively();
    result.ok = true;
    return result;
#endif
}

bool writeHelperPlan(const QString& path, const HelperPlan& plan, QString* error)
{
    if (!shellPathSafe(plan.sourceApp) || !shellPathSafe(plan.targetApp) || (!plan.backupApp.isEmpty() && !shellPathSafe(plan.backupApp))) {
        if (error) *error = msgPathRejected();
        return false;
    }
    QJsonObject object;
    object.insert(QStringLiteral("parentPid"), static_cast<double>(plan.parentPid));
    object.insert(QStringLiteral("sourceApp"), plan.sourceApp);
    object.insert(QStringLiteral("targetApp"), plan.targetApp);
    object.insert(QStringLiteral("backupApp"), plan.backupApp);
    object.insert(QStringLiteral("dataDirectory"), plan.dataDirectory);
    object.insert(QStringLiteral("bundleId"), plan.bundleId);
    object.insert(QStringLiteral("version"), plan.version);
    object.insert(QStringLiteral("authority"), plan.authority);
    object.insert(QStringLiteral("kind"), plan.kind.isEmpty() ? QStringLiteral("macos-app") : plan.kind);
    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) {
        if (error) *error = QStringLiteral("Güncelleme planı yazılamadı.");
        return false;
    }
    file.write(QJsonDocument(object).toJson(QJsonDocument::Compact));
    return true;
}

HelperPlan readHelperPlan(const QString& path, QString* error)
{
    HelperPlan plan;
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) {
        if (error) *error = QStringLiteral("Güncelleme planı okunamadı.");
        return plan;
    }
    const QJsonObject object = QJsonDocument::fromJson(file.readAll()).object();
    plan.parentPid = static_cast<qint64>(object.value(QStringLiteral("parentPid")).toDouble());
    plan.sourceApp = object.value(QStringLiteral("sourceApp")).toString();
    plan.targetApp = object.value(QStringLiteral("targetApp")).toString();
    plan.backupApp = object.value(QStringLiteral("backupApp")).toString();
    plan.dataDirectory = object.value(QStringLiteral("dataDirectory")).toString();
    plan.bundleId = object.value(QStringLiteral("bundleId")).toString();
    plan.version = object.value(QStringLiteral("version")).toString();
    plan.authority = object.value(QStringLiteral("authority")).toString();
    plan.kind = object.value(QStringLiteral("kind")).toString();
    if (plan.kind.isEmpty()) plan.kind = QStringLiteral("macos-app");
    if (plan.targetApp.isEmpty() || plan.sourceApp.isEmpty()) {
        if (error) *error = QStringLiteral("Güncelleme planı eksik.");
    }
    return plan;
}

int runHelper(const QString& planPath, const SignatureProbe& probe, const std::function<bool(const QString& appPath)>& launcher, bool waitForParent)
{
    QString error;
    const HelperPlan plan = readHelperPlan(planPath, &error);
    if (!error.isEmpty()) return 2;
    if (waitForParent && plan.parentPid > 0) {
        for (int attempt = 0; attempt < 60; ++attempt) {
            if (!parentStillRunning(plan.parentPid)) break;
            QThread::msleep(500);
        }
    }
    if (plan.kind == QLatin1String("windows-setup")) {
        if (!plan.sourceApp.endsWith(QStringLiteral(".exe"), Qt::CaseInsensitive) || !QFileInfo::exists(plan.sourceApp)) return 2;
        if (!shellPathSafe(plan.sourceApp)) return 2;
        QStringList args;
        args << QStringLiteral("/VERYSILENT") << QStringLiteral("/NORESTART") << QStringLiteral("/SUPPRESSMSGBOXES") << QStringLiteral("/CLOSEAPPLICATIONS");
        if (launcher) {
            if (!launcher(plan.sourceApp)) return 5;
            return 0;
        }
        if (!QProcess::startDetached(plan.sourceApp, args)) return 4;
        return 0;
    }
    if (plan.authority.trimmed().isEmpty()) return 2;
    ApplyRequest request;
    request.sourceApp = plan.sourceApp;
    request.targetApp = plan.targetApp;
    request.backupApp = plan.backupApp;
    request.dataDirectory = plan.dataDirectory;
    request.writable = QFileInfo(QFileInfo(plan.targetApp).absolutePath()).isWritable();
    const ApplyResult applied = applyUpdate(request, [&](const QString& appPath) {
        const BundleMeta meta = readBundleMeta(appPath);
        const SignatureInfo signature = probe ? probe(appPath) : SignatureInfo{};
        return meta.bundleId == plan.bundleId && meta.version == plan.version && signature.signedOk && signature.notarized && signature.identifier == plan.bundleId && signature.authority.contains(plan.authority);
    });
    if (!applied.ok) return applied.rolledBack ? 3 : 4;
    if (launcher && !launcher(plan.targetApp)) return 5;
    return 0;
}

} // namespace tg
