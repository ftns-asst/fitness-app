import QtQuick
import QtTest
import TrainingAppClient

TestCase {
    id: testCase

    name: "ServerPrograms"
    when: windowShown

    function test_canSavePartialRecordsAndRejectFractionalReps() {
        const backend = createTemporaryObject(backendComponent, testCase);
        const page = createTemporaryObject(pageComponent, testCase, {
            viewModel: backend,
            initialProgramId: "draft-one"
        });
        verify(page !== null);
        compare(page.validRecords.length, 0);

        page.editInput("press", "weight", "82,5");
        compare(page.validRecords.length, 1);
        compare(page.validRecords[0].weight_kg, 82.5);
        compare(page.validRecords[0].reps, 1);
        page.editInput("press", "reps", "4.5");
        compare(page.validRecords.length, 0);
        page.editInput("press", "reps", "5");
        compare(page.validRecords[0].reps, 5);

        backend.accountChanged();
        compare(page.selectedId, "");
        compare(page.validRecords.length, 0);
        compare(backend.listLoads, 1);
    }

    function test_selectsProgramDayAndPreservesItsTargets() {
        const backend = createTemporaryObject(backendComponent, testCase);
        backend.draft = {
            id: "draft-one",
            name: "Upper",
            state: "in_progress",
            weeks: [
                {
                    week_number: 1,
                    days: [
                        {
                            day_of_week: 1,
                            exercises: []
                        }
                    ]
                },
                {
                    week_number: 2,
                    days: [
                        {
                            day_of_week: 4,
                            exercises: [
                                {
                                    exercise_id: "press",
                                    sets: 3,
                                    target_reps: 8,
                                    target_weight_kg: 65,
                                    rest_seconds: 120
                                }
                            ]
                        }
                    ]
                }
            ]
        };
        const page = createTemporaryObject(pageComponent, testCase, {
            viewModel: backend,
            initialProgramId: "draft-one"
        });
        const requested = createTemporaryObject(requestSpy, testCase, {
            target: page
        });
        const open = findChild(page, "openServerWorkoutButton");
        tryCompare(open, "enabled", true);
        open.clicked();
        compare(backend.requestedDay, [2, 4]);
        compare(requested.count, 0);
        backend.workoutReady("another-program", 2, 4, "Other", []);
        compare(requested.count, 0);
        const preparedPlan = [
            {
                exerciseId: "press",
                sets: 3,
                repsMin: 8,
                repsMax: 8,
                weightKg: 65,
                hasWeight: true,
                restSeconds: 120
            }
        ];
        backend.workoutReady("draft-one", 2, 4, "Upper", preparedPlan);
        compare(requested.count, 1);
        const args = requested.signalArguments[0];
        compare([args[0], args[1], args[2], args[3]], ["draft-one", 2, 4, "Upper"]);
        compare(args[4], preparedPlan);
    }

    Component {
        id: requestSpy

        SignalSpy {
            signalName: "workoutRequested"
        }
    }

    Component {
        id: pageComponent

        ServerProgramsPage {
            width: 360
            height: 640
        }
    }

    Component {
        id: backendComponent

        QtObject {
            property bool authenticated: true
            property bool busy: false
            property string errorKey: ""
            property var drafts: []
            property int listLoads: 0
            property var requestedDay: []
            property var draft: ({
                    id: "draft-one",
                    name: "Upper",
                    required_measurements: [
                        {
                            exercise_id: "press",
                            exercise_name: "Press"
                        },
                        {
                            exercise_id: "row",
                            exercise_name: "Row"
                        }
                    ]
                })

            signal accountChanged
            signal workoutReady(string programId, int week, int day, string title, var plan)

            function prepareWorkout(week, day) {
                requestedDay = [week, day];
            }

            function loadDrafts() {
                listLoads++;
            }
        }
    }
}
