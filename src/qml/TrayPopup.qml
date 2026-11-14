import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import QtQuick.Window 2.15
import org.kde.kirigami 2.20 as Kirigami

// The popup of the tray icon: the first tasks of the list, each with a
// checkmark to complete it, and a button that opens the application.
Window {
    id: popup

    signal openRequested()

    width: Kirigami.Units.gridUnit * 18
    height: content.implicitHeight + Kirigami.Units.largeSpacing * 2
    // Opening and closing the popup must leave the main window alone: only
    // "Open Todo" brings it to the front. Window managers raise and focus a
    // window together with the windows it is transient for, and on X11 Qt
    // makes every tool window transient for the application's windows. So
    // on X11 the popup is a menu-like window the window manager does not
    // manage (it takes keyboard and mouse while open); elsewhere it is a
    // tool window without a transient parent. Both: PopupDismisser.
    readonly property bool menuLike: Qt.platform.pluginName === "xcb"

    flags: (menuLike ? Qt.Popup : Qt.Tool) | Qt.FramelessWindowHint | Qt.WindowStaysOnTopHint
    color: Kirigami.Theme.backgroundColor
    title: i18n("Todo")
    // Like a menu, it goes away when something else is clicked.
    onActiveChanged: {
        if (!active) {
            hide()
        }
    }

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.3)
    }

    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.smallSpacing

        Kirigami.Heading {
            level: 4
            text: i18n("Next tasks")
        }
        Repeater {
            model: app.topTasks

            RowLayout {
                objectName: "trayTask-" + modelData.id
                Layout.fillWidth: true
                spacing: Kirigami.Units.smallSpacing

                Text {
                    objectName: "trayTitle-" + modelData.id
                    Layout.fillWidth: true
                    text: modelData.title
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: Kirigami.Theme.textColor
                    font: Kirigami.Theme.defaultFont
                }
                QQC2.ToolButton {
                    objectName: "trayComplete-" + modelData.id
                    icon.name: "checkmark"
                    icon.color: Kirigami.Theme.positiveTextColor
                    QQC2.ToolTip.text: i18n("Complete")
                    QQC2.ToolTip.visible: hovered
                    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                    onClicked: app.complete([modelData.id])
                }
            }
        }
        QQC2.Label {
            objectName: "trayEmpty"
            visible: app.topTasks.length === 0
            text: i18n("No tasks")
            opacity: 0.7
        }
        Kirigami.Separator {
            Layout.fillWidth: true
        }
        QQC2.Button {
            objectName: "trayOpen"
            Layout.fillWidth: true
            text: i18n("Open Todo")
            icon.name: "window"
            onClicked: popup.openRequested()
        }
    }
}
