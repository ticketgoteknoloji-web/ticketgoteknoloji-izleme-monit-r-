#include "OpsController.h"
#include "UpdateManager.h"

#include <QApplication>
#include <QCoreApplication>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIcon>
#include <QPalette>
#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QSystemTrayIcon>
#include <QTimer>
#include <QUrl>

#if defined(Q_OS_MACOS)
#include <AudioToolbox/AudioToolbox.h>
#include <mach-o/dyld.h>
#include <stdlib.h>
#endif

#if defined(Q_OS_MACOS)
void useBundledQt()
{
    char raw[4096];
    uint32_t size = sizeof(raw);
    if (_NSGetExecutablePath(raw, &size) != 0) return;
    char resolved[4096];
    if (!realpath(raw, resolved)) return;
    const QString contents = QFileInfo(QString::fromUtf8(resolved)).dir().absoluteFilePath(QStringLiteral(".."));
    const QString plugins = QDir(contents).filePath(QStringLiteral("PlugIns"));
    if (!QFileInfo::exists(QDir(plugins).filePath(QStringLiteral("platforms/libqcocoa.dylib")))) return;
    const QByteArray pluginBytes = QFile::encodeName(plugins);
    const QByteArray qmlBytes = QFile::encodeName(QDir(contents).filePath(QStringLiteral("Resources/qml")));
    qputenv("QT_PLUGIN_PATH", pluginBytes);
    qputenv("QT_QPA_PLATFORM_PLUGIN_PATH", QFile::encodeName(QDir(plugins).filePath(QStringLiteral("platforms"))));
    qputenv("QML_IMPORT_PATH", qmlBytes);
    qputenv("QML2_IMPORT_PATH", qmlBytes);
}
#endif

namespace {
int probeOnce(int argc, char** argv)
{
    QCoreApplication app(argc, argv);
    QCoreApplication::setOrganizationName(QStringLiteral("TicketGoTeknoloji"));
    QCoreApplication::setApplicationName(QStringLiteral("SistemIzleme"));
    tg::Engine engine;
    QString error;
    if (!engine.start(&error)) {
        fprintf(stderr, "store: %s\n", qPrintable(error));
        return 2;
    }
    QTimer::singleShot(25000, &app, &QCoreApplication::quit);
    QObject::connect(&engine, &tg::Engine::updated, &app, [&app, &engine]() {
        bool apiDone = false;
        bool chatDone = false;
        bool ferryDone = false;
        for (const tg::ModuleSnapshot& module : engine.modules()) {
            if (module.id == QLatin1String("tg-api") && module.status != tg::ModuleStatus::Checking) apiDone = true;
            if (module.id == QLatin1String("gochat-api") && module.status != tg::ModuleStatus::Checking) chatDone = true;
            if (module.id == QLatin1String("tg-ferry") && module.status != tg::ModuleStatus::Checking) ferryDone = true;
        }
        if (apiDone && chatDone && ferryDone) QTimer::singleShot(200, &app, &QCoreApplication::quit);
    });
    app.exec();
    fprintf(stdout, "health_ratio=%s hata_var=%s\n", qPrintable(engine.overview().healthRatio ? QString::number(*engine.overview().healthRatio, 'f', 3) : QStringLiteral("none")), engine.overview().hataVar ? "yes" : "no");
    for (const tg::ModuleSnapshot& module : engine.modules()) {
        const bool interesting = module.id == QLatin1String("tg-api") || module.id == QLatin1String("tg-postgres") || module.id == QLatin1String("tg-redis")
            || module.id == QLatin1String("tg-nginx") || module.id == QLatin1String("tg-docker") || module.id == QLatin1String("tg-turmob")
            || module.id == QLatin1String("tg-iyzico") || module.id == QLatin1String("gochat-api") || module.id == QLatin1String("gochat-fcm")
            || module.id == QLatin1String("gochat-turn") || module.id == QLatin1String("gochat-sos") || module.id == QLatin1String("tg-ferry") || module.id == QLatin1String("tg-web");
        if (!interesting) continue;
        fprintf(stdout, "%s %s latency=%lld %s\n", qPrintable(module.id), qPrintable(tg::statusKey(module.status)), static_cast<long long>(module.latencyMs), qPrintable(module.evidence.left(220)));
    }
    return 0;
}
}

int main(int argc, char** argv)
{
    for (int i = 1; i < argc; ++i) {
        if (QString::fromLocal8Bit(argv[i]) == QLatin1String("--probe-once")) return probeOnce(argc, argv);
    }
#if defined(Q_OS_MACOS)
    useBundledQt();
#endif
    qputenv("QT_QUICK_CONTROLS_STYLE", QByteArrayLiteral("Fusion"));
    QApplication app(argc, argv);
    QPalette light;
    const QColor field(QStringLiteral("#FFFFFF"));
    const QColor ink(QStringLiteral("#243247"));
    const QColor muted(QStringLiteral("#64748B"));
    light.setColor(QPalette::Window, QColor(QStringLiteral("#F5F7FA")));
    light.setColor(QPalette::WindowText, ink);
    light.setColor(QPalette::Base, field);
    light.setColor(QPalette::AlternateBase, QColor(QStringLiteral("#F8FAFC")));
    light.setColor(QPalette::Text, ink);
    light.setColor(QPalette::Button, QColor(QStringLiteral("#EEF2F6")));
    light.setColor(QPalette::ButtonText, ink);
    light.setColor(QPalette::Highlight, QColor(QStringLiteral("#2563EB")));
    light.setColor(QPalette::HighlightedText, QColor(QStringLiteral("#FFFFFF")));
    light.setColor(QPalette::PlaceholderText, muted);
    light.setColor(QPalette::Mid, QColor(QStringLiteral("#CBD5E1")));
    light.setColor(QPalette::Disabled, QPalette::Text, muted);
    light.setColor(QPalette::Disabled, QPalette::ButtonText, muted);
    light.setColor(QPalette::Disabled, QPalette::WindowText, muted);
    app.setPalette(light);
    QCoreApplication::setOrganizationName(QStringLiteral("TicketGoTeknoloji"));
    QCoreApplication::setOrganizationDomain(QStringLiteral("ticketgoteknoloji.com"));
    QCoreApplication::setApplicationName(QStringLiteral("SistemIzleme"));
    QCoreApplication::setApplicationVersion(QStringLiteral(TICKETGO_APP_VERSION));
#if defined(Q_OS_WIN)
    QApplication::setApplicationDisplayName(QStringLiteral("TicketGo İzleme ve Takip Sistemi"));
#else
    QApplication::setApplicationDisplayName(QStringLiteral("Ticket Go İzleme"));
#endif
    const QIcon appIcon(QStringLiteral(":/qt/qml/TicketGo/Ops/qml/assets/logo-program.png"));
    QApplication::setWindowIcon(appIcon);
    tg::Engine engine;
    QString error;
    if (!engine.start(&error)) {
        fprintf(stderr, "%s\n", qPrintable(error));
        return 2;
    }
    QObject::connect(&app, &QCoreApplication::aboutToQuit, &engine, [&engine]() { engine.discardLogSession(); });
    tg::OpsController controller(&engine);
    tg::UpdateManager updates;
    updates.setAllowSystemAuthorization(true);
    updates.setExecutablePath(QCoreApplication::applicationFilePath());
    QSystemTrayIcon tray;
    tray.setIcon(appIcon);
    tray.show();
    QObject::connect(&engine, &tg::Engine::criticalOpened, &app, [&controller, &tray](const QString& title, const QString& body) {
        if (!controller.quietMode() && controller.soundEnabled()) {
#if defined(Q_OS_MACOS)
            AudioServicesPlayAlertSound(kSystemSoundID_UserPreferredAlert);
#endif
        }
        if (!controller.quietMode() && controller.desktopNotify() && QSystemTrayIcon::isSystemTrayAvailable() && QSystemTrayIcon::supportsMessages()) {
            tray.showMessage(title, body, QSystemTrayIcon::Critical, 8000);
        }
    });
    QQmlApplicationEngine qml;
    qml.rootContext()->setContextProperty(QStringLiteral("Ops"), &controller);
    qml.rootContext()->setContextProperty(QStringLiteral("Updates"), &updates);
    QObject::connect(&updates, &tg::UpdateManager::restartRequested, &app, &QCoreApplication::quit);
    QObject::connect(&qml, &QQmlApplicationEngine::objectCreationFailed, &app, []() { QCoreApplication::exit(3); }, Qt::QueuedConnection);
    qml.loadFromModule(QStringLiteral("TicketGo.Ops"), QStringLiteral("Main"));
    if (qml.rootObjects().isEmpty()) return 3;
    return QApplication::exec();
}
