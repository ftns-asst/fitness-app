import QtQuick
import QtTest
import TrainingAppClient

TestCase {
    id: testCase

    name: "DefaultPrograms"
    when: windowShown

    function test_catalogOpensSelectedProgram() {
        const backend = createTemporaryObject(backendComponent, testCase);
        const catalog = createTemporaryObject(catalogComponent, testCase, {
            viewModel: backend
        });
        compare(backend.listRequests, 0);
        catalog.active = true;
        compare(backend.listRequests, 1);
        let selectedId = "";
        catalog.programRequested.connect(programId => selectedId = programId);
        const button = findChild(catalog, "defaultProgramOpen");
        verify(button !== null);
        button.clicked();
        compare(selectedId, "example");
        catalog.active = false;
        catalog.active = true;
        compare(backend.listRequests, 1);
    }

    function test_detailLoadsAndShowsProgram() {
        const backend = createTemporaryObject(backendComponent, testCase);
        const detail = createTemporaryObject(detailComponent, testCase, {
            templateId: "example",
            viewModel: backend
        });
        compare(backend.requestedId, "example");
        compare(findChild(detail, "defaultProgramTitle").text, "Upper");
    }

    Component {
        id: catalogComponent

        DefaultProgramsCatalog {
            width: 360
        }
    }
    Component {
        id: detailComponent

        DefaultProgramDetailPage {
            width: 360
            height: 640
        }
    }
    Component {
        id: backendComponent

        QtObject {
            property int listRequests: 0
            property string requestedId: ""
            property bool loading: false
            property bool loaded: listRequests > 0
            property string errorKey: ""
            property bool detailLoading: false
            property string detailErrorKey: ""
            property var programs: listRequests > 0 ? [
                {
                    id: "example",
                    name: "Upper",
                    days_per_week: 2,
                    weeks_count: 4
                }
            ] : []
            property var detail: requestedId === "example" ? {
                id: "example",
                name: "Upper",
                weeks_count: 4,
                workouts: []
            } : ({})

            function load(goal, level, days) {
                listRequests++;
            }

            function loadDetail(id) {
                requestedId = id;
            }
        }
    }
}
