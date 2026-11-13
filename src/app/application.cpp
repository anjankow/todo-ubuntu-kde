#include "application.h"

#include "appcontroller.h"

#include <KLocalizedContext>

#include <QQmlApplicationEngine>
#include <QQmlContext>
#include <QQuickStyle>
#include <QUrl>

// The QML files are compiled into this (static) library.
static void initResources()
{
    Q_INIT_RESOURCE(qml);
}

void Application::prepare()
{
    // The Plasma look for the Qt Quick controls, unless the user chose another style.
    if (qEnvironmentVariableIsEmpty("QT_QUICK_CONTROLS_STYLE")) {
        QQuickStyle::setStyle(QStringLiteral("org.kde.desktop"));
    }
}

bool Application::load(QQmlApplicationEngine &engine, AppController *controller)
{
    initResources();
    engine.rootContext()->setContextObject(new KLocalizedContext(&engine));
    engine.rootContext()->setContextProperty(QStringLiteral("app"), controller);
    engine.load(QUrl(QStringLiteral("qrc:/qml/Main.qml")));
    return !engine.rootObjects().isEmpty();
}
