#include "app/appcontroller.h"
#include "app/application.h"

#include <KAboutData>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>
#include <QIcon>
#include <QQmlApplicationEngine>

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

    AppController controller;
    QQmlApplicationEngine engine;
    if (!Application::load(engine, &controller)) {
        return 1;
    }
    return app.exec();
}
