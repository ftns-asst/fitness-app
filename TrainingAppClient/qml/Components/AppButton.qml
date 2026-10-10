import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Кнопка (§4.1): primary — CTA 56 px, secondary — 44 px c border, ghost — текст.
Button {
    id: button

    enum Variant {
        Primary,
        Secondary,
        Ghost
    }

    // compact — primary высотой 44 px: кнопка внутри карточек.
    property bool compact: false
    readonly property color textColor: {
        if (variant === AppButton.Primary)
            return Theme.accentForeground;

        return variant === AppButton.Secondary ? Theme.textPrimary : Theme.accent;
    }

    // primary | secondary | ghost
    property int variant: AppButton.Primary

    // Шрифт подписи вынесен в свойство, потому что implicitWidth обязан считать
    // его же: иначе кнопка с мелким шрифтом в узкой карточке всё равно
    // запрашивала бы ширину по основному шрифту и вылезала за край.
    property font labelFont: Typography.bodyStrong

    implicitHeight: variant === AppButton.Primary && !compact ? Theme.buttonPrimary : Theme.buttonSecondary
    // Ширина считается по фактическим отступам, а не по константе: иначе
    // заданный padding не влиял бы на ширину и кнопка не могла сжаться.
    implicitWidth: Math.max(Theme.touchMin * 2, textLabel.implicitWidth + leftPadding + rightPadding)

    // Отзыв на нажатие (Material/iOS): на тач-платформах лёгкое «продавливание»
    // кнопки под пальцем с быстрым пружинным возвратом — нажатие видно сразу,
    // а не через анимацию цвета. На десктопе размер не меняется.
    scale: Theme.touchPlatform && pressed ? 0.97 : 1.0

    Behavior on scale {
        NumberAnimation {
            duration: 90
            easing.type: Easing.OutCubic
        }
    }

    background: Rectangle {
        border.color: Theme.border
        border.width: button.variant === AppButton.Secondary ? Theme.borderWidth : 0
        color: {
            if (button.variant === AppButton.Ghost)
                return button.pressed ? Theme.surfaceMuted : "transparent";

            if (button.variant === AppButton.Secondary)
                return button.pressed ? Theme.surfaceMuted : Theme.surface;

            return button.pressed ? Theme.accentPressed : Theme.accent;
        }
        opacity: button.enabled ? 1.0 : 0.45
        radius: button.variant === AppButton.Primary ? Theme.radiusLg : Theme.radiusMd

        Behavior on color {
            ColorAnimation {
                // Длительность — токен; плавный easing читается как отклик,
                // а не как мигание.
                duration: Theme.pressFeedbackDuration
                easing.type: Easing.OutCubic
            }
        }
    }
    contentItem: Label {
        id: textLabel

        color: button.textColor
        elide: Text.ElideRight
        font: button.labelFont
        horizontalAlignment: Text.AlignHCenter
        opacity: button.enabled ? 1.0 : 0.6
        text: button.text
        verticalAlignment: Text.AlignVCenter
    }
}
