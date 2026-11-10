#pragma once

#include <QString>
#include <QUrl>

namespace tg {

inline QString officialRepoOwner() { return QStringLiteral("ticketgoteknoloji-web"); }
inline QString officialRepoName() { return QStringLiteral("ticketgoteknoloji-izleme-monit-r-"); }
inline QString officialRepositoryUrl()
{
    return QStringLiteral("https://github.com/") + officialRepoOwner() + QLatin1Char('/') + officialRepoName();
}
inline QUrl officialReleasePage() { return QUrl(officialRepositoryUrl() + QStringLiteral("/releases")); }
inline QUrl officialReleaseApi()
{
    return QUrl(QStringLiteral("https://api.github.com/repos/") + officialRepoOwner() + QLatin1Char('/') + officialRepoName() + QStringLiteral("/releases/latest"));
}

inline QString appVersionLiteral()
{
#ifdef TICKETGO_APP_VERSION
    return QString::fromUtf8(TICKETGO_APP_VERSION);
#else
    return QStringLiteral("0.1.3");
#endif
}

inline QString configuredDeveloperAuthority()
{
#ifdef TICKETGO_DEVELOPER_ID
    return QString::fromUtf8(TICKETGO_DEVELOPER_ID).trimmed();
#else
    return {};
#endif
}

inline QString phaseButtonText(const QString& phase)
{
    if (phase == QLatin1String("CHECKING")) return QStringLiteral("Güncellemeler Denetleniyor...");
    if (phase == QLatin1String("UP_TO_DATE")) return QStringLiteral("Uygulamanız Güncel");
    if (phase == QLatin1String("UPDATE_AVAILABLE")) return QStringLiteral("Güncellemeyi İndir");
    if (phase == QLatin1String("DOWNLOADING")) return QStringLiteral("İndiriliyor...");
    if (phase == QLatin1String("VERIFYING")) return QStringLiteral("Doğrulanıyor...");
    if (phase == QLatin1String("READY_TO_INSTALL")) return QStringLiteral("Güncellemeyi Yükle");
    if (phase == QLatin1String("INSTALLING")) return QStringLiteral("Yükleniyor...");
    if (phase == QLatin1String("RESTART_REQUIRED")) return QStringLiteral("Yeniden açılıyor...");
    if (phase == QLatin1String("ERROR")) return QStringLiteral("Tekrar Dene");
    if (phase == QLatin1String("NO_RELEASE")) return QStringLiteral("Henüz Güncelleme Yayınlanmadı");
    if (phase == QLatin1String("UNSUPPORTED_PLATFORM")) return QStringLiteral("Platform desteklenmiyor");
    return QStringLiteral("Güncellemeleri Kontrol Et");
}

inline QString msgNotInstalled() { return QStringLiteral("Program henüz kurulu değil."); }
inline QString msgNoPermission() { return QStringLiteral("Kurulum klasörüne yazma izni yok."); }
inline QString msgNoStableRelease() { return QStringLiteral("Henüz yayımlanmış sürüm yok."); }
inline QString msgRateLimit() { return QStringLiteral("GitHub istek sınırı aşıldı. Bu, sürüm yok sonucu değildir."); }
inline QString msgApiFailure() { return QStringLiteral("GitHub API yanıtı alınamadı."); }
inline QString msgHashMismatch() { return QStringLiteral("SHA-256 uyuşmuyor. Paket kurulmayacak."); }
inline QString msgBadSignature() { return QStringLiteral("Kod imzası doğrulanamadı. Paket çalıştırılmadı."); }
inline QString msgBundleMismatch() { return QStringLiteral("Paket kimliği mevcut uygulamayla aynı değil."); }
inline QString msgVersionMismatch() { return QStringLiteral("Paket sürümü yayın sürümüyle uyuşmuyor."); }
inline QString msgManualOnly() { return QStringLiteral("Otomatik kurulum, doğrulanmış Developer ID imzası ve notarization olmadan yapılmaz. Paket çalıştırılmadı. Gatekeeper kapatılmamalı."); }
inline QString msgWindowsSmartScreen() { return QStringLiteral("Windows paketi kod imzalı değil. SHA-256 doğrulandı. SmartScreen uyarı gösterebilir. Uygulama imzalı olarak gösterilmez."); }
inline QString msgCancelled() { return QStringLiteral("İndirme iptal edildi."); }
inline QString msgMissingAsset() { return QStringLiteral("Bu platform için uygun paket yok."); }
inline QString msgWrongArch() { return QStringLiteral("Paket mimarisi bu bilgisayar ile uyuşmuyor."); }
inline QString msgWrongPlatform() { return QStringLiteral("Paket bu işletim sistemi için değil."); }
inline QString msgBadVersion() { return QStringLiteral("Sürüm etiketi SemVer değil."); }
inline QString msgDownloadFailed() { return QStringLiteral("Paket indirilemedi."); }
inline QString msgPathRejected() { return QStringLiteral("Paket yolu güvenli değil."); }
inline QString msgDeveloperBuild() { return QStringLiteral("Geliştirme kopyası güncellenmez. Program henüz kurulu değil."); }

inline QString phaseIconMode(const QString& phase)
{
    if (phase == QLatin1String("UPDATE_AVAILABLE") || phase == QLatin1String("DOWNLOADING") || phase == QLatin1String("VERIFYING") || phase == QLatin1String("READY_TO_INSTALL") || phase == QLatin1String("INSTALLING") || phase == QLatin1String("RESTART_REQUIRED")) return QStringLiteral("download");
    return QStringLiteral("refresh");
}

inline bool phaseIconSpins(const QString& phase)
{
    return phase == QLatin1String("CHECKING");
}

inline QString phaseTooltip(const QString& phase)
{
    if (phase == QLatin1String("UPDATE_AVAILABLE")) return QStringLiteral("Yeni Güncelleme Mevcut");
    if (phase == QLatin1String("ERROR")) return QStringLiteral("Güncelleme Denetlenemedi");
    if (phase == QLatin1String("READY_TO_INSTALL")) return QStringLiteral("Kuruluma Hazır");
    return phaseButtonText(phase);
}

inline bool phaseBusy(const QString& phase)
{
    return phase == QLatin1String("CHECKING") || phase == QLatin1String("DOWNLOADING") || phase == QLatin1String("VERIFYING") || phase == QLatin1String("INSTALLING");
}

} // namespace tg
