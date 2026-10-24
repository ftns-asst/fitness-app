import QtQuick
import QtTest
import TrainingAppClient 1.0

// Измеритель частоты кадров (`qml/Components/FrameRateMeter.qml`).
//
// Счётчик — единственный способ проверить, что приложение рисует на частоте
// экрана, а не на 60 Гц: на 120-герцевом телефоне кадры идут каждые 8,3 мс.
TestCase {
    id: testCase

    // Выключенный измеритель ничего не считает: измерение заставляет окно
    // рисовать непрерывно, и в обычном запуске его быть не должно.
    function test_stoppedMeterKeepsZero() {
        const meter = createTemporaryObject(meterComponent, testCase);

        verify(meter !== null);

        compare(meter.running, false);
        compare(meter.frameCount, 0);
        compare(meter.framesPerSecond, 0);
    }
    // На включённом измерителе кадры считаются, и частота переводится во
    // «сколько кадров в секунду» — иначе непонятно, 60 это или 120.
    function test_runningMeterReportsRate() {
        const window = createTemporaryObject(meterInWindowComponent, testCase);

        verify(window !== null);

        window.show();
        waitForRendering(window.contentItem);

        const meter = window.meter;

        verify(meter !== null);

        meter.running = true;

        wait(500);

        verify(meter.frameCount > 0, "счётчик кадров не считает");
        verify(meter.framesPerSecond > 0, "частота кадров не измеряется");
    }

    height: 400
    name: "FrameRateMeter"
    width: 400

    Component {
        id: meterComponent

        FrameRateMeter {}
    }
    Component {
        id: meterInWindowComponent

        Window {
            property alias meter: meterInWindow

            height: 400
            visible: false
            width: 400

            FrameRateMeter {
                id: meterInWindow
            }
        }
    }
}
