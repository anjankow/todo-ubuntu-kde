import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import org.kde.kirigami 2.20 as Kirigami

// A label drawn as a rounded chip in the label's color; filled when active.
QQC2.AbstractButton {
    id: chip

    property color chipColor
    property bool active: false

    focusPolicy: Qt.NoFocus
    leftPadding: Kirigami.Units.largeSpacing
    rightPadding: Kirigami.Units.largeSpacing
    topPadding: Kirigami.Units.smallSpacing
    bottomPadding: Kirigami.Units.smallSpacing

    contentItem: QQC2.Label {
        text: chip.text
        // readable on the label color
        color: chip.active ? ((chip.chipColor.r * 0.299 + chip.chipColor.g * 0.587 + chip.chipColor.b * 0.114) > 0.6 ? "black" : "white")
                           : Kirigami.Theme.textColor
    }
    background: Rectangle {
        radius: height / 2
        color: chip.active ? chip.chipColor : (chip.hovered ? Qt.rgba(chip.chipColor.r, chip.chipColor.g, chip.chipColor.b, 0.2) : "transparent")
        border.color: chip.chipColor
        border.width: 2
    }
}
