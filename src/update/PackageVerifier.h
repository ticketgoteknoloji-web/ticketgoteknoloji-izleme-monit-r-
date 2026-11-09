#pragma once

#include <QString>

#include <functional>

namespace tg {

struct SignatureInfo {
    bool signedOk = false;
    bool notarized = false;
    QString identifier;
    QString authority;
    QString teamId;
    QString detail;
};

struct BundleMeta {
    QString bundleId;
    QString version;
    bool ok = false;
};

struct VerifyResult {
    bool hashOk = false;
    bool layoutOk = false;
    bool bundleOk = false;
    bool versionOk = false;
    bool signatureOk = false;
    bool notarized = false;
    bool autoInstallAllowed = false;
    QString appPath;
    QString message;
    QString guidance;
};

using SignatureProbe = std::function<SignatureInfo(const QString& appPath)>;

QString fileSha256(const QString& path);
BundleMeta readBundleMeta(const QString& appPath);
SignatureInfo probeMacSignature(const QString& appPath);
VerifyResult verifyExtractedApp(const QString& appPath, const QString& expectedSha, const QString& actualSha, const QString& expectedBundleId, const QString& expectedVersion, const QString& expectedAuthority, const SignatureProbe& probe);

} // namespace tg
