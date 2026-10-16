import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Плитка правила дня: название, подпись цели и круглая отметка. Тап по плитке
// ставит/снимает отметку («быстрая отметка» — одно касание), у рекомендации
// текстом отметки нет и плитка остаётся информационной.
Item {
    id: tile

    property string caption: ""
    // Можно ли отметить правило (у совета — нет).
    property bool checkable: true
    // Состояние отметки за сегодня: done / failed / skipped / partial / unknown.
    property string checkinState: "unknown"
    property string title: ""

    readonly property bool done: tile.checkinState === "done"
    readonly property bool failed: tile.checkinState === "failed"

    signal toggled
    signal opened

    implicitHeight: 96
    implicitWidth: 150
    scale: area.pressed && tile.checkable ? 0.97 : 1.0

    Behavior on scale {
        NumberAnimation {
            duration: 90
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        id: body

        anchors.fill: parent
        border.color: tile.done ? Theme.accent : (tile.failed ? Theme.negative : Theme.border)
        border.width: tile.done || tile.failed ? 2 : Theme.borderWidth
        color: tile.done ? Theme.accentTint : Theme.surface
        radius: Theme.radiusLg

        Behavior on border.color {
            ColorAnimation {
                duration: 160
            }
        }
        Behavior on color {
            ColorAnimation {
                duration: 160
            }
        }
    }
    // Круглая отметка в правом верхнем углу: заливка + галочка с «пружиной».
    Rectangle {
        id: mark

        anchors.right: parent.right
        anchors.rightMargin: Spacing.listGap
        anchors.top: parent.top
        anchors.topMargin: Spacing.listGap
        border.color: tile.done ? Theme.accent : (tile.failed ? Theme.negative : Theme.border)
        border.width: tile.done ? 0 : 2
        color: tile.done ? Theme.accent : (tile.failed ? Theme.negative : "transparent")
        height: 28
        radius: 14
        visible: tile.checkable
        width: 28

        Behavior on color {
            ColorAnimation {
                duration: 160
            }
        }

        Label {
            id: glyph

            anchors.centerIn: parent
            color: Theme.accentForeground
            font: Typography.captionStrong
            opacity: tile.done || tile.failed ? 1 : 0
            scale: tile.done || tile.failed ? 1 : 0.4
            text: tile.done ? "\u2713" : "\u2715"

            Behavior on opacity {
                NumberAnimation {
                    duration: 120
                }
            }
            Behavior on scale {
                NumberAnimation {
                    duration: 220
                    easing.type: Easing.OutBack
                }
            }
        }
    }
    Column {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: Spacing.listGap
        anchors.left: parent.left
        anchors.leftMargin: Spacing.listGap + 2
        anchors.right: parent.right
        anchors.rightMargin: Spacing.listGap + 2
        spacing: 2

        Label {
            color: Theme.textPrimary
            elide: Text.ElideRight
            font: Typography.bodyStrong
            maximumLineCount: 2
            text: tile.title
            width: parent.width
            wrapMode: Text.WordWrap
        }
        Label {
            color: tile.done ? Theme.accent : Theme.textMuted
            elide: Text.ElideRight
            font: Typography.caption
            text: tile.caption
            visible: text.length > 0
            width: parent.width
        }
    }
    MouseArea {
        id: area

        objectName: "tileArea"
        anchors.fill: parent

        onClicked: {
            if (tile.checkable)
                tile.toggled();
            else
                tile.opened();
        }
    }
}
