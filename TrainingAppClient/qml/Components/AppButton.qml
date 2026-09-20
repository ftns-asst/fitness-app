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

    implicitHeight: variant === AppButton.Primary && !compact ? Theme.buttonPrimary : Theme.buttonSecondary
    implicitWidth: Math.max(Theme.touchMin * 2, textLabel.implicitWidth + Spacing.screenPadding * 2)

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
                duration: 120
            }
        }
    }
    contentItem: Label {
        id: textLabel

        color: button.textColor
        elide: Text.ElideRight
        font: Typography.bodyStrong
        horizontalAlignment: Text.AlignHCenter
        opacity: button.enabled ? 1.0 : 0.6
        text: button.text
        verticalAlignment: Text.AlignVCenter
    }
}
