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
    property var viewModel: typeof ExercisesVM !== "undefined" ? ExercisesVM : null
    readonly property bool hasViewModel: page.viewModel !== undefined && page.viewModel !== null
    readonly property bool failed: page.hasViewModel && page.viewModel.errorKey !== ""
    readonly property bool showList: page.hasViewModel && page.viewModel.exercise_list.count > 0
    // Ключи справочников выбранных фильтров (multi): пустой набор — «Все».
    property var groupFilters: []
    property var equipmentFilters: []

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

        // Фильтры вкладки — источник истины при показе: мини-окно выбора
        // делит каталог с вкладкой и сбрасывает общий поиск на время выбора.
        // Неизменённые значения уходят no-op'ами, лишних запросов нет.
        page.viewModel.setQuery(searchField.text);
        page.viewModel.setMuscleGroups(page.groupFilters);
        page.viewModel.setEquipments(page.equipmentFilters);

        page.viewModel.reload();
        page.viewModel.loadCatalogs();
    }
    // Автодогрузка: доскроллили до конца — запрашиваем следующую страницу.
    // Переключает ключ в наборе фильтра; индекс 0 («Все») набор очищает.
    function toggledFilter(current, keys, index) {
        if (index === 0)
            return [];

        const key = keys[index - 1];
        const next = current.slice();
        const at = next.indexOf(key);

        if (at >= 0)
            next.splice(at, 1);
        else
            next.push(key);

        return next;
    }
    // Индексы чипов набора: «Все» (0) активен, пока набор пуст.
    function activeChipIndices(current, keys) {
        if (current.length === 0)
            return [0];

        return current.map(function (key) {
            return keys.indexOf(key) + 1;
        });
    }
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
        objectName: "muscleGroupChips"
        items: [qsTr("Все")].concat(page.hasViewModel ? page.viewModel.muscleGroups.map(function (key) {
            return ExerciseTexts.muscleGroupTitle(key);
        }) : [])
        selectedIndices: page.hasViewModel ? page.activeChipIndices(page.groupFilters, page.viewModel.muscleGroups) : [0]
        width: parent.width

        onSelected: function (index) {
            if (page.hasViewModel === false)
                return;

            page.groupFilters = page.toggledFilter(page.groupFilters, page.viewModel.muscleGroups, index);
            page.viewModel.setMuscleGroups(page.groupFilters);
        }
    }
    FilterChips {
        objectName: "equipmentChips"
        items: [qsTr("Любой инвентарь")].concat(page.hasViewModel ? page.viewModel.equipments.map(function (key) {
            return ExerciseTexts.equipmentTitle(key);
        }) : [])
        selectedIndices: page.hasViewModel ? page.activeChipIndices(page.equipmentFilters, page.viewModel.equipments) : [0]
        visible: page.hasViewModel && page.viewModel.equipments.length > 0
        width: parent.width

        onSelected: function (index) {
            if (page.hasViewModel === false)
                return;

            page.equipmentFilters = page.toggledFilter(page.equipmentFilters, page.viewModel.equipments, index);
            page.viewModel.setEquipments(page.equipmentFilters);
        }
    }
    // — Каталог плитками: картинка + название, по две в строке —
    // Flow внутри колонки страницы: ширина плитки считается от ширины
    // колонки, а не от Layout (колонка контента не layout).
    Flow {
        id: tileFlow

        readonly property real tileWidth: Math.floor((width - spacing) / 2)

        objectName: "exerciseCatalogTiles"
        spacing: Spacing.listGap
        visible: page.showList
        width: parent.width

        Repeater {
            model: page.showList ? page.viewModel.exercise_list : null

            delegate: CatalogExerciseTile {
                photoUrl: model.photoUrl !== undefined ? model.photoUrl : ""
                title: model.title
                width: tileFlow.tileWidth

                onClicked: page.openRequested(model.id)
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
