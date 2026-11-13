#include "trayicon.h"

#include <KLocalizedString>

#include <QApplication>
#include <QCursor>
#include <QIcon>
#include <QMenu>
#include <QQuickWindow>
#include <QScreen>
#include <QSystemTrayIcon>

QPoint trayPopupPosition(const QRect &anchor, const QSize &popup, const QRect &screen)
{
    const int gap = 4;
    int x = anchor.center().x() - popup.width() / 2;
    // Above the icon when the panel is in the lower half of the screen, else below.
    int y = anchor.center().y() > screen.center().y() ? anchor.top() - gap - popup.height() : anchor.bottom() + 1 + gap;
    x = qBound(screen.left(), x, qMax(screen.left(), screen.right() + 1 - popup.width()));
    y = qBound(screen.top(), y, qMax(screen.top(), screen.bottom() + 1 - popup.height()));
    return {x, y};
}

TrayIcon::TrayIcon(QQuickWindow *mainWindow, QQuickWindow *popup, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_popup(popup)
    , m_icon(new QSystemTrayIcon(QIcon::fromTheme(QStringLiteral("org.anjankow.kdetodo"), QIcon::fromTheme(QStringLiteral("view-task"))), this))
    , m_menu(new QMenu)
{
    m_icon->setToolTip(i18n("Todo"));
    m_menu->addAction(QIcon::fromTheme(QStringLiteral("window")), i18nc("@action", "Open Todo"), this, &TrayIcon::showMainWindow);
    m_menu->addAction(QIcon::fromTheme(QStringLiteral("application-exit")), i18nc("@action", "Quit"), qApp, &QApplication::quit);
    m_icon->setContextMenu(m_menu);
    connect(m_icon, &QSystemTrayIcon::activated, this, [this](QSystemTrayIcon::ActivationReason reason) {
        if (reason == QSystemTrayIcon::Trigger) {
            togglePopup();
        }
    });
    m_icon->show();
}

TrayIcon::~TrayIcon()
{
    delete m_menu;
}

bool TrayIcon::isAvailable()
{
    return QSystemTrayIcon::isSystemTrayAvailable();
}

void TrayIcon::showMainWindow()
{
    m_popup->hide();
    m_mainWindow->show();
    m_mainWindow->raise();
    m_mainWindow->requestActivate();
}

void TrayIcon::togglePopup()
{
    if (m_popup->isVisible()) {
        m_popup->hide();
        return;
    }
    // The tray does not always tell where the icon is; the click was on it.
    QRect anchor = m_icon->geometry();
    if (!anchor.isValid()) {
        anchor = QRect(QCursor::pos(), QSize(1, 1));
    }
    QScreen *screen = QGuiApplication::screenAt(anchor.center());
    if (!screen) {
        screen = QGuiApplication::primaryScreen();
    }
    m_popup->setScreen(screen);
    m_popup->setPosition(trayPopupPosition(anchor, m_popup->size(), screen->availableGeometry()));
    m_popup->show();
    m_popup->raise();
    m_popup->requestActivate();
}
