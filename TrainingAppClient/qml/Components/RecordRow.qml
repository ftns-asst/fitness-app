import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Строка личного рекорда: название, значение жирным, дата справа.
Item {
    id: row

    implicitWidth: 240
    implicitHeight: Theme.touchMin

    property string title: ""
    property string valueText: ""
    property string dateText: ""
    property bool showDivider: true

    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Spacing.screenPadding
        anchors.rightMargin: Spacing.screenPadding
        spacing: Spacing.listGap

        Label {
            Layout.fillWidth: true
            text: row.title
            font: Typography.body
            color: Theme.textPrimary
            elide: Text.ElideRight
        }

        Label {
            Layout.maximumWidth: 120
            text: row.valueText
            font: Typography.bodyStrong
            color: Theme.textPrimary
            elide: Text.ElideRight
        }

        Label {
            Layout.maximumWidth: 72
            text: row.dateText
            font: Typography.caption
            color: Theme.textMuted
            visible: text.length > 0
            elide: Text.ElideRight
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.leftMargin: Spacing.screenPadding
        anchors.rightMargin: Spacing.screenPadding
        height: Theme.borderWidth
        color: Theme.border
        visible: row.showDivider
    }
}
