#include "ui/mainwindow.h"

#include <KAboutData>
#include <KLocalizedString>

#include <QApplication>
#include <QCommandLineParser>

int main(int argc, char **argv)
{
    QApplication app(argc, argv);
    KLocalizedString::setApplicationDomain("kdetodo");

    KAboutData about(
        QStringLiteral("kdetodo"), i18n("Todo"), QStringLiteral(KDETODO_VERSION), i18n("A simple todo list stored in a text file"), KAboutLicense::Custom);
    about.setLicenseText(QStringLiteral("MIT License"));
    about.addAuthor(QStringLiteral("anjankow"), QString(), QStringLiteral("anjankow@protonmail.com"));
    KAboutData::setApplicationData(about);
    QApplication::setWindowIcon(QIcon::fromTheme(QStringLiteral("view-task")));

    QCommandLineParser parser;
    about.setupCommandLine(&parser);
    parser.process(app);
    about.processCommandLine(&parser);

    auto *window = new MainWindow;
    window->show();
    return app.exec();
}
