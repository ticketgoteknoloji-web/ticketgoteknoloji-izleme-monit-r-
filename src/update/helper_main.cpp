#include "InstallPlan.h"
#include "PackageVerifier.h"

#include <QCoreApplication>
#include <QProcess>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 2) return 2;
#if defined(Q_OS_WIN)
    return tg::runHelper(QString::fromLocal8Bit(argv[1]), {}, [](const QString& setupPath) {
        return QProcess::startDetached(setupPath, {QStringLiteral("/VERYSILENT"), QStringLiteral("/NORESTART"), QStringLiteral("/SUPPRESSMSGBOXES"), QStringLiteral("/CLOSEAPPLICATIONS")});
    }, true);
#else
    return tg::runHelper(QString::fromLocal8Bit(argv[1]), tg::probeMacSignature, [](const QString& appPath) {
        return QProcess::startDetached(QStringLiteral("/usr/bin/open"), {appPath});
    }, true);
#endif
}
