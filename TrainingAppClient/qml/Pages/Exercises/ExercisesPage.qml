import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Каталог: разделы, поиск, карточки и закрываемая панель фильтров.
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
    property string catalogType: "all"
    property alias filterPopup: filtersSheet
    readonly property int activeFilterCount: groupFilters.length + equipmentFilters.length
    property string searchText: ""
    property var groupFilters: []
    property var equipmentFilters: []
    property bool searchShown: true
    property real lastScrollDepth: 0
    property real directionDistance: 0
    readonly property bool atCatalogStart: !page.pageFlickable || page.isAtStart(page.pageFlickable)
    readonly property double mediaRefreshAt: hasViewModel ? (viewModel.mediaRefreshAt || 0) : 0
    readonly property bool mediaForeground: page.pageActive && Qt.application.state === Qt.ApplicationActive

    title: qsTr("Упражнения")
    largeTitle: true
    caption: page.viewModel !== undefined && page.viewModel !== null && page.viewModel.fromCache ? qsTr("Показан сохранённый каталог") : ""

    Component.onCompleted: page.reload()
    onPageActiveChanged: {
        if (pageActive)
            page.reload();
    }
    Timer {
        interval: running ? Math.max(1000, page.mediaRefreshAt - Date.now()) : 1000
        running: page.mediaForeground && page.mediaRefreshAt > 0 && !page.viewModel.loading && !page.failed
        onTriggered: page.viewModel.reload()
    }

    function reload() {
        if (page.hasViewModel === false)
            return;

        // Фильтры вкладки — источник истины при показе: мини-окно выбора
        // делит каталог с вкладкой и сбрасывает общий поиск на время выбора.
        // Неизменённые значения уходят no-op'ами, лишних запросов нет.
        page.viewModel.setCatalogType(page.catalogType);
        page.viewModel.setQuery(page.searchText);
        page.viewModel.setMuscleGroups(page.groupFilters);
        page.viewModel.setEquipments(page.equipmentFilters);

        page.viewModel.reload();
        page.viewModel.loadCatalogs();
    }
    function selectSection(index) {
        catalogType = ["all", "own", "public"][index];
        if (hasViewModel)
            viewModel.setCatalogType(catalogType);
        revealCatalogStart();
    }
    function revealCatalogStart() {
        const grid = page.pageFlickable;
        searchShown = true;
        directionDistance = 0;
        if (!grid || !(grid instanceof GridView))
            return;
        grid.cancelFlick();
        grid.forceLayout();
        grid.positionViewAtBeginning();
    }
    function updateSearch(query) {
        if (searchText === query)
            return;
        searchText = query;
        if (hasViewModel)
            viewModel.setQuery(query);
        revealCatalogStart();
    }
    function isAtStart(grid) {
        return grid.atYBeginning || grid.contentY - grid.originY <= 2;
    }
    function trackScroll(grid) {
        const depth = Math.max(0, grid.contentY - grid.originY + grid.topMargin);
        const delta = depth - lastScrollDepth;
        lastScrollDepth = depth;
        if (isAtStart(grid)) {
            searchShown = true;
            directionDistance = 0;
            return;
        }
        if (Math.abs(delta) < 0.5 || grid.verticalOvershoot !== 0)
            return;
        if (delta * directionDistance < 0)
            directionDistance = 0;
        directionDistance += delta;
        if (directionDistance >= Theme.touchMin) {
            searchShown = false;
            directionDistance = 0;
        } else if (directionDistance <= -Spacing.screenPadding) {
            searchShown = true;
            directionDistance = 0;
        }
    }
    function openFilters() {
        filtersSheet.openFor(groupFilters, equipmentFilters);
    }
    function clearFilters() {
        groupFilters = [];
        equipmentFilters = [];
        if (hasViewModel)
            viewModel.setFilters([], []);
        revealCatalogStart();
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

    scrollHeader: Component {
        Item {
            id: searchBar

            objectName: "exerciseSearchBar"
            implicitHeight: page.searchShown ? searchRow.implicitHeight + Spacing.itemGap : 0
            clip: true
            enabled: page.searchShown
            visible: height > 0

            Behavior on implicitHeight {
                NumberAnimation {
                    duration: Theme.selectionDuration
                    easing.type: Easing.OutCubic
                }
            }
            onEnabledChanged: {
                if (!enabled && searchField.activeFocus) {
                    page.forceActiveFocus();
                    Qt.inputMethod.hide();
                }
            }

            RowLayout {
                id: searchRow

                spacing: Spacing.itemGap
                width: parent.width

                AppInput {
                    id: searchField

                    objectName: "exerciseSearchInput"
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    Layout.preferredHeight: Theme.touchMin
                    leftPadding: Spacing.screenPadding
                    rightPadding: text.length > 0 ? Theme.touchMin : Spacing.screenPadding
                    placeholderText: qsTr("Поиск упражнения")
                    surfaceColor: Theme.surface
                    text: page.searchText

                    AppButton {
                        objectName: "exerciseSearchClear"
                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        text: "×"
                        width: Theme.touchMin
                        variant: AppButton.Ghost
                        visible: searchField.text.length > 0
                        Accessible.name: qsTr("Очистить поиск")
                        onClicked: page.updateSearch("")
                    }
                    onAccepted: Qt.inputMethod.hide()
                    onTextEdited: page.updateSearch(text)
                }
                AppButton {
                    objectName: "exerciseBackToTop"
                    text: qsTr("Наверх")
                    variant: AppButton.Secondary
                    visible: !page.atCatalogStart
                    Accessible.name: qsTr("Вернуться к началу каталога")
                    onClicked: page.revealCatalogStart()
                }
            }
        }
    }

    // Один GridView, поэтому раскрытие поисковой панели сохраняет позицию каталога.
    scrollViewComponent: Component {
        GridView {
            id: catalogGrid
            objectName: "exerciseCatalogTiles"
            readonly property real gap: Spacing.listGap
            property bool initialPositionSet: false
            function showInitialHeader() {
                if (!initialPositionSet && count > 0 && headerItem !== null) {
                    forceLayout();
                    positionViewAtBeginning();
                    initialPositionSet = true;
                }
            }
            Component.onCompleted: Qt.callLater(showInitialHeader)
            onCountChanged: Qt.callLater(showInitialHeader)
            onContentYChanged: page.trackScroll(catalogGrid)
            clip: true
            cellWidth: width / 2
            cellHeight: tileSizer.implicitHeight + gap
            model: page.showList ? page.viewModel.exercise_list : null
            reuseItems: true
            cacheBuffer: Math.max(0, height / 2)
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Theme.touchPlatform ? Flickable.DragAndOvershootBounds : Flickable.StopAtBounds
            flickDeceleration: Theme.flickDeceleration
            maximumFlickVelocity: Theme.flickMaxVelocity
            topMargin: Spacing.safeGap
            bottomMargin: Spacing.sectionGap
            ScrollBar.vertical: ScrollBar {
                policy: Theme.touchPlatform ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded
            }
            ScrollIndicator.vertical: Theme.touchPlatform ? scrollIndicator : null
            ScrollIndicator {
                id: scrollIndicator
            }
            CatalogExerciseTile {
                id: tileSizer
                visible: false
                width: catalogGrid.cellWidth - catalogGrid.gap / 2
            }
            header: Column {
                width: catalogGrid.width
                spacing: Spacing.sectionGap
                SegmentedControl {
                    objectName: "exerciseCatalogSections"
                    width: parent.width
                    items: [qsTr("Все"), qsTr("Мои"), qsTr("Сообщество")]
                    currentIndex: ["all", "own", "public"].indexOf(page.catalogType)
                    onSelected: function (index) {
                        page.selectSection(index);
                    }
                }
                RowLayout {
                    width: parent.width
                    spacing: Spacing.itemGap
                    AppButton {
                        objectName: "exerciseFiltersButton"
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: page.activeFilterCount > 0 ? qsTr("Фильтры · %1").arg(page.activeFilterCount) : qsTr("Мышцы и инвентарь")
                        variant: page.activeFilterCount > 0 ? AppButton.Primary : AppButton.Secondary
                        compact: true
                        onClicked: page.openFilters()
                    }
                    AppButton {
                        objectName: "exerciseFiltersClear"
                        text: qsTr("Сбросить")
                        variant: AppButton.Ghost
                        visible: page.activeFilterCount > 0
                        onClicked: page.clearFilters()
                    }
                    AppButton {
                        compact: true
                        text: qsTr("+ Своё")
                        variant: AppButton.Primary
                        onClicked: page.createOwnRequested()
                    }
                }
                Label {
                    width: parent.width
                    text: page.catalogType === "own" ? qsTr("Созданные вами упражнения") : page.catalogType === "public" ? qsTr("Опубликованные пользователями") : qsTr("Встроенные, ваши и упражнения сообщества")
                    font: Typography.caption
                    color: Theme.textMuted
                    wrapMode: Text.WordWrap
                    bottomPadding: Spacing.itemGap
                }
            }

            delegate: Item {
                required property int index
                required property var model
                width: catalogGrid.cellWidth
                height: catalogGrid.cellHeight
                CatalogExerciseTile {
                    x: index % 2 === 0 ? 0 : catalogGrid.gap / 2
                    width: catalogGrid.cellWidth - catalogGrid.gap / 2
                    photoUrl: model.photoUrl !== undefined ? model.photoUrl : ""
                    hasVideo: model.hasVideo === true
                    title: model.title
                    onClicked: page.openRequested(model.id)
                }
            }
            footer: Column {
                width: catalogGrid.width
                spacing: Spacing.sectionGap
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
                        text: page.catalogType === "own" && page.searchText.length === 0 && page.activeFilterCount === 0 ? qsTr("Ваши упражнения появятся здесь. Нажмите «+ Своё», чтобы добавить первое.") : qsTr("Ничего не найдено. Попробуйте изменить запрос или сбросить фильтры.")
                        wrapMode: Text.WordWrap
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
        }
    }
    ExerciseFiltersPopup {
        id: filtersSheet
        muscleGroups: page.hasViewModel ? page.viewModel.muscleGroups : []
        equipments: page.hasViewModel ? page.viewModel.equipments : []
        onApplied: function (groups, equipment) {
            page.groupFilters = groups;
            page.equipmentFilters = equipment;
            if (page.hasViewModel)
                page.viewModel.setFilters(groups, equipment);
            page.revealCatalogStart();
        }
    }
}
