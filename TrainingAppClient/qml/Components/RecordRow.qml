import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Строка личного рекорда: название, значение жирным, дата справа.
Item {
    id: row

    property string dateText: ""
    property bool showDivider: true
    property string title: ""
    property string valueText: ""

    implicitHeight: Theme.touchMin
    implicitWidth: 240

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Spacing.screenPadding
        anchors.rightMargin: Spacing.screenPadding
        spacing: Spacing.listGap

        Label {
            Layout.fillWidth: true
            color: Theme.textPrimary
            elide: Text.ElideRight
            font: Typography.body
            text: row.title
        }
        Label {
            Layout.maximumWidth: 120
            color: Theme.textPrimary
            elide: Text.ElideRight
            font: Typography.bodyStrong
            text: row.valueText
        }
        Label {
            Layout.maximumWidth: 72
            color: Theme.textMuted
            elide: Text.ElideRight
            font: Typography.caption
            text: row.dateText
            visible: text.length > 0
        }
    }
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.leftMargin: Spacing.screenPadding
        anchors.right: parent.right
        anchors.rightMargin: Spacing.screenPadding
        color: Theme.border
        height: Theme.borderWidth
        visible: row.showDivider
    }
}
