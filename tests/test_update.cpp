#include "InstallPlan.h"
#include "PackageVerifier.h"
#include "ReleaseParser.h"
#include "SemVer.h"
#include "UpdateManager.h"
#include "UpdateTypes.h"
#include "ZipSafety.h"

#include <QCryptographicHash>
#include <QDir>
#include <QDirIterator>
#include <QFile>
#include <QHostAddress>
#include <QJsonDocument>
#include <QJsonObject>
#include <QNetworkReply>
#include <QProcess>
#include <QSignalSpy>
#include <QTcpServer>
#include <QTcpSocket>
#include <QTemporaryDir>
#include <QVector>
#include <QtTest>

namespace {

QByteArray sha256(const QByteArray& bytes)
{
    return QCryptographicHash::hash(bytes, QCryptographicHash::Sha256).toHex();
}

void appendU16(QByteArray& out, quint16 value)
{
    out.append(char(value & 0xff));
    out.append(char((value >> 8) & 0xff));
}

void appendU32(QByteArray& out, quint32 value)
{
    out.append(char(value & 0xff));
    out.append(char((value >> 8) & 0xff));
    out.append(char((value >> 16) & 0xff));
    out.append(char((value >> 24) & 0xff));
}

quint32 crc32Of(const QByteArray& data)
{
    static quint32 table[256];
    static bool ready = false;
    if (!ready) {
        for (quint32 index = 0; index < 256; ++index) {
            quint32 value = index;
            for (int bit = 0; bit < 8; ++bit)
                value = (value & 1u) ? (0xedb88320u ^ (value >> 1)) : (value >> 1);
            table[index] = value;
        }
        ready = true;
    }
    quint32 crc = 0xffffffffu;
    for (unsigned char byte : data)
        crc = table[(crc ^ byte) & 0xffu] ^ (crc >> 8);
    return crc ^ 0xffffffffu;
}

QByteArray makeStoredZip(const QVector<QPair<QString, QByteArray>>& entries)
{
    QByteArray locals;
    QByteArray centrals;
    for (const auto& entry : entries) {
        const QByteArray name = entry.first.toUtf8();
        const QByteArray& payload = entry.second;
        const quint32 crc = crc32Of(payload);
        const quint32 offset = static_cast<quint32>(locals.size());
        locals.append("PK\x03\x04");
        appendU16(locals, 20);
        appendU16(locals, 0x800);
        appendU16(locals, 0);
        appendU16(locals, 0);
        appendU16(locals, 0);
        appendU32(locals, crc);
        appendU32(locals, static_cast<quint32>(payload.size()));
        appendU32(locals, static_cast<quint32>(payload.size()));
        appendU16(locals, static_cast<quint16>(name.size()));
        appendU16(locals, 0);
        locals.append(name);
        locals.append(payload);

        centrals.append("PK\x01\x02");
        appendU16(centrals, 20);
        appendU16(centrals, 20);
        appendU16(centrals, 0x800);
        appendU16(centrals, 0);
        appendU16(centrals, 0);
        appendU16(centrals, 0);
        appendU32(centrals, crc);
        appendU32(centrals, static_cast<quint32>(payload.size()));
        appendU32(centrals, static_cast<quint32>(payload.size()));
        appendU16(centrals, static_cast<quint16>(name.size()));
        appendU16(centrals, 0);
        appendU16(centrals, 0);
        appendU16(centrals, 0);
        appendU16(centrals, 0);
        appendU32(centrals, 0);
        appendU32(centrals, offset);
        centrals.append(name);
    }
    const quint32 centralOffset = static_cast<quint32>(locals.size());
    QByteArray zip = locals + centrals;
    zip.append("PK\x05\x06");
    appendU16(zip, 0);
    appendU16(zip, 0);
    appendU16(zip, static_cast<quint16>(entries.size()));
    appendU16(zip, static_cast<quint16>(entries.size()));
    appendU32(zip, static_cast<quint32>(centrals.size()));
    appendU32(zip, centralOffset);
    appendU16(zip, 0);
    return zip;
}

bool writeStoredZip(const QString& zipPath, const QVector<QPair<QString, QByteArray>>& entries)
{
    QFile file(zipPath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Truncate)) return false;
    return file.write(makeStoredZip(entries)) >= 0;
}

QString plist(const QString& bundleId, const QString& version)
{
    return QStringLiteral("<?xml version=\"1.0\" encoding=\"UTF-8\"?><plist version=\"1.0\"><dict><key>CFBundleIdentifier</key><string>%1</string><key>CFBundleShortVersionString</key><string>%2</string></dict></plist>").arg(bundleId, version);
}

bool makeApp(const QString& root, const QString& bundleId, const QString& version, const QString& marker)
{
    const QString app = root + QStringLiteral("/Ticket Go Izleme.app/Contents");
    QDir().mkpath(app + QStringLiteral("/MacOS"));
    QFile info(app + QStringLiteral("/Info.plist"));
    if (!info.open(QIODevice::WriteOnly)) return false;
    info.write(plist(bundleId, version).toUtf8());
    info.close();
    QFile bin(app + QStringLiteral("/MacOS/TicketGoIzleme"));
    if (!bin.open(QIODevice::WriteOnly)) return false;
    bin.write(marker.toUtf8());
    return true;
}

QByteArray zipApp(const QString& root)
{
    const QString appRoot = root + QStringLiteral("/app");
    QVector<QPair<QString, QByteArray>> entries;
    QDirIterator iterator(appRoot, QDir::Files, QDirIterator::Subdirectories);
    while (iterator.hasNext()) {
        iterator.next();
        QFile file(iterator.filePath());
        if (!file.open(QIODevice::ReadOnly)) return {};
        const QString relative = QDir(appRoot).relativeFilePath(iterator.filePath()).replace(QLatin1Char('\\'), QLatin1Char('/'));
        entries.append({relative, file.readAll()});
    }
    if (entries.isEmpty()) return {};
    return makeStoredZip(entries);
}

class LocalHub : public QObject {
    Q_OBJECT
public:
    QHash<QString, QByteArray> files;
    QHash<QString, int> codes;
    QString stall;
    bool sawAgent = false;
    bool listen() { return server.listen(QHostAddress::LocalHost, 0); }
    quint16 port() const { return server.serverPort(); }
    QUrl url(const QString& path) const { return QUrl(QStringLiteral("http://127.0.0.1:%1%2").arg(port()).arg(path)); }
    void arm()
    {
        connect(&server, &QTcpServer::newConnection, this, [this]() {
            while (server.hasPendingConnections()) acceptOne(server.nextPendingConnection());
        });
    }
private:
    void acceptOne(QTcpSocket* socket)
    {
        connect(socket, &QTcpSocket::readyRead, this, [this, socket]() {
            QByteArray buffer = socket->property("buf").toByteArray() + socket->readAll();
            socket->setProperty("buf", buffer);
            const int split = buffer.indexOf("\r\n\r\n");
            if (split < 0) return;
            const QString head = QString::fromUtf8(buffer.left(split));
            if (head.contains(QStringLiteral("User-Agent: TicketGoIzleme/"))) sawAgent = true;
            const QString path = head.section(QLatin1Char(' '), 1, 1);
            if (path == stall) return;
            const int code = codes.value(path, 200);
            const QByteArray payload = files.value(path);
            QByteArray response = "HTTP/1.1 " + QByteArray::number(code) + " X\r\nContent-Length: " + QByteArray::number(payload.size()) + "\r\nConnection: close\r\n";
            if (code == 403) response += "X-RateLimit-Remaining: 0\r\n";
            response += "\r\n" + payload;
            socket->write(response);
            socket->disconnectFromHost();
        });
    }
    QTcpServer server;
};

} // namespace

class TestUpdate : public QObject {
    Q_OBJECT
private slots:
    void semverUnderstandsMinor();
    void buttonTextMatchesPhase();
    void releaseUrlStaysOnRepository();
    void notesDoNotKeepScript();
    void noRelease();
    void apiUnreachable();
    void rateLimitIsNotNoRelease();
    void upToDate();
    void newerRelease();
    void olderReleaseIsNotOffered();
    void invalidSemver();
    void wrongArchitecture();
    void missingAsset();
    void sourceArchiveIsRejected();
    void downloadFails();
    void downloadCancelRemovesPartial();
    void hashMismatch();
    void invalidSignatureBlocksInstall();
    void bundleMismatch();
    void realCodesignRejectsUnsigned();
    void missingInstallAndDeveloperBuild();
    void permissionAndRollbackAndRestart();
    void userDataStaysUntouched();
    void pathTraversalIsRejected();
    void windowsPackagesAreNotOfferedToMac();
    void windowsClientSelectsWindowsSetup();
    void windowsHashMismatchBlocksInstall();
    void windowsDownloadKeepsExeAndReachesReady();
    void windowsHelperLaunchesSetup();
private:
    bool prepare(tg::UpdateManager& manager, LocalHub& hub, QTemporaryDir& dir);
    QByteArray releaseJson(const LocalHub& hub, const QString& tag, const QString& zipName, bool includeManifest) const;
};

bool TestUpdate::prepare(tg::UpdateManager& manager, LocalHub& hub, QTemporaryDir& dir)
{
    if (!dir.isValid() || !hub.listen()) return false;
    hub.arm();
    QCoreApplication::setApplicationVersion(QStringLiteral("0.1.0"));
    manager.setDataDirectory(dir.filePath(QStringLiteral("data")));
    manager.setReleaseUrl(hub.url(QStringLiteral("/releases/latest")));
    manager.setArchForTesting(QStringLiteral("arm64"));
    manager.setExecutablePath(dir.filePath(QStringLiteral("not-installed")));
    return true;
}

QByteArray TestUpdate::releaseJson(const LocalHub& hub, const QString& tag, const QString& zipName, bool includeManifest) const
{
    QString assets = QStringLiteral("{\"name\":\"%1\",\"browser_download_url\":\"%2\",\"size\":1,\"content_type\":\"application/zip\"}").arg(zipName, hub.url(QStringLiteral("/pkg.zip")).toString());
    if (includeManifest) {
        assets += QStringLiteral(",{\"name\":\"TicketGoIzleme-update-manifest.json\",\"browser_download_url\":\"%1\",\"size\":1,\"content_type\":\"application/json\"}").arg(hub.url(QStringLiteral("/manifest.json")).toString());
    }
    return QStringLiteral("{\"tag_name\":\"%1\",\"name\":\"%1\",\"published_at\":\"2026-11-01T09:00:00Z\",\"draft\":false,\"prerelease\":false,\"body\":\"<script>alert(1)</script><b>Not</b>\",\"assets\":[%2]}").arg(tag, assets).toUtf8();
}

void TestUpdate::semverUnderstandsMinor()
{
    QCOMPARE(tg::compareVersions(QStringLiteral("v1.9.0"), QStringLiteral("v1.10.0")), tg::VersionOrder::Newer);
    QCOMPARE(tg::compareVersions(QStringLiteral("1.10.0"), QStringLiteral("v1.9.0")), tg::VersionOrder::Older);
    QCOMPARE(tg::compareVersions(QStringLiteral("v2.0.0"), QStringLiteral("v2.0.0")), tg::VersionOrder::Same);
    QCOMPARE(tg::compareVersions(QStringLiteral("v1"), QStringLiteral("v1.0.0")), tg::VersionOrder::Invalid);
}

void TestUpdate::buttonTextMatchesPhase()
{
    QCOMPARE(tg::phaseButtonText(QStringLiteral("IDLE")), QStringLiteral("Güncellemeleri Kontrol Et"));
    QCOMPARE(tg::phaseButtonText(QStringLiteral("CHECKING")), QStringLiteral("Güncellemeler Denetleniyor..."));
    QCOMPARE(tg::phaseButtonText(QStringLiteral("UP_TO_DATE")), QStringLiteral("Uygulamanız Güncel"));
    QCOMPARE(tg::phaseButtonText(QStringLiteral("UPDATE_AVAILABLE")), QStringLiteral("Güncellemeyi İndir"));
    QCOMPARE(tg::phaseButtonText(QStringLiteral("NO_RELEASE")), QStringLiteral("Henüz Güncelleme Yayınlanmadı"));
    QCOMPARE(tg::phaseButtonText(QStringLiteral("ERROR")), QStringLiteral("Tekrar Dene"));
    QCOMPARE(tg::phaseButtonText(QStringLiteral("VERIFYING")), QStringLiteral("Doğrulanıyor..."));
    QCOMPARE(tg::phaseButtonText(QStringLiteral("READY_TO_INSTALL")), QStringLiteral("Güncellemeyi Yükle"));
    QCOMPARE(tg::phaseIconMode(QStringLiteral("IDLE")), QStringLiteral("refresh"));
    QCOMPARE(tg::phaseIconMode(QStringLiteral("CHECKING")), QStringLiteral("refresh"));
    QVERIFY(tg::phaseIconSpins(QStringLiteral("CHECKING")));
    QVERIFY(!tg::phaseIconSpins(QStringLiteral("UPDATE_AVAILABLE")));
    QCOMPARE(tg::phaseIconMode(QStringLiteral("UPDATE_AVAILABLE")), QStringLiteral("download"));
    QCOMPARE(tg::phaseIconMode(QStringLiteral("DOWNLOADING")), QStringLiteral("download"));
    QCOMPARE(tg::phaseTooltip(QStringLiteral("IDLE")), QStringLiteral("Güncellemeleri Kontrol Et"));
    QCOMPARE(tg::phaseTooltip(QStringLiteral("ERROR")), QStringLiteral("Güncelleme Denetlenemedi"));
    QCOMPARE(tg::phaseTooltip(QStringLiteral("READY_TO_INSTALL")), QStringLiteral("Kuruluma Hazır"));
    QCOMPARE(tg::phaseTooltip(QStringLiteral("UPDATE_AVAILABLE")), QStringLiteral("Yeni Güncelleme Mevcut"));
}

void TestUpdate::releaseUrlStaysOnRepository()
{
    QCOMPARE(tg::officialReleaseApi().toString(), QStringLiteral("https://api.github.com/repos/ticketgoteknoloji-web/ticketgoteknoloji-izleme-monit-r-/releases/latest"));
    QCOMPARE(tg::officialReleasePage().toString(), QStringLiteral("https://github.com/ticketgoteknoloji-web/ticketgoteknoloji-izleme-monit-r-/releases"));
    tg::UpdateManager pinned;
    QCOMPARE(pinned.configuredReleaseApi(), tg::officialReleaseApi());
    pinned.setReleaseUrl(QUrl(QStringLiteral("https://api.github.com/repos/other/repo/releases/latest")));
    QCOMPARE(pinned.configuredReleaseApi(), tg::officialReleaseApi());
    pinned.setReleaseUrl(QUrl(QStringLiteral("https://example.com/update")));
    QCOMPARE(pinned.configuredReleaseApi(), tg::officialReleaseApi());
    QVERIFY(tg::releaseUrlTrusted(QUrl(QStringLiteral("https://api.github.com/repos/ticketgoteknoloji-web/ticketgoteknoloji-izleme-monit-r-/releases/latest"))));
    QVERIFY(!tg::releaseUrlTrusted(QUrl(QStringLiteral("https://api.github.com/repos/other/repo/releases/latest"))));
    QVERIFY(!tg::releaseUrlTrusted(QUrl(QStringLiteral("http://api.github.com/repos/ticketgoteknoloji-web/ticketgoteknoloji-izleme-monit-r-/releases/latest"))));
    QVERIFY(tg::downloadHostTrusted(QUrl(QStringLiteral("https://objects.githubusercontent.com/a"))));
    QVERIFY(!tg::downloadHostTrusted(QUrl(QStringLiteral("https://evil.example/a"))));
}

void TestUpdate::notesDoNotKeepScript()
{
    const QString notes = tg::plainReleaseNotes(QStringLiteral("<script>alert(1)</script><b>Düzeltme</b>"));
    QVERIFY(!notes.contains(QStringLiteral("<script")));
    QVERIFY(notes.contains(QStringLiteral("Düzeltme")));
}

void TestUpdate::noRelease()
{
    QTemporaryDir dir;
    LocalHub hub;
    tg::UpdateManager manager;
    QVERIFY(prepare(manager, hub, dir));
    hub.codes.insert(QStringLiteral("/releases/latest"), 404);
    hub.files.insert(QStringLiteral("/releases/latest"), QByteArray("{\"message\":\"Not Found\"}"));
    manager.checkForUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("NO_RELEASE"), 4000);
    QCOMPARE(manager.buttonText(), QStringLiteral("Henüz Güncelleme Yayınlanmadı"));
    QCOMPARE(manager.iconMode(), QStringLiteral("refresh"));
    QVERIFY(hub.sawAgent);
}

void TestUpdate::apiUnreachable()
{
    QTemporaryDir dir;
    tg::UpdateManager manager;
    manager.setDataDirectory(dir.filePath(QStringLiteral("data")));
    manager.setReleaseUrl(QUrl(QStringLiteral("http://127.0.0.1:1/releases/latest")));
    manager.checkForUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("ERROR"), 4000);
    QCOMPARE(manager.errorMessage(), tg::msgApiFailure());
    QVERIFY(manager.phase() != QLatin1String("NO_RELEASE"));
}

void TestUpdate::rateLimitIsNotNoRelease()
{
    QTemporaryDir dir;
    LocalHub hub;
    tg::UpdateManager manager;
    QVERIFY(prepare(manager, hub, dir));
    hub.codes.insert(QStringLiteral("/releases/latest"), 403);
    hub.files.insert(QStringLiteral("/releases/latest"), QByteArray("{\"message\":\"API rate limit exceeded\"}"));
    manager.checkForUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("ERROR"), 4000);
    QCOMPARE(manager.errorMessage(), tg::msgRateLimit());
}

void TestUpdate::upToDate()
{
    QTemporaryDir dir;
    LocalHub hub;
    tg::UpdateManager manager;
    QVERIFY(prepare(manager, hub, dir));
    hub.files.insert(QStringLiteral("/releases/latest"), releaseJson(hub, QStringLiteral("v0.1.0"), QStringLiteral("TicketGoIzleme-macOS-arm64.zip"), true));
    manager.checkForUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("UP_TO_DATE"), 4000);
    QCOMPARE(manager.buttonText(), QStringLiteral("Uygulamanız Güncel"));
}

void TestUpdate::newerRelease()
{
    QTemporaryDir dir;
    LocalHub hub;
    tg::UpdateManager manager;
    QVERIFY(prepare(manager, hub, dir));
    hub.files.insert(QStringLiteral("/releases/latest"), releaseJson(hub, QStringLiteral("v1.2.0"), QStringLiteral("TicketGoIzleme-macOS-arm64.zip"), true));
    hub.files.insert(QStringLiteral("/manifest.json"), QByteArray("{\"version\":\"1.2.0\",\"channel\":\"stable\",\"assets\":[{\"name\":\"TicketGoIzleme-macOS-arm64.zip\",\"arch\":\"arm64\",\"sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"bundleId\":\"tr.ticketgoteknoloji.izleme\",\"size\":4}]}"));
    manager.checkForUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("UPDATE_AVAILABLE"), 4000);
    QCOMPARE(manager.buttonText(), QStringLiteral("Güncellemeyi İndir"));
    QCOMPARE(manager.iconMode(), QStringLiteral("download"));
    QCOMPARE(manager.tooltipText(), QStringLiteral("Yeni Güncelleme Mevcut"));
    QCOMPARE(manager.latestVersion(), QStringLiteral("1.2.0"));
    QVERIFY(!manager.dialogOpen());
    manager.activateButton();
    QVERIFY(manager.dialogOpen());
    QVERIFY(!manager.releaseNotes().contains(QStringLiteral("<script")));
    QVERIFY(manager.releaseNotes().contains(QStringLiteral("Not")));
}

void TestUpdate::olderReleaseIsNotOffered()
{
    QTemporaryDir dir;
    LocalHub hub;
    tg::UpdateManager manager;
    QVERIFY(prepare(manager, hub, dir));
    hub.files.insert(QStringLiteral("/releases/latest"), releaseJson(hub, QStringLiteral("v0.0.9"), QStringLiteral("TicketGoIzleme-macOS-arm64.zip"), true));
    manager.checkForUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("UP_TO_DATE"), 4000);
    QVERIFY(manager.phase() != QLatin1String("UPDATE_AVAILABLE"));
}

void TestUpdate::invalidSemver()
{
    QTemporaryDir dir;
    LocalHub hub;
    tg::UpdateManager manager;
    QVERIFY(prepare(manager, hub, dir));
    hub.files.insert(QStringLiteral("/releases/latest"), releaseJson(hub, QStringLiteral("v1.2"), QStringLiteral("TicketGoIzleme-macOS-arm64.zip"), false));
    manager.checkForUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("ERROR"), 4000);
    QCOMPARE(manager.errorMessage(), tg::msgBadVersion());
}

void TestUpdate::wrongArchitecture()
{
    QTemporaryDir dir;
    LocalHub hub;
    tg::UpdateManager manager;
    QVERIFY(prepare(manager, hub, dir));
    hub.files.insert(QStringLiteral("/releases/latest"), releaseJson(hub, QStringLiteral("v9.0.0"), QStringLiteral("TicketGoIzleme-macOS-x64.zip"), true));
    manager.checkForUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("ERROR"), 4000);
    QCOMPARE(manager.errorMessage(), tg::msgWrongArch());
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("data/updates/staging/package.zip"))));
}

void TestUpdate::missingAsset()
{
    QTemporaryDir dir;
    LocalHub hub;
    tg::UpdateManager manager;
    QVERIFY(prepare(manager, hub, dir));
    hub.files.insert(QStringLiteral("/releases/latest"), QByteArray("{\"tag_name\":\"v3.0.0\",\"draft\":false,\"prerelease\":false,\"body\":\"\",\"assets\":[]}"));
    manager.checkForUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("ERROR"), 4000);
    QCOMPARE(manager.errorMessage(), tg::msgMissingAsset());
}

void TestUpdate::sourceArchiveIsRejected()
{
    const QByteArray body = QByteArray("{\"tag_name\":\"v3.1.0\",\"draft\":false,\"prerelease\":false,\"assets\":[{\"name\":\"Source code (zip)\",\"browser_download_url\":\"https://github.com/ticketgoteknoloji-web/ticketgoteknoloji-izleme-monit-r-/archive/refs/tags/v3.1.0.zip\",\"size\":10}]}");
    const tg::ParsedRelease parsed = tg::parseLatestRelease(200, body, {});
    QCOMPARE(parsed.kind, tg::ReleaseKind::Ok);
    QCOMPARE(parsed.release.assets.size(), 0);
}

void TestUpdate::downloadFails()
{
    QTemporaryDir dir;
    LocalHub hub;
    tg::UpdateManager manager;
    QVERIFY(prepare(manager, hub, dir));
    hub.files.insert(QStringLiteral("/releases/latest"), releaseJson(hub, QStringLiteral("v1.4.0"), QStringLiteral("TicketGoIzleme-macOS-arm64.zip"), true));
    hub.files.insert(QStringLiteral("/manifest.json"), QByteArray("{\"version\":\"1.4.0\",\"channel\":\"stable\",\"assets\":[{\"name\":\"TicketGoIzleme-macOS-arm64.zip\",\"arch\":\"arm64\",\"sha256\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\",\"bundleId\":\"tr.ticketgoteknoloji.izleme\",\"size\":4}]}"));
    hub.codes.insert(QStringLiteral("/pkg.zip"), 500);
    manager.checkForUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("UPDATE_AVAILABLE"), 4000);
    manager.confirmDownload();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("ERROR"), 4000);
    QCOMPARE(manager.errorMessage(), tg::msgDownloadFailed());
}

void TestUpdate::downloadCancelRemovesPartial()
{
    QTemporaryDir dir;
    LocalHub hub;
    tg::UpdateManager manager;
    QVERIFY(prepare(manager, hub, dir));
    hub.files.insert(QStringLiteral("/releases/latest"), releaseJson(hub, QStringLiteral("v1.5.0"), QStringLiteral("TicketGoIzleme-macOS-arm64.zip"), true));
    hub.files.insert(QStringLiteral("/manifest.json"), QByteArray("{\"version\":\"1.5.0\",\"channel\":\"stable\",\"assets\":[{\"name\":\"TicketGoIzleme-macOS-arm64.zip\",\"arch\":\"arm64\",\"sha256\":\"cccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccccc\",\"bundleId\":\"tr.ticketgoteknoloji.izleme\",\"size\":4}]}"));
    hub.stall = QStringLiteral("/pkg.zip");
    manager.checkForUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("UPDATE_AVAILABLE"), 4000);
    manager.confirmDownload();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("DOWNLOADING"), 4000);
    manager.cancelDownload();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("UPDATE_AVAILABLE"), 4000);
    QCOMPARE(manager.errorMessage(), tg::msgCancelled());
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("data/updates/staging/pkg.zip.partial"))));
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("data/updates/staging/pkg.zip"))));
}

void TestUpdate::hashMismatch()
{
    QTemporaryDir dir;
    QVERIFY(makeApp(dir.filePath(QStringLiteral("app")), QStringLiteral("tr.ticketgoteknoloji.izleme"), QStringLiteral("1.6.0"), QStringLiteral("new")));
    const QByteArray zip = zipApp(dir.path());
    QVERIFY(!zip.isEmpty());
    LocalHub hub;
    tg::UpdateManager manager;
    QVERIFY(prepare(manager, hub, dir));
    hub.files.insert(QStringLiteral("/releases/latest"), releaseJson(hub, QStringLiteral("v1.6.0"), QStringLiteral("TicketGoIzleme-macOS-arm64.zip"), true));
    hub.files.insert(QStringLiteral("/manifest.json"), QByteArray("{\"version\":\"1.6.0\",\"channel\":\"stable\",\"assets\":[{\"name\":\"TicketGoIzleme-macOS-arm64.zip\",\"arch\":\"arm64\",\"sha256\":\"dddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddddd\",\"bundleId\":\"tr.ticketgoteknoloji.izleme\",\"size\":") + QByteArray::number(zip.size()) + "}]}");
    hub.files.insert(QStringLiteral("/pkg.zip"), zip);
    manager.checkForUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("UPDATE_AVAILABLE"), 4000);
    manager.confirmDownload();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("ERROR"), 4000);
    QCOMPARE(manager.errorMessage(), tg::msgHashMismatch());
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("data/updates/staging/pkg.zip"))));
}

void TestUpdate::invalidSignatureBlocksInstall()
{
    QTemporaryDir dir;
    QVERIFY(makeApp(dir.filePath(QStringLiteral("stage")), QStringLiteral("tr.ticketgoteknoloji.izleme"), QStringLiteral("1.7.0"), QStringLiteral("x")));
    const QString app = dir.filePath(QStringLiteral("stage/Ticket Go Izleme.app"));
    const tg::VerifyResult result = tg::verifyExtractedApp(app, QStringLiteral("aa"), QStringLiteral("aa"), QStringLiteral("tr.ticketgoteknoloji.izleme"), QStringLiteral("1.7.0"), QStringLiteral("Developer ID Application: Example"), [](const QString&) {
        tg::SignatureInfo info;
        info.detail = QStringLiteral("unsigned");
        return info;
    });
    QVERIFY(!result.autoInstallAllowed);
    QCOMPARE(result.message, tg::msgBadSignature());
}

void TestUpdate::bundleMismatch()
{
    QTemporaryDir dir;
    QVERIFY(makeApp(dir.path(), QStringLiteral("com.example.other"), QStringLiteral("1.8.0"), QStringLiteral("x")));
    const tg::VerifyResult result = tg::verifyExtractedApp(dir.filePath(QStringLiteral("Ticket Go Izleme.app")), QStringLiteral("bb"), QStringLiteral("bb"), QStringLiteral("tr.ticketgoteknoloji.izleme"), QStringLiteral("1.8.0"), QStringLiteral("Developer ID Application: Example"), [](const QString&) {
        tg::SignatureInfo info;
        info.signedOk = true;
        info.notarized = true;
        info.identifier = QStringLiteral("com.example.other");
        info.authority = QStringLiteral("Developer ID Application: Example");
        return info;
    });
    QVERIFY(!result.autoInstallAllowed);
    QCOMPARE(result.message, tg::msgBundleMismatch());
}

void TestUpdate::realCodesignRejectsUnsigned()
{
#if defined(Q_OS_MACOS)
    QTemporaryDir dir;
    QVERIFY(makeApp(dir.path(), QStringLiteral("tr.ticketgoteknoloji.izleme"), QStringLiteral("0.1.0"), QStringLiteral("x")));
    const tg::SignatureInfo info = tg::probeMacSignature(dir.filePath(QStringLiteral("Ticket Go Izleme.app")));
    QVERIFY(!info.signedOk);
    QVERIFY(!info.notarized);
#else
    // codesign/spctl are macOS-only; unsigned probe must not claim a valid signature.
    const tg::SignatureInfo info = tg::probeMacSignature(QStringLiteral("C:/missing/Ticket Go Izleme.app"));
    QVERIFY(!info.signedOk);
    QVERIFY(!info.notarized);
#endif
}

void TestUpdate::missingInstallAndDeveloperBuild()
{
    const tg::InstallLocation loose = tg::inspectLocation(QStringLiteral("/tmp/TicketGoIzleme"));
    QVERIFY(!loose.installed);
    QCOMPARE(loose.message, tg::msgNotInstalled());
    const tg::InstallLocation installed = tg::inspectLocation(QStringLiteral("/Applications/Ticket Go Izleme.app/Contents/MacOS/TicketGoIzleme"));
    QVERIFY(installed.installed);
    const tg::InstallLocation developer = tg::inspectLocation(QStringLiteral("/Users/ticket-go/Desktop/ticketgoteknoloji sistem analiz /ticket-go-izleme-qt/build-release/TicketGoIzleme.app/Contents/MacOS/TicketGoIzleme"));
    QVERIFY(developer.developerBuild);
    QVERIFY(!developer.installed);
    QTemporaryDir dir;
    QVERIFY(makeApp(dir.filePath(QStringLiteral("src")), QStringLiteral("tr.ticketgoteknoloji.izleme"), QStringLiteral("2.0.0"), QStringLiteral("new")));
    tg::ApplyRequest request;
    request.sourceApp = dir.filePath(QStringLiteral("src/Ticket Go Izleme.app"));
    request.targetApp = QStringLiteral("/Users/ticket-go/Desktop/ticketgoteknoloji sistem analiz /ticket-go-izleme-qt/build/DoNotTouch.app");
    request.backupApp = dir.filePath(QStringLiteral("backup.app"));
    request.writable = true;
    const tg::ApplyResult refused = tg::applyUpdate(request, [](const QString&) { return true; });
    QVERIFY(!refused.ok);
#if defined(Q_OS_MACOS)
    QCOMPARE(refused.message, tg::msgDeveloperBuild());
#else
    QCOMPARE(refused.message, QStringLiteral("Bu kurulum yolu yalnızca macOS uygulama paketleri içindir."));
#endif
}

void TestUpdate::permissionAndRollbackAndRestart()
{
#if defined(Q_OS_MACOS)
    QTemporaryDir dir;
    const QString data = dir.filePath(QStringLiteral("data"));
    QDir().mkpath(data);
    QFile keeper(data + QStringLiteral("/monitor.sqlite"));
    QVERIFY(keeper.open(QIODevice::WriteOnly));
    keeper.write("user-db");
    keeper.close();
    QVERIFY(makeApp(dir.filePath(QStringLiteral("old")), QStringLiteral("tr.ticketgoteknoloji.izleme"), QStringLiteral("1.0.0"), QStringLiteral("old")));
    QVERIFY(makeApp(dir.filePath(QStringLiteral("fresh")), QStringLiteral("tr.ticketgoteknoloji.izleme"), QStringLiteral("2.0.0"), QStringLiteral("new")));
    const QString target = dir.filePath(QStringLiteral("Applications/Ticket Go Izleme.app"));
    QDir().mkpath(dir.filePath(QStringLiteral("Applications")));
    QVERIFY(QDir().rename(dir.filePath(QStringLiteral("old/Ticket Go Izleme.app")), target));
    tg::ApplyRequest denied;
    denied.sourceApp = dir.filePath(QStringLiteral("fresh/Ticket Go Izleme.app"));
    denied.targetApp = target;
    denied.backupApp = dir.filePath(QStringLiteral("backup/Ticket Go Izleme.app"));
    denied.dataDirectory = data;
    denied.writable = false;
    const tg::ApplyResult blocked = tg::applyUpdate(denied, [](const QString&) { return true; });
    QVERIFY(!blocked.ok);
    QCOMPARE(blocked.message, tg::msgNoPermission());
    QFile still(target + QStringLiteral("/Contents/MacOS/TicketGoIzleme"));
    QVERIFY(still.open(QIODevice::ReadOnly));
    QCOMPARE(still.readAll(), QByteArray("old"));
    still.close();

    tg::ApplyRequest rollback = denied;
    rollback.writable = true;
    const tg::ApplyResult rolled = tg::applyUpdate(rollback, [](const QString&) { return false; });
    QVERIFY(!rolled.ok);
    QVERIFY(rolled.rolledBack);
    QFile restored(target + QStringLiteral("/Contents/MacOS/TicketGoIzleme"));
    QVERIFY(restored.open(QIODevice::ReadOnly));
    QCOMPARE(restored.readAll(), QByteArray("old"));
    restored.close();

    tg::HelperPlan plan;
    plan.parentPid = 0;
    plan.sourceApp = denied.sourceApp;
    plan.targetApp = target;
    plan.backupApp = denied.backupApp;
    plan.dataDirectory = data;
    plan.bundleId = QStringLiteral("tr.ticketgoteknoloji.izleme");
    plan.version = QStringLiteral("2.0.0");
    plan.authority = QStringLiteral("Developer ID Application: Example");
    const QString planPath = dir.filePath(QStringLiteral("plan.json"));
    QString error;
    QVERIFY(tg::writeHelperPlan(planPath, plan, &error));
    QString launched;
    const int code = tg::runHelper(planPath, [](const QString&) {
        tg::SignatureInfo info;
        info.signedOk = true;
        info.notarized = true;
        info.identifier = QStringLiteral("tr.ticketgoteknoloji.izleme");
        info.authority = QStringLiteral("Developer ID Application: Example");
        return info;
    }, [&](const QString& appPath) {
        launched = appPath;
        return true;
    }, false);
    QCOMPARE(code, 0);
    QCOMPARE(launched, target);
    QFile updated(target + QStringLiteral("/Contents/MacOS/TicketGoIzleme"));
    QVERIFY(updated.open(QIODevice::ReadOnly));
    QCOMPARE(updated.readAll(), QByteArray("new"));
    QFile dataFile(data + QStringLiteral("/monitor.sqlite"));
    QVERIFY(dataFile.open(QIODevice::ReadOnly));
    QCOMPARE(dataFile.readAll(), QByteArray("user-db"));
    QVERIFY(QFile::exists(denied.backupApp + QStringLiteral("/Contents/Info.plist")));
#else
    // Windows installs via verified Setup.exe helper; macOS .app replacement is unavailable here.
    tg::ApplyRequest request;
    request.sourceApp = QStringLiteral("C:/Temp/Ticket Go Izleme.app");
    request.targetApp = QStringLiteral("C:/Program Files/TicketGo/Ticket Go Izleme.app");
    request.backupApp = QStringLiteral("C:/Temp/backup.app");
    request.writable = true;
    const tg::ApplyResult refused = tg::applyUpdate(request, [](const QString&) { return true; });
    QVERIFY(!refused.ok);
    QCOMPARE(refused.message, QStringLiteral("Bu kurulum yolu yalnızca macOS uygulama paketleri içindir."));
    QVERIFY(!refused.rolledBack);
#endif
}

void TestUpdate::pathTraversalIsRejected()
{
    QTemporaryDir dir;
    const QString zipPath = dir.filePath(QStringLiteral("bad.zip"));
    QVERIFY(writeStoredZip(zipPath, {
        {QStringLiteral("../escape.txt"), QByteArray("no")},
        {QStringLiteral("Ticket Go Izleme.app/Contents/Info.plist"), QByteArray("<plist></plist>")},
    }));
    const tg::ZipInspection inspection = tg::inspectZip(zipPath);
    QVERIFY(!inspection.ok);
    QString error;
    QVERIFY(!tg::extractZip(zipPath, dir.filePath(QStringLiteral("out")), &error));
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("escape.txt"))));
}

void TestUpdate::userDataStaysUntouched()
{
#if defined(Q_OS_MACOS)
    QTemporaryDir dir;
    const QString data = dir.filePath(QStringLiteral("Library/Application Support/TicketGoTeknoloji/SistemIzleme"));
    QDir().mkpath(data);
    QFile db(data + QStringLiteral("/monitor.sqlite"));
    QVERIFY(db.open(QIODevice::WriteOnly));
    db.write("sqlite-bytes");
    db.close();
    QFile key(data + QStringLiteral("/keychain-not-used.txt"));
    QVERIFY(key.open(QIODevice::WriteOnly));
    key.write("do-not-delete");
    key.close();
    QVERIFY(makeApp(dir.filePath(QStringLiteral("old")), QStringLiteral("tr.ticketgoteknoloji.izleme"), QStringLiteral("1.0.0"), QStringLiteral("old")));
    QVERIFY(makeApp(dir.filePath(QStringLiteral("fresh")), QStringLiteral("tr.ticketgoteknoloji.izleme"), QStringLiteral("2.1.0"), QStringLiteral("new")));
    const QString target = dir.filePath(QStringLiteral("Applications/Ticket Go Izleme.app"));
    QDir().mkpath(dir.filePath(QStringLiteral("Applications")));
    QVERIFY(QDir().rename(dir.filePath(QStringLiteral("old/Ticket Go Izleme.app")), target));
    tg::ApplyRequest request;
    request.sourceApp = dir.filePath(QStringLiteral("fresh/Ticket Go Izleme.app"));
    request.targetApp = target;
    request.backupApp = dir.filePath(QStringLiteral("backup/Ticket Go Izleme.app"));
    request.dataDirectory = data;
    request.writable = true;
    const tg::ApplyResult result = tg::applyUpdate(request, [](const QString&) { return false; });
    QVERIFY(result.rolledBack);
    QFile dbAfter(data + QStringLiteral("/monitor.sqlite"));
    QVERIFY(dbAfter.open(QIODevice::ReadOnly));
    QCOMPARE(dbAfter.readAll(), QByteArray("sqlite-bytes"));
    QFile keyAfter(data + QStringLiteral("/keychain-not-used.txt"));
    QVERIFY(keyAfter.open(QIODevice::ReadOnly));
    QCOMPARE(keyAfter.readAll(), QByteArray("do-not-delete"));
#else
    QTemporaryDir dir;
    const QString data = dir.filePath(QStringLiteral("AppData/Roaming/TicketGoTeknoloji/SistemIzleme"));
    QDir().mkpath(data);
    QFile db(data + QStringLiteral("/monitor.sqlite"));
    QVERIFY(db.open(QIODevice::WriteOnly));
    db.write("sqlite-bytes");
    db.close();
    tg::ApplyRequest request;
    request.sourceApp = QStringLiteral("C:/Temp/Ticket Go Izleme.app");
    request.targetApp = QStringLiteral("C:/Program Files/TicketGo/Ticket Go Izleme.app");
    request.backupApp = QStringLiteral("C:/Temp/backup.app");
    request.dataDirectory = data;
    request.writable = true;
    const tg::ApplyResult result = tg::applyUpdate(request, [](const QString&) { return false; });
    QVERIFY(!result.ok);
    QVERIFY(!result.rolledBack);
    QFile dbAfter(data + QStringLiteral("/monitor.sqlite"));
    QVERIFY(dbAfter.open(QIODevice::ReadOnly));
    QCOMPARE(dbAfter.readAll(), QByteArray("sqlite-bytes"));
#endif
}

void TestUpdate::windowsPackagesAreNotOfferedToMac()
{
    QCOMPARE(tg::assetNameForPlatformArch(QStringLiteral("windows"), QStringLiteral("x64")), QStringLiteral("TicketGoIzleme-Windows-x64-Setup.exe"));
    QCOMPARE(tg::assetNameForPlatformArch(QStringLiteral("macos"), QStringLiteral("arm64")), QStringLiteral("TicketGoIzleme-macOS-arm64.zip"));
    QCOMPARE(tg::inferAssetPlatform(QStringLiteral("TicketGoIzleme-Windows-x64-Setup.exe"), QString()), QStringLiteral("windows"));
    QCOMPARE(tg::inferAssetPlatform(QStringLiteral("TicketGoIzleme-macOS-arm64.zip"), QString()), QStringLiteral("macos"));
    QCOMPARE(tg::inferAssetPlatform(QStringLiteral("legacy.zip"), QStringLiteral("macos")), QStringLiteral("macos"));

    tg::ReleaseInfo release;
    release.tag = QStringLiteral("v1.9.0");
    tg::ReleaseAsset windows;
    windows.name = QStringLiteral("TicketGoIzleme-Windows-x64-Setup.exe");
    windows.url = QUrl(QStringLiteral("https://github.com/ticketgoteknoloji-web/ticketgoteknoloji-izleme-monit-r-/releases/download/v1.9.0/TicketGoIzleme-Windows-x64-Setup.exe"));
    windows.platform = QStringLiteral("windows");
    tg::ReleaseAsset manifest;
    manifest.name = QStringLiteral("TicketGoIzleme-update-manifest.json");
    manifest.url = QUrl(QStringLiteral("https://github.com/ticketgoteknoloji-web/ticketgoteknoloji-izleme-monit-r-/releases/download/v1.9.0/TicketGoIzleme-update-manifest.json"));
    release.assets = {windows, manifest};
    const QByteArray body = QByteArrayLiteral("{\"version\":\"1.9.0\",\"channel\":\"stable\",\"assets\":[{\"name\":\"TicketGoIzleme-Windows-x64-Setup.exe\",\"platform\":\"windows\",\"arch\":\"x64\",\"sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"bundleId\":\"tr.ticketgoteknoloji.izleme\",\"size\":4}]}");
    const tg::AssetChoice macChoice = tg::selectReleasePackage(release, QStringLiteral("macos"), QStringLiteral("arm64"), body);
    QVERIFY(!macChoice.ok);
    QCOMPARE(macChoice.message, tg::msgWrongPlatform());
}

void TestUpdate::windowsClientSelectsWindowsSetup()
{
    tg::ReleaseInfo release;
    release.tag = QStringLiteral("v1.9.0");
    tg::ReleaseAsset mac;
    mac.name = QStringLiteral("TicketGoIzleme-macOS-arm64.zip");
    mac.url = QUrl(QStringLiteral("https://github.com/ticketgoteknoloji-web/ticketgoteknoloji-izleme-monit-r-/releases/download/v1.9.0/TicketGoIzleme-macOS-arm64.zip"));
    tg::ReleaseAsset windows;
    windows.name = QStringLiteral("TicketGoIzleme-Windows-x64-Setup.exe");
    windows.url = QUrl(QStringLiteral("https://github.com/ticketgoteknoloji-web/ticketgoteknoloji-izleme-monit-r-/releases/download/v1.9.0/TicketGoIzleme-Windows-x64-Setup.exe"));
    windows.platform = QStringLiteral("windows");
    tg::ReleaseAsset manifest;
    manifest.name = QStringLiteral("TicketGoIzleme-update-manifest.json");
    manifest.url = QUrl(QStringLiteral("https://github.com/ticketgoteknoloji-web/ticketgoteknoloji-izleme-monit-r-/releases/download/v1.9.0/TicketGoIzleme-update-manifest.json"));
    release.assets = {mac, windows, manifest};
    const QByteArray body = QByteArrayLiteral("{\"version\":\"1.9.0\",\"channel\":\"stable\",\"assets\":[{\"name\":\"TicketGoIzleme-macOS-arm64.zip\",\"arch\":\"arm64\",\"sha256\":\"bbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbbb\",\"bundleId\":\"tr.ticketgoteknoloji.izleme\",\"size\":4},{\"name\":\"TicketGoIzleme-Windows-x64-Setup.exe\",\"platform\":\"windows\",\"arch\":\"x64\",\"sha256\":\"aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa\",\"bundleId\":\"tr.ticketgoteknoloji.izleme\",\"size\":8}]}");
    const tg::AssetChoice winChoice = tg::selectReleasePackage(release, QStringLiteral("windows"), QStringLiteral("x64"), body);
    QVERIFY(winChoice.ok);
    QCOMPARE(winChoice.package.name, QStringLiteral("TicketGoIzleme-Windows-x64-Setup.exe"));
    QCOMPARE(winChoice.package.platform, QStringLiteral("windows"));
    QCOMPARE(winChoice.package.sha256, QStringLiteral("aaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaaa"));
    const tg::AssetChoice legacyMac = tg::selectMacPackage(release, QStringLiteral("arm64"), body);
    QVERIFY(legacyMac.ok);
    QCOMPARE(legacyMac.package.name, QStringLiteral("TicketGoIzleme-macOS-arm64.zip"));
}

void TestUpdate::windowsHashMismatchBlocksInstall()
{
    QTemporaryDir dir;
    LocalHub hub;
    tg::UpdateManager manager;
    QVERIFY(prepare(manager, hub, dir));
    manager.setPlatformForTesting(QStringLiteral("windows"));
    manager.setArchForTesting(QStringLiteral("x64"));
    const QString installed = dir.filePath(QStringLiteral("Program Files/TicketGoTeknoloji/TicketGo Izleme/TicketGoIzleme.exe"));
    QDir().mkpath(QFileInfo(installed).absolutePath());
    QFile exe(installed);
    QVERIFY(exe.open(QIODevice::WriteOnly));
    exe.write("installed");
    exe.close();
    manager.setExecutablePath(installed);
    hub.files.insert(QStringLiteral("/releases/latest"), releaseJson(hub, QStringLiteral("v1.9.0"), QStringLiteral("TicketGoIzleme-Windows-x64-Setup.exe"), true));
    hub.files.insert(QStringLiteral("/manifest.json"), QByteArray("{\"version\":\"1.9.0\",\"channel\":\"stable\",\"assets\":[{\"name\":\"TicketGoIzleme-Windows-x64-Setup.exe\",\"platform\":\"windows\",\"arch\":\"x64\",\"sha256\":\"ffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffffff\",\"bundleId\":\"tr.ticketgoteknoloji.izleme\",\"size\":4}]}"));
    hub.files.insert(QStringLiteral("/pkg.zip"), QByteArray("exe!"));
    manager.checkForUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("UPDATE_AVAILABLE"), 4000);
    manager.confirmDownload();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("ERROR"), 8000);
    QCOMPARE(manager.errorMessage(), tg::msgHashMismatch());
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("data/updates/staging/TicketGoIzleme-Windows-x64-Setup.exe"))) == false);
}

void TestUpdate::windowsDownloadKeepsExeAndReachesReady()
{
    QTemporaryDir dir;
    LocalHub hub;
    tg::UpdateManager manager;
    QVERIFY(prepare(manager, hub, dir));
    manager.setPlatformForTesting(QStringLiteral("windows"));
    manager.setArchForTesting(QStringLiteral("x64"));
    // Synthetic installed path must not contain the source-tree marker used by inspectLocation.
    manager.setExecutablePath(QStringLiteral("C:/Users/Demo/AppData/Local/TicketGoTeknoloji/TicketGo Izleme/TicketGoIzleme.exe"));
    const QByteArray payload = QByteArray("setup-bytes");
    const QByteArray digest = sha256(payload);
    hub.files.insert(QStringLiteral("/releases/latest"), releaseJson(hub, QStringLiteral("v1.9.0"), QStringLiteral("TicketGoIzleme-Windows-x64-Setup.exe"), true));
    hub.files.insert(QStringLiteral("/manifest.json"), QByteArray("{\"version\":\"1.9.0\",\"channel\":\"stable\",\"assets\":[{\"name\":\"TicketGoIzleme-Windows-x64-Setup.exe\",\"platform\":\"windows\",\"arch\":\"x64\",\"sha256\":\"") + digest + "\",\"bundleId\":\"tr.ticketgoteknoloji.izleme\",\"size\":" + QByteArray::number(payload.size()) + "}]}");
    hub.files.insert(QStringLiteral("/pkg.zip"), payload);
    manager.checkForUpdates();
    QTRY_COMPARE_WITH_TIMEOUT(manager.phase(), QStringLiteral("UPDATE_AVAILABLE"), 4000);
    manager.confirmDownload();
    QTRY_VERIFY_WITH_TIMEOUT(manager.phase() == QLatin1String("READY_TO_INSTALL") || manager.phase() == QLatin1String("ERROR"), 8000);
    QCOMPARE(manager.errorMessage(), QString());
    QCOMPARE(manager.phase(), QStringLiteral("READY_TO_INSTALL"));
    QVERIFY(QFile::exists(dir.filePath(QStringLiteral("data/updates/staging/TicketGoIzleme-Windows-x64-Setup.exe"))));
    QVERIFY(!QFile::exists(dir.filePath(QStringLiteral("data/updates/staging/package.zip"))));
    QCOMPARE(manager.guidance(), tg::msgWindowsSmartScreen());
}

void TestUpdate::windowsHelperLaunchesSetup()
{
    QTemporaryDir dir;
    const QString setup = dir.filePath(QStringLiteral("TicketGoIzleme-Windows-x64-Setup.exe"));
    QFile file(setup);
    QVERIFY(file.open(QIODevice::WriteOnly));
    file.write("installer");
    file.close();
    tg::HelperPlan plan;
    plan.parentPid = 0;
    plan.sourceApp = setup;
    plan.targetApp = dir.filePath(QStringLiteral("TicketGoIzleme.exe"));
    plan.kind = QStringLiteral("windows-setup");
    plan.authority = QStringLiteral("windows-setup-hash-verified");
    const QString planPath = dir.filePath(QStringLiteral("plan.json"));
    QString error;
    QVERIFY(tg::writeHelperPlan(planPath, plan, &error));
    QString launched;
    const int code = tg::runHelper(planPath, {}, [&](const QString& path) {
        launched = path;
        return true;
    }, false);
    QCOMPARE(code, 0);
    QCOMPARE(launched, setup);
}

QTEST_MAIN(TestUpdate)
#include "test_update.moc"
