import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Heatmap регулярности (§4.8): недели квадратами с четырьмя
// уровнями насыщенности акцента и легендой «реже → чаще».
AppCard {
    id: card

    // Уровни насыщенности: 0 тренировок — серый tile, 1..3 — рост opacity акцента.
    readonly property var levelAlphas: [0.15, 0.35, 0.6, 1.0]
    readonly property string periodText: {
        const n = card.weeks.length;
        const mod10 = n % 10;
        const mod100 = n % 100;

        if (mod10 === 1 && mod100 !== 11) {
            return n + " " + qsTr("неделя");
        }

        if (mod10 >= 2 && mod10 <= 4 && (mod100 < 12 || mod100 > 14)) {
            return n + " " + qsTr("недели");
        }

        return n + " " + qsTr("недель");
    }
    property string title: ""
    property var weeks: []

    function cellColor(value) {
        if (value <= 0) {
            return Theme.surfaceMuted;
        }

        const alpha = card.levelAlphas[Math.min(value, card.levelAlphas.length) - 1];

        return Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, alpha);
    }

    implicitWidth: 320
    spacing: Spacing.listGap

    RowLayout {
        spacing: Spacing.itemGap
        width: parent.width

        Label {
            Layout.fillWidth: true
            color: Theme.textPrimary
            elide: Text.ElideRight
            font: Typography.bodyStrong
            text: card.title
        }
        Label {
            color: Theme.textSecondary
            font: Typography.caption
            text: card.periodText
        }
    }

    // 24 недели референса укладываются в 2 ряда по 12 колонок.
    Grid {
        id: cellsGrid

        columnSpacing: Spacing.itemGap
        columns: 12
        rowSpacing: Spacing.itemGap
        width: parent.width

        Repeater {
            model: card.weeks

            delegate: Rectangle {
                color: card.cellColor(modelData)
                height: width
                radius: 4
                width: (cellsGrid.width - (cellsGrid.columns - 1) * cellsGrid.columnSpacing) / cellsGrid.columns
            }
        }
    }

    // — Легенда насыщенности и шкала (§4.8: число тренировок за неделю) —
    // Подпись занимает остаток строки и переносится: на 360 dp строка иначе
    // не влезает и текст вылезает за карточку.
    RowLayout {
        spacing: Spacing.itemGap
        width: parent.width

        Label {
            color: Theme.textMuted
            font: Typography.caption
            text: qsTr("реже")
        }
        Row {
            spacing: 4

            Repeater {
                model: card.levelAlphas

                delegate: Rectangle {
                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, modelData)
                    height: 14
                    radius: 4
                    width: 14
                }
            }
        }
        Label {
            color: Theme.textMuted
            font: Typography.caption
            text: qsTr("чаще")
        }
        Label {
            Layout.fillWidth: true
            color: Theme.textMuted
            font: Typography.caption
            horizontalAlignment: Text.AlignRight
            text: qsTr("0–3 тренировки в неделю")
            wrapMode: Text.WordWrap
        }
    }
}
