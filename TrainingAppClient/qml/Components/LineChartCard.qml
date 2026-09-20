import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Карточка линейного графика (§4.8): горизонтальные grid-lines,
// accent-полилиния значения и точка на последнем измерении.
AppCard {
    id: card

    property string captionText: ""
    property string title: ""
    property var values: []

    implicitWidth: 320
    spacing: Spacing.itemGap

    Label {
        color: Theme.textPrimary
        elide: Text.ElideRight
        font: Typography.bodyStrong
        text: card.title
        width: parent.width
    }
    Label {
        color: Theme.textSecondary
        elide: Text.ElideRight
        font: Typography.caption
        text: card.captionText
        visible: text.length > 0
        width: parent.width
    }
    Item {

        // Высота графика пропорциональна ширине, но в разумных пределах (§4.8).
        height: Math.round(Math.max(100, Math.min(160, width * 0.38)))
        width: parent.width

        Canvas {
            id: chart

            anchors.fill: parent

            onHeightChanged: chart.requestPaint()
            onPaint: {
                const ctx = getContext("2d");
                const w = width;
                const h = height;

                ctx.reset();

                if (card.values.length < 2) {
                    return;
                }

                // — Горизонтальные grid-lines (§4.8: акцент только у линии) —
                const rows = 3;

                ctx.lineWidth = 1;
                ctx.strokeStyle = Theme.border;

                for (let row = 0; row <= rows; ++row) {
                    const y = Math.round(h * row / rows) + 0.5;

                    ctx.beginPath();
                    ctx.moveTo(0, y);
                    ctx.lineTo(w, y);
                    ctx.stroke();
                }

                // — Нормировка значений в область графика —
                let min = card.values[0];
                let max = card.values[0];

                for (let i = 1; i < card.values.length; ++i) {
                    min = Math.min(min, card.values[i]);
                    max = Math.max(max, card.values[i]);
                }

                const span = max > min ? max - min : 1;
                const topPad = 6;
                const plotH = h - topPad * 2;

                // — Accent-полилиния и конечная точка —
                ctx.lineWidth = 2;
                ctx.strokeStyle = Theme.accent;
                ctx.lineJoin = "round";
                ctx.lineCap = "round";
                ctx.beginPath();

                for (let i = 0; i < card.values.length; ++i) {
                    const x = w * i / (card.values.length - 1);
                    const y = topPad + plotH * (1 - (card.values[i] - min) / span);

                    if (i === 0) {
                        ctx.moveTo(x, y);
                    } else {
                        ctx.lineTo(x, y);
                    }
                }

                ctx.stroke();

                const lastX = w;
                const lastY = topPad + plotH * (1 - (card.values[card.values.length - 1] - min) / span);

                ctx.fillStyle = Theme.accent;
                ctx.beginPath();
                ctx.arc(lastX, lastY, 4, 0, Math.PI * 2);
                ctx.fill();
            }

            // Перерисовка при изменении геометрии: без этого график остаётся
            // «растянутым» прежним кадром после ресайза окна.
            onWidthChanged: chart.requestPaint()

            // Перерисовка при смене данных, акцента и темы.
            Connections {
                function onValuesChanged() {
                    chart.requestPaint();
                }

                target: card
            }
            Connections {
                function onAccentIndexChanged() {
                    chart.requestPaint();
                }
                function onDarkModeChanged() {
                    chart.requestPaint();
                }

                target: Theme
            }
        }
    }
}
