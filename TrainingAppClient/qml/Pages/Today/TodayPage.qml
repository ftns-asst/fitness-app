import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Экран «Сегодня» (§5.1, референс 2080/2081): hero-карточка, план дня, личный рекорд.
// Состояния «день отдыха», «план не выбран», «продолжить» — этап D2.
PageScaffold {
    id: page

    title: MockCatalog.todayPlan.title

    // — Hero card (§4.2): CTA внутри карточки, а не в прокручиваемом списке —
    HeroCard {
        width: parent.width
        badgesText: MockCatalog.todayPlan.badges.join(" · ")
        summaryText: MockCatalog.todayPlan.exerciseCount + " " + qsTr("упражнений") + " · " + MockCatalog.todayPlan.durationText
        weekIndex: MockCatalog.todayPlan.weekIndex
        weeksTotal: MockCatalog.todayPlan.weeksTotal
        ctaText: qsTr("Начать тренировку")

        onStartClicked: Demo.notify(qsTr("Запись тренировки — этап D2"))
    }

    Label {
        text: qsTr("План на сегодня")
        font: Typography.captionStrong
        color: Theme.textSecondary
    }

    AppCard {
        width: parent.width
        paddingVertical: Spacing.itemGap
        paddingHorizontal: 0
        spacing: 0

        Repeater {
            model: MockCatalog.todayExercises

            delegate: ExerciseRow {
                width: parent.width
                number: index + 1
                title: modelData.title
                setsText: modelData.setsText
                weightText: modelData.weightText
                showDivider: index < MockCatalog.todayExercises.length - 1

                onClicked: Demo.notify(qsTr("Карточка упражнения — этап D3"))
            }
        }
    }

    // — Два равноширинных secondary-действия (референс 2080). RowLayout вместо
    // ручного расчёта ширины: кнопки честно делят строку на любой ширине —
    RowLayout {
        width: parent.width
        spacing: Spacing.itemGap

        AppButton {
            Layout.fillWidth: true
            variant: "secondary"
            text: qsTr("Другой план")

            onClicked: Demo.notify(qsTr("Каталог планов — вкладка «Планы»"))
        }

        AppButton {
            Layout.fillWidth: true
            variant: "secondary"
            text: qsTr("История")

            onClicked: Demo.notify(qsTr("История — вкладка «Профиль», этап D4"))
        }
    }

    // — Личный рекорд (референс 2081) —
    AppCard {
        width: parent.width
        spacing: 2

        Label {
            width: parent.width
            text: MockCatalog.todayRecord.label
            font: Typography.caption
            color: Theme.textSecondary
        }

        Label {
            width: parent.width
            text: MockCatalog.todayRecord.title
            font: Typography.bodyStrong
            color: Theme.textPrimary
        }

        Label {
            width: parent.width
            text: MockCatalog.todayRecord.dateText
            font: Typography.caption
            color: Theme.textMuted
        }
    }
}
