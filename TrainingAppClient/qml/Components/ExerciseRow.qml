import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Строка упражнения (§4.3): номер в круглом badge, название, sets×reps, вес справа.
Item {
    id: row

    implicitWidth: 240
    implicitHeight: 56

    property int number: 0
    property string title: ""
    property string setsText: ""
    property string weightText: ""
    property bool showDivider: true

    signal clicked

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusMd
        color: pressArea.pressed ? Theme.surfaceMuted : "transparent"

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
            Layout.preferredWidth: 30
            Layout.preferredHeight: 30
            radius: 15
            color: Theme.surfaceMuted
            visible: row.number > 0

            Label {
                anchors.centerIn: parent
                text: (row.number < 10 ? "0" : "") + row.number
                font: Typography.captionStrong
                color: Theme.textSecondary
            }
        }

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                Layout.fillWidth: true
                text: row.title
                font: Typography.bodyStrong
                color: Theme.textPrimary
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: row.setsText
                font: Typography.caption
                color: Theme.textMuted
                visible: text.length > 0
                elide: Text.ElideRight
            }
        }

        // — Вес справа: приглушённый caption (референс 2080). Ширина ограничена,
        // чтобы длинное значение не сжимало название до нуля —
        Label {
            Layout.maximumWidth: 96
            text: row.weightText
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

    MouseArea {
        id: pressArea

        anchors.fill: parent
        onClicked: row.clicked()
    }
}
