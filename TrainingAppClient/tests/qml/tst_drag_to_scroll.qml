import QtQuick
import QtTest
import TrainingAppClient 1.0

// Жесты по «пустой» области вокруг контента (`qml/Components/DragToScroll.qml`).
//
// На широком экране страница занимает центрированную колонку, и вокруг неё
// остаётся область окна: раньше жест по ней пропадал, и до прокрутки приходилось
// тянуться в центр экрана. Проверяется и попадание жеста, и границы прокрутки,
// и направление инерции: `Flickable::flick()` принимает скорость в своей
// системе координат, и ошибка знака разворачивала бы страницу назад.
TestCase {
    id: testCase

    // Прокрутка начинается от накопленного смещения пальца, поэтому проверяется
    // направление и границы, а не точное число пикселей: драг может потерять
    // порог срабатывания обработчика (§ DragHandler.dragThreshold).
    function test_dragUpScrollsForward() {
        const window = createTemporaryObject(sideAreaComponent, testCase);

        verify(window !== null);

        window.show();
        waitForRendering(window.flickable);

        const flick = window.flickable;

        compare(flick.contentY, 0);

        // Жест идёт по левому полю окна, вне колонки страницы (окно 800, колонка 320).
        mousePress(window.contentItem, 20, 500);
        mouseMove(window.contentItem, 20, 400);
        mouseMove(window.contentItem, 20, 300);
        mouseRelease(window.contentItem, 20, 300);

        verify(flick.contentY > 0, "экран не прокрутился от жеста по пустой области");
    }
    // Противоположный жест возвращает к началу: направление обязано совпадать
    // с движением пальца, а не быть обратным.
    function test_dragDownReturnsToStart() {
        const window = createTemporaryObject(sideAreaComponent, testCase);

        verify(window !== null);

        window.show();
        waitForRendering(window.flickable);

        const flick = window.flickable;

        flick.contentY = 400;

        // Отпускание с задержкой: палец «останавливается» перед отпусканием, и
        // проверяется именно перетаскивание, а не инерция (её проверяет
        // test_releaseKeepsMomentum). Движение — двумя шагами: DragHandler
        // включается на первом событии с нулевым смещением, и одиночное
        // перемещение обработчик не увидел бы.
        mousePress(window.contentItem, 20, 300);
        mouseMove(window.contentItem, 20, 400);
        mouseMove(window.contentItem, 20, 500);
        mouseRelease(window.contentItem, 20, 500, Qt.LeftButton, Qt.NoModifier, 200);

        verify(flick.contentY < 400, "жест вниз не вернул контент назад");
    }
    // Отпускание на ходу оставляет инерцию: Flickable получает скорость в своих
    // координатах, поэтому неверный знак увёл бы страницу в обратную сторону.
    function test_releaseKeepsMomentum() {
        const window = createTemporaryObject(sideAreaComponent, testCase);

        verify(window !== null);

        window.show();
        waitForRendering(window.flickable);

        const flick = window.flickable;

        mousePress(window.contentItem, 20, 600);
        mouseMove(window.contentItem, 20, 560, 20);
        mouseMove(window.contentItem, 20, 520, 20);
        mouseMove(window.contentItem, 20, 480, 20);
        mouseRelease(window.contentItem, 20, 480);

        const released = flick.contentY;

        wait(120);

        verify(flick.contentY > released, "после отпускания прокрутка не продолжилась");
    }
    // Контент короче области: прокручивать нечего, и жест не должен ничего делать.
    function test_shortContentIgnoresDrag() {
        const window = createTemporaryObject(sideAreaComponent, testCase);

        verify(window !== null);

        window.show();
        waitForRendering(window.flickable);

        const flick = window.flickable;

        flick.contentHeight = 100;

        mousePress(window.contentItem, 20, 500);
        mouseMove(window.contentItem, 20, 300);
        mouseRelease(window.contentItem, 20, 300);

        compare(flick.contentY, 0);
    }
    // Цель не задана (страницы без прокрутки): жест игнорируется, а не падает
    // с TypeError на каждом движении.
    function test_missingTargetIsIgnored() {
        const catcher = createTemporaryObject(orphanComponent, testCase);

        verify(catcher !== null);

        catcher.scrollBy(100);

        compare(catcher.scrollable, false);
    }

    // Колесо мыши прокручивает страницу плавно: тик задаёт анимированную цель,
    // а не мгновенный прыжок contentY — прыжки читались как «запаздывание» и
    // рвали прокрутку на ступеньки.
    function test_wheelScrollsPage() {
        const window = createTemporaryObject(sideAreaComponent, testCase);

        verify(window !== null);

        window.show();
        waitForRendering(window.flickable);

        const flick = window.flickable;

        compare(flick.contentY, 0);

        // Тик вниз (отрицательная дельта) прокручивает к следующему контенту.
        mouseWheel(window.contentItem, 20, 500, 0, -120);

        // Анимация стартует с ближайшего кадра: проверяется движение в её ходе.
        wait(60);

        verify(flick.contentY > 0, "колесо не прокрутило страницу по пустой области");

        wait(400);

        // Один тик = 120 px: анимация дотянула ровно до цели (без накрутки
        // и без застревания на полдороги).
        compare(flick.contentY, 120);
    }

    height: 812
    name: "DragToScroll"
    width: 800

    Component {
        id: sideAreaComponent

        Window {
            property alias catcher: catcherInWindow
            property alias flickable: flickInWindow

            height: 812
            visible: false
            width: 800

            // Ловушка объявлена до страницы, как в `Main.qml`: нажатия страницы
            // до неё не доходят, а «пустое» поле окна — её.
            DragToScroll {
                id: catcherInWindow

                anchors.fill: parent
                target: flickInWindow
            }
            Item {
                anchors.centerIn: parent
                height: parent.height
                width: 320

                Flickable {
                    id: flickInWindow

                    anchors.fill: parent
                    boundsBehavior: Flickable.StopAtBounds
                    contentHeight: 2000
                    contentWidth: width

                    Column {
                        spacing: 8

                        Repeater {
                            model: 40

                            delegate: Rectangle {
                                color: "gray"
                                height: 40
                                width: flickInWindow.width
                            }
                        }
                    }
                }
            }
        }
    }
    Component {
        id: orphanComponent

        DragToScroll {
            height: 100
            target: null
            width: 100
        }
    }
}
