import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Экран «Упражнения» (§2.2): поиск + «+ Своё» в одной строке,
// chips-фильтры мышечных групп, каталог отдельными карточками с фото техники.
PageScaffold {
    id: page

    title: qsTr("Упражнения")

    property string query: ""
    property string groupFilter: ""

    readonly property var muscleGroups: {
        const groups = [];

        for (let i = 0; i < MockCatalog.exercises.length; ++i) {
            const group = MockCatalog.exercises[i].muscleGroup;

            if (groups.indexOf(group) < 0) {
                groups.push(group);
            }
        }

        return groups;
    }

    readonly property var visibleExercises: {
        const result = [];
        const query_text = page.query.trim().toLowerCase();

        for (let i = 0; i < MockCatalog.exercises.length; ++i) {
            const item = MockCatalog.exercises[i];
            const matches_group = page.groupFilter.length === 0 || item.muscleGroup === page.groupFilter;
            const matches_query = query_text.length === 0 || item.title.toLowerCase().indexOf(query_text) >= 0;

            if (matches_group && matches_query) {
                result.push(item);
            }
        }

        return result;
    }

    // — Поиск и добавление своего упражнения в одной строке —
    RowLayout {
        width: parent.width
        spacing: Spacing.itemGap

        TextField {
            id: searchField

            Layout.fillWidth: true
            Layout.minimumWidth: 140
            Layout.preferredHeight: Theme.touchMin
            placeholderText: qsTr("Поиск упражнения")
            placeholderTextColor: Theme.textMuted
            font: Typography.body
            color: Theme.textPrimary
            leftPadding: Spacing.screenPadding
            rightPadding: Spacing.screenPadding

            verticalAlignment: Text.AlignVCenter

            background: Rectangle {
                radius: Theme.radiusMd
                color: Theme.surface
                border.width: Theme.borderWidth
                border.color: searchField.activeFocus ? Theme.accent : Theme.border
            }

            onTextChanged: page.query = text
        }

        AppButton {
            variant: "primary"
            compact: true
            text: qsTr("+ Своё")

            onClicked: Demo.notify(qsTr("Форма своего упражнения — этап D3"))
        }
    }

    FilterChips {
        width: parent.width
        items: [qsTr("Все")].concat(page.muscleGroups)
        currentIndex: page.groupFilter.length === 0 ? 0 : page.muscleGroups.indexOf(page.groupFilter) + 1

        onSelected: function (index) {
            page.groupFilter = index === 0 ? "" : page.muscleGroups[index - 1];
        }
    }

    Column {
        width: parent.width
        spacing: Spacing.listGap
        visible: page.visibleExercises.length > 0

        Repeater {
            model: page.visibleExercises

            delegate: CatalogExerciseRow {
                width: parent.width
                title: modelData.title
                subtitle: modelData.subtitle

                onClicked: Demo.notify(qsTr("Карточка упражнения и техника — этап D3"))
            }
        }
    }

    AppCard {
        width: parent.width
        surfaceColor: Theme.surfaceMuted
        borderColor: "transparent"
        elevated: false
        visible: page.visibleExercises.length === 0

        Label {
            width: parent.width
            text: qsTr("Ничего не найдено")
            font: Typography.caption
            color: Theme.textMuted
            horizontalAlignment: Text.AlignHCenter
        }
    }
}
