import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import org.kde.kirigami 2.20 as Kirigami

// Shows a shortcut. A click makes it listen: the next key or combination
// pressed is handed over in `captured` (Esc gives up, a bare modifier key
// is ignored).
QQC2.Button {
    id: field

    property string keys: ""
    property bool capturing: false
    signal captured(string keys)

    text: capturing ? i18n("Press a key") : (keys === "" ? i18n("None") : keys)
    font.bold: !capturing && keys !== ""
    opacity: !capturing && keys === "" ? 0.7 : 1
    highlighted: capturing
    focusPolicy: Qt.StrongFocus

    onClicked: {
        capturing = true
        forceActiveFocus()
    }
    onActiveFocusChanged: {
        if (!activeFocus) {
            capturing = false
        }
    }

    // While listening, application shortcuts (Ctrl+Q ...) must not fire.
    Keys.onShortcutOverride: {
        if (capturing) {
            event.accepted = true
        }
    }
    Keys.onPressed: {
        if (!capturing) {
            return
        }
        event.accepted = true
        if (event.key === Qt.Key_Escape) {
            capturing = false
            return
        }
        var text = app.keyText(event.key, event.modifiers)
        if (text !== "") {
            capturing = false
            captured(text)
        }
    }
}
