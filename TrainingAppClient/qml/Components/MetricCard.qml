import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Карточка KPI (§4.6): подпись, крупное значение, дельта.
// highlight — первая карточка ряда: tinted-accent фон, подпись и дельта акцентом;
// остальные — серые tiles без border и тени.
AppCard {
    id: card

    implicitWidth: 120
    paddingVertical: Spacing.cardPadding - 4
    paddingHorizontal: Spacing.cardPadding - 4
    spacing: 2
    surfaceColor: card.highlight ? Theme.accentTint : Theme.surfaceMuted
    borderColor: "transparent"
    elevated: false

    property bool highlight: false

    // positive | negative | neutral
    property string label: ""
    property string valueText: ""
    property string deltaText: ""
    property string deltaKind: "neutral"

    readonly property color deltaColor: {
        if (deltaKind === "negative")
            return Theme.negative;

        return deltaKind === "positive" ? Theme.accent : Theme.textMuted;
    }

    Label {
        width: parent.width
        text: card.label
        font: Typography.caption
        color: card.highlight ? Theme.accent : Theme.textSecondary
        elide: Text.ElideRight
    }

    // Значение само ужимается по ширине tile: на узких экранах (360 dp) крупный
    // metric иначе вылезает за границы карточки.
    Label {
        width: parent.width
        text: card.valueText
        font: Typography.metric
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: 16
        color: Theme.textPrimary
        elide: Text.ElideRight
    }

    Label {
        width: parent.width
        text: card.deltaText
        font: Typography.captionStrong
        color: card.deltaColor
        visible: text.length > 0
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: 10
        elide: Text.ElideRight
    }
}
