import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Строка упражнения (§4.3): номер в круглом badge, название, sets×reps, вес справа.
Item {
    id: row

    property int number: 0
    property string setsText: ""
    property bool showDivider: true
    property string title: ""
    property string weightText: ""

    signal clicked

    implicitHeight: 56
    implicitWidth: 240

    Rectangle {
        anchors.fill: parent
        color: pressArea.pressed ? Theme.surfaceMuted : "transparent"
        radius: Theme.radiusMd

        Behavior on color {
            ColorAnimation {
                duration: 120
            }
        }
    }
    RowLayout {
        anchors.fill: parent
        anchors.leftMargin: Spacing.screenPadding
        anchors.rightMargin: Spacing.screenPadding
        spacing: Spacing.listGap

        Rectangle {
            Layout.preferredHeight: 30
            Layout.preferredWidth: 30
            color: Theme.surfaceMuted
            radius: 15
            visible: row.number > 0

            Label {
                anchors.centerIn: parent
                color: Theme.textSecondary
                font: Typography.captionStrong
                text: (row.number < 10 ? "0" : "") + row.number
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                Layout.fillWidth: true
                color: Theme.textPrimary
                elide: Text.ElideRight
                font: Typography.bodyStrong
                text: row.title
            }
            Label {
                Layout.fillWidth: true
                color: Theme.textMuted
                elide: Text.ElideRight
                font: Typography.caption
                text: row.setsText
                visible: text.length > 0
            }
        }

        // — Вес справа: приглушённый caption (референс 2080). Ширина ограничена,
        // чтобы длинное значение не сжимало название до нуля —
        Label {
            Layout.maximumWidth: 96
            color: Theme.textMuted
            elide: Text.ElideRight
            font: Typography.caption
            text: row.weightText
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
    MouseArea {
        id: pressArea

        anchors.fill: parent

        onClicked: row.clicked()
    }
}
