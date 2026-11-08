import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Базовое поле ввода. Подпись, ошибка и единицы остаются у формы-владельца.
TextField {
    id: control
    property bool invalid: false
    property color surfaceColor: Theme.surfaceMuted
    property color focusBorderColor: Theme.accent
    property color hoverBorderColor: Theme.border
    property real focusBorderWidth: Theme.borderWidth
    property real errorBorderWidth: Theme.borderWidth
    property int borderAnimationDuration: 0

    color: Theme.textPrimary
    font: Typography.body
    placeholderTextColor: Theme.textMuted
    verticalAlignment: Text.AlignVCenter
    background: Rectangle {
        color: control.surfaceColor
        radius: Theme.radiusMd
        border.color: control.invalid ? Theme.negative : control.activeFocus ? control.focusBorderColor : control.hovered ? control.hoverBorderColor : Theme.border
        border.width: control.invalid ? control.errorBorderWidth : control.activeFocus ? control.focusBorderWidth : Theme.borderWidth
        Behavior on border.color {
            ColorAnimation {
                duration: control.borderAnimationDuration
                easing.type: Easing.OutCubic
            }
        }
    }
}
