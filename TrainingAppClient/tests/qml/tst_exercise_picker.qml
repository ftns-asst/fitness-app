import QtQuick
import QtTest
import TrainingAppClient 1.0

// Окно выбора упражнения в дне программы (ExercisePickerPopup).
//
// Строки каталога рендерятся из ролей id/title/muscleGroups — регрессия: без
// роли в roleNames required-свойство делегата не инициализировалось, и список
// выглядел пустым, хотя модель строки содержала. Раскрытие строки открывает
// форму добавления, и «Добавить» отправляет picked с id, весом и подходами.
TestCase {
    id: testCase

    property var page: null

    ListModel {
        id: catalogList
    }

    Component.onCompleted: {
        catalogList.append({
            id: "builtin-squat",
            title: "Присед",
            muscleGroups: ["legs"],
            subtitle: "Ноги"
        });
        catalogList.append({
            id: "builtin-bench",
            title: "Жим лёжа",
            muscleGroups: ["chest"],
            subtitle: "Грудь"
        });
    }

    QtObject {
        id: catalogStub

        property var exercise_list: catalogList
        property var muscleGroups: ["legs", "chest"]
        property var equipments: ["barbell", "dumbbell"]
        property int catalogLoads: 0
        property string lastType: "all"
        property var lastEquipment: []
        function setCatalogType(type) {
            lastType = type;
        }
        function setFilters(groups, equipment) {
            lastEquipment = equipment;
        }
        property bool loading: false
        property string errorKey: ""
        property bool canLoadMore: false

        function reload() {
            return true;
        }
        function loadMore() {
            return true;
        }
        function setQuery(q) {
        }
        function setMuscleGroups(g) {
        }
        function loadCatalogs() {
            ++catalogLoads;
        }
        function resetFilters() {
        }
    }

    Component {
        id: dayPageComponent

        ProgramDayPage {
            viewModel: editStub
        }
    }

    QtObject {
        id: editStub

        property var day_exercises: ListModel {}

        function addDayExercise(weekday, exerciseId, sets, repsMin, repsMax, weight, hasWeight) {
            return true;
        }
        function copyDay(from, to) {
            return true;
        }
        function removeDayExercise(w, i) {
            return true;
        }
        function selectDay(w) {
            return w;
        }
        function setDayKind(w, k) {
            return true;
        }
    }

    function findByName(parent, name) {
        if (parent === null || parent === undefined)
            return null;

        const kids = parent.children;

        if (kids === undefined)
            return null;

        for (let i = 0; i < kids.length; ++i) {
            if (kids[i].objectName === name)
                return kids[i];

            const nested = testCase.findByName(kids[i], name);

            if (nested !== null)
                return nested;
        }

        return null;
    }

    function init() {
        page = createTemporaryObject(dayPageComponent, testCase, {
            width: 360,
            height: 640
        });
        verify(page !== null);
        page.parent = testCase.Window.window.contentItem;
        wait(0);
    }

    function test_openingDoesNotResetUserSelection() {
        const popup = page.exercisePickerItem;
        popup.catalog = catalogStub;
        popup.open();
        popup.expandedIndex = 0;
        popup.pickedRepsMin = 9;
        popup.pickedRepsMax = 12;
        tryCompare(popup, "opened", true);
        compare(popup.expandedIndex, 0);
        compare(popup.pickedRepsMin, 9);
        compare(popup.pickedRepsMax, 12);
    }
    function test_rowsRender() {
        const popup = page.exercisePickerItem;

        popup.catalog = catalogStub;
        popup.open();
        tryCompare(popup, "opened", true);

        verify(popup.visible, "окно не открылось");
        verify(popup.showList === true, "каталог не признан непустым");

        const list = testCase.findByName(popup.contentItem, "exercisePickerList");

        verify(list !== null, "список не найден");
        compare(list.count, 2, "модель каталога не отдала строки списку");

        const rows = [];

        testCase.collectByName(popup.contentItem, "exercisePickerRow", rows);
        compare(rows.length, 2, "строки каталога не отрисовались в окне");
        compare(rows[0].title, "Присед");
        verify(rows[0].model.id === "builtin-squat", "роль id не дошла до делегата");
        // muscleGroups: реальный AList_Model отдаёт массив, ListModel-заглушка —
        // под-модель. Данные роли проверяет tst_Exercises_VM, здесь — только
        // то, что делегат их получил.
        verify(rows[0].muscleGroups !== undefined, "роль muscleGroups не дошла до делегата");
    }

    // Раскрытие строки открывает форму добавления: кнопка «Добавить» отправляет
    // сигнал picked с id упражнения, весом и числом подходов из полей.
    function test_expandAndPick() {
        const popup = page.exercisePickerItem;

        popup.catalog = catalogStub;
        popup.open();
        tryCompare(popup, "opened", true);

        verify(popup.visible);
        verify(popup.expandedIndex === -1, "окно открылось с раскрытой строкой");

        popup.expandedIndex = 0;
        wait(50);

        const addButton = testCase.findByName(popup.contentItem, "exercisePickerAdd");

        verify(addButton !== null, "в раскрытой строке нет кнопки «Добавить»");
        // Панель проявляется анимацией: ждём её окончания, а не фиксированные 50 мс.
        tryVerify(function () {
            return addButton.visible === true;
        }, 2000, "кнопка «Добавить» не показана");

        signalSpy.target = popup;
        signalSpy.signalName = "picked";
        signalSpy.clear();
        addButton.clicked();

        compare(signalSpy.count, 1, "кнопка «Добавить» не отправила сигнал picked");
        compare(signalSpy.signalArguments[0][0], "builtin-squat", "picked: не тот id упражнения");
        compare(signalSpy.signalArguments[0][1], "Присед", "picked: не то название");
        compare(signalSpy.signalArguments[0][3], 4, "picked: не то число подходов по умолчанию");
        compare(signalSpy.signalArguments[0][4], 5);
        compare(signalSpy.signalArguments[0][5], 8);
    }

    function test_collapsingPrescriptionKeepsValues() {
        const popup = page.exercisePickerItem;
        popup.catalog = catalogStub;
        popup.open();
        tryCompare(popup, "opened", true);
        const row = testCase.findByName(popup.contentItem, "exercisePickerRow");
        row.children[0].clicked();
        testCase.findByName(popup.contentItem, "exerciseRepsMin").edited(12);
        testCase.findByName(popup.contentItem, "exercisePickerCollapse").clicked();
        compare(popup.expandedIndex, -1);
        row.children[0].clicked();
        compare(popup.expandedIndex, 0);
        compare(popup.pickedRepsMin, 12);
        compare(popup.pickedRepsMax, 12);
    }
    function test_dragClosesPickerAndKeepsPrescription() {
        const oldWidth = testCase.Window.window.width;
        const oldHeight = testCase.Window.window.height;
        testCase.Window.window.width = 390;
        testCase.Window.window.height = 812;
        const popup = page.exercisePickerItem;
        popup.catalog = catalogStub;
        popup.open();
        tryCompare(popup, "opened", true);
        const row = testCase.findByName(popup.contentItem, "exercisePickerRow");
        row.children[0].clicked();
        testCase.findByName(popup.contentItem, "exerciseRepsMin").edited(12);
        wait(250);
        const grip = testCase.findByName(popup.contentItem, "exercisePickerGrip");
        const surface = testCase.Window.window.contentItem;
        const start = grip.mapToItem(surface, grip.width / 2, grip.height / 2);
        mousePress(surface, start.x, start.y);
        wait(150);
        mouseMove(surface, start.x, start.y + 50, 30);
        mouseMove(surface, start.x, start.y + 120, 30);
        mouseRelease(surface, start.x, start.y + 120);
        tryCompare(popup, "visible", false);
        popup.open();
        tryCompare(popup, "opened", true);
        row.children[0].clicked();
        compare(popup.pickedRepsMin, 12);
        compare(popup.pickedRepsMax, 12);
        testCase.Window.window.width = oldWidth;
        testCase.Window.window.height = oldHeight;
    }
    function test_repetitionControlsKeepRangeConsistent() {
        const popup = page.exercisePickerItem;
        popup.catalog = catalogStub;
        popup.open();
        tryCompare(popup, "opened", true);
        popup.expandedIndex = 0;
        const minimum = testCase.findByName(popup.contentItem, "exerciseRepsMin");
        const maximum = testCase.findByName(popup.contentItem, "exerciseRepsMax");
        minimum.edited(12);
        compare(popup.pickedRepsMin, 12);
        compare(popup.pickedRepsMax, 12);
        maximum.edited(15);
        compare(popup.pickedRepsMax, 15);
        wait(250);
        const addButton = testCase.findByName(popup.contentItem, "exercisePickerAdd");
        verify(addButton.visible);
        verify(addButton.mapToItem(popup.contentItem, 0, 0).y + addButton.height <= popup.contentItem.height + 1);
        signalSpy.target = popup;
        signalSpy.signalName = "picked";
        signalSpy.clear();
        testCase.findByName(popup.contentItem, "exercisePickerAdd").clicked();
        compare(signalSpy.signalArguments[0][4], 12);
        compare(signalSpy.signalArguments[0][5], 15);
    }

    SignalSpy {
        id: signalSpy
    }

    function collectByName(parent, name, out) {
        if (parent === null || parent === undefined)
            return;

        const kids = parent.children;

        if (kids === undefined)
            return;

        for (let i = 0; i < kids.length; ++i) {
            if (kids[i].objectName === name)
                out.push(kids[i]);

            testCase.collectByName(kids[i], name, out);
        }
    }

    function test_selectionHasOwnScreenAndStableIdentity() {
        const popup = page.exercisePickerItem;
        popup.catalog = catalogStub;
        popup.open();
        tryCompare(popup, "opened", true);
        popup.expandedIndex = 0;
        verify(!findChild(popup.contentItem, "exercisePickerList").visible);
        try {
            catalogList.setProperty(0, "id", "different-exercise");
            signalSpy.target = popup;
            signalSpy.signalName = "picked";
            signalSpy.clear();
            findChild(popup.contentItem, "exercisePickerAdd").clicked();
            compare(signalSpy.signalArguments[0][0], "builtin-squat");
        } finally {
            catalogList.setProperty(0, "id", "builtin-squat");
        }
    }
    function test_catalogSectionsAndEquipmentFilters() {
        const popup = page.exercisePickerItem;
        popup.catalog = catalogStub;
        popup.open();
        tryCompare(popup, "opened", true);
        const sections = findChild(popup.contentItem, "exercisePickerSections");
        verify(sections !== null);
        sections.selected(2);
        compare(catalogStub.lastType, "public");
        popup.openFilters();
        popup.filtersPopup.toggleDraft("equipment", "dumbbell");
        popup.filtersPopup.apply();
        compare(catalogStub.lastEquipment, ["dumbbell"]);
    }

    function test_openingLoadsMissingEquipmentCatalog() {
        const saved = catalogStub.equipments;
        catalogStub.equipments = [];
        catalogStub.catalogLoads = 0;
        try {
            const popup = page.exercisePickerItem;
            popup.catalog = catalogStub;
            popup.open();
            compare(catalogStub.catalogLoads, 1);
        } finally {
            catalogStub.equipments = saved;
        }
    }
    name: "ExercisePicker"
}
