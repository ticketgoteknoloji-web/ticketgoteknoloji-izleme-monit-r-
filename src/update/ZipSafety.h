#pragma once

#include <QString>
#include <QStringList>

namespace tg {

struct ZipInspection {
    bool ok = false;
    QString message;
    QStringList names;
    QString appName;
};

ZipInspection inspectZip(const QString& zipPath);
bool extractZip(const QString& zipPath, const QString& destination, QString* error);
bool treeStaysInside(const QString& root, QString* error);
QString findSingleApp(const QString& root, QString* error);

} // namespace tg
