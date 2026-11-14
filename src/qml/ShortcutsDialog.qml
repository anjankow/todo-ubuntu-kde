import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import org.kde.kirigami 2.20 as Kirigami

// The keyboard shortcuts: four of them can be changed (click one and press
// the keys you want), the others are listed.
QQC2.Dialog {
    id: dialog

    // The action whose last change was refused, and why.
    property string errorAction: ""
    property string errorText: ""

    // "Selected" = the ticked tasks, or the highlighted one when none is ticked.
    readonly property var shortcuts: [
        {keys: "Shift+Enter", action: i18n("Nothing highlighted: start typing a new task. Task highlighted: open its card")},
        {keys: "Enter", action: i18n("In the new-task input: create the task. Task highlighted: open its card")},
        {keys: "Shift+Enter", action: i18n("In the new-task input: new line for the description")},
        {keys: "Esc", action: i18n("Leave the new-task input and remove the highlight")},
        {keys: "Esc Esc", action: i18n("Pressed twice quickly: untick all tasks")},
        {keys: "Up / Down", action: i18n("Move the highlight")},
        {keys: "Shift+Up / Shift+Down", action: i18n("Move the highlight and tick the tasks it passes")},
        {keys: "Space", action: i18n("Tick or untick the highlighted task")},
        {keys: "Ctrl+Up / Ctrl+Down", action: i18n("Move the highlighted task up or down in the list")},
        {keys: "Ctrl+PgDown / Ctrl+PgUp", action: i18n("Next / previous tab")},
        {keys: "Ctrl+A", action: i18n("Tick all tasks of the tab")},
        {keys: "Ctrl+D", action: i18n("Duplicate the selected tasks")},
        {keys: "Shift+Delete", action: i18n("Delete the selected tasks (several ask first)")},
        {keys: "Ctrl+Q", action: i18n("Quit")}
    ]

    function change(action, keys) {
        errorAction = action
        errorText = keys === "" ? app.setShortcut(action, "") : app.setShortcut(action, keys)
        if (errorText === "") {
            errorAction = ""
        }
    }

    function reset(action) {
        errorAction = action
        errorText = app.resetShortcut(action)
        if (errorText === "") {
            errorAction = ""
        }
    }

    anchors.centerIn: QQC2.Overlay.overlay
    width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 32)
    // As tall as the rows need, but never taller than the window; the rows
    // scroll when it is smaller. The height comes from the rows and the
    // rows' width from the dialog's width, so nothing depends on itself.
    height: Math.min(parent.height - Kirigami.Units.gridUnit * 2,
                     rows.height + topPadding + bottomPadding + implicitHeaderHeight + implicitFooterHeight)
    modal: true
    focus: true
    title: i18n("Keyboard shortcuts")
    standardButtons: QQC2.Dialog.Close
    onOpened: {
        errorAction = ""
        errorText = ""
    }

    // Plain items with explicit widths instead of layouts: a layout of
    // wrapping labels asks for its height while it hands out the widths,
    // which is a binding loop whenever the dialog's width changes (the
    // window is resized).
    contentItem: Flickable {
        id: scroll

        // Room for the scroll bar, so that it never changes the width of the rows.
        readonly property int rowWidth: width - Kirigami.Units.gridUnit
        readonly property int keyWidth: Kirigami.Units.gridUnit * 11

        contentWidth: width
        contentHeight: rows.height
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        QQC2.ScrollBar.vertical: QQC2.ScrollBar {
            policy: QQC2.ScrollBar.AsNeeded
        }

        Column {
            id: rows
            width: scroll.rowWidth
            spacing: Kirigami.Units.smallSpacing

            QQC2.Label {
                width: rows.width
                text: i18n("Click a shortcut and press the keys you want. Esc gives up.")
                wrapMode: Text.Wrap
                opacity: 0.8
            }

            Repeater {
                model: app.configurableShortcuts

                Column {
                    width: rows.width
                    spacing: 2

                    Item {
                        width: parent.width
                        height: field.implicitHeight

                        QQC2.Label {
                            objectName: "shortcutName-" + modelData.action
                            anchors.verticalCenter: parent.verticalCenter
                            width: scroll.keyWidth
                            text: modelData.name
                            elide: Text.ElideRight
                        }
                        ShortcutField {
                            id: field
                            objectName: "shortcutField-" + modelData.action
                            x: scroll.keyWidth + Kirigami.Units.largeSpacing
                            width: parent.width - x - clear.width - restore.width - Kirigami.Units.smallSpacing * 2
                            keys: modelData.keys
                            onCaptured: dialog.change(modelData.action, keys)
                        }
                        IconButton {
                            id: clear
                            objectName: "shortcutClear-" + modelData.action
                            anchors.right: restore.left
                            anchors.rightMargin: Kirigami.Units.smallSpacing
                            anchors.verticalCenter: parent.verticalCenter
                            text: i18n("Remove the shortcut")
                            icon.name: "edit-clear"
                            enabled: modelData.keys !== ""
                            onClicked: dialog.change(modelData.action, "")
                        }
                        IconButton {
                            id: restore
                            objectName: "shortcutReset-" + modelData.action
                            anchors.right: parent.right
                            anchors.verticalCenter: parent.verticalCenter
                            text: i18n("Back to the default (%1)", modelData.defaultKeys)
                            icon.name: "edit-undo"
                            enabled: !modelData.isDefault
                            onClicked: dialog.reset(modelData.action)
                        }
                    }
                    // The label sits in an item that takes its height: a label
                    // whose own height follows its implicit height loops.
                    Item {
                        width: parent.width
                        height: message.visible ? message.implicitHeight : 0

                        QQC2.Label {
                            id: message
                            objectName: "shortcutError-" + modelData.action
                            x: scroll.keyWidth + Kirigami.Units.largeSpacing
                            width: parent.width - x
                            visible: dialog.errorAction === modelData.action && dialog.errorText !== ""
                            text: dialog.errorText
                            color: Kirigami.Theme.negativeTextColor
                            wrapMode: Text.Wrap
                            // The style binds this to the line count, which loops with the height.
                            verticalAlignment: Text.AlignTop
                        }
                    }
                }
            }

            Kirigami.Separator {
                width: rows.width
            }

            Repeater {
                model: dialog.shortcuts

                Item {
                    width: rows.width
                    height: Math.max(keys.implicitHeight, action.implicitHeight)

                    QQC2.Label {
                        id: keys
                        objectName: "shortcutKeys-" + index
                        width: scroll.keyWidth
                        text: modelData.keys
                        font.bold: true
                        wrapMode: Text.Wrap
                    }
                    QQC2.Label {
                        id: action
                        objectName: "shortcutAction-" + index
                        x: scroll.keyWidth + Kirigami.Units.largeSpacing
                        width: parent.width - x
                        text: modelData.action
                        wrapMode: Text.Wrap
                    }
                }
            }
        }
    }
}
