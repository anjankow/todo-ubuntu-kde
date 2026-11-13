import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import Qt.labs.platform 1.1 as Platform
import org.kde.kirigami 2.20 as Kirigami

Kirigami.ApplicationWindow {
    id: root

    title: i18n("Todo")
    // Wide enough for every toolbar button.
    width: Math.max(Kirigami.Units.gridUnit * 30, toolBar.implicitWidth + Kirigami.Units.largeSpacing * 4)
    height: Kirigami.Units.gridUnit * 36

    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None
    pageStack.initialPage: mainPage

    function openCard(taskId) {
        if (taskId !== "") {
            taskCard.show(taskId)
        }
    }

    // The folder picker (or a test) hands over the chosen folder.
    function chooseFolder(path) {
        if (app.changeStorageDir(path) === 1) { // AppController.FolderConflict
            conflictDialog.folder = path
            conflictDialog.open()
        }
    }

    // Keys that act on the task list. Returns true when the key was used.
    function taskKey(event) {
        var ctrl = (event.modifiers & Qt.ControlModifier) !== 0
        var list = app.currentModel
        switch (event.key) {
        case Qt.Key_Escape:
            list.currentId = ""
            return true
        case Qt.Key_Up:
            if (ctrl) {
                list.moveCurrent(-1)
            } else {
                list.moveHighlight(-1)
            }
            return true
        case Qt.Key_Down:
            if (ctrl) {
                list.moveCurrent(1)
            } else {
                list.moveHighlight(1)
            }
            return true
        case Qt.Key_Return:
        case Qt.Key_Enter:
            if (list.currentId === "") {
                return false
            }
            openCard(list.currentId)
            return true
        case Qt.Key_Delete:
            app.cancelSelected()
            return true
        case Qt.Key_D:
            return ctrl && (app.duplicateSelected() || true)
        case Qt.Key_A:
            if (ctrl) {
                list.checkAll()
            }
            return ctrl
        case Qt.Key_C:
            return ctrl && app.copySelected()
        case Qt.Key_V:
            return ctrl && app.pasteTasks()
        }
        return false
    }

    Kirigami.Page {
        id: mainPage

        padding: 0
        title: root.title

        ColumnLayout {
            anchors.fill: parent
            spacing: 0

            QQC2.ToolBar {
                id: toolBar
                objectName: "toolBar"
                Layout.fillWidth: true

                RowLayout {
                    id: toolRow
                    anchors.fill: parent
                    spacing: Kirigami.Units.smallSpacing

                    ActionButton {
                        objectName: "completeButton"
                        text: i18n("Complete")
                        icon.name: "task-complete"
                        enabled: app.canComplete
                        onClicked: app.completeChecked()
                    }
                    ActionButton {
                        objectName: "cancelButton"
                        text: i18n("Cancel task")
                        icon.name: "dialog-cancel"
                        enabled: app.canCancel
                        onClicked: app.cancelChecked()
                    }
                    ActionButton {
                        id: postponeButton
                        objectName: "postponeButton"
                        text: i18n("Postpone")
                        icon.name: "chronometer"
                        enabled: app.canPostpone
                        onClicked: postponeMenu.popup(postponeButton, 0, postponeButton.height)

                        PostponeMenu {
                            id: postponeMenu
                            objectName: "postponeMenu"
                            onPresetChosen: app.postponeChecked(preset)
                            onCustomChosen: postponeDialog.ask(function (until) {
                                app.postponeCheckedUntil(until)
                            })
                        }
                    }
                    ActionButton {
                        objectName: "restoreButton"
                        text: i18n("Restore")
                        icon.name: "edit-undo"
                        enabled: app.canRestore
                        onClicked: app.restoreChecked()
                    }
                    ActionButton {
                        objectName: "deleteButton"
                        text: i18n("Delete")
                        icon.name: "edit-delete"
                        enabled: app.canDelete
                        onClicked: app.deleteChecked()
                    }
                    Item {
                        Layout.fillWidth: true
                    }
                    ActionButton {
                        objectName: "labelsButton"
                        text: i18n("Labels…")
                        icon.name: "tag"
                        onClicked: labelsDialog.open()
                    }
                    ActionButton {
                        objectName: "storageButton"
                        text: i18n("Storage folder…")
                        icon.name: "folder"
                        onClicked: folderDialog.open()
                    }
                }
            }

            Kirigami.InlineMessage {
                objectName: "errorMessage"
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.smallSpacing
                type: Kirigami.MessageType.Error
                text: app.errorMessage
                visible: app.errorMessage !== ""
                showCloseButton: true
                onVisibleChanged: {
                    if (!visible) {
                        app.clearError()
                    }
                }
            }

            Flow {
                objectName: "filterBar"
                Layout.fillWidth: true
                Layout.margins: Kirigami.Units.largeSpacing
                Layout.bottomMargin: 0
                spacing: Kirigami.Units.smallSpacing
                visible: app.labels.length > 0

                QQC2.Label {
                    text: i18n("Labels:")
                    height: Kirigami.Units.gridUnit * 1.5
                    verticalAlignment: Text.AlignVCenter
                    opacity: 0.7
                }
                Repeater {
                    model: app.labels

                    LabelChip {
                        objectName: "filter-" + modelData.id
                        text: modelData.name
                        chipColor: modelData.color
                        active: modelData.filtering
                        onClicked: app.labelClicked(modelData.id)
                    }
                }
            }

            QQC2.TabBar {
                id: tabBar
                objectName: "tabBar"
                Layout.fillWidth: true
                Layout.topMargin: Kirigami.Units.largeSpacing
                Layout.leftMargin: Kirigami.Units.largeSpacing
                Layout.rightMargin: Kirigami.Units.largeSpacing
                currentIndex: app.currentTab
                onCurrentIndexChanged: app.currentTab = currentIndex

                QQC2.TabButton {
                    objectName: "tab-active"
                    focusPolicy: Qt.NoFocus
                    text: i18n("Tasks (%1)", app.activeModel.totalCount)
                }
                QQC2.TabButton {
                    objectName: "tab-postponed"
                    focusPolicy: Qt.NoFocus
                    text: i18n("Postponed (%1)", app.postponedModel.totalCount)
                }
                QQC2.TabButton {
                    objectName: "tab-completed"
                    focusPolicy: Qt.NoFocus
                    text: i18n("Completed (%1)", app.completedModel.totalCount)
                }
                QQC2.TabButton {
                    objectName: "tab-cancelled"
                    focusPolicy: Qt.NoFocus
                    text: i18n("Cancelled (%1)", app.cancelledModel.totalCount)
                }
            }

            StackLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: Kirigami.Units.largeSpacing
                currentIndex: app.currentTab

                ColumnLayout {
                    spacing: Kirigami.Units.largeSpacing

                    QQC2.TextArea {
                        id: newTask
                        objectName: "newTaskInput"
                        Layout.fillWidth: true
                        focus: true
                        wrapMode: TextEdit.Wrap
                        font.pointSize: Kirigami.Theme.defaultFont.pointSize + 2
                        placeholderText: i18n("New task… (Shift+Enter adds a description, Enter saves)")

                        Keys.onPressed: {
                            var enter = event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                            var ctrl = (event.modifiers & Qt.ControlModifier) !== 0
                            if (enter && (event.modifiers & Qt.ShiftModifier)) {
                                insert(cursorPosition, "\n") // the description starts on the next line
                                event.accepted = true
                            } else if (enter && text.trim() !== "") {
                                app.addTask(text)
                                clear()
                                event.accepted = true
                            } else if (enter) {
                                clear()
                                root.taskKey(event)
                                event.accepted = true
                            } else if (event.key === Qt.Key_Escape
                                       || (ctrl && (event.key === Qt.Key_Up || event.key === Qt.Key_Down))
                                       || text.length === 0) {
                                // While nothing is typed, the keys belong to the task list.
                                if (root.taskKey(event)) {
                                    event.accepted = true
                                }
                            }
                        }
                        // A cleared draft is dropped; unfinished text stays as a draft.
                        onActiveFocusChanged: {
                            if (!activeFocus && text.trim() === "") {
                                clear()
                            }
                        }
                    }

                    TaskList {
                        objectName: "activeList"
                        Layout.fillWidth: true
                        Layout.fillHeight: true
                        taskModel: app.activeModel
                        mainList: true
                        onOpenRequested: root.openCard(taskId)
                        onRowClicked: newTask.forceActiveFocus()
                    }
                }
                TaskList {
                    objectName: "postponedList"
                    taskModel: app.postponedModel
                    onOpenRequested: root.openCard(taskId)
                }
                TaskList {
                    objectName: "completedList"
                    taskModel: app.completedModel
                    onOpenRequested: root.openCard(taskId)
                }
                TaskList {
                    objectName: "cancelledList"
                    taskModel: app.cancelledModel
                    onOpenRequested: root.openCard(taskId)
                }
            }
        }

        // Keys when the input does not have the focus (other tabs).
        Keys.onPressed: {
            if (root.taskKey(event)) {
                event.accepted = true
            }
        }
    }

    TaskCard {
        id: taskCard
        objectName: "taskCard"
        postponeDialog: postponeDialog
        onClosed: newTask.forceActiveFocus()
    }

    PostponeDialog {
        id: postponeDialog
        objectName: "postponeDialog"
    }

    LabelsDialog {
        id: labelsDialog
        objectName: "labelsDialog"
    }

    QQC2.Dialog {
        id: confirmDialog
        objectName: "confirmDeleteDialog"

        property int count: 0

        anchors.centerIn: QQC2.Overlay.overlay
        modal: true
        focus: true
        title: i18n("Delete tasks")

        contentItem: QQC2.Label {
            text: i18np("Permanently delete %1 task?", "Permanently delete %1 tasks?", confirmDialog.count)
            wrapMode: Text.Wrap
        }
        footer: QQC2.DialogButtonBox {
            QQC2.Button {
                objectName: "confirmDeleteButton"
                text: i18n("Delete")
                icon.name: "edit-delete"
                QQC2.DialogButtonBox.buttonRole: QQC2.DialogButtonBox.AcceptRole
            }
            QQC2.Button {
                objectName: "rejectDeleteButton"
                text: i18n("Cancel")
                QQC2.DialogButtonBox.buttonRole: QQC2.DialogButtonBox.RejectRole
            }
        }
        onAccepted: app.confirmDelete()
    }

    Connections {
        target: app
        function onDeleteConfirmationRequested(count) {
            confirmDialog.count = count
            confirmDialog.open()
        }
    }

    QQC2.Dialog {
        id: conflictDialog
        objectName: "folderConflictDialog"

        property string folder

        anchors.centerIn: QQC2.Overlay.overlay
        modal: true
        focus: true
        title: i18n("Todo file exists")

        contentItem: QQC2.Label {
            text: i18n("The selected folder already contains a todo file.")
            wrapMode: Text.Wrap
        }
        footer: QQC2.DialogButtonBox {
            QQC2.Button {
                objectName: "conflictLoadButton"
                text: i18n("Load it")
                onClicked: {
                    app.changeStorageDir(conflictDialog.folder, 1) // LoadExisting
                    conflictDialog.close()
                }
            }
            QQC2.Button {
                objectName: "conflictOverwriteButton"
                text: i18n("Overwrite")
                onClicked: {
                    app.changeStorageDir(conflictDialog.folder, 2) // Overwrite
                    conflictDialog.close()
                }
            }
            QQC2.Button {
                objectName: "conflictCancelButton"
                text: i18n("Cancel")
                onClicked: conflictDialog.close()
            }
        }
    }

    Platform.FolderDialog {
        id: folderDialog
        title: i18n("Folder for the todo file")
        folder: "file://" + app.storageDir
        onAccepted: root.chooseFolder(app.urlToPath(folder))
    }

    Component.onCompleted: newTask.forceActiveFocus()
}
