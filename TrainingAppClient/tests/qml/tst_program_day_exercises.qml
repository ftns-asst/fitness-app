import QtQuick
import QtTest
import TrainingAppClient 1.0

// Экран настройки дня программы.
//
// Регрессия: счётчик упражнений читал `day_exercises.rowCount()` — вызов метода
// внутри привязки не создаёт зависимости, счётчик замирал на значении первого
// вычисления, контейнер списка оставался `visible: false`, и добавленное
// упражнение появлялось только после переоткрытия экрана. Свойство `count`
// модели несёт сигнал, поэтому привязка обязана жить на нём (тот же урок, что в
// ProgramRulesPickerPage.qml).
TestCase {
    id: testCase

    property var page: null

    // Заглушка модели упражнений дня: ListModel, а не произвольный объект, —
    // Repeater привязывается к ней же, и делегаты обязаны уметь создаваться.
    ListModel {
        id: dayExercisesStub
    }
    // Заглушка ViewModel: методы без побочных эффектов, кроме честного
    // изменения той самой модели, на которую смотрит экран.
    QtObject {
        id: editStub

        property var day_exercises: dayExercisesStub

        function addDayExercise(weekday, exerciseId, sets, repsMin, repsMax, weight, hasWeight) {
            dayExercisesStub.append({
                exerciseId: exerciseId,
                repsText: repsMin + "–" + repsMax,
                orderIndex: dayExercisesStub.count + 1,
                sets: sets,
                targetWeightKg: hasWeight ? weight : 0
            });

            return true;
        }
        function copyDay(fromWeekday, toWeekday) {
            return true;
        }
        function removeDayExercise(weekday, index) {
            dayExercisesStub.remove(index);

            return true;
        }
        function selectDay(weekday) {
            return weekday;
        }
        function setDayKind(weekday, kind) {
            return true;
        }
    }
    Component {
        id: dayPageComponent

        ProgramDayPage {
            viewModel: editStub
        }
    }

    // Тап по строке дня открывает обзор упражнения: сигнал слушает каркас
    // (AppShell). Цель — сама страница, она пересоздаётся на каждый тест.
    SignalSpy {
        id: openSpy

        signalName: "exerciseOpenRequested"
        target: page !== null ? page : null
    }

    function cleanup() {
        page = null;
    }
    // Элементы ищутся по objectName: id компонента снаружи не видны, а страница
    // лежит внутри каркаса со вложенными карточками.
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
        dayExercisesStub.clear();
        page = createTemporaryObject(dayPageComponent, testCase);

        verify(page !== null);

        // Страница переносится в окно теста: его корень — сам TestCase, и без
        // переноса `visible` была бы унаследованным false (тот же приём, что в
        // tst_program_rules_picker).
        page.parent = testCase.Window.window.contentItem;

        // Без явного размера каркас 0×0: строки упражнений остаются вне клипа
        // Flickable, и клики по ним не доставляются (тот же приём, что в
        // tst_exercises_editor_chips). Окно раннера 200×200 — тоже телефонное.
        page.width = 360;
        page.height = 800;
        page.Window.window.width = 390;
        page.Window.window.height = 900;

        wait(0);
    }
    // Счётчик следует за моделью: у `count` есть сигнал, у вызова `rowCount()`
    // его нет — привязка на вызове замирала на значении первого вычисления.
    function test_exerciseCounterFollowsModel() {
        compare(page.exerciseCount, 0);

        dayExercisesStub.append({
            exerciseId: "ex_squat",
            repsText: "5–8",
            orderIndex: 1,
            sets: 4,
            targetWeightKg: 100
        });

        compare(page.exerciseCount, 1);

        dayExercisesStub.append({
            exerciseId: "ex_bench",
            repsText: "8–12",
            orderIndex: 2,
            sets: 3,
            targetWeightKg: 60
        });

        compare(page.exerciseCount, 2);
    }
    // Список появляется сразу после добавления и исчезает после удаления:
    // видимость контейнера считается от того же счётчика, что и подпись экрана.
    function test_listAppearsAndHidesWithCount() {
        const list = testCase.findByName(page, "dayExerciseList");

        verify(list !== null, "в экране дня нет списка упражнений");
        verify(list.visible === false, "список показан при пустом дне");

        page.addExercise();

        compare(page.exerciseCount, 1);
        verify(list.visible === true, "добавленное упражнение не появилось сразу");
        compare(page.caption, "1 упражнений");

        page.removeExercise(0);

        compare(page.exerciseCount, 0);
        verify(list.visible === false, "список остался видимым на пустом дне");
    }
    // Выбор упражнения — в мини-окне: кнопка открывает его, выбор из окна
    // добавляет переданное упражнение с его подходами и весом. ComboBox
    // убран: каталог большой, перебор закрытого списка неудобен.
    function test_pickerOpensAndPicks() {
        const button = testCase.findByName(page, "exercisePickerButton");

        verify(button !== null, "в экране дня нет кнопки выбора упражнения");

        button.clicked();

        // Popup живёт в Overlay, а не в детях страницы: ссылку на него отдаёт
        // свойство exercisePickerItem (поиск по objectName его не находит).
        const popup = page.exercisePickerItem;

        verify(popup !== undefined && popup !== null, "в экране дня нет окна выбора упражнения");
        verify(popup.visible, "окно выбора не открылось по кнопке");

        popup.close();
        // Закрытие анимировано: visible гаснет по окончании exit-перехода.
        tryVerify(function () {
            return popup.visible === false;
        }, 2000, "окно не закрылось по «Закрыть»");

        // Явный выбор: упражнение из окна приходит с его подходами и весом.
        page.addExercise("builtin-squat", 5, 87.5);

        compare(page.exerciseCount, 1);
        compare(dayExercisesStub.get(0).exerciseId, "builtin-squat");
        compare(dayExercisesStub.get(0).sets, 5);
        compare(dayExercisesStub.get(0).targetWeightKg, 87.5);
    }
    // Строка упражнения дня: карточка делегата ищется по роли exerciseId —
    // objectName делегатам Repeater из модели не задать.
    function findExerciseRow(list, index) {
        let seen = 0;

        for (let i = 0; i < list.children.length; ++i) {
            const child = list.children[i];

            if (child !== null && child !== undefined && child.exerciseId !== undefined) {
                if (seen === index)
                    return child;

                ++seen;
            }
        }

        return null;
    }
    // Название для списка дня берётся из каталога VM, а без каталога — из
    // локального набора: id в предписании должен читаться по-человечески.
    function test_exerciseTitleFallsBack() {
        compare(page.exerciseTitle("ex_squat"), "Присед");
        compare(page.exerciseTitle("unknown-id"), "unknown-id");
    }
    // Тап по строке упражнения дня открывает его обзор — тот же вход, что и
    // из каталога вкладки «Упражнения». Клик идёт в центр карточки, мимо
    // кнопки «Убрать»: тап не должен удалять упражнение вместо открытия.
    function test_rowTapOpensExerciseDetail() {
        page.addExercise("ex_squat", 4, 100);

        const list = testCase.findByName(page, "dayExerciseList");
        const row = testCase.findExerciseRow(list, 0);

        verify(row !== null, "строка упражнения не найдена в списке дня");
        verify(openSpy.target !== null, "шпион не слушает страницу");

        openSpy.clear();

        // Клик — в левую четверть карточки, в зону названия: карточка в
        // тестовом окне узкая (Layout.fillWidth работает только внутри
        // раскладки полного экрана), и центр попадал в кнопку «Убрать» —
        // она принимала нажатие, и тап не доходил до TapHandler.
        testCase.mouseClick(row, row.width * 0.25, row.height / 2);
        tryCompare(openSpy, "count", 1);
        compare(openSpy.signalArguments[0][0], "ex_squat", "тап отправил не тот id упражнения");
    }
}
