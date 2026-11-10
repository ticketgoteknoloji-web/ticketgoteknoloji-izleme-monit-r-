#include "PackageVerifier.h"

#include "UpdateTypes.h"

#include <QCryptographicHash>
#include <QFile>
#include <QProcess>
#include <QRegularExpression>

namespace tg {
namespace {

QString capture(const QString& text, const QString& key)
{
    const QRegularExpression pattern(key + QStringLiteral("=(.*)"));
    const QRegularExpressionMatch match = pattern.match(text);
    return match.hasMatch() ? match.captured(1).trimmed() : QString();
}

QString plistString(const QString& xml, const QString& key)
{
    const int keyAt = xml.indexOf(QStringLiteral("<key>") + key + QStringLiteral("</key>"));
    if (keyAt < 0) return {};
    const int valueAt = xml.indexOf(QStringLiteral("<string>"), keyAt);
    const int end = xml.indexOf(QStringLiteral("</string>"), valueAt);
    if (valueAt < 0 || end < 0) return {};
    return xml.mid(valueAt + 8, end - (valueAt + 8)).trimmed();
}

} // namespace

QString fileSha256(const QString& path)
{
    QFile file(path);
    if (!file.open(QIODevice::ReadOnly)) return {};
    QCryptographicHash hash(QCryptographicHash::Sha256);
    while (!file.atEnd()) {
        const QByteArray chunk = file.read(1024 * 256);
        if (chunk.isEmpty()) break;
        hash.addData(chunk);
    }
    return QString::fromLatin1(hash.result().toHex());
}

BundleMeta readBundleMeta(const QString& appPath)
{
    BundleMeta meta;
    QFile file(appPath + QStringLiteral("/Contents/Info.plist"));
    if (!file.open(QIODevice::ReadOnly)) return meta;
    const QString xml = QString::fromUtf8(file.readAll());
    meta.bundleId = plistString(xml, QStringLiteral("CFBundleIdentifier"));
    meta.version = plistString(xml, QStringLiteral("CFBundleShortVersionString"));
    meta.ok = !meta.bundleId.isEmpty() && !meta.version.isEmpty();
    return meta;
}

SignatureInfo probeMacSignature(const QString& appPath)
{
    SignatureInfo info;
#if !defined(Q_OS_MACOS)
    Q_UNUSED(appPath);
    info.detail = msgBadSignature();
    return info;
#else
    QProcess verify;
    verify.start(QStringLiteral("/usr/bin/codesign"), {QStringLiteral("--verify"), QStringLiteral("--deep"), QStringLiteral("--strict"), QStringLiteral("--verbose=2"), appPath});
    if (!verify.waitForFinished(30000) || verify.exitCode() != 0) {
        info.detail = QString::fromUtf8(verify.readAllStandardError()).trimmed();
        if (info.detail.isEmpty()) info.detail = msgBadSignature();
        return info;
    }
    QProcess details;
    details.start(QStringLiteral("/usr/bin/codesign"), {QStringLiteral("-dv"), QStringLiteral("--verbose=4"), appPath});
    details.waitForFinished(30000);
    const QString text = QString::fromUtf8(details.readAllStandardError() + details.readAllStandardOutput());
    info.identifier = capture(text, QStringLiteral("Identifier"));
    info.teamId = capture(text, QStringLiteral("TeamIdentifier"));
    const QRegularExpression authority(QStringLiteral("Authority=(.*)"));
    QRegularExpressionMatchIterator iterator = authority.globalMatch(text);
    QStringList authorities;
    while (iterator.hasNext()) authorities.append(iterator.next().captured(1).trimmed());
    info.authority = authorities.join(QStringLiteral(" | "));
    info.signedOk = !info.identifier.isEmpty() && !info.authority.isEmpty() && info.authority.contains(QStringLiteral("Developer ID Application:"));
    if (info.teamId == QLatin1String("not set")) info.signedOk = false;
    QProcess gate;
    gate.start(QStringLiteral("/usr/sbin/spctl"), {QStringLiteral("--assess"), QStringLiteral("--type"), QStringLiteral("execute"), QStringLiteral("--verbose"), appPath});
    gate.waitForFinished(30000);
    const QString gateText = QString::fromUtf8(gate.readAllStandardError() + gate.readAllStandardOutput());
    info.notarized = gate.exitCode() == 0 && (gateText.contains(QStringLiteral("accepted")) || gateText.contains(QStringLiteral("Notarized")));
    if (!info.signedOk) info.detail = msgBadSignature();
    return info;
#endif
}

VerifyResult verifyExtractedApp(const QString& appPath, const QString& expectedSha, const QString& actualSha, const QString& expectedBundleId, const QString& expectedVersion, const QString& expectedAuthority, const SignatureProbe& probe)
{
    VerifyResult result;
    result.appPath = appPath;
    result.hashOk = !expectedSha.isEmpty() && expectedSha.compare(actualSha, Qt::CaseInsensitive) == 0;
    if (!result.hashOk) {
        result.message = msgHashMismatch();
        return result;
    }
    const BundleMeta meta = readBundleMeta(appPath);
    result.layoutOk = meta.ok;
    result.bundleOk = meta.bundleId == expectedBundleId;
    result.versionOk = meta.version == expectedVersion;
    if (!result.bundleOk) {
        result.message = msgBundleMismatch();
        return result;
    }
    if (!result.versionOk) {
        result.message = msgVersionMismatch();
        return result;
    }
    const SignatureInfo signature = probe ? probe(appPath) : SignatureInfo{};
    result.signatureOk = signature.signedOk && signature.identifier == expectedBundleId;
    result.notarized = signature.notarized;
    const bool authorityOk = !expectedAuthority.trimmed().isEmpty() && signature.authority.contains(expectedAuthority.trimmed());
    result.autoInstallAllowed = result.hashOk && result.bundleOk && result.versionOk && result.signatureOk && result.notarized && authorityOk;
    if (!result.signatureOk || !result.notarized || !authorityOk) {
        result.message = expectedAuthority.trimmed().isEmpty() ? msgManualOnly() : msgBadSignature();
        result.guidance = msgManualOnly();
        return result;
    }
    return result;
}

} // namespace tg
