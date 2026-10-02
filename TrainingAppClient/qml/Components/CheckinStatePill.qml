import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Состояние отметки правила (экран 43, §3.6). Значение приходит строкой от
// C++ (ECheckin_State): done / failed / skipped / partial / unknown.
//
// Цвет несёт смысл, а не украшает: успех — акцентом, провал — красным,
// «нет данных» — приглушённым, потому что «неизвестно» это не «плохо» (§5.5).
Rectangle {
    id: chip

    property string state: "unknown"

    readonly property bool isDone: state === "done"
    readonly property bool isFailed: state === "failed"

    color: chip.isDone ? Theme.accentSoft : (chip.isFailed ? Theme.surfaceMuted : Theme.surfaceMuted)
    implicitHeight: 24
    implicitWidth: chipLabel.implicitWidth + Spacing.listGap * 2
    radius: Theme.radiusSm

    Label {
        id: chipLabel

        anchors.centerIn: parent
        color: chip.isDone ? Theme.accent : (chip.isFailed ? Theme.negative : Theme.textSecondary)
        font: Typography.captionStrong
        text: chip.label
    }

    function label() {
        if (chip.state === "done")
            return qsTr("Выполнено");

        if (chip.state === "failed")
            return qsTr("Не выполнено");

        if (chip.state === "partial")
            return qsTr("Частично");

        if (chip.state === "skipped")
            return qsTr("Пропущено");

        return qsTr("Нет отметки");
    }
}
