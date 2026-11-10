#pragma once

#include "OpsTypes.h"

#include <QString>
#include <QVariantMap>

namespace tg {

QVariantMap buildApiCenter(const QList<ModuleSnapshot>& modules, const QString& environmentText);
bool apiShareableText(const QString& text);

} // namespace tg
