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

            function loadDrafts() {
                listLoads++;
            }
        }
    }
}
