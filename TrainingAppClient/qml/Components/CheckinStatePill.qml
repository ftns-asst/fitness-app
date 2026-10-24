import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Состояние отметки правила (экран 43, §3.6). Значение приходит строкой от
// C++ (ECheckin_State): done / failed / skipped / partial / unknown.
//
// Цвет несёт смысл, а не украшает: успех — акцентом, провал — красным,
// «нет данных» — приглушённым, потому что «неизвестно» это не «плохо» (§5.5).
//
// Свойство называется checkinState, а не state: `state` уже есть у QQuickItem
// (состояния и переходы), и перекрытие молча ломало бы механизм состояний —
// компонент перестал бы переключать их, и qmllint справедливо ругается.
Rectangle {
    id: chip

    property string checkinState: "unknown"

    readonly property bool isDone: chip.checkinState === "done"
    readonly property bool isFailed: chip.checkinState === "failed"

    // Подпись и цвет — таблицей по состоянию. Раньше это была цепочка if/return
    // плюс тернарник, у которого обе ветви совпадали: оттенок провала терялся,
    // а добавление состояния требовало правки в четырёх местах сразу.
    readonly property var palettesByState: ({
            done: {
                background: Theme.accentSoft,
                foreground: Theme.accent,
                label: qsTr("Выполнено")
            },
            failed: {
                background: Theme.surfaceMuted,
                foreground: Theme.negative,
                label: qsTr("Не выполнено")
            },
            partial: {
                background: Theme.surfaceMuted,
                foreground: Theme.textSecondary,
                label: qsTr("Частично")
            },
            skipped: {
                background: Theme.surfaceMuted,
                foreground: Theme.textSecondary,
                label: qsTr("Пропущено")
            },
            unknown: {
                background: Theme.surfaceMuted,
                foreground: Theme.textSecondary,
                label: qsTr("Нет отметки")
            }
        })
    readonly property var colors: chip.palettesByState[chip.checkinState] !== undefined ? chip.palettesByState[chip.checkinState] : chip.palettesByState.unknown

    color: chip.colors.background
    implicitHeight: 24
    implicitWidth: chipLabel.implicitWidth + Spacing.listGap * 2
    radius: Theme.radiusSm

    Label {
        id: chipLabel

        anchors.centerIn: parent
        color: chip.colors.foreground
        font: Typography.captionStrong
        text: chip.colors.label
    }

    // Подпись состояния наружу: тест проверяет таблицу, не залезая в Label.
    function labelText() {
        return chip.colors.label;
    }
}
