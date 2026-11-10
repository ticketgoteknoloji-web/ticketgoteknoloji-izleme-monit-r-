#pragma once

#include "PackageVerifier.h"
#include "UpdateTypes.h"

#include <QElapsedTimer>
#include <QObject>
#include <QUrl>
#include <QVector>

#include <functional>

class QFile;
class QNetworkAccessManager;
class QNetworkReply;

namespace tg {

class UpdateManager : public QObject {
    Q_OBJECT
    Q_PROPERTY(QString phase READ phase NOTIFY changed)
    Q_PROPERTY(QString buttonText READ buttonText NOTIFY changed)
    Q_PROPERTY(QString tooltipText READ tooltipText NOTIFY changed)
    Q_PROPERTY(QString iconMode READ iconMode NOTIFY changed)
    Q_PROPERTY(bool iconSpins READ iconSpins NOTIFY changed)
    Q_PROPERTY(bool busy READ busy NOTIFY changed)
    Q_PROPERTY(QString statusText READ statusText NOTIFY changed)
    Q_PROPERTY(QString lastStatusText READ lastStatusText NOTIFY changed)
    Q_PROPERTY(QString lastCheckedText READ lastCheckedText NOTIFY changed)
    Q_PROPERTY(QString currentVersion READ currentVersion CONSTANT)
    Q_PROPERTY(QString latestVersion READ latestVersion NOTIFY changed)
    Q_PROPERTY(QString publishedText READ publishedText NOTIFY changed)
    Q_PROPERTY(QString releaseNotes READ releaseNotes NOTIFY changed)
    Q_PROPERTY(QString packageBytesText READ packageBytesText NOTIFY changed)
    Q_PROPERTY(QString platformText READ platformText CONSTANT)
    Q_PROPERTY(QString channel READ channel CONSTANT)
    Q_PROPERTY(int progressPercent READ progressPercent NOTIFY changed)
    Q_PROPERTY(QString progressText READ progressText NOTIFY changed)
    Q_PROPERTY(QString transferText READ transferText NOTIFY changed)
    Q_PROPERTY(QString speedText READ speedText NOTIFY changed)
    Q_PROPERTY(QString errorMessage READ errorMessage NOTIFY changed)
    Q_PROPERTY(QString guidance READ guidance NOTIFY changed)
    Q_PROPERTY(bool dialogOpen READ dialogOpen NOTIFY changed)
public:
    explicit UpdateManager(QObject* parent = nullptr);
    QString phase() const { return phase_; }
    QString buttonText() const { return phaseButtonText(phase_); }
    QString tooltipText() const { return phaseTooltip(phase_); }
    QString iconMode() const { return phaseIconMode(phase_); }
    bool iconSpins() const { return phaseIconSpins(phase_); }
    bool busy() const { return phaseBusy(phase_); }
    QUrl configuredReleaseApi() const { return releaseUrl_; }
    QString statusText() const { return statusText_; }
    QString lastStatusText() const { return lastStatusText_; }
    QString lastCheckedText() const { return lastCheckedText_; }
    QString currentVersion() const { return currentVersion_; }
    QString latestVersion() const { return latestVersion_; }
    QString publishedText() const { return publishedText_; }
    QString releaseNotes() const { return releaseNotes_; }
    QString packageBytesText() const { return packageBytesText_; }
    QString platformText() const { return platformText_; }
    QString channel() const { return QStringLiteral("Stable"); }
    int progressPercent() const { return progressPercent_; }
    QString progressText() const { return progressText_; }
    QString transferText() const { return transferText_; }
    QString speedText() const { return speedText_; }
    QString errorMessage() const { return errorMessage_; }
    QString guidance() const { return guidance_; }
    bool dialogOpen() const { return dialogOpen_; }

    void setReleaseUrl(const QUrl& url);
    void setArchForTesting(const QString& arch);
    void setPlatformForTesting(const QString& platform);
    void setSignatureProbe(SignatureProbe probe);
    void setExecutablePath(const QString& path);
    void setDataDirectory(const QString& path);
    void setInstallLauncher(const std::function<bool(const QString& planPath)>& launcher);
    void setAllowSystemAuthorization(bool enabled);
    void setExpectedAuthorityForTesting(const QString& authority);

    Q_INVOKABLE void activateButton();
    Q_INVOKABLE void checkForUpdates();
    Q_INVOKABLE void confirmDownload();
    Q_INVOKABLE void cancelDownload();
    Q_INVOKABLE void confirmInstall();
    Q_INVOKABLE void dismissDialog();

signals:
    void changed();
    void restartRequested();

private:
    enum class Fetch { None, Release, Manifest, Package };
    bool urlAllowed(const QUrl& url) const;
    void setPhase(const QString& phase, const QString& status);
    void fail(const QString& message, const QString& guidance = {});
    void persist() const;
    void loadState();
    void startRequest(const QUrl& url, Fetch kind, const QString& filePath);
    void finishReply();
    void followRedirect(const QUrl& url);
    void takeRelease(int status, const QByteArray& body, const QByteArray& rateRemaining);
    void takeManifest(const QByteArray& body);
    void takePackage();
    void publishAvailable();
    QString formatBytes(qint64 bytes) const;
    void noteProgress();

    QNetworkAccessManager* network_ = nullptr;
    QNetworkReply* reply_ = nullptr;
    QFile* file_ = nullptr;
    Fetch fetch_ = Fetch::None;
    bool cancelRequested_ = false;
    bool loopbackTests_ = false;
    bool allowAuthorization_ = false;
    bool dialogOpen_ = false;
    QUrl releaseUrl_;
    QUrl manifestUrl_;
    QUrl packageUrl_;
    QString phase_ = QStringLiteral("IDLE");
    QString statusText_ = QStringLiteral("Henüz denetlenmedi");
    QString lastStatusText_;
    QString lastCheckedText_;
    QString currentVersion_;
    QString latestVersion_;
    QString latestTag_;
    QString publishedText_;
    QString releaseNotes_;
    QString packageBytesText_;
    QString platformText_;
    QString errorMessage_;
    QString guidance_;
    QString arch_;
    QString updatePlatform_;
    QString authorityOverride_;
    QString executablePath_;
    QString dataDirectory_;
    QString packagePath_;
    QString extractedApp_;
    QString windowsSetupPath_;
    QString expectedSha_;
    QString expectedVersion_;
    qint64 packageBytes_ = -1;
    qint64 receivedBytes_ = 0;
    int progressPercent_ = -1;
    QString progressText_;
    QString transferText_;
    QString speedText_;
    QElapsedTimer elapsed_;
    SignatureProbe probe_;
    std::function<bool(const QString&)> launcher_;
    QByteArray manifestBody_;
    QString releaseBodyNotes_;
    QString releasePublished_;
    QVector<QUrl> declaredDownloads_;
};

} // namespace tg
