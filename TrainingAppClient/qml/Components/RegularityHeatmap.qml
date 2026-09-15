import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Heatmap регулярности (§4.8): недели квадратами с четырьмя
// уровнями насыщенности акцента и легендой «реже → чаще».
AppCard {
    id: card

    implicitWidth: 320
    spacing: Spacing.listGap

    property string title: ""
    property var weeks: []

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

    function cellColor(value) {
        if (value <= 0) {
            return Theme.surfaceMuted;
        }

        const alpha = card.levelAlphas[Math.min(value, card.levelAlphas.length) - 1];

        return Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, alpha);
    }

    RowLayout {
        width: parent.width
        spacing: Spacing.itemGap

        Label {
            Layout.fillWidth: true
            text: card.title
            font: Typography.bodyStrong
            color: Theme.textPrimary
            elide: Text.ElideRight
        }

        Label {
            text: card.periodText
            font: Typography.caption
            color: Theme.textSecondary
        }
    }

    // 24 недели референса укладываются в 2 ряда по 12 колонок.
    Grid {
        id: cellsGrid

        width: parent.width
        columns: 12
        columnSpacing: Spacing.itemGap
        rowSpacing: Spacing.itemGap

        Repeater {
            model: card.weeks

            delegate: Rectangle {
                width: (cellsGrid.width - (cellsGrid.columns - 1) * cellsGrid.columnSpacing) / cellsGrid.columns
                height: width
                radius: 4
                color: card.cellColor(modelData)
            }
        }
    }

    // — Легенда насыщенности и шкала (§4.8: число тренировок за неделю) —
    // Подпись занимает остаток строки и переносится: на 360 dp строка иначе
    // не влезает и текст вылезает за карточку.
    RowLayout {
        width: parent.width
        spacing: Spacing.itemGap

        Label {
            text: qsTr("реже")
            font: Typography.caption
            color: Theme.textMuted
        }

        Row {
            spacing: 4

            Repeater {
                model: card.levelAlphas

                delegate: Rectangle {
                    width: 14
                    height: 14
                    radius: 4
                    color: Qt.rgba(Theme.accent.r, Theme.accent.g, Theme.accent.b, modelData)
                }
            }
        }

        Label {
            text: qsTr("чаще")
            font: Typography.caption
            color: Theme.textMuted
        }

        Label {
            Layout.fillWidth: true
            text: qsTr("0–3 тренировки в неделю")
            font: Typography.caption
            color: Theme.textMuted
            horizontalAlignment: Text.AlignRight
            wrapMode: Text.WordWrap
        }
    }
}
