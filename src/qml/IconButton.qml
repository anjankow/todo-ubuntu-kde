import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import org.kde.kirigami 2.20 as Kirigami

// A button showing only its icon; its text is the hint shown on hover.
// It never takes the keyboard focus, which stays with the task list or
// the new-task input.
QQC2.ToolButton {
    id: button

    display: QQC2.AbstractButton.IconOnly
    focusPolicy: Qt.NoFocus
    QQC2.ToolTip.text: text
    QQC2.ToolTip.visible: hovered
    QQC2.ToolTip.delay: Kirigami.Units.toolTipDelay
}
