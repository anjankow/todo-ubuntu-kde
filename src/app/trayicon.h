#pragma once

#include <QObject>
#include <QPoint>
#include <QRect>
#include <QSize>

class QMenu;
class QQuickWindow;
class QSystemTrayIcon;

// Where the tray popup goes: next to `anchor` (the tray icon, or the pointer
// when the icon's place is unknown), on the side with room, inside `screen`.
QPoint trayPopupPosition(const QRect &anchor, const QSize &popup, const QRect &screen);

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
