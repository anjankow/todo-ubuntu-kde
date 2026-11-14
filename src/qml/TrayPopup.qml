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

    width: Kirigami.Units.gridUnit * 22
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

    // How many tasks of the list are not in the popup.
    readonly property int moreCount: Math.max(0, app.activeModel.totalCount - app.topTasks.length)

    Rectangle {
        anchors.fill: parent
        color: "transparent"
        border.width: 1
        border.color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.3)
    }

    // Laid out like Plasma's notification popup: a large title with the
    // application's icon, then one entry per task with a line on its left,
    // its title in bold and its description below.
    ColumnLayout {
        id: content
        anchors.fill: parent
        anchors.margins: Kirigami.Units.largeSpacing
        spacing: Kirigami.Units.largeSpacing

        RowLayout {
            objectName: "trayHeader"
            spacing: Kirigami.Units.smallSpacing

            Kirigami.Icon {
                objectName: "trayIcon"
                source: "qrc:/qml/icons/app.png"
                implicitWidth: Kirigami.Units.iconSizes.medium
                implicitHeight: Kirigami.Units.iconSizes.medium
            }
            Kirigami.Heading {
                objectName: "trayHeading"
                level: 1
                // As large as the title of Plasma's own popups.
                font.pointSize: Kirigami.Theme.defaultFont.pointSize * 1.6
                text: i18n("Todo")
            }
        }

        Repeater {
            model: app.topTasks

            Item {
                objectName: "trayTask-" + modelData.id
                Layout.fillWidth: true
                implicitHeight: Math.max(texts.implicitHeight, complete.implicitHeight)

                // The line on the left, as notifications have, along the text.
                Rectangle {
                    objectName: "trayLine-" + modelData.id
                    width: 2
                    y: texts.y
                    height: texts.height
                    color: Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.25)
                }
                Column {
                    id: texts
                    x: Kirigami.Units.largeSpacing
                    anchors.verticalCenter: parent.verticalCenter
                    width: parent.width - x - complete.width - Kirigami.Units.smallSpacing
                    spacing: 2

                    Text {
                        objectName: "trayTitle-" + modelData.id
                        width: parent.width
                        text: modelData.title
                        textFormat: Text.PlainText
                        elide: Text.ElideRight
                        color: Kirigami.Theme.textColor
                        font.family: Kirigami.Theme.defaultFont.family
                        font.pointSize: Kirigami.Theme.defaultFont.pointSize + 1
                        font.bold: true
                    }
                    Text {
                        objectName: "trayDescription-" + modelData.id
                        width: parent.width
                        visible: modelData.description !== ""
                        text: modelData.description
                        textFormat: Text.PlainText
                        wrapMode: Text.Wrap
                        maximumLineCount: 2
                        elide: Text.ElideRight
                        color: Kirigami.Theme.textColor
                        font: Kirigami.Theme.defaultFont
                    }
                }
                QQC2.ToolButton {
                    id: complete
                    objectName: "trayComplete-" + modelData.id
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    icon.source: "qrc:/qml/icons/complete.svg"
                    display: QQC2.AbstractButton.IconOnly
                    text: i18n("Complete")
                    QQC2.ToolTip.text: text
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
        // Like the notifications' "Show 26 More": the rest is in the window.
        QQC2.ToolButton {
            objectName: "trayMore"
            visible: popup.moreCount > 0
            text: i18np("Show %1 more", "Show %1 more", popup.moreCount)
            flat: true
            onClicked: popup.openRequested()
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
