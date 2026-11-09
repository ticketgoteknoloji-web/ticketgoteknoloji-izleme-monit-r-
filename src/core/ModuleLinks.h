#pragma once

#include <QString>
#include <QStringList>

namespace tg {

QString integrationIdForModule(const QString& moduleId, const QStringList& integrationIds);
QString moduleIdForIntegration(const QString& integrationId);
QString logSourceForModule(const QString& moduleId);
QStringList childModuleIds(const QString& moduleId);

} // namespace tg
