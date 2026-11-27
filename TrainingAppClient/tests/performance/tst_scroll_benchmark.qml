import QtQuick
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: bench
    name: "ScrollBenchmark"
    when: windowShown
    width: 390
    height: 812
    visible: true
    property url screenSource: "../../qml/Pages/Exercises/ExercisesPage.qml"
    property url wheelSource: "../../qml/Components/DragToScroll.qml"
    FontLoader {
        id: regularFont
        source: "../../resources/InterFont/Inter-Regular.otf"
    }
    FontLoader {
        id: mediumFont
        source: "../../resources/InterFont/Inter-Medium.otf"
    }
    FontLoader {
        id: boldFont
        source: "../../resources/InterFont/Inter-Bold.otf"
    }
    FontLoader {
        id: semiboldFont
        source: "../../resources/InterFont/Inter-SemiBold.otf"
    }
    property var page: null
    property var samples: []
    property real elapsed: 0
    ListModel {
        id: rows
    }
    QtObject {
        id: catalog
        property var exercise_list: rows
        property bool fromCache: true
        property bool loading: false
        property bool canLoadMore: false
        property string errorKey: ""
        property var muscleGroups: ["legs", "chest"]
        property var equipments: ["barbell", "dumbbell"]
        function setQuery(value) {
        }
        function setMuscleGroups(value) {
        }
        function setEquipments(value) {
        }
        function reload() {
        }
        function loadCatalogs() {
        }
        function loadMore() {
        }
    }
    FrameAnimation {
        id: animation
        running: false
        onTriggered: {
            if (frameTime <= 0)
                return;
            bench.samples.push(frameTime * 1000);
            bench.elapsed += frameTime;
            const flick = bench.page.pageFlickable;
            const limit = Math.min(3000, Math.max(0, flick.contentHeight - flick.height));
            const phase = (bench.elapsed % 4) / 2;
            flick.contentY = limit * (phase <= 1 ? phase : 2 - phase);
        }
    }
    function tileCount(item) {
        let count = item instanceof CatalogExerciseTile ? 1 : 0;
        for (const child of item.children)
            count += tileCount(child);
        return count;
    }
    function test_catalogScroll() {
        compare(regularFont.status, FontLoader.Ready);
        compare(mediumFont.status, FontLoader.Ready);
        compare(boldFont.status, FontLoader.Ready);
        compare(semiboldFont.status, FontLoader.Ready);
        const factory = Qt.createComponent(screenSource);
        compare(factory.status, Component.Ready, factory.errorString());
        for (let i = 0; i < 500; ++i)
            rows.append({
                id: "bench_" + i,
                title: "Упражнение " + i + " · длинное название",
                photoUrl: ""
            });
        for (let run = 0; run < 3; ++run) {
            const started = Date.now();
            page = factory.createObject(bench.Window.window.contentItem, {
                viewModel: catalog,
                width: 390,
                height: 812
            });
            verify(page !== null);
            waitForRendering(page);
            const readyMs = Date.now() - started;
            wait(1000);
            samples = [];
            elapsed = 0;
            animation.running = true;
            wait(5000);
            animation.running = false;
            const sorted = samples.slice().sort((a, b) => a - b);
            verify(sorted.length > 0);
            console.info("SCROLL_BENCH " + JSON.stringify({
                run: run + 1,
                rows: rows.count,
                readyMs: readyMs,
                tiles: tileCount(page),
                frames: sorted.length,
                p50Ms: sorted[Math.floor(sorted.length * 0.5)],
                p95Ms: sorted[Math.floor(sorted.length * 0.95)],
                p99Ms: sorted[Math.floor(sorted.length * 0.99)],
                overBudget60: sorted.filter(value => value > 16.667).length,
                maxMs: sorted[sorted.length - 1]
            }));
            page.destroy();
            page = null;
            wait(100);
        }
    }
    Component {
        id: flickComponent
        Flickable {
            width: 390
            height: 400
            contentHeight: 3000
            contentWidth: width
        }
    }
    function test_wheelResponse() {
        const factory = Qt.createComponent(wheelSource);
        compare(factory.status, Component.Ready, factory.errorString());
        const target = createTemporaryObject(flickComponent, bench.Window.window.contentItem);
        const catcher = factory.createObject(bench.Window.window.contentItem, {
            target: target,
            width: 390,
            height: 812
        });
        for (let run = 0; run < 3; ++run) {
            target.contentY = 0;
            const start = Date.now();
            catcher.wheelBy(-120);
            tryVerify(() => Math.abs(target.contentY - 120) < 0.5, 1000);
            console.info("WHEEL_BENCH " + JSON.stringify({
                run: run + 1,
                settleMs: Date.now() - start
            }));
            wait(100);
        }
        if (catcher.wheelPixels !== undefined) {
            catcher.wheelPixels(-30);
            compare(target.contentY, 150);
            console.info("PIXEL_WHEEL_BENCH immediateDelta=30");
        }
        catcher.destroy();
    }
}
