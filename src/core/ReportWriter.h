#pragma once

#include "OpsTypes.h"

namespace tg {

bool writeCsvReport(const QString& path, const QString& kind, const Overview& overview, const QList<ModuleSnapshot>& modules, const QList<Incident>& incidents, QString* error);
bool writePdfReport(const QString& path, const QString& kind, const Overview& overview, const QList<ModuleSnapshot>& modules, const QList<Incident>& incidents, QString* error);
bool writeCsvDocument(const QString& path, const QString& title, const QStringList& lines, QString* error);
bool writePdfDocument(const QString& path, const QString& title, const QString& period, const QStringList& lines, QString* error);

} // namespace tg
