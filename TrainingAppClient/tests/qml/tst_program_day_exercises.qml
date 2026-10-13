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
}
