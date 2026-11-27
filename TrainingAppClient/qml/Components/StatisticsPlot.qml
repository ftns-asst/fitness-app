import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Один временной ряд с подписанной шкалой. Касание выбирает факт, не перехватывая скролл страницы.
Item {
    id: plot
    property var points: []
    property string unit: ""
    property bool showAverage: false
    property bool showSelectionHeader: true
    property int decimals: 1
    property int selectedIndex: -1
    readonly property var cleanPoints: StatisticsUi.rows(points).filter(point => point && StatisticsUi.has(point.value) && !isNaN(Date.parse(point.date))).slice().sort((a, b) => a.date.localeCompare(b.date))
    readonly property int activeIndex: Math.max(0, Math.min(cleanPoints.length - 1, selectedIndex < 0 ? cleanPoints.length - 1 : selectedIndex))
    readonly property var selected: cleanPoints.length > 0 ? cleanPoints[activeIndex] : null
    readonly property real firstTime: cleanPoints.length ? Date.parse(cleanPoints[0].date) : 0
    readonly property real lastTime: cleanPoints.length ? Date.parse(cleanPoints[cleanPoints.length - 1].date) : 0
    readonly property var bounds: valueBounds()
    readonly property real leftGutter: 46
    implicitHeight: cleanPoints.length > 0 ? 232 : 88
    activeFocusOnTab: true
    Accessible.role: Accessible.Graphic
    Accessible.name: selected ? StatisticsUi.number(selected.value, decimals) + " " + unit + ", " + StatisticsUi.date(selected.date) : qsTr("Нет данных")
    onPointsChanged: {
        selectedIndex = -1;
        canvas.requestPaint();
    }
    onShowAverageChanged: canvas.requestPaint()
    onSelectedIndexChanged: canvas.requestPaint()
    Keys.onLeftPressed: selectedIndex = Math.max(0, activeIndex - 1)
    Keys.onRightPressed: selectedIndex = Math.min(cleanPoints.length - 1, activeIndex + 1)

    function valueBounds() {
        let values = [];
        for (const point of cleanPoints) {
            values.push(point.value);
            if (showAverage && StatisticsUi.has(point.average))
                values.push(point.average);
        }
        if (!values.length)
            return {
                min: 0,
                max: 1
            };
        const low = Math.min.apply(null, values), high = Math.max.apply(null, values);
        const padding = Math.max((high - low) * 0.15, Math.abs(high) * 0.005, 0.1);
        return {
            min: low - padding,
            max: high + padding
        };
    }
    function pointX(index) {
        const width = Math.max(0, canvas.width - leftGutter - 8);
        return leftGutter + (lastTime > firstTime ? (Date.parse(cleanPoints[index].date) - firstTime) / (lastTime - firstTime) : 0.5) * width;
    }
    function selectAt(x) {
        if (!cleanPoints.length)
            return;
        let index = 0, distance = Infinity;
        for (let i = 0; i < cleanPoints.length; ++i) {
            const current = Math.abs(pointX(i) - x);
            if (current < distance) {
                index = i;
                distance = current;
            }
        }
        selectedIndex = index;
    }
    RowLayout {
        id: selection
        visible: plot.showSelectionHeader
        height: visible ? implicitHeight : 0
        width: parent.width
        Label {
            Layout.fillWidth: true
            text: plot.selected ? StatisticsUi.date(plot.selected.date) : qsTr("Пока нет записей")
            color: Theme.textSecondary
            font: Typography.caption
        }
        Label {
            text: plot.selected ? StatisticsUi.number(plot.selected.value, plot.decimals) + " " + plot.unit : "—"
            color: Theme.textPrimary
            font: Typography.bodyStrong
        }
    }
    Canvas {
        id: canvas
        objectName: "statisticsCanvas"
        anchors {
            top: selection.bottom
            topMargin: 12
            left: parent.left
            right: parent.right
            bottom: dateLabels.top
            bottomMargin: 8
        }
        onWidthChanged: requestPaint()
        onHeightChanged: requestPaint()
        onPaint: {
            const ctx = getContext("2d");
            ctx.reset();
            if (!plot.cleanPoints.length)
                return;
            const top = 8, height = Math.max(1, canvas.height - 16);
            function y(value) {
                return top + height * (1 - (value - plot.bounds.min) / (plot.bounds.max - plot.bounds.min));
            }
            ctx.font = "11px Inter";
            ctx.textAlign = "left";
            for (let i = 0; i < 3; ++i) {
                const value = plot.bounds.max - (plot.bounds.max - plot.bounds.min) * i / 2;
                ctx.fillStyle = Theme.textMuted;
                ctx.fillText(StatisticsUi.number(value, plot.decimals), 0, y(value) + 4);
                ctx.beginPath();
                ctx.strokeStyle = Theme.borderSubtle;
                ctx.lineWidth = 1;
                ctx.moveTo(plot.leftGutter, y(value));
                ctx.lineTo(canvas.width - 8, y(value));
                ctx.stroke();
            }
            function line(average) {
                ctx.beginPath();
                ctx.strokeStyle = average ? Theme.textMuted : Theme.accent;
                ctx.lineWidth = average ? 1.5 : 2.5;
                ctx.lineJoin = "round";
                ctx.lineCap = "round";
                ctx.setLineDash(average ? [4, 4] : []);
                let started = false;
                for (let i = 0; i < plot.cleanPoints.length; ++i) {
                    const point = plot.cleanPoints[i], value = average ? point.average : point.value;
                    if (!StatisticsUi.has(value)) {
                        started = false;
                        continue;
                    }
                    if (!started)
                        ctx.moveTo(plot.pointX(i), y(value));
                    else
                        ctx.lineTo(plot.pointX(i), y(value));
                    started = true;
                }
                ctx.stroke();
                ctx.setLineDash([]);
            }
            if (plot.showAverage)
                line(true);
            line(false);
            const x = plot.pointX(plot.activeIndex), value = plot.cleanPoints[plot.activeIndex].value;
            ctx.beginPath();
            ctx.strokeStyle = Theme.border;
            ctx.lineWidth = 1;
            ctx.moveTo(x, top);
            ctx.lineTo(x, canvas.height - 8);
            ctx.stroke();
            ctx.beginPath();
            ctx.fillStyle = Theme.accent;
            ctx.arc(x, y(value), 4, 0, Math.PI * 2);
            ctx.fill();
        }
        TapHandler {
            onTapped: eventPoint => plot.selectAt(eventPoint.position.x)
        }
        Connections {
            target: Theme
            function onDarkModeChanged() {
                canvas.requestPaint();
            }
            function onAccentIndexChanged() {
                canvas.requestPaint();
            }
        }
    }
    RowLayout {
        id: dateLabels
        anchors {
            left: parent.left
            leftMargin: plot.leftGutter
            right: parent.right
            bottom: legend.top
            bottomMargin: 8
        }
        Label {
            Layout.fillWidth: true
            text: plot.cleanPoints.length ? StatisticsUi.date(plot.cleanPoints[0].date) : ""
            font: Typography.caption
            color: Theme.textSecondary
        }
        Label {
            text: plot.cleanPoints.length > 1 ? StatisticsUi.date(plot.cleanPoints[plot.cleanPoints.length - 1].date) : ""
            font: Typography.caption
            color: Theme.textSecondary
        }
    }
    Label {
        id: legend
        anchors.bottom: parent.bottom
        width: parent.width
        font: Typography.caption
        color: Theme.textSecondary
        text: plot.cleanPoints.length === 0 ? qsTr("Записи за этот период появятся здесь") : plot.cleanPoints.length === 1 ? qsTr("Первая запись · для динамики нужна следующая") : plot.showAverage ? qsTr("Линия — замеры · пунктир — среднее за 7 дней") : ""
        wrapMode: Text.WordWrap
    }
}
