import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Круглая кнопка шага «−/+» с автоповтором при удержании (мини-окна ввода чисел).
Rectangle {
    id: btn

    property string glyph: ""

    signal activated

    Layout.preferredHeight: Theme.buttonPrimary
    Layout.preferredWidth: Theme.buttonPrimary
    border.color: Theme.border
    border.width: Theme.borderWidth
    color: area.pressed ? Theme.accentTint : Theme.surfaceMuted
    radius: width / 2
    scale: area.pressed ? 0.92 : 1.0

    Behavior on color {
        ColorAnimation {
            duration: Theme.pressFeedbackDuration
        }
    }
    Behavior on scale {
        NumberAnimation {
            duration: 90
            easing.type: Easing.OutCubic
        }
    }

    Label {
        anchors.centerIn: parent
        color: area.pressed ? Theme.accent : Theme.textPrimary
        font: Typography.sectionTitle
        text: btn.glyph
    }
    MouseArea {
        id: area

        anchors.fill: parent

        onCanceled: repeat.stop()
        onPressAndHold: repeat.start()
        onPressed: btn.activated()
        onReleased: repeat.stop()
    }
    Timer {
        id: repeat

        interval: 110
        repeat: true

        onTriggered: btn.activated()
    }
}
