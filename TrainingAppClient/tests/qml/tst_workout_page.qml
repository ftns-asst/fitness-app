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
    function test_mediaFollowsSelectedExercise() {
        const initial = Object.assign({}, stub.exercises[0], {
            exerciseId: "press"
        });
        const next = Object.assign({}, initial, {
            exerciseId: "row",
            title: "Тяга"
        });
        stub.exercises = [initial, next];
        const mediaModel = createTemporaryObject(mediaStub, testCase);
        const page = createTemporaryObject(pageComponent, testCase, {
            mediaViewModel: mediaModel
        });
        const media = findChild(page, "workoutExerciseMedia");
        verify(media !== null);
        tryCompare(mediaModel, "exerciseId", "press");
        tryVerify(() => media.visible);
        tryCompare(media, "height", Math.round(page.height * 0.55));
        compare(media.width, page.width);
        page.selectExercise(1);
        verify(!media.visible);
        tryCompare(mediaModel, "exerciseId", "row");
        tryVerify(() => media.visible);
        compare(media.exerciseId, "row");
        compare(mediaModel.videoRequests, 0);
    }

    function test_scrollCanHideTheEntireMediaHeader() {
        stub.exercises = [Object.assign({}, stub.exercises[0], {
                exerciseId: "press"
            })];
        const model = createTemporaryObject(mediaStub, testCase);
        const page = createTemporaryObject(pageComponent, testCase, {
            mediaViewModel: model
        });
        const media = findChild(page, "workoutExerciseMedia");
        tryCompare(media, "height", Math.round(page.height * 0.55));
        const flick = page.pageFlickable;
        verify(flick.contentHeight - flick.height >= media.height);
        flick.contentY = media.height * 0.5;
        verify(media.backdropOffset > 0);
        verify(media.backdropOpacity < 1);
        flick.contentY = media.height;
        tryVerify(() => media.mapToItem(page, 0, media.height).y <= 0);
        page.saveCurrent();
        compare(stub.completedSets, 1);
        verify(flick.contentY >= media.height - 1);
        flick.contentY = 0;
        verify(media.mapToItem(page, 0, media.height).y > 0);

        mouseDrag(page, 300, 450, 0, -165, Qt.LeftButton);
        tryVerify(() => page.heroCollapsed, 1500, `scroll position ${flick.contentY}, hero ${media.height}`);
        tryVerify(() => !flick.moving);
        flick.contentY = media.height;
        mouseDrag(page, 28, 275, 0, 150, Qt.LeftButton);
        tryCompare(flick, "contentY", 0);
    }

    Component {
        id: mediaStub

        QtObject {
            property string exerciseId: ""
            property bool hasVideo: exerciseId !== ""
            property string videoUrl: ""
            property bool loading: false
            property int videoRequests: 0

            function load(id) {
                exerciseId = id;
            }
            function reset() {
                exerciseId = "";
            }
            function loadVideo() {
                videoRequests++;
            }
        }
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
    function test_exerciseTabsAndNextNameFollowRemainingSets() {
        const first = {
            title: "Жим",
            sets: 1,
            facts: [
                {
                    number: 1,
                    done: false
                }
            ]
        };
        const completed = {
            title: "Тяга",
            sets: 1,
            facts: [
                {
                    number: 1,
                    done: true,
                    weightKg: 25,
                    reps: 8
                }
            ]
        };
        const last = {
            title: "Приседания",
            sets: 1,
            facts: [
                {
                    number: 1,
                    done: false
                }
            ]
        };
        stub.exercises = [first, completed, last];
        stub.totalSets = 3;
        stub.completedSets = 1;

        const page = createTemporaryObject(pageComponent, testCase);
        const tabs = findChild(page, "workoutExerciseTabs");
        const next = findChild(page, "workoutNextExercise");
        compare(tabs.items, ["Жим · 0/1", "Тяга · 1/1", "Приседания · 0/1"]);
        compare(next.text, "Далее: Приседания");
        compare(page.exercisesRemaining, 2);

        tabs.selected(1);
        verify(page.editingSet);
        tabs.selected(0);
        verify(!page.editingSet);
        page.saveCurrent();
        compare(tabs.currentIndex, 2);
        compare(tabs.items[0], "Жим · 1/1");
        compare(page.exercisesRemaining, 1);
        verify(!next.visible);
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
    function test_weightRulerAndRepeatsSupportPreciseInput() {
        const page = createTemporaryObject(pageComponent, testCase);
        const weight = findChild(page, "workoutWeightInput");
        const weightScale = findChild(weight, "workoutValueScale");
        waitForRendering(weightScale);
        weight.text = "62,5";
        verify(weight.valid);
        mouseClick(weightScale, weightScale.width / 2 + weight.tickSpacing, weightScale.height / 2);
        compare(weight.numberValue, 65);

        const reps = findChild(page, "workoutRepsInput");
        const repsScale = findChild(reps, "workoutValueScale");
        reps.text = "10000";
        verify(reps.valid);
        mouseClick(repsScale, repsScale.width / 2 + reps.tickSpacing, repsScale.height / 2);
        compare(reps.numberValue, 10000);
        reps.setValue(1);
        mouseClick(repsScale, repsScale.width / 2 - reps.tickSpacing, repsScale.height / 2);
        compare(reps.numberValue, 1);
    }
    function test_repeatScaleDragDoesNotScrollPage() {
        const page = createTemporaryObject(pageComponent, testCase, {
            width: 360,
            height: 640
        });
        page.extrasExpanded = true;
        const reps = findChild(page, "workoutRepsInput");
        const scale = findChild(reps, "workoutValueScale");
        waitForRendering(scale);
        const verticalPosition = page.pageFlickable.contentY;
        mousePress(scale, scale.width / 2 + 75, scale.height / 2);
        mouseMove(scale, scale.width / 2 + 30, scale.height / 2, 40);
        mouseMove(scale, scale.width / 2 - 60, scale.height / 2, 40);
        mouseRelease(scale, scale.width / 2 - 60, scale.height / 2);
        tryVerify(() => reps.numberValue > 6);
        tryVerify(() => !scale.moving);
        compare(page.pageFlickable.contentY, verticalPosition);
        tryCompare(scale, "contentX", (reps.numberValue - 1) * reps.tickSpacing);
    }
    function test_scaleRespondsToPressAndReturnsToRest() {
        const page = createTemporaryObject(pageComponent, testCase);
        const weight = findChild(page, "workoutWeightInput");
        const scale = findChild(weight, "workoutValueScale");
        const marker = findChild(weight, "workoutScaleMarker");
        waitForRendering(scale);
        const restingHeight = marker.height;
        mousePress(scale, scale.width / 2, scale.height / 2);
        tryVerify(() => marker.height > restingHeight + 4);
        mouseRelease(scale, scale.width / 2, scale.height / 2);
        tryCompare(marker, "height", restingHeight);
        compare(weight.numberValue, 60);
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

    function test_recordingReturnsToTrueTopAndRevealsRest() {
        const page = createTemporaryObject(pageComponent, testCase, {
            width: 360,
            height: 580
        });
        page.extrasExpanded = true;
        const flick = page.pageFlickable;
        tryVerify(() => flick.contentHeight > flick.height);
        const start = flick.originY - flick.topMargin;
        flick.contentY = start + 90;
        page.saveCurrent();
        tryCompare(flick, "contentY", start);
        const rest = findChild(page, "workoutRestCard");
        tryVerify(() => rest.opacity >= 0.99);
        compare(page.progress, 0.5);
    }
}
