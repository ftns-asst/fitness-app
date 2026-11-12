import QtQuick
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: testCase
    name: "StatisticsRedesign"
    visible: true
    when: windowShown
    width: 390
    height: 844

    ListModel {
        id: weightRows
    }
    ListModel {
        id: trendRows
    }
    ListModel {
        id: emptyRows
    }
    QtObject {
        id: stats
        signal changed
        property bool hasStats: true
        property var dashboard: ({
                currentDay: 10,
                totalDays: 28,
                daysLeft: 18,
                targetWeightKg: 75,
                endDate: "2026-11-28"
            })
        property var weight_series: weightRows
        property var trends: trendRows
        property var measurements: emptyRows
        property var weeks: emptyRows
        property var nutritionSummary: emptyRows
        property var goal_progress: emptyRows
        property int completedSessions: 3
        property int setsCount: 18
        property int repsCount: 140
        property real tonnageKg: 1200
        property real durationMinutes: 90
        property bool hasAdherencePercent: false
        property bool hasRuleAdherencePercent: false
        property bool hasBodyWeightDeltaKg: false
        property string errorKey: ""
        property string dataRangeFrom: "2026-10-01"
        property string dataRangeTo: "2026-11-08"
        property var lastRefresh: []
        function refresh(programId, from, to) {
            lastRefresh = [programId, from, to];
            return true;
        }
    }
    Component {
        id: pageComponent
        ProgramStatsPage {
            width: testCase.width
            height: testCase.height
            viewModel: stats
            programViewModel: null
            programId: "p"
            referenceDate: "2026-11-12"
        }
    }
    function init() {
        weightRows.clear();
        trendRows.clear();
        weightRows.append({
            date: "2026-11-01",
            weightKg: 82,
            average7Kg: 82
        });
        weightRows.append({
            date: "2026-11-08",
            weightKg: 81,
            average7Kg: 81.5
        });
        trendRows.append({
            param: "weight",
            start: 82,
            latest: 81,
            delta: -1,
            deltaPercent: -1.2,
            points: 2,
            ratePerWeek: -1,
            forecastAtEnd: 79
        });
        stats.dashboard = {
            currentDay: 10,
            totalDays: 28,
            daysLeft: 18,
            targetWeightKg: 75,
            endDate: "2026-11-28"
        };
        stats.trends = Qt.binding(() => trendRows);
        stats.lastRefresh = [];
    }
    function test_sectionsReplaceLongMixedDashboard() {
        const page = createTemporaryObject(pageComponent, testCase);
        verify(page !== null);
        verify(findChild(page, "statisticsSections") !== null);
        compare(page.sectionIndex, 0);
        verify(findChild(page, "bodyStatistics").visible);
        verify(!findChild(page, "trainingStatistics").visible);
        page.sectionIndex = 1;
        verify(!findChild(page, "bodyStatistics").visible);
        verify(findChild(page, "trainingStatistics").visible);
    }
    function test_periodChangesQueryNotOnlyCaption() {
        const page = createTemporaryObject(pageComponent, testCase, {
            referenceDate: "2026-11-08"
        });
        verify(page !== null);
        page.periodIndex = 0;
        compare(stats.lastRefresh.join(","), "p,2026-11-02,2026-11-08");
        page.periodIndex = 1;
        compare(stats.lastRefresh.join(","), "p,2026-10-12,2026-11-08");
    }
    function test_onlyMeasurementsWithFactsAreSelectable() {
        const page = createTemporaryObject(pageComponent, testCase);
        verify(page !== null);
        compare(page.measurementOptions.length, 1);
        compare(page.measurementOptions[0].id, "weight");
        verify(page.pageFlickable.contentWidth <= page.width);
    }
    function test_periodWithoutMeasurementsPreservesSelectedParameter() {
        trendRows.append({
            param: "waist",
            start: 90,
            latest: 88,
            delta: -2,
            deltaPercent: -2.2,
            points: 2,
            ratePerWeek: -1,
            forecastAtEnd: 86
        });
        const page = createTemporaryObject(pageComponent, testCase);
        verify(page !== null);
        page.measurementIndex = 1;
        compare(page.selectedMeasurement, "waist");
        trendRows.clear();
        stats.changed();
        compare(page.selectedMeasurement, "waist");
        compare(page.measurementOptions.length, 2);
    }

    function test_measurementsAndForecastHaveSeparateViews() {
        const page = createTemporaryObject(pageComponent, testCase);
        page.detailsExpanded = true;
        tryVerify(() => findChild(page, "measurementFacts").visible);
        verify(!findChild(page, "measurementForecast").visible);
        page.openForecast("weight");
        tryVerify(() => findChild(page, "measurementForecast").visible);
        verify(findChild(page, "measurementFacts").visible);
        findChild(page, "measurementForecastPopup").close();
        verify(findChild(page, "statisticsBackBar") !== null);
    }

    function test_forecastSelectionDoesNotChangeChart() {
        trendRows.append({
            param: "waist",
            start: 90,
            latest: 88,
            delta: -2,
            deltaPercent: -2.2,
            points: 2,
            ratePerWeek: -1,
            forecastAtEnd: 86
        });
        const page = createTemporaryObject(pageComponent, testCase);
        page.openForecast("waist");
        compare(page.selectedMeasurement, "weight");
        compare(page.forecastMeasurement, "waist");
        compare(page.forecastTrend.latest, 88);
        tryCompare(findChild(page, "measurementForecastPopup"), "opened", true);
    }
    function test_forecastUnavailableReasonsAreExplicit() {
        stats.trends = [
            {
                param: "weight",
                points: 1,
                latest: 82,
                start: 82,
                ratePerWeek: null,
                forecastAtEnd: null
            }
        ];
        const page = createTemporaryObject(pageComponent, testCase);
        verify(!page.forecastAvailable);
        verify(page.forecastMessage.includes("два замера"));
        stats.trends = [
            {
                param: "weight",
                points: 2,
                start: 82,
                latest: 81,
                ratePerWeek: -1,
                forecastAtEnd: 79
            }
        ];
        stats.changed();
        stats.dashboard = {
            endDate: ""
        };
        verify(!page.forecastAvailable);
        verify(page.forecastMessage.includes("срок"));
        stats.dashboard = {
            endDate: "2026-11-01"
        };
        verify(!page.forecastAvailable);
        verify(page.forecastMessage.includes("заверш"));
        stats.dashboard = {
            endDate: "2026-11-28"
        };
        verify(page.forecastAvailable);
    }
    function test_invalidProjectionIsNotPresentedAsBodyMeasurement() {
        stats.trends = [
            {
                param: "weight",
                points: 2,
                start: 82,
                latest: 60,
                ratePerWeek: -22,
                forecastAtEnd: -20
            }
        ];
        const page = createTemporaryObject(pageComponent, testCase);
        verify(!page.forecastAvailable);
        verify(page.forecastMessage.includes("неприменим"));
    }

    function test_programChangeResetsForecastContext() {
        const page = createTemporaryObject(pageComponent, testCase);
        page.openForecast("waist");
        page.programId = "other";
        compare(page.forecastMeasurement, "weight");
        verify(!page.detailsExpanded);
        tryCompare(findChild(page, "measurementForecastPopup"), "opened", false);
    }

    function test_forecastCloseButton() {
        const page = createTemporaryObject(pageComponent, testCase);
        page.openForecast("weight");
        const popup = findChild(page, "measurementForecastPopup");
        tryCompare(popup, "opened", true);
        mouseClick(findChild(page, "forecastCloseButton"));
        tryCompare(popup, "opened", false);
    }
}
