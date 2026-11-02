import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Карта изменений тела (FR-T10): строка — замер, столбец — параметр, в ячейке
// изменение к старту либо к предыдущему замеру. Тон ячейки — доля изменения от
// наибольшего в столбце: у параметров разные единицы, и общая шкала обесцветила
// бы обхваты рядом с весом. Уменьшение — акцентным цветом, рост —
// предупреждающим; знак числа дублирует тон, а не уступает ему.
AppCard {
    id: card

    // 0 — к старту, 1 — к предыдущему замеру.
    property int mode: 0
    // Список строк: date и cells (param, fromStart, fromPrevious, intensity).
    property var model: null
    // Размеры ячейки заданы именами, а не числами в разметке (CODE_STYLE §12).
    readonly property real cellHeight: 36
    readonly property real cellWidth: 56
    readonly property real dateWidth: 52
    // Тон ячейки: минимальный, чтобы любое изменение было видно, и размах до
    // самого большого.
    readonly property real heatMinimum: 0.12
    readonly property real heatRange: 0.5

    // «2026-10-01» → «01.10».
    function shortDate(isoDate) {
        const parts = String(isoDate).split("-");

        return parts.length === 3 ? parts[2] + "." + parts[1] : isoDate;
    }

    function cellText(cell) {
        const value = card.cellValue(cell);

        if (value === null)
            return "—";

        if (value === 0)
            return "0";

        return (value > 0 ? "+" : "−") + Qt.locale().toString(Math.abs(value), "f", 1);
    }

    function cellIntensity(cell) {
        return card.mode === 0 ? cell.startIntensity : cell.previousIntensity;
    }

    // Число или null: пустое значение из C++ приходит в QML как undefined, и
    // проверка `=== null` его бы пропустила.
    function cellValue(cell) {
        const value = card.mode === 0 ? cell.fromStart : cell.fromPrevious;

        return typeof value === "number" ? value : null;
    }

    spacing: Spacing.itemGap

    Label {
        color: Theme.textPrimary
        font: Typography.bodyStrong
        text: qsTr("Карта изменений")
        width: parent.width
    }
    SegmentedControl {
        currentIndex: card.mode
        // Собственная ширина контрола — 300: в карточке на экране 360 места 292, и
        // аудит видел бы сжатый блок. Контрол тянется по ширине карточки.
        implicitWidth: 240
        items: [qsTr("К старту"), qsTr("К предыдущему")]
        width: parent.width

        onSelected: function (index) {
            card.mode = index;
        }
    }
    // Девять столбцов не помещаются в экран: таблица листается по горизонтали.
    Flickable {
        id: scroller

        boundsBehavior: Flickable.StopAtBounds
        clip: true
        contentHeight: grid.height
        contentWidth: grid.width
        flickableDirection: Flickable.HorizontalFlick
        height: grid.height
        width: parent.width

        Column {
            id: grid

            Row {
                Item {
                    height: card.cellHeight
                    width: card.dateWidth
                }
                Repeater {
                    model: MeasurementTexts.keys

                    delegate: Label {
                        required property string modelData

                        color: Theme.textSecondary
                        elide: Text.ElideRight
                        font: Typography.captionStrong
                        height: card.cellHeight
                        horizontalAlignment: Text.AlignHCenter
                        text: MeasurementTexts.title(modelData)
                        verticalAlignment: Text.AlignVCenter
                        width: card.cellWidth
                    }
                }
            }
            Repeater {
                model: card.model

                delegate: Row {
                    id: mapRow

                    required property var model

                    Label {
                        color: Theme.textSecondary
                        font: Typography.caption
                        height: card.cellHeight
                        text: card.shortDate(mapRow.model.date)
                        verticalAlignment: Text.AlignVCenter
                        width: card.dateWidth
                    }
                    Repeater {
                        model: mapRow.model.cells

                        delegate: Item {
                            id: cell

                            required property var modelData
                            readonly property real change: card.cellValue(cell.modelData) === null ? 0 : card.cellValue(cell.modelData)

                            height: card.cellHeight
                            width: card.cellWidth

                            Rectangle {
                                anchors.fill: parent
                                anchors.margins: 1
                                color: cell.change < 0 ? Theme.accent : Theme.warning
                                opacity: cell.change === 0 ? 0 : card.heatMinimum + card.heatRange * card.cellIntensity(cell.modelData)
                                radius: Theme.radiusSm
                            }
                            Label {
                                anchors.centerIn: parent
                                color: Theme.textPrimary
                                font: Typography.caption
                                text: card.cellText(cell.modelData)
                            }
                        }
                    }
                }
            }
        }
    }
}
