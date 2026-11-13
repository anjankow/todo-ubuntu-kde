import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2
import QtQuick.Layouts 1.15
import org.kde.kirigami 2.20 as Kirigami

// Asks for the date and time until which tasks are postponed.
QQC2.Dialog {
    id: dialog

    property var whenChosen: null // function (date)

    function ask(callback) {
        var suggestion = app.suggestedPostponeTime()
        year.value = suggestion.getFullYear()
        month.value = suggestion.getMonth() + 1
        day.value = suggestion.getDate()
        hour.value = suggestion.getHours()
        minute.value = suggestion.getMinutes()
        whenChosen = callback
        open()
    }

    anchors.centerIn: QQC2.Overlay.overlay
    modal: true
    focus: true
    title: i18n("Postpone until")
    standardButtons: QQC2.Dialog.Ok | QQC2.Dialog.Cancel
    onAccepted: {
        if (whenChosen) {
            whenChosen(new Date(year.value, month.value - 1, day.value, hour.value, minute.value))
        }
    }

    contentItem: Item {
        implicitWidth: grid.implicitWidth
        implicitHeight: grid.implicitHeight

    GridLayout {
        id: grid
        anchors.fill: parent
        columns: 4
        columnSpacing: Kirigami.Units.smallSpacing

        QQC2.Label {
            text: i18n("Date:")
        }
        QQC2.SpinBox {
            id: year
            objectName: "postponeYear"
            from: 2000
            to: 2999
            textFromValue: function (value) {
                return value
            }
        }
        QQC2.SpinBox {
            id: month
            objectName: "postponeMonth"
            from: 1
            to: 12
        }
        QQC2.SpinBox {
            id: day
            objectName: "postponeDay"
            from: 1
            to: 31
        }
        QQC2.Label {
            text: i18n("Time:")
        }
        QQC2.SpinBox {
            id: hour
            objectName: "postponeHour"
            from: 0
            to: 23
        }
        QQC2.SpinBox {
            id: minute
            objectName: "postponeMinute"
            from: 0
            to: 59
        }
    }
    }
}
