#include "trayicon.h"

#include <KLocalizedString>

#include <QApplication>
#include <QCursor>
#include <QIcon>
#include <QKeyEvent>
#include <QMenu>
#include <QMouseEvent>
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

PopupDismisser::PopupDismisser(QWindow *popup)
    : QObject(popup)
    , m_popup(popup)
{
    popup->installEventFilter(this);
}

bool PopupDismisser::eventFilter(QObject *watched, QEvent *event)
{
    if (watched != m_popup) {
        return false;
    }
    switch (event->type()) {
    case QEvent::Show:
        // Comes before the window is mapped. A window declared inside another
        // one in QML is its transient child; detaching it only here (not at
        // load, when Qt still waits for the parent to become visible) keeps
        // Qt's own bookkeeping of that parent intact.
        if (m_popup->transientParent()) {
            m_popup->setTransientParent(nullptr);
        }
        break;
    case QEvent::Expose:
        if (m_popup->isExposed() && m_popup->type() == Qt::Popup) {
            m_popup->setKeyboardGrabEnabled(true);
            m_popup->setMouseGrabEnabled(true);
        }
        break;
    case QEvent::Hide:
        m_popup->setKeyboardGrabEnabled(false);
        m_popup->setMouseGrabEnabled(false);
        break;
    case QEvent::MouseButtonPress:
        // While grabbed, presses anywhere on the screen arrive here.
        if (!QRect(QPoint(0, 0), m_popup->size()).contains(static_cast<QMouseEvent *>(event)->pos())) {
            m_popup->hide();
            return true;
        }
        break;
    case QEvent::KeyPress:
        if (static_cast<QKeyEvent *>(event)->key() == Qt::Key_Escape) {
            m_popup->hide();
            return true;
        }
        break;
    default:
        break;
    }
    return false;
}

TrayIcon::TrayIcon(QQuickWindow *mainWindow, QQuickWindow *popup, QObject *parent)
    : QObject(parent)
    , m_mainWindow(mainWindow)
    , m_popup(popup)
    , m_icon(new QSystemTrayIcon(QIcon::fromTheme(QStringLiteral("org.anjankow.kdetodo"), QIcon(QStringLiteral(":/qml/icons/app.png"))), this))
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
    new PopupDismisser(popup);
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
    // A menu-like popup works without the focus; taking it would change
    // which window is active, and closing the popup would hand the focus
    // to the main window.
    if (m_popup->type() != Qt::Popup) {
        m_popup->requestActivate();
    }
}
