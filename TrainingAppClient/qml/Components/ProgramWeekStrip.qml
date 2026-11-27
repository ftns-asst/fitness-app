import QtQuick
import TrainingAppClient

// Общая полоса недели для сводки и раскрытого раздела редактора.
Row {
    id: strip

    property var daysModel: null
    property bool showCount: false
    property bool cycleSchedule: false
    readonly property int dayCount: daysModel && daysModel.count !== undefined ? daysModel.count : 7
    readonly property var weekdayNames: [qsTr("Пн"), qsTr("Вт"), qsTr("Ср"), qsTr("Чт"), qsTr("Пт"), qsTr("Сб"), qsTr("Вс")]
    signal dayRequested(int weekday, int kind)

    spacing: 4
    height: showCount ? 64 : 46

    Repeater {
        model: strip.daysModel
        delegate: DayTile {
            width: Math.max(0, (strip.width - strip.spacing * (strip.dayCount - 1)) / Math.max(1, strip.dayCount))
            height: strip.showCount ? heightTall : heightCompact
            weekdayName: strip.cycleSchedule ? qsTr("%1").arg(model.weekday) : strip.weekdayNames[model.weekday - 1]
            tileKind: model.kind
            showCount: strip.showCount
            countText: countFor(model.kind, model.exerciseCount)
            isEmptyDay: model.kind !== "rest" && model.exerciseCount === 0
            onClicked: strip.dayRequested(model.weekday, model.kind === "workout" ? 0 : model.kind === "cardio" ? 2 : model.kind === "mobility" ? 3 : 1)
        }
    }
}
