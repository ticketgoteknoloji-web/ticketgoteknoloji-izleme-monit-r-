#include "InstallPlan.h"
#include "PackageVerifier.h"

#include <QCoreApplication>
#include <QProcess>

int main(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    if (argc != 2) return 2;
    return tg::runHelper(QString::fromLocal8Bit(argv[1]), tg::probeMacSignature, [](const QString& appPath) {
        return QProcess::startDetached(QStringLiteral("/usr/bin/open"), {appPath});
    }, true);
}
