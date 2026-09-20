import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Карточка KPI (§4.6): подпись, крупное значение, дельта.
// highlight — первая карточка ряда: tinted-accent фон, подпись и дельта акцентом;
// остальные — серые tiles без border и тени.
AppCard {
    id: card

    readonly property color deltaColor: {
        if (deltaKind === "negative")
            return Theme.negative;

        return deltaKind === "positive" ? Theme.accent : Theme.textMuted;
    }
    property string deltaKind: "neutral"
    property string deltaText: ""
    property bool highlight: false

    // positive | negative | neutral
    property string label: ""
    property string valueText: ""

    borderColor: "transparent"
    elevated: false
    implicitWidth: 120
    paddingHorizontal: Spacing.cardPadding - 4
    paddingVertical: Spacing.cardPadding - 4
    spacing: 2
    surfaceColor: card.highlight ? Theme.accentTint : Theme.surfaceMuted

    Label {
        color: card.highlight ? Theme.accent : Theme.textSecondary
        elide: Text.ElideRight
        font: Typography.caption
        text: card.label
        width: parent.width
    }

    // Значение само ужимается по ширине tile: на узких экранах (360 dp) крупный
    // metric иначе вылезает за границы карточки.
    Label {
        color: Theme.textPrimary
        elide: Text.ElideRight
        font: Typography.metric
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: 16
        text: card.valueText
        width: parent.width
    }
    Label {
        color: card.deltaColor
        elide: Text.ElideRight
        font: Typography.captionStrong
        fontSizeMode: Text.HorizontalFit
        minimumPixelSize: 10
        text: card.deltaText
        visible: text.length > 0
        width: parent.width
    }
}
