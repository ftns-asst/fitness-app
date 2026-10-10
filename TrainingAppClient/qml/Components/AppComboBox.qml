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

    implicitHeight: Theme.buttonPrimary
    padding: Spacing.cardPadding

    background: Rectangle {
        border.color: control.activeFocus ? Theme.accent : Theme.border
        border.width: control.activeFocus ? 2 : Theme.borderWidth
        color: Theme.surfaceMuted
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
    indicator: Text {
        // Штатная стрелка заменена на символ темы: цвет и размер задаются
        // здесь, а не стилем Qt.
        color: Theme.textMuted
        font: Typography.body
        text: "▾"
        x: control.width - width - control.rightPadding
        y: (control.height - height) / 2
    }
    delegate: ItemDelegate {
        required property var modelData

        width: control.width
        height: Theme.touchMin
        highlighted: control.highlightedIndex === control.index

        background: Rectangle {
            color: highlighted ? Theme.accentTint : "transparent"
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
