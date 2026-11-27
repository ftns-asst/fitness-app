import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Выпадающий список в стиле приложения. Штатный ComboBox рисуется текущим
// стилем Qt (на.Basic — серые квадратные кнопки), поэтому его геометрия,
// скругления и цвета не совпадали с AppTextField и выбивались из общей
// картины. Здесь фон, рамка и высота те же, что у поля формы.
ComboBox {
    id: control

    // Значение выбора: QML-код читает currentValue, а не индекс.
    property var items: []

    hoverEnabled: !Theme.touchPlatform

    implicitHeight: Theme.buttonPrimary
    padding: Spacing.cardPadding

    background: Rectangle {
        border.color: control.activeFocus || control.popup.visible ? Theme.accent : control.hovered ? Theme.textMuted : Theme.border
        border.width: control.activeFocus ? 2 : Theme.borderWidth
        color: control.down ? Theme.accentSurface : Theme.surfaceMuted

        Behavior on color {
            ColorAnimation {
                duration: Theme.pressFeedbackDuration
                easing.type: Easing.OutCubic
            }
        }
        radius: Theme.radiusMd

        Behavior on border.color {
            ColorAnimation {
                duration: Theme.pressFeedbackDuration
                easing.type: Easing.OutCubic
            }
        }
    }
    contentItem: Label {
        leftPadding: control.leftPadding
        // Отступ справа оставлен под шеврон, иначе длинный текст налезал бы
        // на него.
        rightPadding: control.rightPadding + control.indicator.width
        color: Theme.textPrimary
        font: Typography.body
        text: control.displayText
        verticalAlignment: Text.AlignVCenter
    }
    indicator: AppIcon {
        name: "chevronDown"
        iconColor: Theme.textMuted
        rotation: control.popup.visible ? 180 : 0

        Behavior on rotation {
            NumberAnimation {
                duration: Theme.selectionDuration
                easing.type: Easing.OutCubic
            }
        }
        x: control.width - width - control.rightPadding
        y: (control.height - height) / 2
    }
    delegate: ItemDelegate {
        id: option

        required property int index
        required property var modelData

        hoverEnabled: !Theme.touchPlatform

        width: control.width
        height: Theme.touchMin
        highlighted: control.highlightedIndex === option.index

        background: Rectangle {
            color: option.down || option.highlighted ? Theme.accentTint : option.hovered ? Theme.surfaceMuted : "transparent"

            Behavior on color {
                ColorAnimation {
                    duration: Theme.pressFeedbackDuration
                    easing.type: Easing.OutCubic
                }
            }
        }
        contentItem: Label {
            color: Theme.textPrimary
            font: Typography.body
            text: modelData !== undefined && modelData.title !== undefined ? modelData.title : modelData
            verticalAlignment: Text.AlignVCenter
        }
    }
    popup: Popup {
        y: control.height + Spacing.itemGap
        width: control.width
        implicitHeight: Math.min(contentItem.implicitHeight + 8, 320)
        padding: 4
        transformOrigin: Item.Top

        enter: Transition {
            NumberAnimation {
                property: "scale"
                from: 0.98
                to: 1
                duration: Theme.popupEnterDuration
                easing.type: Easing.OutCubic
            }
        }
        exit: Transition {
            NumberAnimation {
                property: "scale"
                to: 0.98
                duration: Theme.popupExitDuration
                easing.type: Easing.InCubic
            }
        }

        contentItem: ListView {
            clip: true
            implicitHeight: contentHeight
            model: control.popup.visible ? control.delegateModel : null

            // Прокрутка появляется только при реальном переполнении: на трёх
            // вариантах полоса съедала бы высоту списка.
            ScrollBar.vertical: ScrollBar {}
        }
        background: Rectangle {
            border.color: Theme.border
            border.width: Theme.borderWidth
            color: Theme.surface
            radius: Theme.radiusMd
        }
    }

    textRole: "title"
    valueRole: "id"
}
