import QtQuick
import QtQuick.Window
import QtTest
import TrainingAppClient 1.0

// Оверлей счётчика кадров (`qml/Components/FpsOverlay.qml`): включается
// тумблером в «Профиль → Параметры → Оформление», отвечает на вопрос «почему
// скролл не плавный» числами прямо на телефоне — без логов и adb.
TestCase {
    id: testCase

    function cleanup() {
        Theme.fpsCounter = false;
    }
    // Выключен по умолчанию: измерение заставляет окно рисовать непрерывно.
    function test_hiddenByDefault() {
        const window = createTemporaryObject(overlayComponent, testCase);

        verify(window !== null);

        window.show();
        waitForRendering(window.overlay);

        compare(window.overlay.visible, false);
        compare(window.overlay.meterRunning, false, "выключенный счётчик не должен гонять окно кадрами");
    }
    // Включение настройки показывает оверлей и запускает измерение.
    function test_visibleWhenEnabled() {
        const window = createTemporaryObject(overlayComponent, testCase);

        verify(window !== null);

        window.show();
        waitForRendering(window.overlay);

        Theme.fpsCounter = true;

        compare(window.overlay.visible, true);
        compare(window.overlay.meterRunning, true);

        // Текст отвечает на вопрос «сколько кадров»: число и подпись.
        verify(window.overlay.fpsText.length > 0, "оверлей без текста не читается");
        verify(window.overlay.fpsText.indexOf("fps") >= 0, "в тексте счётчика нет единицы измерения");

        // Подложка тёмная и полупрозрачная: дефолтный белый Rectangle читался
        // как сплошное белое пятно, на котором белый текст невиден. Проверяется
        // непрозрачность (меньше 1) и затемнение (светлее тонированной панели
        // быть не должно).
        verify(window.overlay.color.a < 1.0, "подложка должна быть полупрозрачной");
        // hslLightness в шкале 0–1: тёмная подложка — заметно ниже середины.
        verify(window.overlay.color.hslLightness < 0.5, "подложка должна быть тёмной, чтобы белый текст читался");
    }

    height: 812
    name: "FpsOverlay"
    width: 390

    Component {
        id: overlayComponent

        Window {
            property alias overlay: overlayItem

            height: 812
            visible: false
            width: 390

            FpsOverlay {
                id: overlayItem

                anchors.top: parent.top
                anchors.right: parent.right
            }
        }
    }
}
