import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Экран «Сегодня» (§5.1, референс 2080/2081): hero-карточка, план дня, личный рекорд.
// Состояния «день отдыха», «план не выбран», «продолжить» — этап D2.
PageScaffold {
    id: page

    required property var viewModel

    title: viewModel.plan.title

    // — Hero card (§4.2): CTA внутри карточки, а не в прокручиваемом списке —
    HeroCard {
        badgesText: page.viewModel.plan.badges.join(" · ")
        ctaText: qsTr("Начать тренировку")
        summaryText: page.viewModel.plan.exerciseCount + " " + qsTr("упражнений") + " · " + page.viewModel.plan.durationText
        weekIndex: page.viewModel.plan.weekIndex
        weeksTotal: page.viewModel.plan.weeksTotal
        width: parent.width

        onStartClicked: Demo.notify(qsTr("Запись тренировки — этап D2"))
    }
    Label {
        color: Theme.textSecondary
        font: Typography.captionStrong
        text: qsTr("План на сегодня")
    }
    AppCard {
        paddingHorizontal: 0
        paddingVertical: Spacing.itemGap
        spacing: 0
        width: parent.width

        Repeater {
            model: page.viewModel.exercises

            delegate: ExerciseRow {
                number: index + 1
                setsText: modelData.setsText
                showDivider: index < page.viewModel.exercises.length - 1
                title: modelData.title
                weightText: modelData.weightText
                width: parent.width

                onClicked: Demo.notify(qsTr("Карточка упражнения — этап D3"))
            }
        }
    }

    // — Два равноширинных secondary-действия (референс 2080). RowLayout вместо
    // ручного расчёта ширины: кнопки честно делят строку на любой ширине —
    RowLayout {
        spacing: Spacing.itemGap
        width: parent.width

        AppButton {
            Layout.fillWidth: true
            text: qsTr("Другой план")
            variant: AppButton.Secondary

            onClicked: Demo.notify(qsTr("Каталог планов — вкладка «Планы»"))
        }
        AppButton {
            Layout.fillWidth: true
            text: qsTr("История")
            variant: AppButton.Secondary

            onClicked: Demo.notify(qsTr("История — вкладка «Профиль», этап D4"))
        }
    }

    // — Личный рекорд (референс 2081) —
    AppCard {
        spacing: 2
        width: parent.width

        Label {
            color: Theme.textSecondary
            font: Typography.caption
            text: page.viewModel.record.label
            width: parent.width
        }
        Label {
            color: Theme.textPrimary
            font: Typography.bodyStrong
            text: page.viewModel.record.title
            width: parent.width
        }
        Label {
            color: Theme.textMuted
            font: Typography.caption
            text: page.viewModel.record.dateText
            width: parent.width
        }
    }
}
