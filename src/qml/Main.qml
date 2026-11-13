import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import Qt.labs.platform 1.1 as Platform
import org.kde.kirigami 2.20 as Kirigami

Kirigami.ApplicationWindow {
    id: root

    title: i18n("Todo")
    width: Kirigami.Units.gridUnit * 30
    height: Kirigami.Units.gridUnit * 36

    pageStack.globalToolBar.style: Kirigami.ApplicationHeaderStyle.None
    pageStack.initialPage: mainPage

    function showMainWindow() {
        trayPopup.hide()
        root.show()
        root.raise()
        root.requestActivate()
    }

    // With the tray icon the application keeps running without its window.
    onClosing: {
        if (app.keepRunningInTray) {
            close.accepted = false
            root.hide()
        }
    }

    Shortcut {
        sequence: StandardKey.Quit
        context: Qt.ApplicationShortcut
        onActivated: Qt.quit()
    }

    TrayPopup {
        id: trayPopup
        objectName: "trayPopup"
        onOpenRequested: root.showMainWindow()
    }

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

    // Escape pressed twice in a row within this time unticks all tasks.
    readonly property int doubleEscapeInterval: 500
    property double lastEscape: 0

    function focusList() {
        listKeys.forceActiveFocus()
    }

    // Shift+Enter with nothing highlighted: the only key way into the input.
    function focusInput() {
        app.currentTab = 0 // the input is on the Tasks tab
        newTask.forceActiveFocus()
    }

    // Keys that act on the task list. Returns true when the key was used.
    function taskKey(event) {
        var ctrl = (event.modifiers & Qt.ControlModifier) !== 0
        var shift = (event.modifiers & Qt.ShiftModifier) !== 0
        var list = app.currentModel
        switch (event.key) {
        case Qt.Key_Escape:
            var now = Date.now()
            if (now - lastEscape <= doubleEscapeInterval) {
                list.clearChecked()
                lastEscape = 0
            } else {
                lastEscape = now
            }
            // Leaves the input (the keys then go to the task list) and drops the highlight.
            list.currentId = ""
            focusList()
            return true
        case Qt.Key_Up:
        case Qt.Key_Down:
            var delta = event.key === Qt.Key_Up ? -1 : 1
            if (ctrl) {
                list.moveCurrent(delta)
            } else if (shift) {
                list.extendSelection(delta)
            } else {
                list.moveHighlight(delta)
            }
            focusList()
            return true
        case Qt.Key_Return:
        case Qt.Key_Enter:
            if (list.currentId !== "") {
                openCard(list.currentId)
            } else if (shift) {
                focusInput()
            }
            return true
        case Qt.Key_PageUp:
        case Qt.Key_PageDown:
            if (!ctrl) {
                return false
            }
            // Ctrl+PgDown: next tab, Ctrl+PgUp: previous tab, both wrap around.
            app.currentTab = (app.currentTab + (event.key === Qt.Key_PageDown ? 1 : 3)) % 4
            focusList()
            return true
        case Qt.Key_Space:
            list.toggleCurrentChecked()
            return true
        case Qt.Key_Delete:
            if (shift) {
                app.deleteSelected()
            } else {
                app.cancelSelected()
            }
            return true
        case Qt.Key_D:
            if (ctrl) {
                app.duplicateSelected()
            }
            return ctrl
        case Qt.Key_A:
            if (ctrl) {
                list.checkAll()
            }
            return ctrl
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

                    Item {
                        Layout.fillWidth: true
                    }
                    IconButton {
                        id: settingsButton
                        objectName: "settingsButton"
                        text: i18n("Settings")
                        icon.name: "configure"
                        onClicked: settingsMenu.popup(settingsButton, 0, settingsButton.height)

                        QQC2.Menu {
                            id: settingsMenu
                            objectName: "settingsMenu"

                            QQC2.MenuItem {
                                objectName: "settingsLabels"
                                text: i18n("Labels…")
                                icon.name: "tag"
                                onTriggered: labelsDialog.open()
                            }
                            QQC2.MenuItem {
                                objectName: "settingsStorage"
                                text: i18n("Storage folder…")
                                icon.name: "folder"
                                onTriggered: folderDialog.open()
                            }
                            QQC2.MenuItem {
                                objectName: "settingsShortcuts"
                                text: i18n("Keyboard shortcuts…")
                                icon.name: "input-keyboard"
                                onTriggered: shortcutsDialog.open()
                            }
                        }
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

            ColumnLayout {
                Layout.fillWidth: true
                Layout.fillHeight: true
                Layout.margins: Kirigami.Units.largeSpacing
                spacing: Kirigami.Units.largeSpacing

                QQC2.TextArea {
                    id: newTask
                    objectName: "newTaskInput"
                    Layout.fillWidth: true
                    // The input belongs to the Tasks tab.
                    visible: app.currentTab === 0
                    wrapMode: TextEdit.Wrap
                    font.pointSize: Kirigami.Theme.defaultFont.pointSize + 2
                    placeholderText: i18n("New task (press Shift+Enter to focus)")

                    Keys.onPressed: {
                        var enter = event.key === Qt.Key_Return || event.key === Qt.Key_Enter
                        var ctrl = (event.modifiers & Qt.ControlModifier) !== 0
                        if (enter && (event.modifiers & Qt.ShiftModifier)) {
                            insert(cursorPosition, "\n") // the description starts on the next line
                            event.accepted = true
                        } else if (enter) {
                            if (text.trim() !== "") {
                                app.addTask(text) // the input keeps the focus for the next task
                            }
                            clear()
                            event.accepted = true
                        } else if (event.key === Qt.Key_Escape
                                   || (ctrl && (event.key === Qt.Key_Up || event.key === Qt.Key_Down
                                                || event.key === Qt.Key_PageUp || event.key === Qt.Key_PageDown))
                                   || (text.length === 0 && (event.key === Qt.Key_Up || event.key === Qt.Key_Down))) {
                            // These leave the input for the task list.
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

                // The state buttons: below the input, above the list.
                RowLayout {
                    id: stateRow
                    objectName: "stateRow"
                    spacing: Kirigami.Units.smallSpacing

                    IconButton {
                        objectName: "completeButton"
                        text: i18n("Complete")
                        iconFile: "complete"
                        // Each tab shows only the buttons that apply to it.
                        visible: app.currentTab !== 2 // not on Completed
                        enabled: app.canComplete
                        onClicked: app.completeSelected()
                    }
                    IconButton {
                        id: postponeButton
                        objectName: "postponeButton"
                        text: i18n("Postpone")
                        iconFile: "postpone"
                        themed: true
                        visible: app.currentTab === 0 || app.currentTab === 1 // Tasks, Postponed
                        enabled: app.canPostpone
                        onClicked: postponeMenu.popup(postponeButton, 0, postponeButton.height)

                        PostponeMenu {
                            id: postponeMenu
                            objectName: "postponeMenu"
                            onPresetChosen: app.postponeSelected(preset)
                            onCustomChosen: postponeDialog.ask(function (until) {
                                app.postponeSelectedUntil(until)
                            })
                        }
                    }
                    IconButton {
                        objectName: "cancelButton"
                        text: i18n("Cancel task")
                        iconFile: "cancel"
                        themed: true
                        visible: app.currentTab !== 3 // not on Cancelled
                        enabled: app.canCancel
                        onClicked: app.cancelSelected()
                    }
                    IconButton {
                        objectName: "deleteButton"
                        text: i18n("Delete")
                        iconFile: "delete"
                        enabled: app.canDelete
                        onClicked: app.deleteSelected()
                    }
                    IconButton {
                        objectName: "restoreButton"
                        text: i18n("Restore")
                        icon.name: "edit-undo"
                        visible: app.currentTab !== 0 // not on Tasks
                        enabled: app.canRestore
                        onClicked: app.restoreSelected()
                    }
                }

                StackLayout {
                    Layout.fillWidth: true
                    Layout.fillHeight: true
                    currentIndex: app.currentTab

                    TaskList {
                        objectName: "activeList"
                        taskModel: app.activeModel
                        mainList: true
                        onOpenRequested: root.openCard(taskId)
                        onRowClicked: root.focusList()
                    }
                    TaskList {
                        objectName: "postponedList"
                        taskModel: app.postponedModel
                        onOpenRequested: root.openCard(taskId)
                        onRowClicked: root.focusList()
                    }
                    TaskList {
                        objectName: "completedList"
                        taskModel: app.completedModel
                        onOpenRequested: root.openCard(taskId)
                        onRowClicked: root.focusList()
                    }
                    TaskList {
                        objectName: "cancelledList"
                        taskModel: app.cancelledModel
                        onOpenRequested: root.openCard(taskId)
                        onRowClicked: root.focusList()
                    }
                }
            }
        }

        // Has the keyboard focus whenever the input does not: the keys then
        // go to the task list.
        Item {
            id: listKeys
            objectName: "listKeys"

            Keys.onPressed: {
                if (root.taskKey(event)) {
                    event.accepted = true
                }
            }
        }
    }

    TaskCard {
        id: taskCard
        objectName: "taskCard"
        postponeDialog: postponeDialog
        onClosed: root.focusList()
    }

    PostponeDialog {
        id: postponeDialog
        objectName: "postponeDialog"
    }

    LabelsDialog {
        id: labelsDialog
        objectName: "labelsDialog"
        onClosed: root.focusList()
    }

    ShortcutsDialog {
        id: shortcutsDialog
        objectName: "shortcutsDialog"
        onClosed: root.focusList()
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

    // At start nothing is highlighted and the keys go to the task list.
    Component.onCompleted: focusList()
}
