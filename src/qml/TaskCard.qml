import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami

// The task card: shows and edits title, description and labels of a task.
// Complete, Postpone and Cancel task save the edits, change the state at
// once and close the card.
QQC2.Dialog {
    id: card

    property string taskId
    property string taskState
    property var labelIds: []
    property var postponeDialog

    function show(id) {
        var task = app.task(id)
        if (task.id === undefined) {
            return
        }
        taskId = id
        taskState = task.state
        labelIds = task.labelIds
        titleField.text = task.title
        descriptionArea.text = task.description
        open()
        titleField.forceActiveFocus()
    }

    // Returns false when the task cannot be saved (it needs a title).
    function save() {
        if (titleField.text.trim() === "") {
            titleField.forceActiveFocus()
            return false
        }
        return app.updateTask(taskId, titleField.text, descriptionArea.text, labelIds)
    }

    function saveAndClose() {
        if (save()) {
            close()
        }
    }

    function setLabel(id, on) {
        var ids = labelIds.filter(function (other) {
            return other !== id
        })
        if (on) {
            ids.push(id)
        }
        labelIds = ids
    }

    anchors.centerIn: QQC2.Overlay.overlay
    width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 28)
    modal: true
    focus: true
    title: i18n("Task")

    contentItem: ColumnLayout {
        spacing: Kirigami.Units.largeSpacing

        QQC2.TextField {
            id: titleField
            objectName: "cardTitle"
            Layout.fillWidth: true
            placeholderText: i18n("Title")
            font.pointSize: Kirigami.Theme.defaultFont.pointSize + 2
            onAccepted: card.saveAndClose()
        }
        QQC2.ScrollView {
            Layout.fillWidth: true
            Layout.preferredHeight: Kirigami.Units.gridUnit * 8

            QQC2.TextArea {
                id: descriptionArea
                objectName: "cardDescription"
                placeholderText: i18n("Description")
                wrapMode: TextEdit.Wrap
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: Kirigami.Units.smallSpacing
            visible: app.labels.length > 0

            Repeater {
                model: app.labels

                LabelChip {
                    objectName: "cardLabel-" + modelData.id
                    text: modelData.name
                    chipColor: modelData.color
                    active: card.labelIds.indexOf(modelData.id) >= 0
                    onClicked: card.setLabel(modelData.id, !active)
                }
            }
        }
        RowLayout {
            spacing: Kirigami.Units.smallSpacing

            IconButton {
                objectName: "cardComplete"
                text: i18n("Complete")
                iconFile: "complete"
                visible: card.taskState !== "completed"
                onClicked: {
                    if (card.save()) {
                        app.complete([card.taskId])
                        card.close()
                    }
                }
            }
            IconButton {
                id: cardPostponeButton
                objectName: "cardPostpone"
                text: i18n("Postpone")
                iconFile: "postpone"
                themed: true
                visible: card.taskState === "active" || card.taskState === "postponed"
                onClicked: cardPostponeMenu.popup(cardPostponeButton, 0, cardPostponeButton.height)

                PostponeMenu {
                    id: cardPostponeMenu
                    objectName: "cardPostponeMenu"
                    onPresetChosen: {
                        if (card.save()) {
                            app.postpone([card.taskId], preset)
                            card.close()
                        }
                    }
                    onCustomChosen: {
                        if (card.save()) {
                            card.postponeDialog.ask(function (until) {
                                app.postponeUntil([card.taskId], until)
                                card.close()
                            })
                        }
                    }
                }
            }
            IconButton {
                objectName: "cardCancelTask"
                text: i18n("Cancel task")
                iconFile: "cancel"
                themed: true
                visible: card.taskState !== "cancelled"
                onClicked: {
                    if (card.save()) {
                        app.cancel([card.taskId])
                        card.close()
                    }
                }
            }
        }
    }

    footer: QQC2.DialogButtonBox {
        QQC2.Button {
            objectName: "cardSave"
            text: i18n("Save")
            icon.name: "document-save"
            onClicked: card.saveAndClose()
        }
        QQC2.Button {
            objectName: "cardClose"
            text: i18n("Close")
            onClicked: card.close()
        }
    }
}
