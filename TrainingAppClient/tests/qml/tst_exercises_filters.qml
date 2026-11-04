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
        property var lastGroups: []
        property var lastEquipments: []

        function reload() {
            return true;
        }
        function loadMore() {
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
