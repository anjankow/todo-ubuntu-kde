import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2

// A toolbar button. It never takes the keyboard focus, which stays in the
// new-task input.
QQC2.ToolButton {
    display: QQC2.AbstractButton.TextBesideIcon
    focusPolicy: Qt.NoFocus
}
