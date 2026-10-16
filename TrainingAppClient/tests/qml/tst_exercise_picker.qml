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
        page = createTemporaryObject(dayPageComponent, testCase);
        verify(page !== null);
        page.parent = testCase.Window.window.contentItem;
        wait(0);
    }

    function test_rowsRender() {
        const popup = page.exercisePickerItem;

        popup.catalog = catalogStub;
        popup.open();
        wait(200);

        verify(popup.visible, "окно не открылось");
        verify(popup.showList === true, "каталог не признан непустым");

        const list = testCase.findByName(popup.contentItem, "exercisePickerList");

        verify(list !== null, "список не найден");
        compare(list.count, 2, "модель каталога не отдала строки списку");

        const rows = [];

        testCase.collectByName(popup.contentItem, "exercisePickerRow", rows);
        compare(rows.length, 2, "строки каталога не отрисовались в окне");
        compare(rows[0].title, "Присед");
        verify(rows[0].id === "builtin-squat", "роль id не дошла до делегата");
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
        wait(200);

        verify(popup.visible);
        verify(popup.expandedIndex === -1, "окно открылось с раскрытой строкой");

        popup.expandedIndex = 0;
        wait(50);

        const addButton = testCase.findByName(popup.contentItem, "exercisePickerAdd");

        verify(addButton !== null, "в раскрытой строке нет кнопки «Добавить»");
        verify(addButton.visible === true, "кнопка «Добавить» не показана");

        signalSpy.target = popup;
        signalSpy.signalName = "picked";
        signalSpy.clear();
        addButton.clicked();

        compare(signalSpy.count, 1, "кнопка «Добавить» не отправила сигнал picked");
        compare(signalSpy.signalArguments[0][0], "builtin-squat", "picked: не тот id упражнения");
        compare(signalSpy.signalArguments[0][1], "Присед", "picked: не то название");
        compare(signalSpy.signalArguments[0][3], 4, "picked: не то число подходов по умолчанию");
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

    name: "ExercisePicker"
}
