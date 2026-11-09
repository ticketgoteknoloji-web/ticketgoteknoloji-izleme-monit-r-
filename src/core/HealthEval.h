#pragma once

#include "OpsTypes.h"

#include <QJsonArray>

namespace tg {

struct ContractCheck {
    bool ok = false;
    QString error;
    ModuleStatus overall = ModuleStatus::Unknown;
    QList<QPair<QString, ModuleStatus>> components;
    std::optional<double> responseMs;
    std::optional<double> errorRate;
    std::optional<double> rpm;
};

struct OpenApiHint {
    QString method;
    QString path;
};

enum class HealthClass {
    Healthy,
    Warning,
    Critical,
    Disconnected,
    Unknown,
    Disabled,
    NotConfigured
};

HealthClass classifyStatus(ModuleStatus status);
QString healthClassKey(HealthClass healthClass);
QString healthClassLabel(HealthClass healthClass);
bool isProblemClass(HealthClass healthClass);
bool shownOnOverview(ModuleStatus status);
QList<ModuleSnapshot> applyParentGuard(const QList<ModuleSnapshot>& modules);

ModuleSnapshot makeModule(const QString& id, const QString& group, const QString& name, ModuleStatus status, const QString& evidence, qint64 latencyMs = -1);

QList<ModuleSnapshot> evaluateTicketGo(const HttpResult& health, const HttpResult& live, const HttpResult& ready, const HttpResult& identity, const HttpResult& deps, bool depsAttempted);

QList<ModuleSnapshot> evaluateGoChat(const HttpResult& gochat, const HttpResult& parentHealth);

ModuleSnapshot evaluateReachability(const QString& id, const QString& name, const HttpResult& http);

QList<ModuleSnapshot> evaluateFerryApp(const HttpResult& http);

ModuleSnapshot evaluateHttpAdapter(const Integration& integration, const HttpResult& http);

ContractCheck validateContract(const QJsonObject& object);
QList<ModuleSnapshot> evaluateContract(const Integration& integration, const HttpResult& http);

QList<OpenApiHint> suggestOpenApi(const QByteArray& json, QString* error);
Overview summarize(const QList<ModuleSnapshot>& modules, bool hataVar);

} // namespace tg
