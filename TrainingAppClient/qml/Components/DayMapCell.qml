import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Карточка дня на неделе-сетке. Показывает вид дня, число упражнений и готовность
// дня, а сам день открывает по тапу — недели в один экран не влезает, а семь
// кнопок подряд читаются хуже, чем сетка (экран 42, §3.4).
Item {
    id: cell

    // Свойства названы не так же, как роли модели: одноимённые свойства затеняли бы
    // роли, и значения из AList_Model не доходили бы до карточки.
    property int cellWeekday
    property string cellWeekdayName
    property string cellKind
    property int cellExerciseCount

    // Отдых готов, как только он помечен: упражнения в нём не нужны.
    readonly property bool isRest: cellKind === "rest"
    readonly property bool ready: isRest ? true : cellExerciseCount > 0
    signal clicked

    implicitHeight: 96
    implicitWidth: 148

    // Карточка целиком — цель касания: отдельная мелкая кнопка внутри карточки
    // на телефоне не попадает под палец (§4.1).
    Rectangle {
        anchors.fill: parent
        border.color: cell.ready ? Theme.border : Theme.warning
        border.width: cell.ready ? Theme.borderWidth : 2
        color: cell.isRest ? Theme.surfaceMuted : Theme.surface
        radius: Theme.radiusLg

        Behavior on color {
            ColorAnimation {
                duration: 120
            }
        }
    }
    Column {
        anchors.fill: parent
        anchors.margins: Spacing.itemGap
        spacing: 2

        Label {
            color: Theme.textMuted
            font: Typography.captionStrong
            text: cell.cellWeekdayName
        }
        Label {
            color: cell.isRest ? Theme.textSecondary : Theme.textPrimary
            elide: Text.ElideRight
            font: Typography.bodyStrong
            text: cell.kindText()
            width: parent.width
        }
        // Статус готовности виден сразу, без открытия дня: иначе «пустой» день
        // обнаруживается только при попытке создать программу.
        Label {
            color: cell.ready ? Theme.textMuted : Theme.warning
            font: Typography.caption
            text: cell.isRest ? qsTr("отдых") : (cell.cellExerciseCount > 0 ? qsTr("%1 упр.").arg(cell.cellExerciseCount) : qsTr("не заполнен"))
            width: parent.width
        }
        Item {
            width: 1
            height: 1

            // Стрелка вправо — приглашение зайти внутрь дня.
            Label {
                anchors.bottom: parent.bottom
                anchors.right: parent.right
                color: Theme.textMuted
                font: Typography.caption
                text: "›"
            }
        }
    }
    TapHandler {
        onTapped: cell.clicked()
    }
    // Подпись вида дня переводится здесь, потому что в C++ это строки (§7).
    function kindText() {
        if (cellKind === "workout")
            return qsTr("Тренировка");
        if (cellKind === "cardio")
            return qsTr("Кардио");
        if (cellKind === "mobility")
            return qsTr("Мобилити");

        return qsTr("Отдых");
    }
}
