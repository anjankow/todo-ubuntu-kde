import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import org.kde.kirigami 2.20 as Kirigami

// A button showing only its icon; its text is the hint shown on hover.
// It never takes the keyboard focus, which stays with the task list or
// the new-task input.
QQC2.ToolButton {
    id: button

    // A bundled icon (src/qml/icons/<iconFile>.svg). With `themed`, the
    // "-on-dark" variant is used while the colour scheme is dark.
    property string iconFile: ""
    property bool themed: false
    // Dark colour scheme: dark background. (The text colour cannot tell:
    // a disabled button's text is a middle gray in both schemes.)
    property bool darkTheme: Kirigami.Theme.backgroundColor.hslLightness < 0.5

    display: QQC2.AbstractButton.IconOnly
    focusPolicy: Qt.NoFocus
    icon.source: iconFile === "" ? "" : "qrc:/qml/icons/" + iconFile + (themed && darkTheme ? "-on-dark" : "") + ".svg"
    // Bundled icons keep their colours, so a disabled button shows it this way.
    opacity: enabled ? 1 : 0.4
    QQC2.ToolTip.text: text
    QQC2.ToolTip.visible: hovered
    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
}
