import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Кнопка (§4.1): primary — CTA 56 px, secondary — 44 px c border, ghost — текст.
Button {
    id: button

    // primary | secondary | ghost
    property string variant: "primary"

    // compact — primary высотой 44 px: кнопка внутри карточек.
    property bool compact: false

    implicitHeight: variant === "primary" && !compact ? Theme.buttonPrimary : Theme.buttonSecondary
    implicitWidth: Math.max(Theme.touchMin * 2, textLabel.implicitWidth + Spacing.screenPadding * 2)

    readonly property color textColor: {
        if (variant === "primary")
            return Theme.accentForeground;

        return variant === "secondary" ? Theme.textPrimary : Theme.accent;
    }

    background: Rectangle {
        radius: button.variant === "primary" ? Theme.radiusLg : Theme.radiusMd
        color: {
            if (button.variant === "ghost")
                return button.pressed ? Theme.surfaceMuted : "transparent";

            if (button.variant === "secondary")
                return button.pressed ? Theme.surfaceMuted : Theme.surface;

            return button.pressed ? Theme.accentPressed : Theme.accent;
        }
        border.width: button.variant === "secondary" ? Theme.borderWidth : 0
        border.color: Theme.border
        opacity: button.enabled ? 1.0 : 0.45

        Behavior on color {
            ColorAnimation {
                duration: 120
            }
        }
    }

    contentItem: Label {
        id: textLabel

        text: button.text
        font: Typography.bodyStrong
        color: button.textColor
        opacity: button.enabled ? 1.0 : 0.6
        horizontalAlignment: Text.AlignHCenter
        verticalAlignment: Text.AlignVCenter
        elide: Text.ElideRight
    }
}
