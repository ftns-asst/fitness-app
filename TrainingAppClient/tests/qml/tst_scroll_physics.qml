import QtQuick
import QtQuick.Window
import QtTest
import TrainingAppClient 1.0

// Физика вертикальной прокрутки страниц (`PageScaffold`).
//
// Скорости Flickable в Qt — логические пиксели (dp), и умножать их на
// плотность экрана нельзя: путь выбега (v²/2a) при одновременном
// масштабировании скорости и декелерации растёт во столько же раз — на
// DPR 2.75 лёгкий флик уносил почти три экрана. Проверяется, что страница
// берёт токены напрямую: декелерация мягче дефолта Qt, но умеренно —
// «докатывается», а не летит вся страница.
TestCase {
    id: testCase

    // Позиции скролла выравниваются по целым пикселям: дробные координаты
    // заставляют GPU на каждом кадре сэмплировать атласы глифов и карточек
    // по нецелым позициям — на телефоне это просаживает кадр (120 → ~86 fps).
    function test_pixelAlignedForCheapFrames() {
        const window = createTemporaryObject(pageComponent, testCase);

        verify(window !== null);

        window.show();
        waitForRendering(window.flick);

        compare(window.flick.pixelAligned, true);
    }
    // Страница получает токены напрямую, без какого-либо масштаба
    // (в том числе на touch-платформах: DPR на физику не влияет).
    function test_pageUsesThemePhysics() {
        const window = createTemporaryObject(pageComponent, testCase);

        verify(window !== null);

        window.show();
        waitForRendering(window.flick);

        compare(window.flick.flickDeceleration, Theme.flickDeceleration);
        compare(window.flick.maximumFlickVelocity, Theme.flickMaxVelocity);
    }
    // Токены сбалансированы: замедление ниже дефолта Qt (1500) — выбег дольше
    // и мягче, но не экстремально (от 1000 и выше) — иначе лёгкий флик уносил
    // всю страницу. Скорость — дефолтная и не завышена.
    function test_tokensTunedForMomentum() {
        verify(Theme.flickDeceleration >= 1000, "декелерация не ниже 1000: лёгкий флик не должен улетать на весь экран");
        verify(Theme.flickDeceleration < 1500, "декелерация мягче дефолта Qt (1500): выбег дольше");
        verify(Theme.flickMaxVelocity === 2500, "скорость — дефолт Qt, не завышена");
    }

    height: 812
    name: "ScrollPhysics"
    width: 390

    Component {
        id: pageComponent

        Window {
            property alias flick: pageItem.pageFlickable
            property alias page: pageItem

            height: 812
            visible: false
            width: 390

            PageScaffold {
                id: pageItem

                anchors.fill: parent
                title: "Физика"

                Rectangle {
                    color: "gray"
                    height: 3000
                    width: parent.width
                }
            }
        }
    }
}
