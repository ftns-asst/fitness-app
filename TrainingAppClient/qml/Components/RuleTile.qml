import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Плитка правила дня: название, значение «съедено / цель» и прогресс-бар.
//
// Два жеста: круглая отметка справа сверху — быстрое «выполнено» (у числового
// правила значение заполняется целью); тап по самой плитке открывает ввод числа
// (калории, белок, вода…). У рекомендации числа нет — плитка информационная.
Item {
    id: tile

    property string caption: ""
    // Можно ли отметить правило (у совета — нет).
    property bool checkable: true
    // Состояние отметки за сегодня: done / failed / skipped / partial / unknown.
    property string checkinState: "unknown"
    // Границы цели; −1 — границы нет. «Не ниже» — goalMin, «не выше» — goalMax.
    property real goalMax: -1
    property real goalMin: -1
    // Правило измеряется числом (есть цель): тогда тап открывает ввод.
    property bool numeric: false
    property string title: ""
    // Подпись единицы («ккал», «г»…).
    property string unitText: ""
    // Записанное за сегодня число; −1 — значения нет.
    property real value: -1

    readonly property bool done: tile.checkinState === "done"
    readonly property bool failed: tile.checkinState === "failed"
    readonly property bool hasValue: tile.value >= 0
    // Верхняя точка шкалы: цель «не ниже» или верхняя граница «не выше».
    readonly property real scaleTarget: tile.goalMin > 0 ? tile.goalMin : tile.goalMax
    // Превышение верхней границы красит полосу: «не выше» нарушено.
    readonly property bool exceeded: tile.numeric && tile.hasValue && tile.goalMax > 0 && tile.value > tile.goalMax
    readonly property color accentColor: tile.exceeded || tile.failed ? Theme.negative : Theme.accent
    // Доля заполнения полосы 0…1. Без числа полоса показывает только отметку.
    readonly property real progress: {
        if (tile.numeric === false || tile.scaleTarget <= 0)
            return tile.done ? 1 : 0;

        return tile.hasValue ? Math.min(1, tile.value / tile.scaleTarget) : (tile.done ? 1 : 0);
    }

    signal edit
    signal opened
    signal toggled

    function formatNumber(number) {
        return Number.isInteger(number) ? String(number) : number.toFixed(1);
    }
    // «1800 / 2200 ккал», «1800 ккал» или «цель 2200 ккал» без значения.
    function valueText() {
        if (tile.numeric === false)
            return tile.caption;

        const unit = tile.unitText.length > 0 ? " " + tile.unitText : "";

        if (tile.hasValue)
            return tile.scaleTarget > 0 ? formatNumber(tile.value) + " / " + formatNumber(tile.scaleTarget) + unit : formatNumber(tile.value) + unit;

        return tile.scaleTarget > 0 ? qsTr("цель ") + formatNumber(tile.scaleTarget) + unit : "";
    }

    implicitHeight: 112
    implicitWidth: 150
    scale: bodyArea.pressed ? 0.97 : 1.0

    Behavior on scale {
        NumberAnimation {
            duration: 90
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        id: body

        anchors.fill: parent
        border.color: tile.done ? Theme.accent : (tile.failed || tile.exceeded ? Theme.negative : Theme.border)
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
    // Тап по телу плитки: ввод числа или (у совета) переход к программе.
    MouseArea {
        id: bodyArea

        anchors.fill: parent
        objectName: "tileArea"

        onClicked: {
            if (tile.checkable === false)
                tile.opened();
            else if (tile.numeric)
                tile.edit();
            else
                tile.toggled();
        }
    }
    Label {
        id: titleLabel

        anchors.left: parent.left
        anchors.leftMargin: Spacing.listGap + 2
        anchors.right: markArea.left
        anchors.rightMargin: 4
        anchors.top: parent.top
        anchors.topMargin: Spacing.listGap + 2
        color: Theme.textPrimary
        elide: Text.ElideRight
        font: Typography.bodyStrong
        maximumLineCount: 2
        text: tile.title
        wrapMode: Text.WordWrap
    }
    Label {
        id: valueLabel

        anchors.bottom: bar.top
        anchors.bottomMargin: 6
        anchors.left: parent.left
        anchors.leftMargin: Spacing.listGap + 2
        anchors.right: parent.right
        anchors.rightMargin: Spacing.listGap + 2
        color: tile.exceeded ? Theme.negative : (tile.hasValue || tile.done ? Theme.accent : Theme.textMuted)
        elide: Text.ElideRight
        font: Typography.captionStrong
        text: tile.valueText()
        visible: text.length > 0
    }
    // Прогресс-бар: заполняется плавно при вводе и отметке.
    Rectangle {
        id: bar

        anchors.bottom: parent.bottom
        anchors.bottomMargin: Spacing.listGap + 2
        anchors.left: parent.left
        anchors.leftMargin: Spacing.listGap + 2
        anchors.right: parent.right
        anchors.rightMargin: Spacing.listGap + 2
        color: Theme.surfaceMuted
        height: 6
        radius: 3
        visible: tile.checkable

        Rectangle {
            color: tile.accentColor
            height: parent.height
            radius: 3
            width: parent.width * tile.progress

            Behavior on color {
                ColorAnimation {
                    duration: 160
                }
            }
            Behavior on width {
                NumberAnimation {
                    duration: 260
                    easing.type: Easing.OutCubic
                }
            }
        }
    }
    // Круглая отметка в правом верхнем углу: зона касания 44 px, сама отметка 28 px.
    MouseArea {
        id: markArea

        anchors.right: parent.right
        anchors.top: parent.top
        height: Theme.touchMin
        objectName: "tileMark"
        visible: tile.checkable
        width: Theme.touchMin

        onClicked: tile.toggled()

        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: Spacing.listGap
            anchors.top: parent.top
            anchors.topMargin: Spacing.listGap
            border.color: tile.done ? Theme.accent : (tile.failed ? Theme.negative : Theme.border)
            border.width: tile.done || tile.failed ? 0 : 2
            color: tile.done ? Theme.accent : (tile.failed ? Theme.negative : "transparent")
            height: 28
            radius: 14
            scale: markArea.pressed ? 0.88 : 1.0
            width: 28

            Behavior on color {
                ColorAnimation {
                    duration: 160
                }
            }
            Behavior on scale {
                NumberAnimation {
                    duration: 90
                    easing.type: Easing.OutCubic
                }
            }

            Label {
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
    }
}
