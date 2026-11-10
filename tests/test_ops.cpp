#include "CredentialStore.h"
#include "HealthEval.h"
#include "LogHub.h"
#include "ModuleLinks.h"
#include "IncidentTracker.h"
#include "AppPrefs.h"
#include "ReportStats.h"
#include "PathProbe.h"
#include "ReportWriter.h"
#include "ApiCatalog.h"
#include "OpsTypes.h"
#include "Store.h"

#include <QFile>
#include <QHostAddress>
#include <QPair>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest>

namespace {
tg::HttpResult jsonResult(int status, const QString& body, qint64 latency = 40)
{
    tg::HttpResult result;
    result.attempted = true;
    result.transportOk = true;
    result.status = status;
    result.latencyMs = latency;
    const QJsonDocument document = QJsonDocument::fromJson(body.toUtf8());
    result.jsonOk = document.isObject();
    if (result.jsonOk) result.json = document.object();
    return result;
}

tg::HttpResult failed(const QString& detail)
{
    tg::HttpResult result;
    result.attempted = true;
    result.errorClass = QStringLiteral("connection");
    result.detail = detail;
    return result;
}
}

class TestOps : public QObject {
    Q_OBJECT
private slots:
    void ssrfBlocksDangerousTargets();
    void redactHidesSecrets();
    void gochatDisabledAndPartial();
    void ticketGoDoesNotAssumeHealthy();
    void ratioAndIncidents();
    void storeKeepsRecordWithoutSecret();
    void contractFixtureIsNotLiveData();
    void ferryConfigIsNotASale();
    void healthClassHidesUnknownFromHealthy();
    void badKeyIsNotEchoed();
    void logSourcesStayUnavailableWithoutAccess();
    void moduleDetailUsesOwnSource();
    void deleteKeepsProtectedRecords();
    void hiddenHubLogStaysOnBackendView();
    void settingsRejectShortInterval();
    void reportUsesMeasuredSamplesOnly();
    void acknowledgeDoesNotResolve();
    void latencyStatsNeedEnoughSamples();
    void logReadFailureNotes();
    void silenceDoesNotResolveIncident();
    void pingOutputIsNotStoredAsZero();
    void authEnvelopeDoesNotKeepRejectedToken();
    void logSessionSurvivesNonAuthErrors();
    void integrationFormRejectsBadInput();
    void loginKeychainRoundTrip();
    void netgsmReadySignal();
    void iyzicoPaymentEngine();
    void apiCenterUsesMeasuredStatus();
};

void TestOps::ssrfBlocksDangerousTargets()
{
    QVERIFY(tg::addressBlocked(QHostAddress(QStringLiteral("169.254.169.254")), true));
    QVERIFY(tg::addressBlocked(QHostAddress(QStringLiteral("127.0.0.1")), false));
    QVERIFY(!tg::addressBlocked(QHostAddress(QStringLiteral("127.0.0.1")), true));
    QVERIFY(!tg::precheckUrl(QUrl(QStringLiteral("http://api.ticket-go.net/health")), {}, false).ok);
    QVERIFY(!tg::precheckUrl(QUrl(QStringLiteral("https://user:secret@api.ticket-go.net/health")), {}, false).ok);
    QVERIFY(!tg::precheckUrl(QUrl(QStringLiteral("https://169.254.169.254/latest")), {}, true).ok);
    QVERIFY(tg::precheckUrl(QUrl(QStringLiteral("https://api.ticket-go.net/health")), {}, false).ok);
    QVERIFY(!tg::precheckUrl(QUrl(QStringLiteral("https://evil.example/health")), {}, false).ok);
}

void TestOps::redactHidesSecrets()
{
    const QString cleaned = tg::redactText(QStringLiteral("Authorization: Bearer abc.def.ghi {\"otp\":\"123456\",\"password\":\"hunter\"} +905551112233"));
    QVERIFY(!cleaned.contains(QStringLiteral("abc.def.ghi")));
    QVERIFY(!cleaned.contains(QStringLiteral("hunter")));
    QVERIFY(!cleaned.contains(QStringLiteral("123456")));
    QVERIFY(!cleaned.contains(QStringLiteral("5551112233")));
    QVERIFY(tg::redactText(QStringLiteral("gecikme 148 ms")).contains(QStringLiteral("148")));
}

void TestOps::gochatDisabledAndPartial()
{
    tg::HttpResult missing;
    const auto waiting = tg::evaluateGoChat(missing, missing);
    QCOMPARE(waiting.first().status, tg::ModuleStatus::Checking);
    const auto disabled = tg::evaluateGoChat(jsonResult(404, QStringLiteral("{}")), jsonResult(200, QStringLiteral("{\"status\":\"ok\"}")));
    for (const tg::ModuleSnapshot& module : disabled) QCOMPARE(module.status, tg::ModuleStatus::Disabled);
    const QString body = QStringLiteral(R"({"module":"gochat","enabled":true,"sms":"unconfigured","smsDeliveryProven":false,"fcm":{"status":"unconfigured"},"fcmDeliveryProven":false,"realtime":"websocket"})");
    const auto live = tg::evaluateGoChat(jsonResult(200, body), {});
    auto find = [&](const QString& id) {
        for (const tg::ModuleSnapshot& module : live) if (module.id == id) return module.status;
        return tg::ModuleStatus::Unknown;
    };
    QCOMPARE(find(QStringLiteral("gochat-api")), tg::ModuleStatus::Healthy);
    QCOMPARE(find(QStringLiteral("gochat-otp")), tg::ModuleStatus::ConfigMissing);
    QCOMPARE(find(QStringLiteral("gochat-fcm")), tg::ModuleStatus::ConfigMissing);
    QCOMPARE(find(QStringLiteral("gochat-turn")), tg::ModuleStatus::Unsupported);
    QCOMPARE(find(QStringLiteral("gochat-sos")), tg::ModuleStatus::Unsupported);
    QCOMPARE(find(QStringLiteral("gochat-voice")), tg::ModuleStatus::Unsupported);
}

void TestOps::ticketGoDoesNotAssumeHealthy()
{
    const auto only = tg::evaluateTicketGo(jsonResult(200, QStringLiteral("{\"status\":\"ok\"}")), failed(QStringLiteral("yok")), failed(QStringLiteral("yok")), {}, {}, false);
    auto find = [&](const QString& id) {
        for (const tg::ModuleSnapshot& module : only) if (module.id == id) return module;
        return tg::ModuleSnapshot{};
    };
    QVERIFY(find(QStringLiteral("tg-api")).status != tg::ModuleStatus::Healthy);
    QCOMPARE(find(QStringLiteral("tg-postgres")).status, tg::ModuleStatus::Unknown);
    QCOMPARE(find(QStringLiteral("tg-nginx")).status, tg::ModuleStatus::NotConfigured);
    QCOMPARE(find(QStringLiteral("tg-docker")).status, tg::ModuleStatus::NotConfigured);
    QCOMPARE(find(QStringLiteral("tg-turmob")).status, tg::ModuleStatus::NotConfigured);
    QCOMPARE(find(QStringLiteral("tg-jwt")).status, tg::ModuleStatus::NotConfigured);
    const QString ready = QStringLiteral(R"({"success":true,"data":{"status":"ready","postgres":"down","redis":"disabled","database":"down"}})");
    const auto down = tg::evaluateTicketGo(jsonResult(200, QStringLiteral("{\"status\":\"ok\"}")), jsonResult(200, QStringLiteral("{\"success\":true}")), jsonResult(503, ready), {}, {}, false);
    auto findDown = [&](const QString& id) {
        for (const tg::ModuleSnapshot& module : down) if (module.id == id) return module.status;
        return tg::ModuleStatus::Unknown;
    };
    QCOMPARE(findDown(QStringLiteral("tg-postgres")), tg::ModuleStatus::Critical);
    QCOMPARE(findDown(QStringLiteral("tg-redis")), tg::ModuleStatus::Disabled);
    QCOMPARE(findDown(QStringLiteral("tg-nginx")), tg::ModuleStatus::NotConfigured);
}

void TestOps::ratioAndIncidents()
{
    tg::ModuleSnapshot configured = tg::makeModule(QStringLiteral("a"), QStringLiteral("ticket-go"), QStringLiteral("A"), tg::ModuleStatus::Healthy, QStringLiteral("ok"));
    tg::ModuleSnapshot missing = tg::makeModule(QStringLiteral("b"), QStringLiteral("ticket-go"), QStringLiteral("B"), tg::ModuleStatus::NotConfigured, QStringLiteral("yok"));
    tg::ModuleSnapshot bad = tg::makeModule(QStringLiteral("c"), QStringLiteral("ticket-go"), QStringLiteral("C"), tg::ModuleStatus::Critical, QStringLiteral("down"));
    const tg::Overview overview = tg::summarize({configured, missing, bad}, false);
    QVERIFY(overview.healthRatio.has_value());
    QCOMPARE(*overview.healthRatio, 0.5);
    QCOMPARE(overview.notConfigured, 1);
    tg::IncidentTracker tracker;
    tg::ModuleSnapshot lost = tg::makeModule(QStringLiteral("net"), QStringLiteral("ticket-go"), QStringLiteral("API"), tg::ModuleStatus::ConnectionError, QStringLiteral("kopuk"));
    QVERIFY(tracker.newlyOpened({lost}, QStringLiteral("Log kaynağı bağlı değil")).isEmpty());
    QVERIFY(tracker.newlyOpened({lost}, QStringLiteral("Log kaynağı bağlı değil")).isEmpty());
    const auto opened = tracker.newlyOpened({lost}, QStringLiteral("Log kaynağı bağlı değil"));
    QCOMPARE(opened.size(), 1);
    QCOMPARE(opened.first().severity, QStringLiteral("critical"));
    QVERIFY(opened.first().message.contains(QStringLiteral("kopuk")));
    QCOMPARE(tracker.openCriticalCount(), 1);
}

void TestOps::storeKeepsRecordWithoutSecret()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    const QString dbPath = dir.filePath(QStringLiteral("ops.sqlite"));
    {
        tg::Store store;
        QString error;
        QVERIFY2(store.open(dbPath, &error), qPrintable(error));
        tg::Integration integration;
        integration.id = QStringLiteral("custom-1");
        integration.name = QStringLiteral("Deneme");
        integration.baseUrl = QStringLiteral("https://api.ticket-go.net");
        integration.healthPath = QStringLiteral("/health");
        integration.kind = QStringLiteral("custom");
        QVERIFY(store.saveIntegration(integration, &error));
        store.addSample(QStringLiteral("custom-custom-1"), tg::ModuleStatus::Healthy, 20, 200, QStringLiteral("HTTP 200"));
    }
    {
        tg::Store store;
        QString error;
        QVERIFY(store.open(dbPath, &error));
        bool found = false;
        for (const tg::Integration& item : store.integrations()) {
            if (item.id == QLatin1String("custom-1")) {
                found = true;
                QCOMPARE(item.name, QStringLiteral("Deneme"));
            }
        }
        QVERIFY(found);
        store.setSetting(QStringLiteral("overview_filter"), QStringLiteral("critical"));
    }
    {
        tg::Store store;
        QString error;
        QVERIFY(store.open(dbPath, &error));
        QCOMPARE(store.setting(QStringLiteral("overview_filter"), QStringLiteral("problems")), QStringLiteral("critical"));
    }
    QFile file(dbPath);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray bytes = file.readAll();
    QVERIFY(!bytes.contains("super-secret-token"));
    QVERIFY(!bytes.contains("hunter2"));
}

void TestOps::contractFixtureIsNotLiveData()
{
    const QJsonObject fixture = QJsonDocument::fromJson(QByteArray(R"({"schemaVersion":"1.0","service":"example-service","environment":"production","status":"healthy","timestamp":"2026-11-09T08:00:00Z","metrics":{"responseTimeMs":120,"errorRate":0.01,"requestsPerMinute":450},"components":[{"name":"database","status":"healthy"}]})")).object();
    const tg::ContractCheck check = tg::validateContract(fixture);
    QVERIFY(check.ok);
    QCOMPARE(check.overall, tg::ModuleStatus::Healthy);
    QCOMPARE(check.components.size(), 1);
    tg::Integration integration;
    integration.id = QStringLiteral("fixture");
    integration.name = QStringLiteral("Örnek");
    const auto bad = tg::evaluateContract(integration, jsonResult(200, QStringLiteral("{\"status\":\"ok\"}")));
    QCOMPARE(bad.first().status, tg::ModuleStatus::Warning);
}

void TestOps::ferryConfigIsNotASale()
{
    tg::HttpResult missing;
    QCOMPARE(tg::evaluateFerryApp(missing).first().status, tg::ModuleStatus::Checking);
    const auto unpublished = tg::evaluateFerryApp(jsonResult(200, QStringLiteral("{\"success\":true,\"data\":{\"appName\":\"Ticket-Go\",\"published\":false,\"isActive\":true,\"maintenanceMode\":false,\"apiBaseUrl\":\"https://api.ticket-go.net\"}}")));
    QCOMPARE(unpublished.first().status, tg::ModuleStatus::NotReleased);
    QVERIFY(unpublished.first().evidence.contains(QStringLiteral("published=false")));
    QVERIFY(!unpublished.first().evidence.contains(QStringLiteral("api.ticket-go.net")));
    const auto live = tg::evaluateFerryApp(jsonResult(200, QStringLiteral("{\"data\":{\"appName\":\"Ticket-Go\",\"published\":true,\"isActive\":true,\"maintenanceMode\":false}}")));
    QCOMPARE(live.first().status, tg::ModuleStatus::Limited);
    const auto down = tg::evaluateFerryApp(jsonResult(404, QStringLiteral("{}")));
    QCOMPARE(down.first().status, tg::ModuleStatus::Disabled);
}

void TestOps::healthClassHidesUnknownFromHealthy()
{
    QCOMPARE(tg::classifyStatus(tg::ModuleStatus::Healthy), tg::HealthClass::Healthy);
    QCOMPARE(tg::classifyStatus(tg::ModuleStatus::Warning), tg::HealthClass::Warning);
    QCOMPARE(tg::classifyStatus(tg::ModuleStatus::Critical), tg::HealthClass::Critical);
    QCOMPARE(tg::classifyStatus(tg::ModuleStatus::ConnectionError), tg::HealthClass::Disconnected);
    QCOMPARE(tg::classifyStatus(tg::ModuleStatus::NotConfigured), tg::HealthClass::NotConfigured);
    QCOMPARE(tg::classifyStatus(tg::ModuleStatus::ConfigMissing), tg::HealthClass::NotConfigured);
    QCOMPARE(tg::classifyStatus(tg::ModuleStatus::Disabled), tg::HealthClass::Disabled);
    QCOMPARE(tg::classifyStatus(tg::ModuleStatus::Limited), tg::HealthClass::Unknown);
    QCOMPARE(tg::classifyStatus(tg::ModuleStatus::Unsupported), tg::HealthClass::Unknown);
    QCOMPARE(tg::classifyStatus(tg::ModuleStatus::Checking), tg::HealthClass::Unknown);
    QCOMPARE(tg::classifyStatus(tg::ModuleStatus::NotReleased), tg::HealthClass::Warning);
    QVERIFY(tg::shownOnOverview(tg::ModuleStatus::Healthy));
    QVERIFY(!tg::shownOnOverview(tg::ModuleStatus::Warning));
    QVERIFY(!tg::shownOnOverview(tg::ModuleStatus::Critical));
    QVERIFY(!tg::shownOnOverview(tg::ModuleStatus::ConnectionError));
    QVERIFY(!tg::shownOnOverview(tg::ModuleStatus::Unknown));
    QVERIFY(!tg::shownOnOverview(tg::ModuleStatus::Disabled));
    QVERIFY(!tg::shownOnOverview(tg::ModuleStatus::NotConfigured));
    QVERIFY(!tg::shownOnOverview(tg::ModuleStatus::Limited));
    QVERIFY(!tg::shownOnOverview(tg::ModuleStatus::NotReleased));
    QVERIFY(!tg::isProblemClass(tg::classifyStatus(tg::ModuleStatus::Healthy)));
    QVERIFY(!tg::isProblemClass(tg::classifyStatus(tg::ModuleStatus::NotConfigured)));
    QVERIFY(tg::isProblemClass(tg::classifyStatus(tg::ModuleStatus::Warning)));
    QVERIFY(tg::isProblemClass(tg::classifyStatus(tg::ModuleStatus::ConnectionError)));
    const tg::ModuleSnapshot partial = tg::makeModule(QStringLiteral("part"), QStringLiteral("ticket-go"), QStringLiteral("Kısmi"), tg::ModuleStatus::Limited, QStringLiteral("eksik"));
    const tg::Overview partialOverview = tg::summarize({partial}, false);
    QVERIFY(!partialOverview.healthRatio.has_value());
    QCOMPARE(partialOverview.healthy, 0);
    QCOMPARE(partialOverview.unmonitored, 1);
    tg::ModuleSnapshot api = tg::makeModule(QStringLiteral("tg-api"), QStringLiteral("ticket-go"), QStringLiteral("Backend API"), tg::ModuleStatus::Healthy, QStringLiteral("ok"));
    tg::ModuleSnapshot postgres = tg::makeModule(QStringLiteral("tg-postgres"), QStringLiteral("ticket-go"), QStringLiteral("PostgreSQL"), tg::ModuleStatus::Critical, QStringLiteral("down"));
    tg::ModuleSnapshot nginx = tg::makeModule(QStringLiteral("tg-nginx"), QStringLiteral("ticket-go"), QStringLiteral("Nginx"), tg::ModuleStatus::NotConfigured, QStringLiteral("yok"));
    const auto guarded = tg::applyParentGuard({api, postgres, nginx});
    tg::ModuleStatus apiStatus = tg::ModuleStatus::Unknown;
    for (const tg::ModuleSnapshot& module : guarded) {
        if (module.id == QLatin1String("tg-api")) apiStatus = module.status;
        if (module.id == QLatin1String("tg-nginx")) QCOMPARE(module.status, tg::ModuleStatus::NotConfigured);
    }
    QCOMPARE(apiStatus, tg::ModuleStatus::Critical);
    const tg::Overview rolled = tg::summarize(guarded, false);
    QCOMPARE(rolled.healthy, 0);
    QCOMPARE(rolled.critical, 2);
    QCOMPARE(rolled.notConfigured, 1);
    QVERIFY(rolled.healthRatio.has_value());
    QCOMPARE(*rolled.healthRatio, 0.0);
    tg::ModuleSnapshot chat = tg::makeModule(QStringLiteral("gochat-api"), QStringLiteral("gochat"), QStringLiteral("Go-Chat API"), tg::ModuleStatus::Healthy, QStringLiteral("ok"));
    tg::ModuleSnapshot turn = tg::makeModule(QStringLiteral("gochat-turn"), QStringLiteral("gochat"), QStringLiteral("TURN"), tg::ModuleStatus::Unsupported, QStringLiteral("yok"));
    const auto chatGuarded = tg::applyParentGuard({chat, turn});
    for (const tg::ModuleSnapshot& module : chatGuarded) {
        if (module.id == QLatin1String("gochat-api")) QCOMPARE(module.status, tg::ModuleStatus::Healthy);
    }
}

void TestOps::badKeyIsNotEchoed()
{
    tg::Integration integration;
    integration.id = QStringLiteral("x");
    integration.name = QStringLiteral("X");
    integration.expectedStatus = 200;
    tg::HttpResult result = jsonResult(401, QStringLiteral("{\"token\":\"super-secret-token\"}"));
    const tg::ModuleSnapshot module = tg::evaluateHttpAdapter(integration, result);
    QCOMPARE(module.status, tg::ModuleStatus::AuthRejected);
    QVERIFY(!module.evidence.contains(QStringLiteral("super-secret-token")));
    QVERIFY(!module.lastError.contains(QStringLiteral("super-secret-token")));
}

void TestOps::logSourcesStayUnavailableWithoutAccess()
{
    const QList<tg::LogSource> catalog = tg::discoveredLogSources();
    bool auth = false;
    bool nginx = false;
    for (const tg::LogSource& source : catalog) {
        if (source.id == QLatin1String("auth")) {
            auth = true;
            QVERIFY(source.hubBacked);
        }
        if (source.id == QLatin1String("nginx")) {
            nginx = true;
            QCOMPARE(source.access, QStringLiteral("unavailable"));
        }
    }
    QVERIFY(auth);
    QVERIFY(nginx);
    QList<tg::LogSource> closed = catalog;
    tg::applySourceCounts(closed, {}, false);
    for (const tg::LogSource& source : closed) {
        if (source.id == QLatin1String("auth")) {
            QCOMPARE(source.access, QStringLiteral("unavailable"));
            QCOMPARE(source.total, -1);
        }
    }
    const QList<tg::LogRecord> rows = tg::parseLocalLog(QStringLiteral("2026-11-09T10:00:00Z info: servis ayakta\nERROR: baglanti koptu\n{\"level\":\"info\",\"message\":\"gochat body hello\"}\n"));
    QCOMPARE(rows.at(0).level, QStringLiteral("INFO"));
    QCOMPARE(rows.at(1).level, QStringLiteral("ERROR"));
    QVERIFY(rows.at(2).message.contains(QStringLiteral("gösterilmez")));
    const QString masked = tg::redactLog(QStringLiteral("Bearer eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxMjM0NTY3ODkwIn0.dozjgNryP4J3jVmNHl0w5N_XgL0n3I9PlFUP0THsR8U"));
    QVERIFY(!masked.contains(QStringLiteral("eyJhbGci")));
    const QString analysis = tg::analyzeLogRecord(rows.at(1), rows);
    QVERIFY(analysis.contains(QStringLiteral("Kök neden henüz doğrulanamadı")));
}

void TestOps::moduleDetailUsesOwnSource()
{
    QCOMPARE(tg::integrationIdForModule(QStringLiteral("tg-iyzico"), {}), QStringLiteral("ticket-go-backend"));
    QCOMPARE(tg::integrationIdForModule(QStringLiteral("gochat-fcm"), {}), QStringLiteral("go-chat-api"));
    QCOMPARE(tg::integrationIdForModule(QStringLiteral("tg-ferry"), {}), QStringLiteral("ticket-go-ferry"));
    QCOMPARE(tg::logSourceForModule(QStringLiteral("tg-netgsm")), QStringLiteral("sms"));
    QCOMPARE(tg::logSourceForModule(QStringLiteral("tg-iyzico")), QStringLiteral("payments"));
    QVERIFY(tg::logSourceForModule(QStringLiteral("tg-nginx")).isEmpty());
    QVERIFY(tg::logSourceForModule(QStringLiteral("gochat-api")).isEmpty());
    const QStringList children = tg::childModuleIds(QStringLiteral("gochat-api"));
    QVERIFY(children.contains(QStringLiteral("gochat-fcm")));
    QVERIFY(!children.contains(QStringLiteral("tg-postgres")));
    QVERIFY(!tg::childModuleIds(QStringLiteral("tg-api")).contains(QStringLiteral("gochat-api")));
}

void TestOps::deleteKeepsProtectedRecords()
{
    tg::LogRecord first;
    first.id = QStringLiteral("row-1");
    first.message = QStringLiteral("ilk");
    tg::LogRecord duplicate = first;
    tg::LogRecord second;
    second.id = QStringLiteral("row-2");
    second.message = QStringLiteral("yeni");
    int fresh = 0;
    const QList<tg::LogRecord> merged = tg::mergeLogRecords({first}, {duplicate, second}, 200, &fresh);
    QCOMPARE(merged.size(), 2);
    QCOMPARE(fresh, 1);

    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    tg::Store store;
    QString error;
    QVERIFY2(store.open(dir.filePath(QStringLiteral("ops.sqlite")), &error), qPrintable(error));
    tg::Integration custom;
    custom.id = QStringLiteral("user-api");
    custom.name = QStringLiteral("Kullanıcı API");
    custom.baseUrl = QStringLiteral("https://api.ticket-go.net");
    custom.healthPath = QStringLiteral("/health");
    custom.locked = false;
    QVERIFY(store.saveIntegration(custom, &error));
    QVERIFY(store.removeIntegration(custom.id, &error));
    store.appendAudit(QStringLiteral("yerel-entegrasyon-silindi"), custom.id, QStringLiteral("Uzak servis kapatılmadı."));
    bool customGone = true;
    bool builtinRemains = false;
    for (const tg::Integration& item : store.integrations()) {
        if (item.id == QLatin1String("user-api")) customGone = false;
        if (item.id == QLatin1String("ticket-go-backend") && item.locked) builtinRemains = true;
    }
    QVERIFY(customGone);
    QVERIFY(builtinRemains);
    QVERIFY(!store.removeIntegration(QStringLiteral("ticket-go-backend"), &error));
    QCOMPARE(store.auditCount(), 1);
    tg::Store reopened;
    QVERIFY(reopened.open(dir.filePath(QStringLiteral("ops.sqlite")), &error));
    for (const tg::Integration& item : reopened.integrations()) QVERIFY(item.id != QLatin1String("user-api"));
    QCOMPARE(reopened.auditCount(), 1);
}

void TestOps::hiddenHubLogStaysOnBackendView()
{
    tg::LogRecord kept;
    kept.id = QStringLiteral("hub-keep");
    kept.message = QStringLiteral("duruyor");
    tg::LogRecord hidden;
    hidden.id = QStringLiteral("hub-hide");
    hidden.message = QStringLiteral("gizlenen");
    const QList<tg::LogRecord> visible = tg::omitHiddenLogs({kept, hidden}, {QStringLiteral("hub-hide")});
    QCOMPARE(visible.size(), 1);
    QCOMPARE(visible.first().id, QStringLiteral("hub-keep"));
    QCOMPARE(tg::omitHiddenLogs({kept}, {}).size(), 1);
}

void TestOps::settingsRejectShortInterval()
{
    const tg::PrefCheck tooFast = tg::checkMonitorPrefs(5, 8, 4, 1500, 1, QStringLiteral("high"));
    QVERIFY(!tooFast.ok);
    QVERIFY(tooFast.error.contains(QStringLiteral("Kayıt yazılmadı")));
    const tg::PrefCheck ok = tg::checkMonitorPrefs(15, 8, 0, 1500, 1, QStringLiteral("low"));
    QVERIFY(ok.ok);
    const tg::PrefCheck badQuality = tg::checkMonitorPrefs(60, 8, 4, 1500, 1, QStringLiteral("ultra"));
    QVERIFY(!badQuality.ok);
}

void TestOps::reportUsesMeasuredSamplesOnly()
{
    QCOMPARE(tg::availabilityText(tg::summarizeProbes({})), QStringLiteral("Hesaplanamadı"));
    tg::ProbeSample ok;
    ok.status = QStringLiteral("healthy");
    ok.latencyMs = 10;
    tg::ProbeSample bad;
    bad.status = QStringLiteral("critical");
    bad.latencyMs = 30;
    tg::ProbeSample idle;
    idle.status = QStringLiteral("not_configured");
    idle.latencyMs = -1;
    const tg::PeriodStats stats = tg::summarizeProbes({ok, bad, idle});
    QVERIFY(stats.availability.has_value());
    QCOMPARE(stats.measured, 2);
    QCOMPARE(stats.excluded, 1);
    QCOMPARE(int(*stats.availability * 100), 50);
    QVERIFY(!stats.p95LatencyMs.has_value());
    QString error;
    QDateTime from;
    QDateTime to;
    QVERIFY(!tg::resolveReportRange(QStringLiteral("custom"), QStringLiteral("2026-11-09T12:00:00"), QStringLiteral("2026-11-09T11:00:00"), QDateTime::currentDateTime(), &from, &to, &error));
    QCOMPARE(tg::csvCell(QStringLiteral("=CMD")), QStringLiteral("\"'=CMD\""));
    QTemporaryDir dir;
    const QString csv = dir.filePath(QStringLiteral("rapor.csv"));
    QVERIFY(tg::writeCsvDocument(csv, QStringLiteral("TICKET GO TEKNOLOJİ"), {QStringLiteral("Erişilebilirlik: Hesaplanamadı")}, &error));
    QFile file(csv);
    QVERIFY(file.open(QIODevice::ReadOnly));
    const QByteArray bytes = file.readAll();
    QVERIFY(bytes.startsWith("\xEF\xBB\xBF"));
    QVERIFY(bytes.contains("Hesaplanamad"));
    tg::Store store;
    QVERIFY(store.open(dir.filePath(QStringLiteral("ops.sqlite")), &error));
    store.addSample(QStringLiteral("tg-api"), tg::ModuleStatus::Healthy, 12, 200, QStringLiteral("ok"));
    QVERIFY(store.insertSavedReport(QStringLiteral("r1"), QStringLiteral("Genel"), QStringLiteral("overview"), QStringLiteral("dönem"), QStringLiteral("ticket-go"), QStringLiteral("kayıt"), QStringLiteral("kaydedildi"), QString(), QStringLiteral("özet"), &error));
    QVERIFY(store.removeSavedReport(QStringLiteral("r1"), &error));
    int badCount = 0;
    QCOMPARE(store.sampleCount(QStringLiteral("tg-api"), QDateTime::fromMSecsSinceEpoch(0), &badCount), 1);
    QVERIFY(store.savedReport(QStringLiteral("r1")).isEmpty());
    QCOMPARE(store.auditCount(), 0);
}

void TestOps::acknowledgeDoesNotResolve()
{
    tg::Incident incident;
    incident.id = QStringLiteral("i1");
    incident.state = QStringLiteral("open");
    incident.severity = QStringLiteral("critical");
    tg::IncidentTracker tracker;
    tracker.restore({incident});
    tracker.acknowledge(QStringLiteral("i1"));
    QCOMPARE(tracker.all().first().state, QStringLiteral("acknowledged"));
    QVERIFY(tracker.all().first().resolvedAt.isNull());
}

void TestOps::latencyStatsNeedEnoughSamples()
{
    const tg::SeriesStats empty = tg::summarizeSeries({});
    QCOMPARE(empty.count, 0);
    QVERIFY(!empty.p95.has_value());
    QList<double> values;
    for (int i = 1; i <= 20; ++i) values.push_back(i * 10.0);
    const tg::SeriesStats stats = tg::summarizeSeries(values);
    QCOMPARE(stats.count, 20);
    QCOMPARE(*stats.minimum, 10.0);
    QCOMPARE(*stats.maximum, 200.0);
    QVERIFY(stats.p95.has_value());
    const tg::SeriesStats shortSeries = tg::summarizeSeries({40.0, 80.0});
    QVERIFY(!shortSeries.p95.has_value());
    QList<QPair<qint64, double>> points;
    for (int i = 0; i < 100; ++i) points.push_back({i, i + 0.5});
    const auto thin = tg::thinSeries(points, 8);
    QVERIFY(thin.size() <= 9);
    QCOMPARE(thin.first().first, 0);
    QCOMPARE(thin.last().first, 99);
    QCOMPARE(tg::thinSeries(points.mid(0, 3), 48).size(), 3);
}

void TestOps::logReadFailureNotes()
{
    const QString missing = tg::logReadFailureNote(false, true, 0, QString());
    QVERIFY(missing.contains(QStringLiteral("Log veri kaynağına erişilemiyor")));
    QVERIFY(missing.contains(QStringLiteral("Kimliksiz istek gönderilmedi")));
    QCOMPARE(tg::logReadFailureNote(true, true, 401, QStringLiteral("UNAUTHENTICATED")), QStringLiteral("Oturum doğrulanamadı. Kod: UNAUTHENTICATED"));
    QCOMPARE(tg::logReadFailureNote(true, true, 403, QStringLiteral("FORBIDDEN")), QStringLiteral("İzleme hesabınızın Sistem Logları okuma yetkisi yok. Kod: FORBIDDEN"));
    QCOMPARE(tg::logReadFailureNote(true, true, 404, QString()), QStringLiteral("Sistem Logları API bulunamadı."));
    QVERIFY(tg::logReadFailureNote(true, true, 500, QStringLiteral("INTERNAL_ERROR")).startsWith(QStringLiteral("Backend log servisi hata verdi.")));
    QCOMPARE(tg::logReadFailureNote(true, false, 0, QString(), QStringLiteral("timeout")), QStringLiteral("Log sunucusundan yanıt alınamadı."));
    QCOMPARE(tg::logReadFailureNote(true, false, 0, QString()), QStringLiteral("Log veri kaynağına erişilemiyor."));
    QJsonArray items;
    QJsonObject row;
    row.insert(QStringLiteral("id"), QStringLiteral("log-1"));
    row.insert(QStringLiteral("logNo"), QStringLiteral("SL-1"));
    row.insert(QStringLiteral("level"), QStringLiteral("error"));
    row.insert(QStringLiteral("module"), QStringLiteral("api"));
    row.insert(QStringLiteral("logType"), QStringLiteral("http"));
    row.insert(QStringLiteral("message"), QStringLiteral("baglanti koptu"));
    row.insert(QStringLiteral("createdAt"), QStringLiteral("2026-11-09T12:00:00.000Z"));
    row.insert(QStringLiteral("date"), QStringLiteral("2026-11-09"));
    row.insert(QStringLiteral("time"), QStringLiteral("12:00:00"));
    row.insert(QStringLiteral("statusCode"), 500);
    items.append(row);
    const QList<tg::LogRecord> parsed = tg::parseHubLogItems(items);
    QCOMPARE(parsed.size(), 1);
    QCOMPARE(parsed.first().id, QStringLiteral("log-1"));
    QCOMPARE(parsed.first().date, QStringLiteral("2026-11-09"));
    QCOMPARE(parsed.first().clock, QStringLiteral("12:00:00"));
    QCOMPARE(parsed.first().level, QStringLiteral("ERROR"));
    QCOMPARE(parsed.first().module, QStringLiteral("api"));
    QCOMPARE(parsed.first().service, QStringLiteral("http"));
    QCOMPARE(parsed.first().httpCode, QStringLiteral("500"));
    QVERIFY(!tg::logReadFailureNote(true, true, 401, QStringLiteral("eyJhbGciOi.secret")).contains(QStringLiteral("eyJ")));
    QJsonObject rich;
    rich.insert(QStringLiteral("id"), QStringLiteral("log-2"));
    rich.insert(QStringLiteral("level"), QStringLiteral("info"));
    rich.insert(QStringLiteral("logType"), QStringLiteral("sms"));
    rich.insert(QStringLiteral("userEmail"), QStringLiteral("admin@ticket-go.net"));
    rich.insert(QStringLiteral("durationMs"), 42);
    rich.insert(QStringLiteral("status"), QStringLiteral("success"));
    rich.insert(QStringLiteral("message"), QStringLiteral("eyJhbGciOiJIUzI1NiIsInR5cCI6IkpXVCJ9.eyJzdWIiOiIxIn0.signature"));
    QJsonArray richItems;
    richItems.append(rich);
    const tg::LogRecord masked = tg::parseHubLogItems(richItems).first();
    QCOMPARE(masked.service, QStringLiteral("sms"));
    QCOMPARE(masked.duration, QStringLiteral("42"));
    QCOMPARE(masked.outcome, QStringLiteral("success"));
    QCOMPARE(masked.user, QStringLiteral("[e-posta gizlendi]"));
    QVERIFY(!masked.message.contains(QStringLiteral("eyJ")));
    tg::LogQuery query;
    query.logType = QStringLiteral("sms");
    query.page = 2;
    const QUrl listUrl = tg::systemLogRequestUrl(query, false, QString());
    QVERIFY(listUrl.path().endsWith(QStringLiteral("/api/v1/system-logs")));
    QVERIFY(listUrl.query().contains(QStringLiteral("logType=sms")));
    QVERIFY(listUrl.query().contains(QStringLiteral("page=2")));
    QVERIFY(!listUrl.toString().contains(QStringLiteral("retention")));
    QVERIFY(!listUrl.toString().contains(QStringLiteral("seed")));
    const QUrl liveUrl = tg::systemLogRequestUrl(query, true, QStringLiteral("2026-11-09T12:00:00Z"));
    QVERIFY(liveUrl.path().endsWith(QStringLiteral("/live")));
    QVERIFY(liveUrl.query().contains(QStringLiteral("since=2026-11-09T12:00:00Z")));
    QVERIFY(liveUrl.query().contains(QStringLiteral("limit=20")));
    QCOMPARE(tg::hubLogSections().size(), 14);
    QCOMPARE(tg::logPollIntervalMs(QStringLiteral("1")), 5000);
    QCOMPARE(tg::logPollIntervalMs(QStringLiteral("15")), 15000);
    QCOMPARE(tg::logPollIntervalMs(QStringLiteral("nope")), 5000);
    tg::LogRecord first = masked;
    tg::LogRecord second = masked;
    second.id = QStringLiteral("log-3");
    int fresh = 0;
    const auto merged = tg::mergeLogRecords({first}, {first, second}, 80, &fresh);
    QCOMPARE(merged.size(), 2);
    QCOMPARE(fresh, 1);
}

void TestOps::silenceDoesNotResolveIncident()
{
    tg::IncidentTracker tracker;
    tg::ModuleSnapshot lost = tg::makeModule(QStringLiteral("tg-api"), QStringLiteral("ticket-go"), QStringLiteral("API"), tg::ModuleStatus::ConnectionError, QStringLiteral("kopuk"));
    lost.lastHttp = 503;
    tracker.newlyOpened({lost}, QStringLiteral("yok"));
    tracker.newlyOpened({lost}, QStringLiteral("yok"));
    const auto opened = tracker.newlyOpened({lost}, QStringLiteral("yok"));
    QCOMPARE(opened.size(), 1);
    QCOMPARE(opened.first().httpStatus, 503);
    QCOMPARE(tracker.openCriticalCount(), 1);
    const QString id = opened.first().id;
    const QDateTime future = QDateTime::currentDateTime().addSecs(60);
    QVERIFY(tg::incidentSilenceActive(id + QLatin1Char('|') + future.toString(Qt::ISODate), id, QDateTime::currentDateTime()));
    QVERIFY(!tg::incidentSilenceActive(id + QLatin1Char('|') + QDateTime::currentDateTime().addSecs(-60).toString(Qt::ISODate), id, QDateTime::currentDateTime()));
    QCOMPARE(tracker.all().first().state, QStringLiteral("open"));
    tg::ModuleSnapshot healthy = tg::makeModule(QStringLiteral("tg-api"), QStringLiteral("ticket-go"), QStringLiteral("API"), tg::ModuleStatus::Healthy, QStringLiteral("ok"));
    tracker.newlyOpened({healthy}, QStringLiteral("yok"));
    QCOMPARE(tracker.all().first().state, QStringLiteral("open"));
    tracker.newlyOpened({healthy}, QStringLiteral("yok"));
    QCOMPARE(tracker.all().first().state, QStringLiteral("resolved"));
    QCOMPARE(tracker.openCriticalCount(), 0);
    QVERIFY(tracker.all().first().resolvedAt.isValid());
}

void TestOps::pingOutputIsNotStoredAsZero()
{
    const tg::PingRead reply = tg::readPingOutput(QStringLiteral("64 bytes from 1.2.3.4: icmp_seq=0 ttl=54 time=21.345 ms\n1 packets transmitted, 1 packets received, 0.0% packet loss\n"));
    QVERIFY(reply.replied);
    QCOMPARE(reply.lossPercent, 0);
    QVERIFY(reply.latencyMs > 21.0 && reply.latencyMs < 22.0);
    const tg::PingRead lost = tg::readPingOutput(QStringLiteral("1 packets transmitted, 0 packets received, 100.0% packet loss\n"));
    QVERIFY(!lost.replied);
    QCOMPARE(lost.lossPercent, 100);
    QVERIFY(lost.latencyMs < 0);
    const tg::PingRead empty = tg::readPingOutput(QString());
    QVERIFY(!empty.replied);
    QVERIFY(empty.latencyMs < 0);
}

void TestOps::authEnvelopeDoesNotKeepRejectedToken()
{
    const QJsonObject denied = QJsonDocument::fromJson(QByteArrayLiteral("{\"success\":false,\"error\":{\"code\":\"UNAUTHENTICATED\",\"message\":\"İç yönetim erişimi için kimlik gerekli (JWT)\"}}")).object();
    const tg::AuthEnvelope missing = tg::readAuthEnvelope(401, denied);
    QVERIFY(missing.accessToken.isEmpty());
    QCOMPARE(missing.code, QStringLiteral("UNAUTHENTICATED"));
    QVERIFY(!tg::shouldRefreshLogSession(401, false, false));
    QVERIFY(tg::shouldRefreshLogSession(401, true, false));
    QVERIFY(!tg::shouldRefreshLogSession(401, true, true));
    QVERIFY(!tg::shouldRefreshLogSession(403, true, false));
    const QJsonObject ok = QJsonDocument::fromJson(QByteArrayLiteral("{\"success\":true,\"data\":{\"accessToken\":\"access-token-value-not-real\",\"refreshToken\":\"refresh-token-value\",\"user\":{\"username\":\"ops\"}}}")).object();
    const tg::AuthEnvelope session = tg::readAuthEnvelope(200, ok);
    QCOMPARE(session.accessToken, QStringLiteral("access-token-value-not-real"));
    QCOMPARE(session.account, QStringLiteral("ops"));
    const QJsonObject mfa = QJsonDocument::fromJson(QByteArrayLiteral("{\"success\":true,\"data\":{\"mfaRequired\":true,\"challengeToken\":\"challenge-token-value-not-real\",\"accessToken\":\"must-not-count-as-session-token\"}}")).object();
    const tg::AuthEnvelope challenge = tg::readAuthEnvelope(200, mfa);
    QVERIFY(challenge.mfaRequired);
    QVERIFY(challenge.accessToken.isEmpty());
    const tg::SessionDecision waiting = tg::decideAuthResult(true, 200, challenge);
    QCOMPARE(waiting.phase, QStringLiteral("mfa"));
    QVERIFY(!waiting.storeSession);
    const tg::SessionDecision opened = tg::decideAuthResult(true, 200, session);
    QCOMPARE(opened.phase, QStringLiteral("open"));
    QVERIFY(opened.storeSession);
    const tg::SessionDecision deniedStep = tg::decideAuthResult(true, 401, missing);
    QCOMPARE(deniedStep.phase, QStringLiteral("error"));
    QVERIFY(!deniedStep.storeSession);
    QCOMPARE(tg::logReadFailureNote(true, true, 403, QString()), QStringLiteral("İzleme hesabınızın Sistem Logları okuma yetkisi yok."));
}

void TestOps::logSessionSurvivesNonAuthErrors()
{
    QVERIFY(!tg::adoptPreviousLogSession());
    QCOMPARE(tg::emptyLogResultNote(), QStringLiteral("Kayıt bulunamadı"));
    QCOMPARE(tg::logSessionPhaseAfterResult(QStringLiteral("open"), 403, true, false), QStringLiteral("open"));
    QCOMPARE(tg::logSessionPhaseAfterResult(QStringLiteral("open"), 0, false, false), QStringLiteral("open"));
    QCOMPARE(tg::logSessionPhaseAfterResult(QStringLiteral("open"), 500, true, false), QStringLiteral("open"));
    QCOMPARE(tg::logSessionPhaseAfterResult(QStringLiteral("open"), 401, true, true), QStringLiteral("open"));
    QCOMPARE(tg::logSessionPhaseAfterResult(QStringLiteral("open"), 401, true, false), QStringLiteral("none"));
    QCOMPARE(tg::logSessionPhaseAfterResult(QStringLiteral("none"), 401, true, false), QStringLiteral("none"));
    QCOMPARE(tg::logReadFailureNote(true, false, 0, QString(), QStringLiteral("timeout")), QStringLiteral("Log sunucusundan yanıt alınamadı."));
    QCOMPARE(tg::logReadFailureNote(true, true, 403, QStringLiteral("FORBIDDEN")), QStringLiteral("İzleme hesabınızın Sistem Logları okuma yetkisi yok. Kod: FORBIDDEN"));
}

void TestOps::integrationFormRejectsBadInput()
{
    tg::Integration valid;
    valid.name = QStringLiteral("Kayit");
    valid.category = QStringLiteral("Uygulama");
    valid.baseUrl = QStringLiteral("https://api.ticket-go.net");
    valid.healthPath = QStringLiteral("/health");
    valid.method = QStringLiteral("GET");
    valid.authType = QStringLiteral("none");
    valid.intervalMs = 60000;
    valid.timeoutMs = 8000;
    valid.expectedStatus = 200;
    valid.latencyWarnMs = 1500;
    valid.failThreshold = 3;
    valid.adapter = QStringLiteral("http");
    const QVariantMap ok = tg::integrationFieldErrors(valid, {});
    QVERIFY(!ok.contains(QStringLiteral("summary")));
    QCOMPARE(ok.value(QStringLiteral("healthPathValue")).toString(), QStringLiteral("/health"));

    tg::Integration nameless = valid;
    nameless.name.clear();
    QVERIFY(tg::integrationFieldErrors(nameless, {}).value(QStringLiteral("summary")).toString().contains(QStringLiteral("Entegrasyon adı")));

    tg::Integration plain = valid;
    plain.baseUrl = QStringLiteral("http://api.ticket-go.net");
    QVERIFY(tg::integrationFieldErrors(plain, {}).value(QStringLiteral("summary")).toString().contains(QStringLiteral("HTTPS")));

    tg::Integration fast = valid;
    fast.intervalMs = 1000;
    QVERIFY(tg::integrationFieldErrors(fast, {}).value(QStringLiteral("summary")).toString().contains(QStringLiteral("Kontrol aralığı")));

    tg::Integration negative = valid;
    negative.timeoutMs = -1;
    QVERIFY(tg::integrationFieldErrors(negative, {}).value(QStringLiteral("summary")).toString().contains(QStringLiteral("Zaman aşımı")));

    tg::Integration dotted = valid;
    dotted.healthPath = QStringLiteral("/../secret");
    QVERIFY(tg::integrationFieldErrors(dotted, {}).value(QStringLiteral("summary")).toString().contains(QStringLiteral("Health yolu")));

    tg::Integration other = valid;
    other.id = QStringLiteral("var");
    tg::Integration duplicate = valid;
    duplicate.id = QStringLiteral("yeni");
    QVERIFY(tg::integrationFieldErrors(duplicate, {other}).value(QStringLiteral("summary")).toString().contains(QStringLiteral("başka bir kayıtta")));

    tg::Integration full = valid;
    full.healthPath = QStringLiteral("https://api.ticket-go.net/health");
    QCOMPARE(tg::integrationFieldErrors(full, {}).value(QStringLiteral("healthPathValue")).toString(), QStringLiteral("/health"));

    QString authError;
    const QByteArray header = tg::basicAuthorizationValue(QStringLiteral("user:s3cret"), &authError);
    QVERIFY(header.startsWith("Basic "));
    QVERIFY(!header.contains("s3cret"));
    QVERIFY(authError.isEmpty());
    QVERIFY(tg::basicAuthorizationValue(QStringLiteral("sadece"), &authError).isEmpty());
    QVERIFY(authError.contains(QStringLiteral("gösterilmez")));

    QCOMPARE(tg::integrationBucket(tg::ModuleStatus::Healthy, false), QStringLiteral("healthy"));
    QCOMPARE(tg::integrationBucket(tg::ModuleStatus::Healthy, true), QStringLiteral("disabled"));
    QCOMPARE(tg::integrationBucket(tg::ModuleStatus::Checking, false), QStringLiteral("unconfigured"));
    QCOMPARE(tg::integrationBucket(tg::ModuleStatus::ConnectionError, false), QStringLiteral("connection"));
    QCOMPARE(tg::moduleIdForIntegration(QStringLiteral("user-api")), QStringLiteral("custom-user-api"));

    tg::HttpResult http = jsonResult(200, QStringLiteral("{\"status\":\"ok\"}"));
    const tg::ModuleSnapshot statusOnly = tg::evaluateHttpAdapter(valid, http);
    QCOMPARE(statusOnly.status, tg::ModuleStatus::Healthy);
    QVERIFY(statusOnly.evidence.contains(QStringLiteral("Gövde saklanmadı")));
    valid.adapter = QStringLiteral("contract-1.0");
    QCOMPARE(tg::evaluateContract(valid, http).first().status, tg::ModuleStatus::Warning);
}

void TestOps::loginKeychainRoundTrip()
{
#if !defined(Q_OS_MACOS)
    QSKIP("macOS anahtar zinciri");
#else
    tg::KeychainCredentialStore store;
    const QString account = QStringLiteral("izleme-keychain-probe");
    const QString secret = QStringLiteral("probe-not-a-token");
    store.removeSecret(account, nullptr);
    QString error;
    QVERIFY2(store.setSecret(account, secret, &error), qPrintable(error));
    bool ok = false;
    error.clear();
    const QString readBack = tg::KeychainCredentialStore().secret(account, &ok, &error);
    QVERIFY2(ok, qPrintable(error));
    QCOMPARE(readBack, secret);
    error.clear();
    QVERIFY2(store.removeSecret(account, &error), qPrintable(error));
    QVERIFY(!tg::KeychainCredentialStore().contains(account));
#endif
}

void TestOps::netgsmReadySignal()
{
    auto find = [](const QList<tg::ModuleSnapshot>& modules, const QString& id) {
        for (const tg::ModuleSnapshot& module : modules) if (module.id == id) return module;
        return tg::ModuleSnapshot{};
    };
    const QString health = QStringLiteral("{\"status\":\"ok\"}");
    const auto ready = [&](const QString& netgsm) {
        return QStringLiteral("{\"success\":true,\"data\":{\"status\":\"ready\",\"postgres\":\"up\",\"integrations\":{\"netgsm\":%1,\"smtp\":{\"status\":\"ok\",\"enabled\":true,\"configured\":true},\"liknoss\":{\"status\":\"ok\",\"enabled\":true,\"configured\":true}}}}").arg(netgsm);
    };
    const auto ok = tg::evaluateTicketGo(jsonResult(200, health), failed(QStringLiteral("yok")), jsonResult(200, ready(QStringLiteral("{\"status\":\"ok\",\"enabled\":true,\"configured\":true}"))), {}, {}, false);
    const tg::ModuleSnapshot netgsm = find(ok, QStringLiteral("tg-netgsm"));
    QCOMPARE(netgsm.status, tg::ModuleStatus::Healthy);
    QCOMPARE(netgsm.connection, QStringLiteral("Bağlı"));
    QCOMPARE(tg::moduleStatusLabel(netgsm.id, netgsm.status), QStringLiteral("Çalışıyor"));
    QCOMPARE(tg::toneFor(netgsm.status), QStringLiteral("green"));
    QCOMPARE(netgsm.evidence, QStringLiteral("NetGSM etkin ve yapılandırılmış. Backend durum sinyali: ok."));
    QCOMPARE(find(ok, QStringLiteral("tg-mfa")).status, tg::ModuleStatus::Limited);
    QCOMPARE(find(ok, QStringLiteral("tg-liknoss")).status, tg::ModuleStatus::Limited);
    QCOMPARE(find(ok, QStringLiteral("tg-postgres")).status, tg::ModuleStatus::Healthy);

    const auto disabled = tg::evaluateTicketGo(jsonResult(200, health), failed(QStringLiteral("yok")), jsonResult(200, ready(QStringLiteral("{\"status\":\"ok\",\"enabled\":false,\"configured\":true}"))), {}, {}, false);
    QCOMPARE(find(disabled, QStringLiteral("tg-netgsm")).status, tg::ModuleStatus::Disabled);
    const auto missing = tg::evaluateTicketGo(jsonResult(200, health), failed(QStringLiteral("yok")), jsonResult(200, ready(QStringLiteral("{\"status\":\"ok\",\"enabled\":true,\"configured\":false}"))), {}, {}, false);
    QCOMPARE(find(missing, QStringLiteral("tg-netgsm")).status, tg::ModuleStatus::ConfigMissing);
    const auto broken = tg::evaluateTicketGo(jsonResult(200, health), failed(QStringLiteral("yok")), jsonResult(200, ready(QStringLiteral("{\"status\":\"error\",\"enabled\":true,\"configured\":true}"))), {}, {}, false);
    QCOMPARE(find(broken, QStringLiteral("tg-netgsm")).status, tg::ModuleStatus::Critical);
    const auto down = tg::evaluateTicketGo(jsonResult(200, health), failed(QStringLiteral("yok")), jsonResult(200, ready(QStringLiteral("{\"status\":\"down\",\"enabled\":true,\"configured\":true}"))), {}, {}, false);
    QCOMPARE(find(down, QStringLiteral("tg-netgsm")).status, tg::ModuleStatus::Critical);

    tg::HttpResult offline;
    offline.attempted = true;
    offline.transportOk = false;
    offline.detail = QStringLiteral("timeout");
    const auto lost = tg::evaluateTicketGo(jsonResult(200, health), failed(QStringLiteral("yok")), offline, {}, {}, false);
    QVERIFY(find(lost, QStringLiteral("tg-netgsm")).status != tg::ModuleStatus::Healthy);
    QCOMPARE(find(lost, QStringLiteral("tg-netgsm")).status, tg::ModuleStatus::Unknown);
    QCOMPARE(find(lost, QStringLiteral("tg-mfa")).status, tg::ModuleStatus::NotConfigured);
}

void TestOps::iyzicoPaymentEngine()
{
    auto find = [](const QList<tg::ModuleSnapshot>& modules, const QString& id) {
        for (const tg::ModuleSnapshot& module : modules) if (module.id == id) return module;
        return tg::ModuleSnapshot{};
    };
    const QString health = QStringLiteral("{\"status\":\"ok\"}");
    const auto ready = [](const QString& engine) {
        return QStringLiteral("{\"success\":true,\"data\":{\"status\":\"ready\",\"postgres\":\"up\",\"integrations\":{\"smtp\":{\"status\":\"ok\",\"enabled\":true,\"configured\":true}},\"paymentEngine\":%1}}").arg(engine);
    };
    const auto live = tg::evaluateTicketGo(jsonResult(200, health), failed(QStringLiteral("yok")), jsonResult(200, ready(QStringLiteral("{\"status\":\"LIVE\",\"ready\":true,\"provider\":\"iyzico\"}"))), {}, {}, false);
    const tg::ModuleSnapshot iyzico = find(live, QStringLiteral("tg-iyzico"));
    QCOMPARE(iyzico.status, tg::ModuleStatus::Healthy);
    QCOMPARE(iyzico.connection, QStringLiteral("Bağlı"));
    QCOMPARE(tg::moduleStatusLabel(iyzico.id, iyzico.status), QStringLiteral("Çalışıyor"));
    QCOMPARE(tg::toneFor(iyzico.status), QStringLiteral("green"));
    QCOMPARE(iyzico.evidence, QStringLiteral("İyzico ödeme motoru canlı ve hazır."));
    QVERIFY(!iyzico.evidence.contains(QStringLiteral("tahsilat")));
    QCOMPARE(find(live, QStringLiteral("tg-postgres")).status, tg::ModuleStatus::Healthy);
    QCOMPARE(find(live, QStringLiteral("tg-mfa")).status, tg::ModuleStatus::Limited);

    const auto incomplete = tg::evaluateTicketGo(jsonResult(200, health), failed(QStringLiteral("yok")), jsonResult(200, ready(QStringLiteral("{\"status\":\"CONFIG_INCOMPLETE\",\"ready\":false,\"provider\":\"iyzico\"}"))), {}, {}, false);
    QCOMPARE(find(incomplete, QStringLiteral("tg-iyzico")).status, tg::ModuleStatus::ConfigMissing);
    const auto notReady = tg::evaluateTicketGo(jsonResult(200, health), failed(QStringLiteral("yok")), jsonResult(200, ready(QStringLiteral("{\"status\":\"LIVE\",\"ready\":false,\"provider\":\"iyzico\"}"))), {}, {}, false);
    QVERIFY(find(notReady, QStringLiteral("tg-iyzico")).status != tg::ModuleStatus::Healthy);
    QCOMPARE(find(notReady, QStringLiteral("tg-iyzico")).status, tg::ModuleStatus::ConfigMissing);
    const auto other = tg::evaluateTicketGo(jsonResult(200, health), failed(QStringLiteral("yok")), jsonResult(200, ready(QStringLiteral("{\"status\":\"LIVE\",\"ready\":true,\"provider\":\"other\"}"))), {}, {}, false);
    QCOMPARE(find(other, QStringLiteral("tg-iyzico")).status, tg::ModuleStatus::Limited);

    tg::HttpResult offline;
    offline.attempted = true;
    offline.transportOk = false;
    offline.detail = QStringLiteral("timeout");
    const auto lost = tg::evaluateTicketGo(jsonResult(200, health), failed(QStringLiteral("yok")), offline, {}, {}, false);
    QVERIFY(find(lost, QStringLiteral("tg-iyzico")).status != tg::ModuleStatus::Healthy);
}

void TestOps::apiCenterUsesMeasuredStatus()
{
    tg::ModuleSnapshot api = tg::makeModule(QStringLiteral("tg-api"), QStringLiteral("ticket-go"), QStringLiteral("Backend API"), tg::ModuleStatus::Healthy, QStringLiteral("GET /health HTTP 200. Sürüm alanı: 2.0.0+abc."));
    tg::ModuleSnapshot chat = tg::makeModule(QStringLiteral("gochat-api"), QStringLiteral("gochat"), QStringLiteral("Go-Chat API"), tg::ModuleStatus::Healthy, QStringLiteral("GET /api/v1/gochat/health HTTP 200, enabled=true."));
    tg::ModuleSnapshot ferry = tg::makeModule(QStringLiteral("tg-ferry"), QStringLiteral("ticket-go"), QStringLiteral("Feribot bileti satış"), tg::ModuleStatus::NotReleased, QStringLiteral("GET /api/v1/mobile-app/config HTTP 200 · published=false. Bilet satışı ve ödeme çağrılmadı."));
    tg::ModuleSnapshot liknoss = tg::makeModule(QStringLiteral("tg-liknoss"), QStringLiteral("ticket-go"), QStringLiteral("Liknoss"), tg::ModuleStatus::Disabled, QStringLiteral("disabled"));
    const QVariantMap center = tg::buildApiCenter({api, chat, ferry, liknoss}, QStringLiteral("production"));
    QCOMPARE(center.value(QStringLiteral("groupCount")).toInt(), 3);
    QCOMPARE(center.value(QStringLiteral("activeCount")).toInt(), 2);
    QCOMPARE(center.value(QStringLiteral("partnerText")).toString(), QStringLiteral("—"));
    QCOMPARE(center.value(QStringLiteral("shareableCount")).toInt(), 6);
    QCOMPARE(center.value(QStringLiteral("internalCount")).toInt(), 9);
    const QVariantList apis = center.value(QStringLiteral("apis")).toList();
    QCOMPARE(apis.size(), 3);
    const QVariantMap ticket = apis.at(0).toMap();
    const QVariantMap gochat = apis.at(1).toMap();
    const QVariantMap sale = apis.at(2).toMap();
    QCOMPARE(ticket.value(QStringLiteral("name")).toString(), QStringLiteral("Ticket-Go Backend API"));
    QCOMPARE(ticket.value(QStringLiteral("statusLabel")).toString(), QStringLiteral("Çalışıyor"));
    QCOMPARE(ticket.value(QStringLiteral("tone")).toString(), QStringLiteral("green"));
    QCOMPARE(ticket.value(QStringLiteral("version")).toString(), QStringLiteral("2.0.0+abc"));
    QCOMPARE(ticket.value(QStringLiteral("baseUrl")).toString(), QStringLiteral("https://api.ticket-go.net"));
    QCOMPARE(gochat.value(QStringLiteral("name")).toString(), QStringLiteral("Go-Chat API"));
    QCOMPARE(gochat.value(QStringLiteral("statusLabel")).toString(), QStringLiteral("Çalışıyor"));
    QCOMPARE(gochat.value(QStringLiteral("version")).toString(), QStringLiteral("—"));
    QVERIFY(!gochat.value(QStringLiteral("summary")).toString().contains(QStringLiteral("mesaj içeriği")));
    QCOMPARE(sale.value(QStringLiteral("statusLabel")).toString(), QStringLiteral("Ölçüm yok"));
    QCOMPARE(sale.value(QStringLiteral("tone")).toString(), QStringLiteral("gray"));
    QCOMPARE(sale.value(QStringLiteral("connection")).toString(), QStringLiteral("Bağlı"));
    QCOMPARE(sale.value(QStringLiteral("configLabel")).toString(), QStringLiteral("Henüz yayınlanmadı"));
    QCOMPARE(sale.value(QStringLiteral("liknossLabel")).toString(), QStringLiteral("Devre dışı"));
    QCOMPARE(sale.value(QStringLiteral("capabilities")).toList().size(), 9);
    QVERIFY(sale.value(QStringLiteral("verifiedHealthy")).toBool() == false);
    int paths = 0;
    for (const QVariant& api : apis) {
        for (const QVariant& endpoint : api.toMap().value(QStringLiteral("endpoints")).toList()) {
            const QString blob = endpoint.toMap().value(QStringLiteral("params")).toString() + endpoint.toMap().value(QStringLiteral("summary")).toString();
            QVERIFY(!blob.contains(QStringLiteral("Bearer ")));
            QVERIFY(!blob.contains(QStringLiteral(".env")));
            ++paths;
        }
    }
    QCOMPARE(paths, 15);
    QVERIFY(tg::apiShareableText(QStringLiteral("https://api.ticket-go.net")));
    QVERIFY(tg::apiShareableText(QStringLiteral("/health")));
    QVERIFY(tg::apiShareableText(QStringLiteral("/api/v1/gochat/health")));
    QVERIFY(tg::apiShareableText(QStringLiteral("/api/v1/mobile-app/config")));
    QVERIFY(!tg::apiShareableText(QStringLiteral("/api/v1/auth/login")));
    QVERIFY(!tg::apiShareableText(QStringLiteral("/health/dependencies")));
    QVERIFY(!tg::apiShareableText(QStringLiteral("Bearer secret")));
    QVERIFY(!tg::apiShareableText(QStringLiteral("https://evil.example/health")));

    const QVariantMap empty = tg::buildApiCenter({}, QString());
    QCOMPARE(empty.value(QStringLiteral("activeCount")).toInt(), 0);
    for (const QVariant& api : empty.value(QStringLiteral("apis")).toList()) {
        QCOMPARE(api.toMap().value(QStringLiteral("statusLabel")).toString(), QStringLiteral("Ölçüm yok"));
        QCOMPARE(api.toMap().value(QStringLiteral("lastCheck")).toString(), QStringLiteral("—"));
    }
    tg::ModuleSnapshot lost = tg::makeModule(QStringLiteral("tg-api"), QStringLiteral("ticket-go"), QStringLiteral("Backend API"), tg::ModuleStatus::ConnectionError, QStringLiteral("ulaşılamadı"));
    tg::ModuleSnapshot ferryLost = tg::makeModule(QStringLiteral("tg-ferry"), QStringLiteral("ticket-go"), QStringLiteral("Feribot"), tg::ModuleStatus::ConnectionError, QStringLiteral("ulaşılamadı"));
    const QVariantMap down = tg::buildApiCenter({lost, ferryLost}, QStringLiteral("Ölçülmedi"));
    QCOMPARE(down.value(QStringLiteral("activeCount")).toInt(), 0);
    QCOMPARE(down.value(QStringLiteral("apis")).toList().at(0).toMap().value(QStringLiteral("statusLabel")).toString(), QStringLiteral("Bağlantı hatası"));
    QCOMPARE(down.value(QStringLiteral("apis")).toList().at(2).toMap().value(QStringLiteral("statusLabel")).toString(), QStringLiteral("Bağlantı hatası"));
}

QTEST_GUILESS_MAIN(TestOps)
#include "test_ops.moc"
