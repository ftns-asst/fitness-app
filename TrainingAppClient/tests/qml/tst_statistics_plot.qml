import QtQuick
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: testCase
    name: "StatisticsPlot"
    visible: true
    when: windowShown
    width: 390
    height: 600
    ListModel {
        id: nestedRows
    }
    Component {
        id: plotComponent
        StatisticsPlot {
            width: 350
            height: 232
            unit: "кг"
        }
    }
    function test_missingValuesStayMissingAndZeroIsValid() {
        const plot = createTemporaryObject(plotComponent, testCase, {
            points: [
                {
                    date: "2026-11-01",
                    value: null
                },
                {
                    date: "2026-11-02",
                    value: 0
                },
                {
                    date: "2026-11-03",
                    value: 82
                },
                {
                    date: "invalid",
                    value: 81
                }
            ]
        });
        verify(plot !== null);
        compare(plot.cleanPoints.length, 2);
        compare(plot.cleanPoints[0].value, 0);
    }
    function test_spacingUsesCalendarDatesAndTapSelectsFact() {
        const plot = createTemporaryObject(plotComponent, testCase, {
            points: [
                {
                    date: "2026-11-01",
                    value: 82
                },
                {
                    date: "2026-11-02",
                    value: 81.5
                },
                {
                    date: "2026-11-11",
                    value: 80
                }
            ]
        });
        verify(plot !== null);
        const span = plot.pointX(2) - plot.pointX(0);
        fuzzyCompare((plot.pointX(1) - plot.pointX(0)) / span, 0.1, 0.001);
        const canvas = findChild(plot, "statisticsCanvas");
        verify(canvas !== null);
        mouseClick(canvas, plot.pointX(1), canvas.height / 2);
        compare(plot.selected.date, "2026-11-02");
        compare(plot.selected.value, 81.5);
    }
    function test_singleMeasurementIsVisibleAndCentered() {
        const plot = createTemporaryObject(plotComponent, testCase, {
            points: [
                {
                    date: "2026-11-01",
                    value: 82
                }
            ]
        });
        verify(plot !== null);
        compare(plot.selected.value, 82);
        verify(plot.pointX(0) > plot.width / 2);
        verify(plot.bounds.min < 82 && plot.bounds.max > 82);
    }
    function test_nestedModelPointsAreSupported() {
        nestedRows.clear();
        nestedRows.append({
            points: [
                {
                    date: "2026-11-01",
                    value: 180
                },
                {
                    date: "2026-11-02",
                    value: 175
                }
            ]
        });
        const plot = createTemporaryObject(plotComponent, testCase, {
            points: nestedRows.get(0).points
        });
        verify(plot !== null);
        compare(plot.cleanPoints.length, 2);
        compare(plot.selected.value, 175);
    }
}
