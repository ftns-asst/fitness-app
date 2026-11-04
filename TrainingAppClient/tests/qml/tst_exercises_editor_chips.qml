import QtQuick
import QtTest
import TrainingAppClient 1.0

// Чипы multi-выбора в форме своего упражнения (ExercisesEditorPage).
//
// Регрессия: чип отправлял в ViewModel переведённое название (chipText,
// например «Грудь»), а selected-проверка и валидация submit() сравнивают с
// ключом справочника («chest»). Выбор не отображался, сохранение падало с
// invalid_value. Тап по чипу обязан класть в selected ровно ключ.
TestCase {
    id: testCase

    property var page: null

    QtObject {
        id: editorStub

        property string name: ""
        property string description: ""
        property bool isPublic: false
        property bool selected: false
        property var availableMuscleGroups: ["chest", "legs", "back"]
        property var availableEquipments: ["barbell", "dumbbell"]
        property var selectedMuscleGroups: []
        property var selectedEquipments: []
        property string errorKey: ""
        property bool busy: false

        signal fieldsChanged

        function reset() {
            editorStub.selectedMuscleGroups = [];
            editorStub.selectedEquipments = [];
        }
        function loadCatalogs() {
        }
        function toggleMuscleGroup(group) {
            const list = editorStub.selectedMuscleGroups.slice();
            const index = list.indexOf(group);

            if (index >= 0)
                list.splice(index, 1);
            else
                list.push(group);

            editorStub.selectedMuscleGroups = list;
            return true;
        }
        function toggleEquipment(equipment) {
            const list = editorStub.selectedEquipments.slice();
            const index = list.indexOf(equipment);

            if (index >= 0)
                list.splice(index, 1);
            else
                list.push(equipment);

            editorStub.selectedEquipments = list;
            return true;
        }
        function submit() {
            return true;
        }
        function remove() {
            return true;
        }
    }

    Component {
        id: editorPageComponent

        ExercisesEditorPage {
            viewModel: editorStub
        }
    }

    function collectChips(parent, out) {
        if (parent === null || parent === undefined)
            return;

        const kids = parent.children;

        if (kids === undefined)
            return;

        for (let i = 0; i < kids.length; ++i) {
            if (kids[i].chipValue !== undefined)
                out.push(kids[i]);

            testCase.collectChips(kids[i], out);
        }
    }

    function chipByValue(chips, value) {
        for (let i = 0; i < chips.length; ++i) {
            if (chips[i].chipValue === value)
                return chips[i];
        }

        return null;
    }

    // Чипы живут ниже полей имени/описания: без прокрутки тап уходит мимо
    // видимой области экрана.
    function scrollChipIntoView(chip) {
        const flick = page.pageFlickable;

        if (flick === null || flick === undefined)
            return;

        // mapToItem отдаёт визуальную позицию относительно начала Flickable;
        // в координатах контента она сдвинута на contentY.
        const mapped = chip.mapToItem(flick, 0, 0);
        const content_y = mapped.y + flick.contentY;
        const target = content_y - flick.height / 2;

        flick.contentY = Math.max(0, Math.min(target, flick.contentHeight - flick.height));
        wait(50);
    }

    function init() {
        page = createTemporaryObject(editorPageComponent, testCase);
        verify(page !== null);
        page.parent = testCase.Window.window.contentItem;
        // Без явного размера каркас 0x0: Flickable получает отрицательную
        // высоту, и тапы по чипам уходят за пределы окна.
        page.width = 360;
        page.height = 800;
        // Окно раннера 200x200: чипы ниже полей формы остаются за кадром.
        page.Window.window.width = 390;
        page.Window.window.height = 900;
        wait(0);
    }

    // Тап по чипу группы мышц: в selectedMuscleGroups попадает ключ
    // справочника, а не переведённое название; чип отображает выбор.
    function test_muscleChipTapStoresKey() {
        const chips = [];

        testCase.collectChips(page, chips);
        verify(chips.length >= 5, "чипы групп мышц и инвентаря не отрисовались");

        const chest = testCase.chipByValue(chips, "chest");
        testCase.scrollChipIntoView(chest);

        verify(chest !== null, "чип «chest» не найден");
        verify(chest.selected === false, "чип выбран до тапа");

        mouseClick(chest, chest.width / 2, chest.height / 2);
        wait(50);

        compare(editorStub.selectedMuscleGroups.length, 1, "тап по чипу не изменил выбор");
        compare(editorStub.selectedMuscleGroups[0], "chest", "в выбор попало не значение справочника");
        verify(chest.selected === true, "чип не отобразил выбор");

        // Повторный тап снимает выбор.
        mouseClick(chest, chest.width / 2, chest.height / 2);
        wait(50);
        compare(editorStub.selectedMuscleGroups.length, 0, "повторный тап не снял выбор");
    }

    // Тап по чипу инвентаря — тот же контракт для equipment.
    function test_equipmentChipTapStoresKey() {
        const chips = [];

        testCase.collectChips(page, chips);

        const dumbbell = testCase.chipByValue(chips, "dumbbell");
        testCase.scrollChipIntoView(dumbbell);

        verify(dumbbell !== null, "чип «dumbbell» не найден");

        mouseClick(dumbbell, dumbbell.width / 2, dumbbell.height / 2);
        wait(50);

        compare(editorStub.selectedEquipments.length, 1, "тап по чипу инвентаря не изменил выбор");
        compare(editorStub.selectedEquipments[0], "dumbbell", "в выбор инвентаря попало не значение справочника");
        verify(dumbbell.selected === true, "чип инвентаря не отобразил выбор");
    }

    name: "ExercisesEditorChips"
}
