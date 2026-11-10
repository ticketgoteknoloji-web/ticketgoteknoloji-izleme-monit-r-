#pragma once

#include "PackageVerifier.h"

#include <QString>

namespace tg {

struct InstallLocation {
    bool installed = false;
    bool developerBuild = false;
    QString bundlePath;
    QString message;
};

struct ApplyRequest {
    QString sourceApp;
    QString targetApp;
    QString backupApp;
    QString dataDirectory;
    bool writable = false;
};

struct ApplyResult {
    bool ok = false;
    bool rolledBack = false;
    QString message;
    QString backupApp;
};

struct HelperPlan {
    qint64 parentPid = 0;
    QString sourceApp;
    QString targetApp;
    QString backupApp;
    QString dataDirectory;
    QString bundleId;
    QString version;
    QString authority;
    QString kind;
};

InstallLocation inspectLocation(const QString& executablePath);
bool shellPathSafe(const QString& path);
ApplyResult applyUpdate(const ApplyRequest& request, const std::function<bool(const QString& appPath)>& accept);
bool writeHelperPlan(const QString& path, const HelperPlan& plan, QString* error);
HelperPlan readHelperPlan(const QString& path, QString* error);
int runHelper(const QString& planPath, const SignatureProbe& probe, const std::function<bool(const QString& appPath)>& launcher, bool waitForParent);

} // namespace tg
