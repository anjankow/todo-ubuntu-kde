import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami

// Lists the keyboard shortcuts.
QQC2.Dialog {
    id: dialog

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
        {keys: "Ctrl+PgUp / Ctrl+PgDown", action: i18n("Next / previous tab")},
        {keys: "Ctrl+A", action: i18n("Tick all tasks of the tab")},
        {keys: "Ctrl+D", action: i18n("Duplicate the selected tasks")},
        {keys: "Delete", action: i18n("Cancel the selected tasks")},
        {keys: "Shift+Delete", action: i18n("Delete the selected tasks (several ask first)")},
        {keys: "Ctrl+Q", action: i18n("Quit")}
    ]

    anchors.centerIn: QQC2.Overlay.overlay
    width: Math.min(parent.width - Kirigami.Units.gridUnit * 2, Kirigami.Units.gridUnit * 30)
    modal: true
    focus: true
    title: i18n("Keyboard shortcuts")
    standardButtons: QQC2.Dialog.Close

    contentItem: GridLayout {
        columns: 2
        columnSpacing: Kirigami.Units.largeSpacing
        rowSpacing: Kirigami.Units.smallSpacing

        Repeater {
            model: dialog.shortcuts.length * 2

            QQC2.Label {
                readonly property var entry: dialog.shortcuts[Math.floor(index / 2)]
                readonly property bool isKey: index % 2 === 0

                objectName: isKey ? "shortcutKeys-" + Math.floor(index / 2) : "shortcutAction-" + Math.floor(index / 2)
                Layout.fillWidth: !isKey
                Layout.alignment: Qt.AlignTop
                text: isKey ? entry.keys : entry.action
                font.bold: isKey
                wrapMode: isKey ? Text.NoWrap : Text.Wrap
            }
        }
    }
}
