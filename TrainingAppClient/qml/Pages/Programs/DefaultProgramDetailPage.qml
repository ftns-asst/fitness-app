import QtQuick
import QtQuick.Controls
import TrainingAppClient

PageScaffold {
    id: page

    required property string templateId
    property var viewModel: typeof DefaultProgramsVM !== "undefined" ? DefaultProgramsVM : null
    readonly property var detail: viewModel ? viewModel.detail : ({})
    readonly property var workouts: (detail.workouts || []).filter(workout => workout.week_number === week).sort((a, b) => a.order_index - b.order_index)
    property int week: 1

    signal backRequested

    showHeader: false

    Component.onCompleted: reload()

    function reload() {
        if (viewModel)
            viewModel.loadDetail(templateId);
    }

    BackBar {
        text: qsTr("Каталог")
        width: parent.width

        onClicked: page.backRequested()
    }
    Label {
        objectName: "defaultProgramTitle"
        color: Theme.textPrimary
        font: Typography.sectionTitle
        text: page.detail.name || ""
        textFormat: Text.PlainText
        visible: text.length > 0
        width: parent.width
        wrapMode: Text.WordWrap
    }
    Label {
        color: Theme.textSecondary
        font: Typography.body
        text: page.viewModel && page.viewModel.detailLoading ? qsTr("Загружаем программу…") : qsTr("Не удалось загрузить программу.")
        visible: !page.detail.id
        width: parent.width
        wrapMode: Text.WordWrap
    }
    AppButton {
        text: qsTr("Повторить")
        variant: AppButton.Secondary
        visible: page.viewModel !== null && page.viewModel.detailErrorKey !== ""
        width: parent.width

        onClicked: page.reload()
    }
    ComboBox {
        Accessible.name: qsTr("Неделя программы")
        currentIndex: page.week - 1
        model: Array.from({
            length: page.detail.weeks_count || 0
        }, (_, index) => qsTr("Неделя %1").arg(index + 1))
        visible: count > 1
        width: parent.width

        onActivated: index => page.week = index + 1
    }
    Label {
        color: Theme.textSecondary
        font: Typography.body
        text: qsTr("На этой неделе нет тренировок.")
        visible: !!page.detail.id && page.workouts.length === 0
        width: parent.width
        wrapMode: Text.WordWrap
    }
    Repeater {
        model: page.workouts

        delegate: AppCard {
            id: workoutCard

            required property var modelData
            readonly property var workoutTemplate: modelData.template || ({})

            spacing: Spacing.itemGap
            width: parent.width

            Label {
                color: Theme.textPrimary
                font: Typography.sectionTitle
                text: workoutCard.workoutTemplate.name || qsTr("Тренировка")
                textFormat: Text.PlainText
                width: parent.width
                wrapMode: Text.WordWrap
            }
            Repeater {
                model: (workoutCard.workoutTemplate.exercises || []).slice().sort((a, b) => a.order_index - b.order_index)

                delegate: Column {
                    required property var modelData

                    width: parent.width

                    Label {
                        color: Theme.textPrimary
                        font: Typography.body
                        text: modelData.exercise_name || ""
                        textFormat: Text.PlainText
                        width: parent.width
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        color: Theme.textSecondary
                        font: Typography.caption
                        text: qsTr("%1 × %2").arg(modelData.sets).arg(modelData.target_reps) + (modelData.target_percent_1rm ? qsTr(" · %1% от 1ПМ").arg(modelData.target_percent_1rm) : "") + (modelData.rest_seconds ? qsTr(" · отдых %1 с").arg(modelData.rest_seconds) : "")
                        width: parent.width
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
}
