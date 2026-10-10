import QtQuick
import QtTest
import TrainingAppClient 1.0

// WeightRuler и RepsWheel (спринт S8, issue 11).
//
// Проверяется не «ползунок двигается», а инвариант, ради которого он и
// написан: значение всегда кратно шагу инвентаря. Промежуточное «54,37 кг»
// нельзя ни выставить, ни купить, и запись с таким весом бессмысленна — поэтому
// любое попадание вне решётки считается дефектом, а не округлением «на глаз».
TestCase {
    id: testCase

    property var createdObject: null

    function cleanup() {
        createdObject = null;
    }
    function test_weight_ruler_snaps_to_step() {
        createdObject = createTemporaryObject(rulerComponent, testCase);

        verify(createdObject !== null);

        // Прямая установка «грязного» значения обязана приводиться к решётке:
        // иначе на экране появилось бы то, до чего нельзя дотянуть ползунком.
        createdObject.setValue(54.37);

        // Проверяется именно результат, а не введённое число: исходное 54,37
        // вне решётки по определению, и проверять надо то, что осталось в ползунке.
        const quotient = createdObject.value / createdObject.step;

        verify(Math.abs(quotient - Math.round(quotient)) < 0.001, "значение вне решётки шага: " + createdObject.value);
        compare(createdObject.value, 55.0);
    }
    function test_weight_ruler_values_are_always_multiple() {
        createdObject = createTemporaryObject(rulerComponent, testCase);

        verify(createdObject !== null);

        // Перебор по всей шкале: дефект мог бы прятаться в середине, как это
        // делает ошибка округления на нечётном номере шага.
        for (let step = 0; step <= createdObject.itemCount - 1; ++step) {
            const value = createdObject.snappedValue(step);
            const quotient = value / createdObject.step;

            verify(Math.abs(quotient - Math.round(quotient)) < 0.001, "шаг " + step + " дал некратное значение " + value);
            verify(value >= createdObject.from && value <= createdObject.to, "значение вне диапазона: " + value);
        }
    }
    function test_weight_ruler_clamps_to_range() {
        createdObject = createTemporaryObject(rulerComponent, testCase);

        verify(createdObject !== null);

        // За пределами диапазона ползунок не уезжает: значения 500 кг не бывает,
        // а -10 — тем более.
        createdObject.setValue(1000);
        compare(createdObject.value, createdObject.to);

        createdObject.setValue(-50);
        compare(createdObject.value, createdObject.from);
    }
    function test_weight_ruler_emits_edited_only_on_change() {
        createdObject = createTemporaryObject(rulerComponent, testCase);

        verify(createdObject !== null);

        var received = [];

        createdObject.edited.connect(function (value) {
            received.push(value);
        });

        createdObject.setValue(60);
        compare(received.length, 1);
        // Повторная установка того же значения не должна шуметь сигналом: под
        // записью подхода это лишний пересчёт и лишняя запись в базу.
        createdObject.setValue(60);
        compare(received.length, 1);
    }
    function test_reps_wheel_produces_integers() {
        createdObject = createTemporaryObject(repsWheelComponent, testCase);

        verify(createdObject !== null);

        // Повторы всегда целые: «8,5 повтора» — ошибка ввода, а не значение.
        for (let step = 0; step < createdObject.itemCount; ++step) {
            const value = createdObject.from + step * createdObject.step;

            verify(value === Math.round(value), "повторы не целые: " + value);
        }
    }
    function test_reps_wheel_clamps_and_snaps() {
        createdObject = createTemporaryObject(repsWheelComponent, testCase);

        verify(createdObject !== null);

        createdObject.setValue(1000);
        compare(createdObject.value, createdObject.to);

        createdObject.setValue(0);
        compare(createdObject.value, createdObject.from);
    }

    function test_weight_ruler_formats_weight_with_comma() {
        createdObject = createTemporaryObject(rulerComponent, testCase);

        verify(createdObject !== null);

        // В зале и в референсе 2090 вес пишут через запятую. JS печатает точку,
        // и без разделителя подпись значения расходилась с подписями меток.
        compare(createdObject.formatWeight(60), "60");
        compare(createdObject.formatWeight(62.5), "62,5");

        // Целые значения не должны получать «.0»: подпись «60,0 кг» выглядит как
        // опечатка, когда рядом стоит «Шаг 2,5 кг».
        verify(createdObject.formatWeight(60).indexOf(",") === -1, "целый вес получил дробную часть");
    }
    // Шкала — как в макетах: несколько равномерных подписей с короткими
    // делениями над ними, без плотной решётки штрихов между ними. Проверяется
    // именно вид: плотная линейка с прореживанием выглядела «гравировкой» и не
    // совпадала ни с одним макетом — у всех пять чисел и пять штрихов.
    // Шкала 20–200 выбрана не случайно: её подписи — ровно «20 65 110 155 200»
    // из макета 04-day.html, до значения.
    function test_weight_ruler_scale_matches_mockup() {
        createdObject = createTemporaryObject(plateRulerComponent, testCase);

        verify(createdObject !== null);

        wait(0);

        const scale = createdObject.scaleArea;
        const marks = [];

        for (let i = 0; i < scale.children.length; ++i) {
            const child = scale.children[i];

            if (child.markValue !== undefined)
                marks.push(child);
        }

        // Пять подписей, как во всех макетах (`20 65 110 155 200`).
        compare(marks.length, 5, "подписей на шкале не пять: " + marks.length);

        // Первая и последняя подпись — края диапазона, и деления равномерны:
        // без этого «куда тащить» приходится угадывать.
        compare(marks[0].markValue, createdObject.from);
        compare(marks[marks.length - 1].markValue, createdObject.to);

        for (let i = 1; i < marks.length; ++i) {
            const gap = marks[i].markX - marks[i - 1].markX;

            verify(gap >= createdObject.labelMinGap - 1, "подписи наезжают: " + gap + " px");
        }

        // Крайние деления выше внутренних: края диапазона читаются первыми
        // (`:first/:last-child::before` в макете).
        verify(marks[0].tickHeight > marks[1].tickHeight, "краевое деление не выше внутреннего");
        verify(marks[marks.length - 1].tickHeight > marks[1].tickHeight, "краевое деление не выше внутреннего");

        // Краевые подписи прижаты к краям шкалы, а не висят в отрыве: иначе
        // «200» вылезает за карточку или рвёт ритм space-between.
        verify(marks[0].markLabel.x <= 1, "первая подпись не у левого края: " + marks[0].markLabel.x);
        verify(marks[marks.length - 1].markLabel.x + marks[marks.length - 1].markLabel.width >= scale.width - 1, "последняя подпись не у правого края");
    }
    function test_weight_ruler_has_no_overflowing_fill() {
        createdObject = createTemporaryObject(rulerComponent, testCase);

        verify(createdObject !== null);

        // У линейки не должно быть потомков шире или выше неё: заливка высотой во
        // весь компонент перекрывала соседние карточки экрана. Проверяется по
        // геометрии всех детей, а не по конкретному элементу — дефект был ровно
        // в том, что размер задавался высотой родителя.
        for (let i = 0; i < createdObject.children.length; ++i) {
            const child = createdObject.children[i];

            if (child.visible === false || child.width === undefined || child.height === undefined)
                continue;

            verify(child.height <= createdObject.height + 1, "дочерний элемент выше линейки: " + child.height + " > " + createdObject.height);
        }
    }

    // Ширина линейки — во всю ширину родителя. В обычной Column (каркас страницы
    // и внутренности AppCard) `Layout.fillWidth` не работает, и ребёнок берёт
    // свой implicitWidth: шкала выходила 280 px при 346 доступных — сжатой
    // полосой у левого края вместо `.slider-track` во всю ширину карточки из
    // макета. Проверяется именно растяжение, а не конкретное число: важно, что
    // ширина приходит от колонки, а не от самого компонента.
    function test_weight_ruler_stretches_to_parent_width_in_plain_column() {
        createdObject = createTemporaryObject(columnHostComponent, testCase);

        verify(createdObject !== null);

        wait(0);

        const ruler = createdObject.ruler;

        verify(ruler !== null, "в колонке нет линейки");
        verify(ruler.width > ruler.implicitWidth, "линейка взяла implicitWidth вместо ширины колонки: " + ruler.width);
        compare(ruler.width, createdObject.width);
    }

    // На узкой шкале подписей становится меньше, а не слипаются: на ширине
    // карточки помещаются все пять, а в узких местах лишние убираются. Две —
    // минимум: одна подпись — уже не шкала, а точка.
    function test_weight_ruler_narrow_scale_drops_labels() {
        createdObject = createTemporaryObject(rulerComponent, testCase, {
            width: 120
        });

        verify(createdObject !== null);

        wait(0);

        verify(createdObject.scaleLabelCount < 5, "узкая шкала не отказалась от подписей: " + createdObject.scaleLabelCount);
        verify(createdObject.scaleLabelCount >= 2, "на шкале меньше двух подписей");

        const gap = createdObject.scaleWidth / (createdObject.scaleLabelCount - 1);

        verify(gap >= createdObject.labelMinGap - 1, "подписи наезжают: " + gap + " px");
    }

    // Регрессия клавиатуры на телефоне: поле обязано получить фокус в момент,
    // когда оно уже видно. На Android фокус в ещё не показавшееся поле не
    // поднимает системную клавиатуру — ввод «открывался», а набирать было
    // некуда. В qmltestrunner системной клавиатуры нет, но поднимает её именно
    // activeFocus, поэтому проверяется он.
    //
    // Линейка переносится в окно теста: каркас qmltestrunner прячет дерево
    // TestCase — даже пустой Item в нём читается как visible == false, и фокус
    // в невидимом поле не приходит. Невидимое поле — ровно то состояние,
    // которое на Android и должно быть исправлено, поэтому тест обязан
    // проверять настоящий видимый случай.
    function test_weight_ruler_manual_input_grabs_focus() {
        createdObject = createTemporaryObject(plateRulerComponent, testCase);

        verify(createdObject !== null);
        createdObject.parent = testCase.Window.window.contentItem;

        wait(0);

        createdObject.openManualInput();
        verify(createdObject.manualInputActive, "ввод не открылся");

        // Фокус приходит отложенно (Qt.callLater): проверка в тот же тик —
        // гарантированный ложный провал.
        wait(50);

        const field = createdObject.manualField;

        verify(field !== null, "нет поля ручного ввода");
        verify(field.visible, "поле ввода не показалось");
        verify(field.activeFocus, "поле ввода не получило фокус — клавиатура не поднимется");
    }

    // Последнее деление обязано приходиться на правый край шкалы: подпись `to`
    // стоит в точке своего значения, и правый край без числа — «шкала
    // обрывается», а не «доезжает до конца».
    function test_weight_ruler_last_tick_reaches_right_edge() {
        createdObject = createTemporaryObject(denseRulerComponent, testCase);

        verify(createdObject !== null);

        wait(0);

        const scale = createdObject.scaleArea;
        const marks = [];

        for (let i = 0; i < scale.children.length; ++i) {
            const child = scale.children[i];

            if (child.markValue !== undefined)
                marks.push(child);
        }

        compare(marks.length, createdObject.scaleLabelCount);
        compare(marks[marks.length - 1].markValue, createdObject.to);
        compare(marks[marks.length - 1].markX, scale.width);
    }

    // Ползунок должен быть крупным: зона захвата — не менее 44 px по высоте, а
    // полоса — не тоньше 6 px. Требование из референса 2090 и `.slider-input`
    // в theme.css, и оно неочевидно из кода: сузить зону до одного пикселя
    // компонент не сломает, а попадать пальцем станет невозможно.
    function test_weight_ruler_has_finger_sized_target() {
        createdObject = createTemporaryObject(rulerComponent, testCase);

        verify(createdObject !== null);

        wait(0);

        const touch = createdObject.touchArea;

        verify(touch !== undefined && touch !== null, "нет зоны захвата");

        // 44 px — минимум, который считают надёжной целью для пальца.
        verify(touch.height >= 44, "зона захвата ниже 44 px: " + touch.height);

        // Зона должна перекрывать сам трек: если она выше или ниже его, мимо
        // ползунка попасть можно, хотя формально зона большая.
        const trackTop = createdObject.valueHeight + createdObject.headGap;
        const touchTop = touch.y;
        const touchBottom = touch.y + touch.height;

        verify(touchTop <= trackTop, "зона захвата начинается ниже трека: " + touchTop + " > " + trackTop);
        verify(touchBottom >= trackTop + createdObject.trackHeight, "зона захвата кончается выше низа трека");

        // Трек — заметная полоса, а не линия в пиксель.
        verify(createdObject.trackHeight >= 6, "трек тоньше 6 px: " + createdObject.trackHeight);
    }

    // Метки растут вниз от трека, а не вверх: вверх они наезжали на заливку, и
    // дорожка переставала читаться.
    function test_weight_ruler_ticks_hang_below_track() {
        createdObject = createTemporaryObject(rulerComponent, testCase);

        verify(createdObject !== null);

        wait(0);

        const ticksTop = createdObject.trackTop + createdObject.trackHeight;

        verify(ticksTop > createdObject.trackTop, "метки начинаются на уровне трека, а не под ним");
        verify(ticksTop >= createdObject.trackTop + createdObject.trackHeight, "метки растут вверх от трека");

        // Подписи — под метками, и обе полосы помещаются в компонент: иначе
        // нижние подписи уезжали за его пределы и обрезались.
        verify(createdObject.valueHeight + createdObject.headGap + createdObject.trackHeight + createdObject.ticksHeight + createdObject.labelsHeight <= createdObject.height + 0.5, "шкала не помещается в высоту компонента");
    }

    // Ползунок-черта и заливка обязаны стоять на одном значении: раньше
    // позиция считалась отдельно для меток и для ползунка, и он уезжал
    // относительно делений на половину шага.
    function test_weight_ruler_thumb_matches_value_position() {
        createdObject = createTemporaryObject(rulerComponent, testCase);

        verify(createdObject !== null);

        wait(0);

        // Значения по краям шкалы и в середине: ползунок обязан вставать на место в
        // каждом из них, а не «примерно туда».
        for (const target of [createdObject.from, (createdObject.from + createdObject.to) / 2, createdObject.to]) {
            createdObject.setValue(target);

            const expectedX = createdObject.scaleMargin + (createdObject.value - createdObject.from) / createdObject.step * createdObject.stepPx;
            const thumbCenter = createdObject.thumbCenter;

            verify(Math.abs(thumbCenter - expectedX) < 0.5, "ползунок стоит не на своём значении: " + thumbCenter + " вместо " + expectedX);
        }
    }

    // Ручной ввод: значение обязано набираться с клавиатуры. Ползунок даёт
    // масштаб, но на шкале 35–200 с шагом 0,1 шаг приходится на доли пикселя,
    // и вес 74,3 кг пальцем не поставить в принципе — без ручного ввода он был
    // бы недостижим, а не неудобен.
    function test_weight_ruler_manual_input_applies_value() {
        createdObject = createTemporaryObject(denseRulerComponent, testCase);

        verify(createdObject !== null);

        wait(0);

        var received = [];
        createdObject.edited.connect(function (value) {
            received.push(value);
        });

        createdObject.openManualInput();
        verify(createdObject.manualInputActive, "ввод не открылся");

        // Запятая — разделитель в русской раскладке, точка — тоже должна
        // пониматься: иначе набранное на латинице «74.3» молча игнорировалось бы.
        createdObject.manualText = "74,3";
        createdObject.commitManualInput();

        verify(!createdObject.manualInputActive, "ввод не закрылся после подтверждения");
        compare(received.length, 1);
        compare(createdObject.value, 74.3);
    }

    // Мусор в поле не должен превращаться в 0: пустой ввод отменяет правку.
    // Иначе промах мимо «Готово» записал бы в замер нулевые килограммы.
    function test_weight_ruler_manual_input_ignores_garbage() {
        createdObject = createTemporaryObject(denseRulerComponent, testCase);

        verify(createdObject !== null);

        wait(0);

        createdObject.setValue(80);
        createdObject.openManualInput();
        createdObject.manualText = "кг";
        createdObject.commitManualInput();

        compare(createdObject.value, 80);
        compare(createdObject.manualText, "");
    }

    // Значение из поля приводится к решётке шага, как и любое другое: ползунок
    // обещал кратные шагу значения, и ручной ввод не должен быть исключением.
    function test_weight_ruler_manual_input_snaps_to_step() {
        createdObject = createTemporaryObject(plateRulerComponent, testCase);

        verify(createdObject !== null);

        wait(0);

        createdObject.openManualInput();
        // 63,7 кг нельзя выставить на блитах: шаг 2,5.
        createdObject.manualText = "63,7";
        createdObject.commitManualInput();

        const quotient = createdObject.value / createdObject.step;

        verify(Math.abs(quotient - Math.round(quotient)) < 0.001, "ручной ввод обошёл решётку шага: " + createdObject.value);
        compare(createdObject.value, 62.5);
    }

    // Зона нажатия на значение — крупная цель, а не само число: короткое «80»
    // занимает вдвое меньше места, чем «100,0», и нажимать на него пальцем
    // заметно труднее.
    function test_weight_ruler_value_tap_target_is_wide() {
        createdObject = createTemporaryObject(denseRulerComponent, testCase);

        verify(createdObject !== null);

        wait(0);

        const tap = createdObject.valueTap;

        verify(tap !== undefined && tap !== null, "нет зоны нажатия на значение");
        verify(tap.height >= 44, "зона нажатия на значение ниже 44 px: " + tap.height);
        verify(tap.width >= createdObject.minimumTapWidth, "зона нажатия на значение уже " + createdObject.minimumTapWidth + " px: " + tap.width);
    }

    // Перетаскивание не обрывается страницей. `PageScaffold` — это Flickable, и он
    // забирает захват у ребёнка после первых же пикселей движения: без
    // preventStealing значение замирало на том, что успело записаться, и жест
    // выглядел «постоянно прерывающимся». Проверка идёт в том же окружении —
    // линейка внутри Flickable, — иначе дефект не воспроизводится.
    function test_weight_ruler_drag_survives_parent_flickable() {
        createdObject = createTemporaryObject(flickableHostComponent, testCase);

        verify(createdObject !== null);

        createdObject.parent = testCase.Window.window.contentItem;

        wait(0);

        const ruler = createdObject.ruler;
        const touch = ruler.touchArea;
        const center = touch.width / 2;

        // Нажатие обязано поставить ползунок под палец: без этого тап «сюда» не
        // работал, и на сенсорном экране ползунок выглядел неотзывчивым.
        mousePress(touch, center, touch.height / 2);
        compare(ruler.value, ruler.snappedValue(ruler.indexAt(center)), "нажатие не поставило ползунок под палец");

        // Движение на 60 px вправо: значение едет за пальцем, страница — нет.
        mouseMove(touch, center + 60, touch.height / 2);

        const expected = ruler.snappedValue(ruler.indexAt(center + 60));

        compare(ruler.value, expected, "перетаскивание оборвалось: значение " + ruler.value + " вместо " + expected);
        verify(expected > ruler.snappedValue(ruler.indexAt(center)), "движение вправо не увеличило значение");

        mouseRelease(touch, center + 60, touch.height / 2);

        compare(createdObject.page.contentY, 0, "страница уехала вместо перетаскивания ползунка");
        // Отпускание значения не откатывает: палец ушёл, выбор остался.
        compare(ruler.value, expected);
    }
    // Отклик на зажатие: черта растёт под пальцем. Без роста единственным
    // признаком жеста остаётся число, которое палец и закрывает.
    function test_weight_ruler_thumb_grows_while_pressed() {
        createdObject = createTemporaryObject(flickableHostComponent, testCase);

        verify(createdObject !== null);

        createdObject.parent = testCase.Window.window.contentItem;

        wait(0);

        const ruler = createdObject.ruler;
        const touch = ruler.touchArea;
        const idleWidth = ruler.thumbBar.width;
        const idleHeight = ruler.thumbBar.height;

        mousePress(touch, touch.width / 2, touch.height / 2);
        // Рост анимирован (Behavior): сразу после нажатия свойство стоит в начале
        // анимации, и проверка «стало больше» провалилась бы на исправном коде.
        wait(200);

        verify(ruler.gripActive, "ползунок не считает себя зажатым");
        verify(ruler.thumbBar.width > idleWidth, "черта не выросла по ширине: " + ruler.thumbBar.width);
        verify(ruler.thumbBar.height > idleHeight, "черта не выросла по высоте: " + ruler.thumbBar.height);

        mouseRelease(touch, touch.width / 2, touch.height / 2);
        wait(200);

        compare(ruler.gripActive, false);
        compare(ruler.thumbBar.width, idleWidth);
        compare(ruler.thumbBar.height, idleHeight);
    }
    // Колесо повторов — тот же класс дефекта: жест забирает страница, и протяжка
    // теряется. Отклик колеса — рост выбранной цифры под пальцем.
    function test_reps_wheel_drag_survives_parent_flickable() {
        createdObject = createTemporaryObject(wheelHostComponent, testCase);

        verify(createdObject !== null);

        createdObject.parent = testCase.Window.window.contentItem;

        wait(0);

        const wheel = createdObject.wheel;
        const touch = wheel.dragTouch;
        const start = wheel.value;

        verify(touch !== undefined && touch !== null, "нет зоны захвата колеса");

        mousePress(touch, touch.width / 2, touch.height / 2);
        compare(wheel.gripActive, true, "колесо не считает себя зажатым");

        mouseMove(touch, touch.width / 2 + wheel.itemWidth * 2, touch.height / 2);
        compare(wheel.value, start + 2, "протяжка колеса потерялась");

        mouseRelease(touch, touch.width / 2 + wheel.itemWidth * 2, touch.height / 2);

        compare(wheel.gripActive, false);
        compare(createdObject.page.contentY, 0, "страница уехала вместо прокрутки колеса");
    }
    // Касание во время прокрутки страницы — жест прокрутки: значение линейки
    // не меняется. Без этого быстрый «тап-стоп» по полосе линейки прыгал
    // значением под палец (дефект из отзыва о тач-проверке: «перепрыгивает,
    // дергается, когда быстро ставишь и убираешь палец»).
    function test_weight_ruler_ignores_touch_while_page_scrolling() {
        createdObject = createTemporaryObject(flickableHostComponent, testCase);

        verify(createdObject !== null);

        createdObject.parent = testCase.Window.window.contentItem;

        wait(0);

        const ruler = createdObject.ruler;
        const touch = ruler.touchArea;
        const original = ruler.value;

        // Долгая инерция: контент длиннее области, скорость высокая — за время
        // до нажатия прокрутка не успеет доехать до границы.
        createdObject.page.flick(0, -3000);
        wait(20);
        verify(createdObject.page.moving, "инерция не началась — тест не воспроизводит условие");

        mousePress(touch, touch.width / 2, touch.height / 2);
        compare(ruler.value, original, "касание во время прокрутки переставило значение");
        verify(!createdObject.page.flicking, "прокрутка не остановилась по касанию");

        mouseRelease(touch, touch.width / 2, touch.height / 2);

        compare(ruler.value, original, "отпускание во время прокрутки переставило значение");
    }
    // Вертикальный свайп по полосе линейки прокручивает страницу, а не ведёт
    // ползунок: отбор захвата у вертикального Flickable осесознающий, а
    // тап-позиционирование откатывается, когда жест отобран прокруткой.
    function test_weight_ruler_vertical_swipe_scrolls_page() {
        createdObject = createTemporaryObject(flickableHostComponent, testCase);

        verify(createdObject !== null);

        createdObject.parent = testCase.Window.window.contentItem;

        wait(0);

        const ruler = createdObject.ruler;
        const touch = ruler.touchArea;
        const original = ruler.value;

        // Далеко от ползунка: зона явного захвата не сработает, жест по оси Y
        // принадлежит странице.
        mousePress(touch, touch.width - 20, touch.height / 2);
        mouseMove(touch, touch.width - 20, touch.height / 2 - 40);
        mouseMove(touch, touch.width - 20, touch.height / 2 - 60);

        verify(createdObject.page.moving || createdObject.page.contentY !== 0, "страница не прокрутилась свайпом через линейку");
        compare(ruler.value, original, "свайп через линейку оставил в ней чужое значение");

        mouseRelease(touch, touch.width - 20, touch.height / 2 - 60);
    }
    // Касание колеса во время прокрутки страницы — жест прокрутки: число под
    // пальцем не переставляется.
    function test_reps_wheel_ignores_touch_while_page_scrolling() {
        createdObject = createTemporaryObject(wheelHostComponent, testCase);

        verify(createdObject !== null);

        createdObject.parent = testCase.Window.window.contentItem;

        wait(0);

        const wheel = createdObject.wheel;
        const touch = wheel.dragTouch;
        const original = wheel.value;

        createdObject.page.flick(0, -3000);
        wait(20);
        verify(createdObject.page.moving, "инерция не началась — тест не воспроизводит условие");

        mousePress(touch, touch.width / 2, touch.height / 2);
        compare(wheel.value, original, "касание во время прокрутки переставило число");

        mouseRelease(touch, touch.width / 2, touch.height / 2);

        compare(wheel.value, original);
    }
    // Удержание черты при вертикальном движении пальца не сбрасывает значение:
    // черта берётся как есть (без перескока к пальцу) и не откатывается, когда
    // прокрутка перехватывает жест (отзыв о тач-проверке: «ползунок резко
    // сбрасывается, когда двигаю палец вертикально, держа ползунок»).
    function test_weight_ruler_thumb_hold_survives_vertical_movement() {
        createdObject = createTemporaryObject(flickableHostComponent, testCase);

        verify(createdObject !== null);

        createdObject.parent = testCase.Window.window.contentItem;

        wait(0);

        const ruler = createdObject.ruler;
        const touch = ruler.touchArea;
        const original = ruler.value;
        const thumbX = ruler.thumbCenter;

        mousePress(touch, thumbX, touch.height / 2);
        compare(ruler.value, original, "захват черты перескочил значением к пальцу");
        verify(ruler.gripActive, "черта не считает себя зажатой");

        mouseMove(touch, thumbX, touch.height / 2 - 40);
        mouseMove(touch, thumbX, touch.height / 2 - 70);

        compare(ruler.value, original, "вертикальное движение при удержании черты сбросило значение");
        compare(createdObject.page.contentY, 0, "страница прокрутилась при удержании черты");

        mouseRelease(touch, thumbX, touch.height / 2 - 70);

        compare(ruler.value, original, "отпускание при удержании черты сбросило значение");
    }
    // Активный ползунок блокирует прокрутку страницы (отзыв: «блокировать
    // вертикальный прокрут, когда активен слайдер»): захват черты гасит
    // interactive у родительского Flickable, отпускание возвращает.
    function test_weight_ruler_thumb_press_locks_page_scroll() {
        createdObject = createTemporaryObject(flickableHostComponent, testCase);

        verify(createdObject !== null);

        createdObject.parent = testCase.Window.window.contentItem;

        wait(0);

        const ruler = createdObject.ruler;
        const touch = ruler.touchArea;
        const thumbX = ruler.thumbCenter;

        mousePress(touch, thumbX, touch.height / 2);
        verify(createdObject.page.interactive === false, "захват черты не заблокировал прокрутку страницы");

        mouseRelease(touch, thumbX, touch.height / 2);
        verify(createdObject.page.interactive === true, "отпускание черты не вернуло прокрутку страницы");
    }
    // Подтверждённое горизонтальное намерение на дорожке тоже блокирует
    // прокрутку: палец ведёт ползунок, а не страницу.
    function test_weight_ruler_horizontal_intent_locks_page_scroll() {
        createdObject = createTemporaryObject(flickableHostComponent, testCase);

        verify(createdObject !== null);

        createdObject.parent = testCase.Window.window.contentItem;

        wait(0);

        const ruler = createdObject.ruler;
        const touch = ruler.touchArea;
        const startX = touch.width - 40;

        mousePress(touch, startX, touch.height / 2);
        mouseMove(touch, startX + 30, touch.height / 2);

        verify(createdObject.page.interactive === false, "горизонтальное ведение не заблокировало прокрутку");

        mouseRelease(touch, startX + 30, touch.height / 2);
        verify(createdObject.page.interactive === true, "отпускание не вернуло прокрутку страницы");
    }
    // Вертикальное намерение прокрутку НЕ блокирует: свайп через линейку
    // остаётся странице, иначе полоса во всю ширину карточки была бы мёртвой
    // зоной прокрутки.
    function test_weight_ruler_vertical_intent_keeps_scroll_unlocked() {
        createdObject = createTemporaryObject(flickableHostComponent, testCase);

        verify(createdObject !== null);

        createdObject.parent = testCase.Window.window.contentItem;

        wait(0);

        const ruler = createdObject.ruler;
        const touch = ruler.touchArea;

        mousePress(touch, touch.width - 20, touch.height / 2);
        mouseMove(touch, touch.width - 20, touch.height / 2 - 30);

        verify(createdObject.page.interactive === true, "вертикальный свайп заблокировал прокрутку страницы");

        mouseRelease(touch, touch.width - 20, touch.height / 2 - 30);
        verify(createdObject.page.interactive === true);
    }
    // Колесо повторов: подтверждённая горизонталь блокирует прокрутку до
    // отпускания — ведение цифр всегда достаётся колесу.
    function test_reps_wheel_horizontal_intent_locks_page_scroll() {
        createdObject = createTemporaryObject(wheelHostComponent, testCase);

        verify(createdObject !== null);

        createdObject.parent = testCase.Window.window.contentItem;

        wait(0);

        const wheel = createdObject.wheel;
        const touch = wheel.dragTouch;

        mousePress(touch, touch.width / 2, touch.height / 2);
        mouseMove(touch, touch.width / 2 + wheel.itemWidth * 2, touch.height / 2);

        verify(createdObject.page.interactive === false, "ведение цифр не заблокировало прокрутку");

        mouseRelease(touch, touch.width / 2 + wheel.itemWidth * 2, touch.height / 2);
        verify(createdObject.page.interactive === true, "отпускание не вернуло прокрутку страницы");
    }
    // Обычная Column — ровно то окружение, в котором линейку держит `AppCard`:
    // каркас страницы и карточка раскладывают детей колонкой, а не ColumnLayout'ом.
    Component {
        id: columnHostComponent

        Item {
            id: columnHost

            readonly property alias ruler: columnRuler
            height: 200
            width: 346

            Column {
                anchors.fill: parent
                spacing: 8

                WeightRuler {
                    id: columnRuler

                    from: 35
                    to: 200
                }
            }
        }
    }
    // Окружение линейки на экране: `PageScaffold` — это Flickable, который
    // отбирает жест у ребёнка. Без него дефект «перетаскивание обрывается» не
    // воспроизводится, и починка осталась бы непроверенной. Полоса прокрутки
    // начинается выше линейки: жест только по линейке — то, что проверяется.
    Component {
        id: flickableHostComponent

        Item {
            id: flickableHost

            readonly property alias page: hostFlick
            readonly property alias ruler: hostedRuler
            height: 200
            width: 360

            Flickable {
                id: hostFlick

                anchors.fill: parent
                contentHeight: 400
                contentWidth: width

                Column {
                    spacing: 8
                    width: parent.width

                    Item {
                        height: 40
                        width: 1
                    }
                    WeightRuler {
                        id: hostedRuler

                        from: 20
                        height: 96
                        step: 5
                        to: 200
                        value: 60
                        width: 360
                    }
                }
            }
        }
    }
    // Колесо повторов в том же окружении: проверяется, что протяжка колеса не
    // превращается в прокрутку страницы.
    Component {
        id: wheelHostComponent

        Item {
            id: wheelHost

            readonly property alias page: wheelFlick
            readonly property alias wheel: hostedWheel
            height: 200
            width: 360

            Flickable {
                id: wheelFlick

                anchors.fill: parent
                contentHeight: 400
                contentWidth: width

                Column {
                    spacing: 8
                    width: parent.width

                    Item {
                        height: 40
                        width: 1
                    }
                    RepsWheel {
                        id: hostedWheel

                        height: 64
                        width: 280
                    }
                }
            }
        }
    }
    Component {
        id: rulerComponent

        WeightRuler {
            height: 96
            width: 360
        }
    }
    // Шкала из замеров: самый плотный случай в приложении (шаг 0,1 на 0–300).
    Component {
        id: denseRulerComponent

        WeightRuler {
            from: 35
            height: 96
            step: 0.1
            to: 200
            width: 360
        }
    }
    // Шкала инвентаря: шаг 2,5 кг, как в записи подхода.
    Component {
        id: plateRulerComponent

        WeightRuler {
            from: 20
            height: 96
            step: 2.5
            to: 200
            value: 60
            width: 360
        }
    }
    Component {
        id: repsWheelComponent

        RepsWheel {
            height: 64
            width: 280
        }
    }
}
