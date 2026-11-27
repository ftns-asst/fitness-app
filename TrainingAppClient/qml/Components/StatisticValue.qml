import QtQuick
import QtQuick.Controls
import TrainingAppClient

Item {
    id: metric
    property string label: ""
    property string valueText: "—"
    property string detail: ""
    property font valueFont: Typography.sectionTitle
    property font labelFont: Typography.caption
    property color valueColor: Theme.textPrimary
    implicitWidth: 120
    implicitHeight: content.implicitHeight
    Column {
        id: content
        width: parent.width
        spacing: 4
        Label {
            width: parent.width
            text: metric.label
            color: Theme.textSecondary
            font: metric.labelFont
            wrapMode: Text.WordWrap
        }
        Label {
            width: parent.width
            text: metric.valueText
            color: metric.valueColor
            font: metric.valueFont
            wrapMode: Text.WordWrap
        }
        Label {
            width: parent.width
            text: metric.detail
            visible: text.length > 0
            color: Theme.textSecondary
            font: Typography.caption
            wrapMode: Text.WordWrap
        }
    }
}
