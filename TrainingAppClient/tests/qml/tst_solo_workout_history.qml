import QtQuick
import QtTest
import TrainingAppClient

TestCase {
    id: testCase
    name: "SoloWorkoutHistory"
    width: 390
    height: 700
    visible: true
    when: windowShown

    function test_historyShowsQueueAndClearsSelectionOnLogout() {
        const sync = createTemporaryObject(syncComponent, testCase);
        const page = createTemporaryObject(pageComponent, testCase, {
            sync: sync
        });
        compare(sync.refreshes, 1);
        const row = findChild(page, "soloWorkoutHistoryRow");
        verify(row !== null);
        compare(row.subtitle, "Ожидает отправки");
        row.clicked();
        compare(page.selectedId, "local");
        compare(page.selected.id, "local");
        sync.userId = "";
        sync.accountChanged();
        compare(page.selectedId, "");
        verify(page.selected.id === undefined);
    }

    Component {
        id: pageComponent
        SoloWorkoutHistoryPage {
            width: testCase.width
            height: testCase.height
        }
    }
    Component {
        id: syncComponent
        QtObject {
            property string userId: "account"
            property string errorKey: ""
            property bool loading: false
            property bool hasMore: false
            property var history: [
                {
                    id: "local",
                    sync: "pending",
                    completed_at: "2026-11-24T12:00:00Z",
                    setCount: 2
                }
            ]
            property var detail: ({})
            property int refreshes: 0
            signal accountChanged
            function refreshHistory() {
                refreshes++;
            }
            function openWorkout(id) {
                detail = {
                    id: id,
                    completed_at: "2026-11-24T12:00:00Z",
                    exercises: []
                };
            }
        }
    }
}
