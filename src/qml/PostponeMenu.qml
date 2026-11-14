import QtQuick 2.15
import QtQuick.Controls 2.15 as QQC2

QQC2.Menu {
    signal presetChosen(int preset) // AppController.Preset
    signal customChosen()

    QQC2.MenuItem {
        objectName: "postponeTomorrow"
        text: i18n("Until tomorrow")
        onTriggered: presetChosen(0)
    }
    QQC2.MenuItem {
        objectName: "postponeThreeDays"
        text: i18n("3 days")
        onTriggered: presetChosen(1)
    }
    QQC2.MenuItem {
        objectName: "postponeOneWeek"
        text: i18n("1 week")
        onTriggered: presetChosen(2)
    }
    QQC2.MenuItem {
        objectName: "postponeOneMonth"
        text: i18n("1 month")
        onTriggered: presetChosen(3)
    }
    QQC2.MenuSeparator {}
    QQC2.MenuItem {
        objectName: "postponeCustom"
        text: i18n("Custom")
        onTriggered: customChosen()
    }
}
