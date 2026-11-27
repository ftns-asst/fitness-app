import QtQuick
import QtTest
import TrainingAppClient 1.0

// Каталог: разделы, поиск фильтров, черновик множественного выбора и применение.
TestCase {
    id: testCase

    property var page: null

    ListModel {
        id: emptyList
    }

    QtObject {
        id: catalogStub

        property var exercise_list: emptyList
        property var muscleGroups: ["chest", "legs", "back"]
        property var equipments: ["barbell", "dumbbell"]
        property bool loading: false
        property bool fromCache: false
        property string errorKey: ""
        property bool canLoadMore: false
        property int loadMoreCalls: 0
        property string lastType: "all"
        property int filterCalls: 0
        property var lastGroups: []
        property var lastEquipments: []

        function setCatalogType(value) {
            lastType = value;
            return true;
        }
        function setFilters(groups, equipment) {
            lastGroups = groups;
            lastEquipments = equipment;
            ++filterCalls;
            return true;
        }
        function reload() {
            return true;
        }
        function loadMore() {
            ++loadMoreCalls;
            loading = true;
            return true;
        }
        function setQuery(q) {
        }
        function setMuscleGroups(g) {
            catalogStub.lastGroups = g;
            return true;
        }
        function setEquipments(e) {
            catalogStub.lastEquipments = e;
            return true;
        }
        function loadCatalogs() {
        }
    }

    Component {
        id: pageComponent

        ExercisesPage {
            viewModel: catalogStub
        }
    }

    function init() {
        catalogStub.equipments = ["barbell", "dumbbell"];
        emptyList.clear();
        catalogStub.canLoadMore = false;
        catalogStub.loading = false;
        catalogStub.loadMoreCalls = 0;
        catalogStub.lastType = "all";
        catalogStub.filterCalls = 0;
        catalogStub.lastGroups = [];
        catalogStub.lastEquipments = [];
        page = createTemporaryObject(pageComponent, testCase, {
            "width": 390,
            "height": 800
        });
        verify(page !== null);
    }

    function chipsNamed(name) {
        const found = findChild(page, name);
        verify(found !== null, name);
        return found;
    }

    function countTiles(item) {
        let count = item instanceof CatalogExerciseTile ? 1 : 0;
        for (const child of item.children)
            count += countTiles(child);
        return count;
    }
    function test_asyncFiltersDoNotHideSearch() {
        catalogStub.equipments = [];
        page.parent = testCase.Window.window.contentItem;
        for (let i = 0; i < 100; ++i)
            emptyList.append({
                id: "exercise_" + i,
                title: "Упражнение " + i,
                photoUrl: ""
            });
        waitForRendering(page);
        const grid = page.pageFlickable;
        grid.positionViewAtBeginning();
        const search = findChild(page, "exerciseSearchInput");
        verify(search.mapToItem(grid, 0, 0).y >= -1);
        catalogStub.equipments = ["barbell", "dumbbell"];
        wait(50);
        verify(search.mapToItem(grid, 0, 0).y >= -1, "загрузка фильтров не должна прятать поиск");
    }
    function test_largeCatalogRecyclesTilesAndKeepsNavigation() {
        page.parent = testCase.Window.window.contentItem;
        for (let i = 0; i < 500; ++i)
            emptyList.append({
                id: "exercise_" + i,
                title: "Упражнение " + i,
                photoUrl: ""
            });
        waitForRendering(page);
        const grid = page.pageFlickable;
        compare(grid.count, 500);
        const search = findChild(page, "exerciseSearchInput");
        verify(search !== null);
        verify(search.mapToItem(grid, 0, 0).y >= -1, "поиск должен быть виден при открытии каталога");
        verify(countTiles(page) < 50);
        grid.positionViewAtIndex(480, GridView.Beginning);
        tryVerify(() => grid.itemAtIndex(480) !== null);
        const spy = Qt.createQmlObject('import QtTest; SignalSpy {}', testCase);
        spy.target = page;
        spy.signalName = "openRequested";
        grid.itemAtIndex(480).children[0].clicked();
        compare(spy.signalArguments[0][0], "exercise_480");
        verify(countTiles(page) < 50);
        spy.destroy();
        catalogStub.canLoadMore = true;
        grid.positionViewAtEnd();
        tryCompare(catalogStub, "loadMoreCalls", 1);
    }
    function test_sectionsChangeServerSelection() {
        const sections = findChild(page, "exerciseCatalogSections");
        verify(sections !== null);
        sections.selected(1);
        compare(catalogStub.lastType, "own");
        sections.selected(2);
        compare(catalogStub.lastType, "public");
        sections.selected(0);
        compare(catalogStub.lastType, "all");
    }
    function test_filterDraftAppliesOnceAndCancelDiscardsChanges() {
        page.openFilters();
        page.filterPopup.toggleDraft("group", "chest");
        page.filterPopup.toggleDraft("equipment", "dumbbell");
        compare(catalogStub.lastGroups, []);
        page.filterPopup.close();
        page.openFilters();
        compare(page.filterPopup.draftGroups, []);
        page.filterPopup.toggleDraft("group", "back");
        page.filterPopup.toggleDraft("group", "chest");
        page.filterPopup.toggleDraft("equipment", "barbell");
        page.filterPopup.apply();
        compare(catalogStub.lastGroups, ["back", "chest"]);
        compare(catalogStub.lastEquipments, ["barbell"]);
        compare(catalogStub.filterCalls, 1);
    }
    function test_filterSearchAndReset() {
        page.openFilters();
        page.filterPopup.query = "гант";
        compare(page.filterPopup.filterKeys("equipment"), ["dumbbell"]);
        compare(page.filterPopup.filterKeys("group"), []);
        page.filterPopup.query = "";
        page.filterPopup.toggleDraft("group", "chest");
        page.filterPopup.apply();
        compare(page.activeFilterCount, 1);
        page.clearFilters();
        compare(page.activeFilterCount, 0);
        compare(catalogStub.lastGroups, []);
    }
    function test_equipmentSectionIsOneTapAway() {
        page.openFilters();
        const sections = findChild(page.filterPopup.contentItem, "exerciseFilterSections");
        verify(sections !== null);
        sections.selected(1);
        compare(page.filterPopup.kind, "equipment");
        verify(findChild(page.filterPopup.contentItem, "exerciseFilter_equipment_barbell") !== null);
        verify(findChild(page.filterPopup.contentItem, "exerciseFilter_group_chest") === null);
    }
    function test_openSheetIsDestroyedWithPage() {
        page.openFilters();
        const popup = page.filterPopup;
        verify(Qt.isQtObject(popup));
        page.destroy();
        page = null;
        tryVerify(function () {
            return !Qt.isQtObject(popup);
        });
    }
}
