#include "app/appcontroller.h"
#include "app/application.h"
#include "app/trayicon.h"

#include <KAboutData>
#include <KDBusService>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QQmlApplicationEngine>
#include <QQuickWindow>

#include <memory>

int main(int argc, char **argv)
{
    Application::prepare();
    // A QApplication: the Plasma style of the controls draws with widgets' styles.
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("kdetodo");

    KAboutData about(
        QStringLiteral("kdetodo"), i18n("Todo"), QStringLiteral(KDETODO_VERSION), i18n("A simple todo list stored in a text file"), KAboutLicense::Custom);
    about.setLicenseText(QStringLiteral("MIT License"));
    about.addAuthor(QStringLiteral("anjankow"), QString(), QStringLiteral("anjankow@protonmail.com"));
    about.setDesktopFileName(QStringLiteral("org.anjankow.kdetodo"));
    KAboutData::setApplicationData(about);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("org.anjankow.kdetodo"), QIcon::fromTheme(QStringLiteral("view-task"))));

    QCommandLineParser parser;
    about.setupCommandLine(&parser);
    parser.process(app);
    about.processCommandLine(&parser);

    // One instance only: starting the application again raises this one.
    // Without a D-Bus session (rare outside a desktop) it still starts.
    const KDBusService service(KDBusService::Unique | KDBusService::NoExitOnFailure);

    AppController controller;
    controller.setKeepRunningInTray(TrayIcon::isAvailable());
    QQmlApplicationEngine engine;
    if (!Application::load(engine, &controller)) {
        return 1;
    }
    auto *window = qobject_cast<QQuickWindow *>(engine.rootObjects().constFirst());
    auto *popup = window ? window->findChild<QQuickWindow *>(QStringLiteral("trayPopup")) : nullptr;
    if (!window || !popup) {
        return 1;
    }
    QObject::connect(&service, &KDBusService::activateRequested, window, [window] {
        QMetaObject::invokeMethod(window, "showMainWindow");
    });
    std::unique_ptr<TrayIcon> tray;
    if (TrayIcon::isAvailable()) {
        QApplication::setQuitOnLastWindowClosed(false); // it lives on in the tray
        tray = std::make_unique<TrayIcon>(window, popup);
    }
    return app.exec();
}
