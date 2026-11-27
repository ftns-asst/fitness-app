import QtQuick
import QtQuick.Controls
import TrainingAppClient

Column {
    id: catalog

    property var viewModel: typeof DefaultProgramsVM !== "undefined" ? DefaultProgramsVM : null
    property var server: typeof ServerProgramsVM !== "undefined" ? ServerProgramsVM : null
    property bool active: false
    property bool requested: false
    property bool filtersOpen: false
    property var days: []

    signal programRequested(string programId)
    signal serverDraftsRequested

    spacing: Spacing.listGap

    onActiveChanged: {
        if (active && !requested)
            reload();
    }

    function reload() {
        if (viewModel) {
            requested = true;
            viewModel.load(goal.selectedValue, level.selectedValue, days);
        }
    }

    AppButton {
        text: qsTr("Серверные программы")
        variant: AppButton.Secondary
        visible: catalog.server !== null
        width: parent.width

        onClicked: catalog.serverDraftsRequested()
    }
    AppButton {
        text: catalog.filtersOpen ? qsTr("Скрыть фильтры") : qsTr("Фильтры")
        variant: AppButton.Secondary
        width: parent.width

        onClicked: catalog.filtersOpen = !catalog.filtersOpen
    }
    AppCard {
        spacing: Spacing.itemGap
        visible: catalog.filtersOpen
        width: parent.width

        FormChoice {
            id: goal

            label: qsTr("Цель")
            options: [
                {
                    value: "",
                    label: qsTr("Любая")
                },
                {
                    value: "strength",
                    label: qsTr("Сила")
                },
                {
                    value: "muscle",
                    label: qsTr("Мышцы")
                },
                {
                    value: "fitness",
                    label: qsTr("Форма")
                },
                {
                    value: "fat_loss",
                    label: qsTr("Снижение веса")
                },
                {
                    value: "endurance",
                    label: qsTr("Выносливость")
                }
            ]
            width: parent.width

            onSelected: value => {
                selectedValue = value;
                catalog.reload();
            }
        }
        FormChoice {
            id: level

            label: qsTr("Уровень")
            options: [
                {
                    value: "",
                    label: qsTr("Любой")
                },
                {
                    value: "beginner",
                    label: qsTr("Новичок")
                },
                {
                    value: "intermediate",
                    label: qsTr("Средний")
                },
                {
                    value: "advanced",
                    label: qsTr("Продвинутый")
                }
            ]
            width: parent.width

            onSelected: value => {
                selectedValue = value;
                catalog.reload();
            }
        }
        Label {
            color: Theme.textSecondary
            font: Typography.captionStrong
            text: qsTr("Доступные дни")
        }
        ProgramWeekdayPicker {
            days: catalog.days
            width: parent.width

            onDaysSelected: days => {
                catalog.days = days;
                catalog.reload();
            }
        }
    }

    Label {
        color: Theme.textSecondary
        font: Typography.body
        text: !catalog.viewModel || catalog.viewModel.errorKey ? qsTr("Не удалось загрузить каталог.") : catalog.viewModel.loading ? qsTr("Загружаем программы…") : qsTr("Нет программ с такими параметрами.")
        visible: !catalog.viewModel || catalog.viewModel.loading || catalog.viewModel.errorKey !== "" || (catalog.viewModel.loaded && catalog.viewModel.programs.length === 0)
        width: parent.width
        wrapMode: Text.WordWrap
    }
    AppButton {
        text: qsTr("Повторить")
        variant: AppButton.Secondary
        visible: catalog.viewModel !== null && catalog.viewModel.errorKey !== ""
        width: parent.width

        onClicked: catalog.reload()
    }
    Repeater {
        model: catalog.viewModel ? catalog.viewModel.programs : []

        delegate: AppCard {
            id: programCard

            required property var modelData

            spacing: Spacing.itemGap
            width: parent.width

            Label {
                color: Theme.textPrimary
                font: Typography.sectionTitle
                text: programCard.modelData.name
                textFormat: Text.PlainText
                width: parent.width
                wrapMode: Text.WordWrap
            }
            Label {
                color: Theme.textSecondary
                font: Typography.body
                text: qsTr("Недель: %1 · тренировок в неделю: %2").arg(programCard.modelData.weeks_count).arg(programCard.modelData.days_per_week)
                width: parent.width
                wrapMode: Text.WordWrap
            }
            AppButton {
                objectName: "defaultProgramOpen"
                text: qsTr("Подробнее")
                variant: AppButton.Secondary
                width: parent.width

                onClicked: catalog.programRequested(programCard.modelData.id)
            }
        }
    }
}
