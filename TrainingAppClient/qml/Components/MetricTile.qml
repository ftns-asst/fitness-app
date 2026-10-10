import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Плитка метрики в форме своего правила (макет `08-program-rules`,
// `.metric-tile`). Метрика выбирается плитками, а не выпадающим списком: у
// каждой метрики своя единица измерения, и в списке её приходилось держать в
// голове — плитка показывает «Вода · мл» сразу. Сетка три в ряд помещает
// каталог метрик целиком, без прокрутки.
//
// Выбранная — заливкой акцентом: в форме это единственное поле, от которого
// зависят и единица, и диапазон цели, поэтому выбор обязан читаться без нажатия.
Item {
    id: tile

    property bool selected: false
    property string title: ""
    property string unit: ""

    signal clicked

    readonly property int contentGap: 3
    readonly property int minHeight: 56
    readonly property int paddingH: 8
    readonly property int paddingV: 10

    implicitHeight: Math.max(tile.minHeight, content.implicitHeight + tile.paddingV * 2)
    implicitWidth: 104

    Rectangle {
        anchors.fill: parent
        border.color: tile.selected ? Theme.accent : "transparent"
        border.width: Theme.borderWidth
        color: tile.selected ? Theme.accent : Theme.surfaceMuted
        radius: Theme.radiusMd

        Behavior on color {
            ColorAnimation {
                duration: Theme.pressFeedbackDuration
                easing.type: Easing.OutCubic
            }
        }
    }
    Column {
        id: content

        anchors.centerIn: parent
        spacing: tile.contentGap
        width: parent.width - tile.paddingH * 2

        Label {
            color: tile.selected ? Theme.accentForeground : Theme.textSecondary
            font: tile.selected ? Typography.captionStrong : Typography.caption
            horizontalAlignment: Text.AlignHCenter
            text: tile.title
            width: parent.width
            wrapMode: Text.WordWrap
        }
        Label {
            // Подпись единицы приглушена и на выбранной плитке: «мл» рядом с
            // «Вода» — пояснение, а не второе название метрики.
            color: tile.selected ? Theme.accentForeground : Theme.textMuted
            font: Typography.caption
            horizontalAlignment: Text.AlignHCenter
            opacity: tile.selected ? 0.8 : 1.0
            text: tile.unit
            visible: text.length > 0
            width: parent.width
        }
    }
    TapHandler {
        onTapped: tile.clicked()
    }
}
