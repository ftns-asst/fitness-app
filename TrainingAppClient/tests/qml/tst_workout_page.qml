import QtQuick
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: testCase
    name: "WorkoutPage"
    visible: true
    when: windowShown
    width: 390
    height: 844

    QtObject {
        id: stub
        property var exercises: []
        property string title: "Верх тела"
        property string errorDetail: ""
        property bool completed: false
        property int completedSets: 0
        property int totalSets: 2
        property int restRemaining: 0
        property real tonnageKg: 0
        property int durationSeconds: 1520
        property int completedExercises: 1
        function adjustRest(seconds) {
            restRemaining += seconds;
            return true;
        }
        property var saved: []
        function refreshClock() {
        }
        function skipRest() {
            restRemaining = 0;
            return true;
        }
        function finish() {
            completed = true;
            return true;
        }
        function saveSet(exerciseIndex, number, weight, reps, rpe, note) {
            saved = [exerciseIndex, number, weight, reps, rpe, note];
            let rows = JSON.parse(JSON.stringify(exercises));
            rows[exerciseIndex].facts[number - 1] = {
                number: number,
                done: true,
                weightKg: weight,
                reps: reps,
                rpe: rpe >= 5 ? rpe : null,
                note: note
            };
            exercises = rows;
            completedSets = rows.reduce((n, row) => n + row.facts.filter(fact => fact.done).length, 0);
            restRemaining = 90;
            return true;
        }
    }
    FontLoader {
        source: "../../resources/InterFont/Inter-Regular.otf"
    }
    FontLoader {
        source: "../../resources/InterFont/Inter-Medium.otf"
    }
    FontLoader {
        source: "../../resources/InterFont/Inter-SemiBold.otf"
    }
    FontLoader {
        source: "../../resources/InterFont/Inter-Bold.otf"
    }
    Component {
        id: pageComponent
        WorkoutPage {
            width: testCase.width
            height: testCase.height
            viewModel: stub
        }
    }
    function init() {
        stub.completed = false;
        stub.completedSets = 0;
        stub.restRemaining = 0;
        stub.saved = [];
        stub.totalSets = 2;
        stub.exercises = [
            {
                title: "Жим лёжа",
                sets: 2,
                repsMin: 6,
                repsMax: 8,
                weightKg: 60,
                previous: "60 кг × 7",
                facts: [
                    {
                        number: 1,
                        done: false
                    },
                    {
                        number: 2,
                        done: false
                    }
                ]
            }
        ];
    }
    function test_saveUsesEnteredFactsAndAdvances() {
        const page = createTemporaryObject(pageComponent, testCase);
        verify(page !== null);
        compare(findChild(page, "workoutWeightInput").text, "60");
        findChild(page, "workoutWeightInput").text = "62.5";
        findChild(page, "workoutRepsInput").text = "7";
        const saveButton = findChild(page, "workoutSaveSetButton");
        verify(saveButton.enabled);
        verify(saveButton.mapToItem(page, 0, 0).y < page.height);
        wait(100);
        mouseClick(saveButton);
        compare(stub.saved.join(","), "0,1,62.5,7,-1,");
        compare(page.setNumber, 2);
        compare(findChild(page, "workoutWeightInput").numberValue, 62.5);
        page.choose(0, 1);
        compare(findChild(page, "workoutWeightInput").numberValue, 62.5);
        compare(findChild(page, "workoutRepsInput").text, "7");
    }
    function test_emptyWorkoutCannotFinish() {
        const page = createTemporaryObject(pageComponent, testCase);
        verify(page !== null);
        verify(!findChild(page, "workoutFinishButton").enabled);
        verify(page.pageFlickable.contentWidth <= page.width);
    }

    function test_optionalFactsSurviveEditing() {
        const page = createTemporaryObject(pageComponent, testCase);
        page.rpeValue = 8;
        findChild(page, "workoutNoteInput").text = "Изменил хват";
        page.saveCurrent();
        compare(stub.saved[4], 8);
        compare(stub.saved[5], "Изменил хват");
        page.choose(0, 1);
        verify(page.editingSet);
        compare(page.rpeValue, 8);
        compare(findChild(page, "workoutNoteInput").text, "Изменил хват");
    }
    function test_selectedExerciseAdvancesWithoutJumpingBack() {
        stub.exercises = stub.exercises.concat([JSON.parse(JSON.stringify(stub.exercises[0]))]);
        stub.totalSets = 4;
        const page = createTemporaryObject(pageComponent, testCase);
        page.selectExercise(1);
        page.saveCurrent();
        compare(page.exerciseIndex, 1);
        compare(page.setNumber, 2);
        compare(stub.saved[0], 1);
    }
    function test_finishAndSummary() {
        const page = createTemporaryObject(pageComponent, testCase);
        page.saveCurrent();
        tryCompare(findChild(page, "workoutSubmitCooldown"), "running", false);
        compare(page.setNumber, 2);
        page.saveCurrent();
        compare(stub.saved[1], 2);
        compare(stub.completedSets, 2);
        verify(page.allRecorded);
        verify(findChild(page, "workoutFinishButton").visible);
        verify(!findChild(page, "workoutSaveSetButton").visible);
        tryCompare(findChild(page, "workoutSubmitCooldown"), "running", false);
        page.requestFinish();
        verify(page.finished);
        page.expandedResult = 0;
        wait(250);
        verify(page.pageFlickable.contentWidth <= page.width);
    }
    function test_invalidValuesDoNotSave() {
        const page = createTemporaryObject(pageComponent, testCase);
        findChild(page, "workoutRepsInput").text = "";
        page.saveCurrent();
        compare(stub.saved.length, 0);
        verify(!findChild(page, "workoutSaveSetButton").enabled);
    }
    function test_stepperBoundsAndDecimalComma() {
        const page = createTemporaryObject(pageComponent, testCase);
        const weight = findChild(page, "workoutWeightInput");
        weight.text = "62,5";
        verify(weight.valid);
        weight.change(1);
        compare(weight.numberValue, 65);
        weight.setValue(0);
        weight.change(-1);
        compare(weight.numberValue, 0);
        const reps = findChild(page, "workoutRepsInput");
        reps.setValue(1);
        reps.change(-1);
        compare(reps.numberValue, 1);
    }
    function test_actionRemainsPinnedOnSmallScreen() {
        const page = createTemporaryObject(pageComponent, testCase, {
            width: 360,
            height: 640
        });
        const action = findChild(page, "workoutSaveSetButton");
        const before = action.mapToItem(page, 0, 0).y;
        page.pageFlickable.contentY = 200;
        compare(action.mapToItem(page, 0, 0).y, before);
        verify(before + action.height <= page.height);
    }
}
