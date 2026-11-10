#pragma once

#include <QDateTime>
#include <QJsonArray>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QUrl>

namespace tg {

struct LogRecord {
    QString id;
    QString time;
    QString level;
    QString application;
    QString module;
    QString service;
    QString eventId;
    QString requestId;
    QString httpCode;
    QString errorCode;
    QString message;
    QString detail;
    QString stack;
    QString correlationId;
    QString endpoint;
    QString sourceId;
    QString action;
    QString date;
    QString clock;
    QString user;
    QString ip;
    QString duration;
    QString outcome;
    QString method;
};

struct LogSource {
    QString id;
    QString name;
    QString hubModule;
    QString hubType;
    bool hubBacked = false;
    QString access = QStringLiteral("unavailable");
    QString reason;
    QString lastTime;
    int total = -1;
    int infos = -1;
    int errors = -1;
    int warnings = -1;
    int criticals = -1;
    QString levels;
};

struct LogQuery {
    QString sourceId;
    QString level;
    QString range;
    QString text;
    QString httpCode;
    QString errorCode;
    QString requestId;
    QString service;
    QString logType;
    QString dateFrom;
    QString dateTo;
    QString user;
    QString ip;
    QString endpoint;
    QString outcome;
    QString action;
    int page = 1;
};

struct HubLogSection {
    QString code;
    QString label;
    QString logType;
    QString level;
};

QList<LogSource> discoveredLogSources();
QList<HubLogSection> hubLogSections();
int logPollIntervalMs(const QString& secondsText);
QUrl systemLogRequestUrl(const LogQuery& query, bool live, const QString& since);
QString normalizeLogLevel(const QString& raw);
QList<LogRecord> parseLocalLog(const QString& text);
QList<LogRecord> parseHubLogItems(const QJsonArray& items);
QList<LogRecord> filterLogRecords(const QList<LogRecord>& records, const LogQuery& query);
QString logDateFrom(const QString& range, const QDateTime& now);
QString analyzeLogRecord(const LogRecord& row, const QList<LogRecord>& page);
QString logReadFailureNote(bool tokenPresent, bool transportOk, int status, const QString& errorCode, const QString& errorClass = {});

struct AuthEnvelope {
    bool mfaRequired = false;
    QString accessToken;
    QString refreshToken;
    QString challengeToken;
    QString account;
    QString code;
    QString message;
    QString mfaMethod;
    QString maskedEmail;
};

struct SessionDecision {
    QString phase;
    QString message;
    bool storeSession = false;
};

AuthEnvelope readAuthEnvelope(int status, const QJsonObject& json);
SessionDecision decideAuthResult(bool transportOk, int status, const AuthEnvelope& envelope);
bool shouldRefreshLogSession(int status, bool hasRefresh, bool alreadyTried);
bool adoptPreviousLogSession();
QString logSessionPhaseAfterResult(const QString& phase, int status, bool transportOk, bool refreshWillRun);
QString emptyLogResultNote();
void applySourceCounts(QList<LogSource>& sources, const QList<LogRecord>& records, bool hubConnected, const QString& unavailableReason = QString());
QList<LogRecord> mergeLogRecords(const QList<LogRecord>& previous, const QList<LogRecord>& incoming, int cap, int* freshOut);
QList<LogRecord> omitHiddenLogs(const QList<LogRecord>& rows, const QStringList& hiddenIds);

} // namespace tg
