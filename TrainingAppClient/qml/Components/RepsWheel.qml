import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Колесо ввода повторов (спринт S8, issue 11): прокрутка с привязкой к целому
// числу, крупная выбранная цифра в центре.
//
// Отличие от WeightRuler намеренное: повторы всегда целые и диапазон узкий
// (1…50), поэтому метки не нужны — нужен только ряд чисел и то, что выбранное
// стоит на месте. Привязка к шагу здесь тоже обязательна, но шаг равен 1: «8,5
// повтора» не бывает, и такое значение означало бы ошибку ввода.
//
// Значение — свойство, наружу отдаётся сигналом `edited`, как у остальных
// компонентов ввода: колесо ничего не знает про подходы.
Item {
    id: wheel

    property int from: 1
    property int to: 50
    property int value: 8
    // Шаг привязки. Для повторов это 1, но оставлено свойством: тот же механизм
    // используется там, где нужен шаг 2 или 5 (например, число подходов).
    property int step: 1

    signal edited(int value)

    // Расстояние между соседними числами, px. Задаётся явно, а не считается от
    // ширины: на 360 px влезает пять чисел, и они должны оставаться читаемыми.
    readonly property real itemWidth: 44
    readonly property int itemCount: Math.max(1, Math.floor((wheel.to - wheel.from) / wheel.step) + 1)
    // Палец на колесе прямо сейчас. По этому состоянию растёт выбранное число:
    // без роста не видно, что колесо «взято», — палец закрывает цифру.
    readonly property bool gripActive: dragTouch.pressed

    implicitHeight: 64
    implicitWidth: 280

    // Значение приводится к решётке при любом изменении, а не только при вводе:
    // иначе внешняя установка (например, 8 из предписания) дала бы «8,5».
    function normalize(number) {
        const clamped = Math.max(wheel.from, Math.min(wheel.to, number));
        const steps = Math.round((clamped - wheel.from) / wheel.step);

        return wheel.from + steps * wheel.step;
    }

    function commitValue(index) {
        const snapped = wheel.normalize(wheel.from + index * wheel.step);

        if (wheel.value === snapped)
            return;

        wheel.value = snapped;
        wheel.edited(snapped);
    }

    function setValue(newValue) {
        wheel.commitValue(Math.round((wheel.normalize(newValue) - wheel.from) / wheel.step));
    }

    // Ряд чисел. Позиция считается от выбранного: выбранное всегда в центре, и
    // прокрутка не «пружинит» назад — иначе число уходило бы из-под пальца.
    Flickable {
        id: flick

        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        contentHeight: height
        // Смещение считается вручную, а не через contentX: иначе округление до
        // кратного шага приходилось бы делать на каждый кадр прокрутки.
        contentWidth: wheel.itemCount * wheel.itemWidth
        // Центр колеса = середина компонента.
        x: Math.max(0, Math.min(flick.width - flick.contentWidth, flick.width / 2 - (Math.round((wheel.value - wheel.from) / wheel.step) + 0.5) * wheel.itemWidth))

        onXChanged: wheel.syncFromX()
        onWidthChanged: wheel.syncFromX()

        Row {
            id: numberRow

            Repeater {
                model: wheel.itemCount

                delegate: Label {
                    required property int index

                    readonly property int itemValue: wheel.from + index * wheel.step
                    readonly property bool isSelected: itemValue === wheel.value

                    height: flick.height
                    // Выбранное крупное и акцентное: разница в один шаг должна
                    // читаться мгновенно, без чтения мелкого ряда.
                    color: isSelected ? Theme.accent : Theme.textSecondary
                    font: isSelected ? Typography.bodyStrong : Typography.body
                    horizontalAlignment: Text.AlignHCenter
                    // Выбранное ещё и растёт под пальцем: пока тянешь, палец
                    // закрывает цифру, и по одному цвету не понять, что колесо
                    // в руке, а не «залипло».
                    scale: isSelected && wheel.gripActive ? 1.3 : 1
                    text: String(itemValue)
                    transformOrigin: Item.Center
                    verticalAlignment: Text.AlignVCenter
                    width: wheel.itemWidth

                    Behavior on scale {
                        // Рост выбранного числа — отклик на зажатие колеса.
                        NumberAnimation {
                            duration: Theme.pressFeedbackDuration
                            easing.type: Easing.OutCubic
                        }
                    }
                }
            }
        }
    }

    // Индикатор выбранного: вертикальная черта по центру. Без неё строка
    // «вот это число» читалась бы по размеру шрифта, а на 360 px соседние числа
    // того же размера сливались бы.
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        anchors.top: parent.top
        color: Theme.accent
        height: parent.height
        opacity: 0.5
        width: 2
    }

    function syncFromX() {
        if (width <= 0)
            return;

        const center = flick.x + flick.width / 2;
        const index = Math.round((center - wheel.itemWidth / 2) / wheel.itemWidth);

        wheel.commitValue(Math.max(0, Math.min(wheel.itemCount - 1, index)));
    }

    // Номер под точкой x. Выбранное число стоит по центру колеса, поэтому номер —
    // это текущее значение плюс пройденные от центра шаги. Считается от значения,
    // а не через `flick.x`: положение прокрутки зависит от ширины, а номер под
    // пальцем меняться от этого не должен. Числа колеса наружу — они нужны тесту:
    // иначе и рост цифры, и привязку зоны захвата можно выключить молча.
    readonly property alias dragTouch: dragTouch
    readonly property alias numberRow: numberRow

    function indexAt(xPosition) {
        const steps = Math.round((xPosition - wheel.width / 2) / wheel.itemWidth);
        const index = Math.round((wheel.value - wheel.from) / wheel.step) + steps;

        return Math.max(0, Math.min(wheel.itemCount - 1, index));
    }

    // — Сосуществование с прокруткой страницы — то же, что в WeightRuler:
    // колесо лежит в Flickable страницы, и касание во время прокрутки не
    // должно переставлять число («перепрыгивало» при быстром касании и
    // отпускании пальца). Предок ищется обходом родителей: компонент не хочет
    // знать про PageScaffold.
    property var pageScroll: null
    // Прокрутка заблокирована активным колесом прямо сейчас: горизонтальное
    // ведение цифр подтверждено — страница стоит. Отпускание всегда возвращает
    // прокрутку, включая страховку onDestruction.
    property bool pageScrollLocked: false

    Component.onCompleted: {
        let ancestor = wheel.parent;

        while (ancestor !== null && ancestor !== undefined) {
            if (ancestor instanceof Flickable) {
                wheel.pageScroll = ancestor;
                return;
            }

            ancestor = ancestor.parent;
        }
    }
    // Прокрутка едет прямо сейчас (drag или инерция). Прежнее «мёртвое окно»
    // 200 мс после остановки откатано: оно заставляло колесо игнорировать
    // касания сразу после скролла, и страница ощущалась запаздывающей.
    function pageScrollBusy() {
        return wheel.pageScroll !== null && wheel.pageScroll !== undefined && wheel.pageScroll.moving === true;
    }

    // Блокировка прокрутки на время активного колеса: interactive=false гасит
    // и текущее движение, и отбор жеста — ведение цифр всегда достаётся
    // колесу, как только подтверждена горизонталь.
    function lockPageScroll() {
        if (wheel.pageScrollLocked || wheel.pageScroll === null || wheel.pageScroll === undefined)
            return;

        wheel.pageScrollLocked = true;
        wheel.pageScroll.interactive = false;
    }

    function unlockPageScroll() {
        if (!wheel.pageScrollLocked || wheel.pageScroll === null || wheel.pageScroll === undefined)
            return;

        wheel.pageScrollLocked = false;
        wheel.pageScroll.interactive = true;
    }

    // Страховка: колесо разрушено посреди жеста — прокрутка обязана вернуться.
    Component.onDestruction: wheel.unlockPageScroll()

    // Жест: нажатие ставит колесо на число под пальцем, движение ведёт его за
    // пальцем. Оба действия обслуживает один обработчик — для колеса это одно
    // действие «кручу, пока не увижу нужное», и раньше нажатие-и-протяжка
    // распадались на «клик сработал, протяжка потерялась».
    MouseArea {
        id: dragTouch

        anchors.fill: parent
        // Страница (`Flickable` в PageScaffold) не отбирает горизонтальное
        // ведение: отбор захвата у ребёнка у Qt осесознающий — вертикальный
        // Flickable забирает жест только по превышению порога на оси Y
        // (QQuickFlickablePrivate::handleMoveEvent), поэтому протяжка цифр
        // остаётся у колеса и без preventStealing, а вертикальный свайп через
        // колесо прокручивает страницу. Постоянный preventStealing делал свайпы
        // по этой полосе «мёртвыми»: колесо стоит в центре записи подхода, и
        // палец по нему проводит часто.
        z: 1

        // Значение на момент нажатия: если жест перехватит прокрутка, число
        // возвращается (см. onCanceled) — диагональный свайп успевал сдвинуть
        // номер на шаг до того, как вертикаль забирала жест.
        property real pressValue: 0
        // Точка нажатия: от неё считается намерение жеста (горизонталь —
        // колесо, вертикаль — страница).
        property real pressX: 0
        property real pressY: 0
        // Ведение цифр подтверждено (|dx| ≥ 8 и больше вертикали): прокрутка
        // страницы заблокирована до отпускания.
        property bool dragMode: false

        onPositionChanged: function (event) {
            if (!pressed)
                return;

            if (!dragTouch.dragMode) {
                // Развязка жестов: горизонтальное намерение забирает колесо и
                // блокирует прокрутку; вертикальное остаётся странице — свайп
                // «по касательной» через колесо по-прежнему листает экран.
                const dx = event.x - dragTouch.pressX;
                const dy = event.y - dragTouch.pressY;

                if (Math.abs(dx) < 8 || Math.abs(dx) <= Math.abs(dy))
                    return;

                dragTouch.dragMode = true;
                dragTouch.preventStealing = true;
                wheel.lockPageScroll();
            }

            wheel.commitValue(wheel.indexAt(event.x));
        }

        onPressed: function (event) {
            // Касание во время прокрутки — жест прокрутки: число под пальцем
            // не переставляем и прокрутку не перехватываем.
            if (wheel.pageScrollBusy()) {
                event.accepted = false;
                return;
            }

            dragTouch.pressValue = wheel.value;
            dragTouch.pressX = event.x;
            dragTouch.pressY = event.y;
            wheel.commitValue(wheel.indexAt(event.x));
        }

        onCanceled: {
            // Прокрутка отобрала жест: перестановка отменяется — значение
            // возвращается к тому, что было до касания.
            if (wheel.value !== dragTouch.pressValue)
                wheel.setValue(dragTouch.pressValue);

            wheel.unlockPageScroll();
            dragTouch.preventStealing = false;
            dragTouch.pressValue = 0;
            dragTouch.dragMode = false;
        }
        onReleased: {
            wheel.unlockPageScroll();
            dragTouch.preventStealing = false;
            dragTouch.pressValue = 0;
            dragTouch.dragMode = false;
        }
    }
}
