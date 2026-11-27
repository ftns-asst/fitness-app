import QtQuick
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: testCase
    name: "StrengthPage"
    visible: true
    when: windowShown
    width: 480
    height: 900

    QtObject {
        id: stub
        property bool hasData: false
        property string programTitle: "Test program"
        property string errorKey: ""
        property int repTolerance: 2
        property var programIndex: ({
                percent: null,
                better: 0,
                worse: 0,
                evaluated: 0
            })
        property var exercises: exerciseRows
        property var splits: splitRows
        property var status_rows: sourceHistoryRows
        property var status_plot: plotRows
        property var lastReload: []
        signal changed
        function reload(userId, programId) {
            lastReload = [userId, programId];
            return true;
        }
    }
    ListModel {
        id: exerciseRows
    }
    ListModel {
        id: splitRows
    }
    ListModel {
        id: sourceHistoryRows
    }
    ListModel {
        id: plotRows
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
        StrengthPage {
            width: 390
            height: 844
            viewModel: stub
            userId: "u"
            programId: "p"
        }
    }

    function init() {
        stub.hasData = false;
        stub.errorKey = "";
        stub.lastReload = [];
        exerciseRows.clear();
        splitRows.clear();
        sourceHistoryRows.clear();
        plotRows.clear();
    }
    function test_routesExplicitIdentity() {
        const page = createTemporaryObject(pageComponent, testCase);
        verify(page !== null);
        compare(stub.lastReload[0], "u");
        compare(stub.lastReload[1], "p");
        page.programId = "other";
        compare(stub.lastReload[1], "other");
    }
    function test_missingFactsRemainMissing() {
        const page = createTemporaryObject(pageComponent, testCase);
        verify(page !== null);
        compare(page.numberText(null), "—");
        compare(page.numberText(0), "0");
        compare(page.signedText(5), "+5");
        verify(!page.hasData);
    }
    function test_embeddedAndStandaloneBack() {
        const page = createTemporaryObject(pageComponent, testCase);
        verify(page !== null);
        verify(!page.embedded);
        const back = findChild(page, "strengthBack");
        verify(back !== null);
        verify(back.visible);
        page.embedded = true;
        verify(!back.visible);
        compare(page.title, "");
    }
    function test_dataAndNarrowWidths_data() {
        return [
            {
                tag: "360",
                width: 360
            },
            {
                tag: "390",
                width: 390
            },
            {
                tag: "480",
                width: 480
            }
        ];
    }
    function test_dataAndNarrowWidths(data) {
        stub.hasData = true;
        exerciseRows.append({
            exerciseTitle: "Очень длинное название упражнения для проверки переноса",
            splitId: "upper",
            splitTitle: "Верх",
            status: "better",
            symbol: "▲",
            statusText: "Лучше",
            reasonText: "Повторов больше",
            startWeightKg: 60,
            startReps: 8,
            previousWeightKg: 60,
            previousReps: 8,
            currentWeightKg: 60,
            currentReps: 10,
            startDeltaKg: 0,
            startDeltaPercent: 0,
            deltaKg: 0,
            deltaPercent: 0,
            better: 1,
            worse: 0,
            evaluated: 1,
            indexPercent: 100,
            rangeNote: "",
            nextGoal: ""
        });
        const page = createTemporaryObject(pageComponent, testCase, {
            width: data.width
        });
        verify(page !== null);
        wait(0);
        verify(page.hasData);
        verify(page.pageFlickable.contentWidth <= data.width);
        verify(findChild(page, "strengthExerciseCard") !== null);
    }
    function test_absentViewModelIsSafe() {
        const page = createTemporaryObject(pageComponent, testCase, {
            viewModel: null
        });
        verify(page !== null);
        verify(!page.hasData);
        page.reload();
    }
    function test_programStatusHistoryIsPaged() {
        stub.hasData = true;
        for (let i = 0; i < 10; ++i)
            plotRows.append({
                date: "2026-11-" + String(i + 1).padStart(2, "0"),
                splitId: "upper",
                better: 1,
                same: 0,
                start: 0,
                attention: 0,
                worse: 0,
                total: 1
            });
        const page = createTemporaryObject(pageComponent, testCase);
        verify(page !== null);
        compare(page.statusWindow.length, 8);
        compare(page.statusWindow[0].date, "2026-11-10");
        page.statusPage = 1;
        compare(page.statusWindow.length, 2);
        page.sectionIndex = 3;
        wait(200);
        verify(page.pageFlickable.contentWidth <= page.width);
    }
    function test_selectorStartsWithRecordedExercises() {
        stub.hasData = true;
        exerciseRows.append({
            exerciseId: "planned",
            exerciseTitle: "Без записей",
            splitId: "upper",
            currentWeightKg: 0,
            currentSets: 0
        });
        exerciseRows.append({
            exerciseId: "recorded",
            exerciseTitle: "С результатом",
            splitId: "upper",
            currentWeightKg: 60,
            currentSets: 2
        });
        const page = createTemporaryObject(pageComponent, testCase);
        verify(page !== null);
        compare(page.exerciseRows.length, 1);
        compare(page.selectedExercise.exerciseId, "recorded");
    }

    function test_exerciseHistoryPagesAndResets() {
        stub.hasData = true;
        exerciseRows.append({
            exerciseId: "press",
            exerciseTitle: "Жим",
            splitId: "upper",
            currentSets: 2,
            currentWeightKg: 60
        });
        for (let i = 0; i < 9; ++i)
            sourceHistoryRows.append({
                exerciseId: "press",
                splitId: "upper",
                sessionId: "s" + i,
                date: "2026-10-" + String(i + 1).padStart(2, "0"),
                currentWeightKg: 60,
                currentReps: 8,
                currentSets: 2,
                status: "same",
                reasonText: "Вес и повторы сохранены",
                deltaKg: 0,
                deltaPercent: 0
            });
        const page = createTemporaryObject(pageComponent, testCase);
        tryVerify(() => page.historyWindow.length === 6);
        compare(page.historyWindow[0].date, "2026-10-09");
        page.sectionIndex = 1;
        wait(250);
        mouseClick(findChild(page, "exerciseHistoryRow0"));
        const detail = findChild(page, "exerciseHistoryDetail");
        tryCompare(detail, "opened", true);
        compare(page.selectedHistoryRecord.date, "2026-10-09");
        detail.close();
        tryCompare(detail, "opened", false);
        page.historyPage = 1;
        compare(page.historyWindow.length, 3);
        page.periodIndex = 1;
        compare(page.historyPage, 0);
        page.selectedSplitIndex = 1;
        compare(page.historyPage, 0);
        page.sectionIndex = 0;
        wait(250);
        verify(page.pageFlickable.contentWidth <= page.width);
    }

    function test_sectionsShowOneContextAtATime() {
        stub.hasData = true;
        exerciseRows.append({
            exerciseId: "press",
            exerciseTitle: "Жим",
            splitId: "upper",
            currentSets: 2,
            currentWeightKg: 60
        });
        const page = createTemporaryObject(pageComponent, testCase);
        const chart = findChild(page, "strengthExerciseCard");
        const history = findChild(page, "exerciseHistorySection");
        const summary = findChild(page, "programStrengthSummary");
        verify(chart.visible);
        verify(!history.visible);
        verify(!summary.visible);
        page.sectionIndex = 1;
        verify(!chart.visible);
        verify(history.visible);
        verify(!summary.visible);
        page.sectionIndex = 2;
        verify(!chart.visible);
        verify(!history.visible);
        verify(summary.visible);
        verify(!findChild(page, "strengthExercisePicker").visible);
    }
}
