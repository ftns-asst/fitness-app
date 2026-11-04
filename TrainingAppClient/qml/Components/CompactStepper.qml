import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Компактный степпер «− значение +» для мини-окон: крупные круглые кнопки
// (touchMin), значение по центру, подпись сверху. Удержание кнопки повторяет
// шаг; значение при смене коротко «подпрыгивает» — изменение видно без чтения.
ColumnLayout {
    id: stepper

    property int decimals: 0
    property real from: 0
    property string label: ""
    property real step: 1
    property string suffix: ""
    property real to: 100
    property real value: 0

    signal edited(real value)

    function change(delta) {
        const next = Math.min(stepper.to, Math.max(stepper.from, Math.round((stepper.value + delta) * 1000) / 1000));

        if (next !== stepper.value)
            stepper.edited(next);
    }

    spacing: 4

    Label {
        Layout.fillWidth: true
        color: Theme.textSecondary
        font: Typography.captionStrong
        horizontalAlignment: Text.AlignHCenter
        text: stepper.label
        visible: text.length > 0
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: Spacing.itemGap

        StepperButton {
            glyph: "\u2212"
            step: -stepper.step

            onActivated: stepper.change(-stepper.step)
        }
        Label {
            id: valueLabel

            Layout.fillWidth: true
            Layout.minimumWidth: 0
            color: Theme.textPrimary
            elide: Text.ElideRight
            font: Typography.sectionTitle
            horizontalAlignment: Text.AlignHCenter
            text: Number(stepper.value).toFixed(stepper.decimals) + stepper.suffix

            // «Подпрыгивание» при смене значения.
            SequentialAnimation {
                id: bump

                NumberAnimation {
                    duration: 70
                    easing.type: Easing.OutCubic
                    property: "scale"
                    target: valueLabel
                    to: 1.14
                }
                NumberAnimation {
                    duration: 110
                    easing.type: Easing.OutBack
                    property: "scale"
                    target: valueLabel
                    to: 1.0
                }
            }
            onTextChanged: bump.restart()
        }
        StepperButton {
            glyph: "+"
            step: stepper.step

            onActivated: stepper.change(stepper.step)
        }
    }

    // Круглая кнопка шага с автоповтором при удержании.
    component StepperButton: Rectangle {
        id: btn

        property string glyph: ""
        property real step: 1

        signal activated

        Layout.preferredHeight: Theme.touchMin
        Layout.preferredWidth: Theme.touchMin
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

            onPressAndHold: repeat.start()
            onPressed: btn.activated()
            onReleased: repeat.stop()
            onCanceled: repeat.stop()
        }
        Timer {
            id: repeat

            interval: 110
            repeat: true

            onTriggered: btn.activated()
        }
    }
}
