#pragma once

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSize>

class QMenu;
class QQuickWindow;
class QWindow;
class QSystemTrayIcon;

// Where the tray popup goes: next to `anchor` (the tray icon, or the pointer
// when the icon's place is unknown), on the side with room, inside `screen`.
QPoint trayPopupPosition(const QRect &anchor, const QSize &popup, const QRect &screen);

// Makes a popup window behave like a menu that leaves the other windows
// alone: it closes on a mouse press outside it or on Esc, and it is never
// transient for another window (window managers raise and focus a window
// together with the windows it is transient for). For a Qt::Popup window
// (X11) it also grabs keyboard and mouse while the popup is shown, since
// such a window never gets the focus.
class PopupDismisser : public QObject
{
public:
    explicit PopupDismisser(QWindow *popup);

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QWindow *m_popup;
};

// The application's icon in the system tray. A click shows the popup with
// the first tasks; the context menu opens the window or quits.
class TrayIcon : public QObject
{
    Q_OBJECT
public:
    // `mainWindow` is the application window, `popup` the tray popup window.
    TrayIcon(QQuickWindow *mainWindow, QQuickWindow *popup, QObject *parent = nullptr);
    ~TrayIcon() override;

    static bool isAvailable();
    void showMainWindow();

private:
    void togglePopup();

    QQuickWindow *m_mainWindow;
    QQuickWindow *m_popup;
    QSystemTrayIcon *m_icon;
    QMenu *m_menu;
};
