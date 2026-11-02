import QtQuick
import QtTest
import TrainingAppClient 1.0

// Фильтры каталога упражнений (ExercisesPage): группы мышц и инвентарь —
// multi-выбор. Тап по чипу переключает ключ в наборе и отправляет в
// ViewModel весь набор; чип «Все» (индекс 0) набор очищает и активен, пока
// набор пуст.
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
        property var lastGroups: []
        property var lastEquipments: []

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
    function test_groupsAreMultiSelect() {
        const chips = chipsNamed("muscleGroupChips");

        compare(chips.selectedIndices, [0]);

        chips.selected(1);
        chips.selected(3);
        compare(catalogStub.lastGroups, ["chest", "back"]);
        compare(chips.selectedIndices, [1, 3]);

        chips.selected(1);
        compare(catalogStub.lastGroups, ["back"]);
    }

    function test_allChipClearsSet() {
        const chips = chipsNamed("muscleGroupChips");

        chips.selected(2);
        chips.selected(0);
        compare(catalogStub.lastGroups, []);
        compare(chips.selectedIndices, [0]);
    }

    function test_equipmentFilterIsIndependent() {
        const groups = chipsNamed("muscleGroupChips");
        const equipment = chipsNamed("equipmentChips");

        groups.selected(1);
        equipment.selected(2);
        compare(catalogStub.lastGroups, ["chest"]);
        compare(catalogStub.lastEquipments, ["dumbbell"]);
        compare(equipment.selectedIndices, [2]);
    }
}
