import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami

// Manages the user-defined labels: a name plus one of the 12 palette colors.
QQC2.Dialog {
    id: dialog

    property string selectedId: ""
    property int colorIndex: 0

    function select(label) {
        selectedId = label.id
        nameField.text = label.name
        colorIndex = label.colorIndex
    }

    anchors.centerIn: QQC2.Overlay.overlay
    width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 22)
    modal: true
    focus: true
    title: i18n("Labels")
    onOpened: {
        selectedId = ""
        nameField.text = ""
        nameField.forceActiveFocus()
    }

    contentItem: ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        Flow {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: app.labels

                LabelChip {
                    objectName: "label-" + modelData.id
                    text: modelData.name
                    chipColor: modelData.color
                    active: dialog.selectedId === modelData.id
                    onClicked: dialog.select(modelData)
                }
            }
        }
        QQC2.TextField {
            id: nameField
            objectName: "labelName"
            Layout.fillWidth: true
            placeholderText: i18n("Label name")
        }
        Grid {
            columns: 6
            spacing: Kirigami.Units.smallSpacing

            Repeater {
                model: app.palette

                Rectangle {
                    objectName: "color-" + index
                    width: Kirigami.Units.gridUnit * 1.6
                    height: width
                    radius: Kirigami.Units.smallSpacing
                    color: modelData
                    border.width: dialog.colorIndex === index ? 3 : 0
                    border.color: Kirigami.Theme.textColor

                    MouseArea {
                        anchors.fill: parent
                        onClicked: dialog.colorIndex = index
                    }
                }
            }
        }
        RowLayout {
            QQC2.Button {
                objectName: "labelAdd"
                text: i18n("Add")
                icon.name: "list-add"
                onClicked: {
                    if (app.addLabel(nameField.text, dialog.colorIndex)) {
                        nameField.text = ""
                        dialog.selectedId = ""
                    }
                }
            }
            QQC2.Button {
                objectName: "labelSave"
                text: i18n("Save changes")
                icon.name: "document-save"
                enabled: dialog.selectedId !== ""
                onClicked: app.updateLabel(dialog.selectedId, nameField.text, dialog.colorIndex)
            }
            QQC2.Button {
                objectName: "labelDelete"
                text: i18n("Delete")
                icon.name: "edit-delete"
                enabled: dialog.selectedId !== ""
                onClicked: {
                    app.removeLabel(dialog.selectedId)
                    dialog.selectedId = ""
                    nameField.text = ""
                }
            }
        }
    }

    footer: QQC2.DialogButtonBox {
        QQC2.Button {
            objectName: "labelsClose"
            text: i18n("Close")
            onClicked: dialog.close()
        }
    }
}
