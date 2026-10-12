import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Экран «Упражнения» (§2.2): поиск + «+ Своё» в одной строке, chips-фильтры
// мышечных групп, каталог отдельными карточками.
//
// Данные — с живого API через Avm_Exercises (спринт U4): поиск и фильтры —
// серверные параметры, пагинация автодогружается при доскролле. Без сети
// репозиторий отдаёт кэш с маркером fromCache — каталог остаётся читаемым
// с плашкой «сохранённый каталог». ViewModel приходит контекстным свойством
// (ExercisesVM); в qmltestrunner его нет — все обращения под guard'ом null.
PageScaffold {
    id: page

    // Контекстное свойство ExercisesVM (Avm_Exercises); в qmltestrunner его
    // нет — typeof безопасен на необъявленном идентификаторе, страница
    // откатывается в состояния без данных.
    readonly property var viewModel: typeof ExercisesVM !== "undefined" ? ExercisesVM : null
    readonly property bool hasViewModel: page.viewModel !== undefined && page.viewModel !== null
    readonly property bool failed: page.hasViewModel && page.viewModel.errorKey !== ""
    readonly property bool showList: page.hasViewModel && page.viewModel.exercise_list.count > 0
    property string groupFilter: ""

    title: qsTr("Упражнения")
    caption: page.viewModel !== undefined && page.viewModel !== null && page.viewModel.fromCache ? qsTr("Показан сохранённый каталог") : ""

    Component.onCompleted: page.reload()
    onVisibleChanged: {
        if (visible)
            page.reload();
    }

    function reload() {
        if (page.hasViewModel === false)
            return;

        page.viewModel.reload();
        page.viewModel.loadCatalogs();
    }
    // Автодогрузка: доскроллили до конца — запрашиваем следующую страницу.
    function maybeLoadMore() {
        if (page.hasViewModel === false)
            return;

        if (page.viewModel.canLoadMore && page.viewModel.loading === false && page.showList)
            page.viewModel.loadMore();
    }
    Connections {
        function onAtYEndChanged() {
            if (page.pageFlickable.atYEnd)
                page.maybeLoadMore();
        }

        target: page.pageFlickable
    }

    // — Обработка «+ Своё»: открытие формы создания упражнения —
    signal createOwnRequested
    signal openRequested(string exerciseId)

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

            onTextChanged: {
                if (page.hasViewModel)
                    page.viewModel.setQuery(text);
            }
        }
        AppButton {
            compact: true
            text: qsTr("+ Своё")
            variant: AppButton.Primary

            onClicked: page.createOwnRequested()
        }
    }
    FilterChips {
        currentIndex: page.groupFilter.length === 0 ? 0 : (page.hasViewModel ? page.viewModel.muscleGroups.indexOf(page.groupFilter) + 1 : 0)
        items: [qsTr("Все")].concat(page.hasViewModel ? page.viewModel.muscleGroups : [])
        width: parent.width

        onSelected: function (index) {
            page.groupFilter = index === 0 ? "" : page.viewModel.muscleGroups[index - 1];

            if (page.hasViewModel)
                page.viewModel.setMuscleGroups(page.groupFilter.length === 0 ? [] : [page.groupFilter]);
        }
    }
    // — Каталог: роли title/subtitle — mock-контракт, делегат не менялся —
    Column {
        spacing: Spacing.listGap
        visible: page.showList
        width: parent.width

        Repeater {
            model: page.showList ? page.viewModel.exercise_list : null

            delegate: CatalogExerciseRow {
                subtitle: ExerciseTexts.groupsSubtitle(model.muscleGroups)
                title: model.title
                width: parent.width

                onClicked: Demo.notify(qsTr("Карточка упражнения и техника — в разработке"))
            }
        }
    }
    // — Загрузка первой страницы —
    AppCard {
        borderColor: "transparent"
        elevated: false
        surfaceColor: Theme.surfaceMuted
        visible: page.hasViewModel && page.viewModel.loading && page.showList === false && page.failed === false
        width: parent.width

        Label {
            color: Theme.textMuted
            font: Typography.caption
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Загрузка…")
            width: parent.width
        }
    }
    // — Пусто: сервер ответил пустой страницей, а не «мы не загрузили» —
    AppCard {
        borderColor: "transparent"
        elevated: false
        surfaceColor: Theme.surfaceMuted
        visible: page.hasViewModel && page.viewModel.loading === false && page.failed === false && page.showList === false
        width: parent.width

        Label {
            color: Theme.textMuted
            font: Typography.caption
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Ничего не найдено")
            width: parent.width
        }
    }
    // — Ошибка сервера: повтор остаётся доступен —
    AppCard {
        width: parent.width
        visible: page.failed

        ColumnLayout {
            width: parent.width
            spacing: Spacing.itemGap

            Label {
                Layout.fillWidth: true
                color: Theme.textSecondary
                font: Typography.body
                text: page.failed ? ApiErrorText.textFor(page.viewModel.errorKey) : ""
                wrapMode: Text.WordWrap
            }
            AppButton {
                Layout.fillWidth: true
                onClicked: page.reload()
                text: qsTr("Повторить")
            }
        }
    }
}
