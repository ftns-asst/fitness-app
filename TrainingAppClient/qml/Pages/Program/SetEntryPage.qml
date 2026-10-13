import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Демонстрационный экран ввода подхода (спринт S8, issue 11).
//
// Компоненты WeightRuler и RepsWheel не висят в статистике: ими вводят вес и
// повторы подхода, и проверять их надо там, где ими пользуются. Сама запись
// фактов — этап D2 (AsWorkout_Logger); здесь экран показывает только ввод, и
// поэтому подписи «подход 1» берутся из аргументов, а не из модели.
//
// Пять компоновок (360/390/480, обычная и тёмная темы) проверяются
// ui_smoke_set-entry_* — ввод с ползунками ломает вёрстку иначе, чем карточки:
// у линейки своя высота, а колесо прокручивается.
PageScaffold {
    id: page

    caption: qsTr("Вес и повторы с привязкой к инвентарю")
    title: qsTr("Запись подхода")

    signal backRequested

    BackBar {
        onClicked: page.backRequested()
        text: qsTr("Назад")
        visible: true
        width: parent.width
    }

    // Значения формы — свойства страницы, а не привязки к моделям: экран
    // проверяет поведение ползунков, а данных записи пока нет.
    property double weightValue: 60
    property int repsValue: 8

    AppCard {
        spacing: Spacing.itemGap
        width: parent.width

        Label {
            color: Theme.textPrimary
            font: Typography.sectionTitle
            text: qsTr("Приседания со штангой")
            width: parent.width
            wrapMode: Text.WordWrap
        }
        Label {
            color: Theme.textMuted
            font: Typography.caption
            text: qsTr("Цель: 4 × 8–10")
            width: parent.width
            wrapMode: Text.WordWrap
        }
    }

    // — Вес: линейка с шагом 2,5 кг —
    AppCard {
        spacing: Spacing.itemGap
        width: parent.width

        Label {
            color: Theme.textSecondary
            font: Typography.captionStrong
            text: qsTr("Вес, кг")
            width: parent.width
        }
        WeightRuler {
            // 20–200 кг: как в макетах записи подхода и экрана дня. Диапазон по
            // умолчанию 0–300 давал шаг 2,5 в 0,3 px, и штангу нельзя было ни
            // поставить, ни прочитать по шкале.
            from: 20
            // Подсказка — часть линейки (`.slider-hint` в макете 07-states):
            // шаг подсказан явно, без него человек ставит «62,37» и
            // удивляется, что таких блинов нет.
            hint: qsTr("Шаг 2,5 кг — как блины в зале")
            to: 200
            width: parent.width
            value: page.weightValue

            onEdited: function (newValue) {
                page.weightValue = newValue;
            }
        }
    }

    // — Повторы: колесо целых чисел —
    AppCard {
        spacing: Spacing.itemGap
        width: parent.width

        Label {
            color: Theme.textSecondary
            font: Typography.captionStrong
            text: qsTr("Повторы")
            width: parent.width
        }
        RepsWheel {
            width: parent.width
            value: page.repsValue

            onEdited: function (newValue) {
                page.repsValue = newValue;
            }
        }
    }

    footer: RowLayout {
        AppButton {
            Layout.fillWidth: true
            text: qsTr("Записать подход")
            variant: AppButton.Primary

            onClicked: Demo.notify(qsTr("Запись подхода — этап D2"))
        }
    }
}
