import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami

// The tasks of one tab, each drawn as a card showing only the title.
// One click highlights a task, a double click opens its card.
QQC2.ScrollView {
    id: scroll

    property var taskModel
    property bool mainList: false // the Tasks tab: reordering and quick complete
    signal openRequested(string taskId)
    signal rowClicked()

    ListView {
        id: list
        objectName: scroll.objectName + "View"

        model: scroll.taskModel
        spacing: Kirigami.Units.smallSpacing * 2
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        delegate: Item {
            id: row
            objectName: "task-" + model.taskId

            readonly property bool highlighted: scroll.taskModel.currentId === model.taskId
            // the pointer is over the task (or over its quick-complete button)
            readonly property bool hovered: mouse.containsMouse || quickComplete.hovered || returnedMark.hovered

            width: list.width
            height: content.implicitHeight + Kirigami.Units.largeSpacing * 2

            Rectangle {
                id: card
                // Cards use the view colors (white in the light theme) on the window background.
                Kirigami.Theme.colorSet: Kirigami.Theme.View
                Kirigami.Theme.inherit: false
                anchors.fill: parent
                radius: Kirigami.Units.smallSpacing * 1.5
                color: row.highlighted ? Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.3))
                                       : row.hovered ? Qt.tint(Kirigami.Theme.backgroundColor, Qt.rgba(Kirigami.Theme.highlightColor.r, Kirigami.Theme.highlightColor.g, Kirigami.Theme.highlightColor.b, 0.1))
                                                       : Kirigami.Theme.backgroundColor
                border.width: 1
                border.color: row.highlighted ? Kirigami.Theme.highlightColor
                                              : Qt.rgba(Kirigami.Theme.textColor.r, Kirigami.Theme.textColor.g, Kirigami.Theme.textColor.b, 0.2)
            }

            MouseArea {
                id: mouse
                anchors.fill: parent
                hoverEnabled: true
                onClicked: {
                    scroll.taskModel.currentId = model.taskId
                    scroll.rowClicked()
                }
                onDoubleClicked: scroll.openRequested(model.taskId)
            }

            RowLayout {
                id: content
                anchors.fill: parent
                anchors.margins: Kirigami.Units.largeSpacing
                anchors.leftMargin: Kirigami.Units.smallSpacing
                spacing: Kirigami.Units.smallSpacing

                Kirigami.ListItemDragHandle {
                    objectName: "handle-" + model.taskId
                    visible: scroll.mainList
                    listItem: row
                    listView: list
                    onMoveRequested: scroll.taskModel.moveTask(oldIndex, newIndex)
                }
                QQC2.CheckBox {
                    objectName: "check-" + model.taskId
                    focusPolicy: Qt.NoFocus
                    checked: model.checked
                    onToggled: scroll.taskModel.setChecked(model.taskId, checked)
                }
                Row {
                    spacing: 3
                    visible: model.labelColors.length > 0

                    Repeater {
                        model: labelColors

                        Rectangle {
                            width: 10
                            height: 10
                            radius: 5
                            color: modelData
                        }
                    }
                }
                // Plain Text items: a Label would take the hover away from the row.
                Text {
                    objectName: "title-" + model.taskId
                    Layout.fillWidth: true
                    text: model.title
                    textFormat: Text.PlainText
                    elide: Text.ElideRight
                    color: Kirigami.Theme.textColor
                    font.family: Kirigami.Theme.defaultFont.family
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize + 2
                }
                // Room for the alarm icon and the quick-complete checkmark.
                Item {
                    visible: model.returned
                    implicitWidth: returnedMark.width + quickComplete.width + Kirigami.Units.smallSpacing
                }
                Text {
                    visible: model.postponedText !== ""
                    text: i18n("until %1", model.postponedText)
                    textFormat: Text.PlainText
                    color: Kirigami.Theme.textColor
                    font: Kirigami.Theme.defaultFont
                    opacity: 0.7
                }
            }

            // A postponed task that came back by itself: stays until it is
            // clicked or the task changes its state (also after a restart).
            QQC2.ToolButton {
                id: returnedMark
                objectName: "returned-" + model.taskId
                anchors.right: quickComplete.left
                anchors.rightMargin: Kirigami.Units.smallSpacing
                anchors.verticalCenter: parent.verticalCenter
                visible: model.returned
                focusPolicy: Qt.NoFocus
                display: QQC2.AbstractButton.IconOnly
                text: i18n("Back from Postponed: time to do it")
                icon.source: "qrc:/qml/icons/alarm.svg"
                QQC2.ToolTip.text: text
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: app.dismissReturned(model.taskId)
            }

            // Quick complete: shown over the end of the title while hovering.
            // Hidden with opacity: it keeps its place for the alarm icon.
            QQC2.RoundButton {
                id: quickComplete
                objectName: "complete-" + model.taskId
                anchors.right: parent.right
                anchors.rightMargin: Kirigami.Units.smallSpacing
                anchors.verticalCenter: parent.verticalCenter
                visible: scroll.mainList
                opacity: row.hovered ? 1 : 0
                enabled: row.hovered
                focusPolicy: Qt.NoFocus
                icon.name: "checkmark"
                icon.color: Kirigami.Theme.positiveTextColor
                QQC2.ToolTip.text: i18n("Complete")
                QQC2.ToolTip.visible: hovered
                QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
                onClicked: app.complete([model.taskId])
            }
        }
    }
}
