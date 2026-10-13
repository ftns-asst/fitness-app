import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Экран «Упражнения» (§2.2): поиск + «+ Своё» в одной строке,
// chips-фильтры мышечных групп, каталог отдельными карточками с фото техники.
PageScaffold {
    id: page

    required property var viewModel
    property string groupFilter: ""
    readonly property var muscleGroups: viewModel.muscleGroups
    property string query: ""
    readonly property var visibleExercises: viewModel.filteredExercises(query, groupFilter)

    title: qsTr("Упражнения")

    // — Поиск и добавление своего упражнения в одной строке —
    RowLayout {
        spacing: Spacing.itemGap
        width: parent.width

        TextField {
            id: searchField

            Layout.fillWidth: true
            Layout.minimumWidth: 140
            Layout.preferredHeight: Theme.touchMin
            color: Theme.textPrimary
            font: Typography.body
            leftPadding: Spacing.screenPadding
            placeholderText: qsTr("Поиск упражнения")
            placeholderTextColor: Theme.textMuted
            rightPadding: Spacing.screenPadding
            verticalAlignment: Text.AlignVCenter

            background: Rectangle {
                border.color: searchField.activeFocus ? Theme.accent : Theme.border
                border.width: Theme.borderWidth
                color: Theme.surface
                radius: Theme.radiusMd
            }

            onTextChanged: page.query = text
        }
        AppButton {
            compact: true
            text: qsTr("+ Своё")
            variant: AppButton.Primary

            onClicked: Demo.notify(qsTr("Форма своего упражнения — этап D3"))
        }
    }
    FilterChips {
        currentIndex: page.groupFilter.length === 0 ? 0 : page.muscleGroups.indexOf(page.groupFilter) + 1
        items: [qsTr("Все")].concat(page.muscleGroups)
        width: parent.width

        onSelected: function (index) {
            page.groupFilter = index === 0 ? "" : page.muscleGroups[index - 1];
        }
    }
    Column {
        spacing: Spacing.listGap
        visible: page.visibleExercises.length > 0
        width: parent.width

        Repeater {
            model: page.visibleExercises

            delegate: CatalogExerciseRow {
                subtitle: modelData.subtitle
                title: modelData.title
                width: parent.width

                onClicked: Demo.notify(qsTr("Карточка упражнения и техника — этап D3"))
            }
        }
    }
    AppCard {
        borderColor: "transparent"
        elevated: false
        surfaceColor: Theme.surfaceMuted
        visible: page.visibleExercises.length === 0
        width: parent.width

        Label {
            color: Theme.textMuted
            font: Typography.caption
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Ничего не найдено")
            width: parent.width
        }
    }
}
