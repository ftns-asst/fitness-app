import QtQuick
import QtQuick.Controls
import TrainingAppClient

Flow {
    id: picker

    property var days: []
    property int maximum: 7

    signal daysSelected(var days)

    Repeater {
        model: [qsTr("Пн"), qsTr("Вт"), qsTr("Ср"), qsTr("Чт"), qsTr("Пт"), qsTr("Сб"), qsTr("Вс")]

        delegate: CheckBox {
            id: weekday

            required property int index
            required property string modelData
            readonly property int dayNumber: index + 1

            checked: picker.days.indexOf(dayNumber) >= 0
            enabled: checked || picker.days.length < picker.maximum
            font: Typography.caption
            indicator: SelectionMark {
                checked: weekday.checked
                x: weekday.leftPadding
                anchors.verticalCenter: parent.verticalCenter
            }
            text: modelData

            onToggled: picker.daysSelected(checked ? picker.days.concat([dayNumber]) : picker.days.filter(day => day !== dayNumber))
        }
    }
}
