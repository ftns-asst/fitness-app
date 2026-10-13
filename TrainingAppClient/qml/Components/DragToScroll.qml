import QtQuick
import TrainingAppClient

// Прокрутка страницы жестом по «пустой» области вокруг контента.
//
// На широком экране контент живёт в центрированной колонке (`Theme.contentMaxWidth`),
// и вокруг неё остаётся область окна, которая странице не принадлежит: жест по ней
// раньше пропадал, и до прокрутки приходилось тянуться в центр. Компонент ловит
// перетаскивание и колесо и передаёт их прокручиваемой области активного экрана.
//
// Сам ничего не рисует и не перехватывает нажатия страницы: он объявлен ниже
// активного экрана, поэтому нажатия кнопок, ползунков и самой страницы до него
// не доходят.
Item {
    id: catcher

    // Нижняя (верхняя) граница прокрутки и сама цель. `target` — Flickable
    // активного экрана; null означает «прокручивать нечего».
    readonly property real maxContentY: catcher.target !== null && catcher.target !== undefined ? Math.max(-catcher.target.topMargin, catcher.target.contentHeight + catcher.target.bottomMargin - catcher.target.height) : 0
    readonly property real minContentY: catcher.target !== null && catcher.target !== undefined ? -catcher.target.topMargin : 0
    // Прокрутка нужна, только когда контент длиннее области.
    readonly property bool scrollable: catcher.target !== null && catcher.target !== undefined && catcher.maxContentY > catcher.minContentY
    property var target: null

    // Смещение «пальца»: положительное — палец вниз, контент едет вниз.
    function scrollBy(dy) {
        if (catcher.scrollable === false)
            return;

        catcher.target.contentY = Math.min(Math.max(catcher.target.contentY - dy, catcher.minContentY), catcher.maxContentY);
    }

    // Отпускание с движением: инерция остаётся родной для Flickable, а не
    // самодельной — жест по «пустому» месту должен ощущаться как жест по списку.
    //
    // Скорость передаётся без смены знака: у `Flickable::flick()` положительная
    // y-скорость двигает контент вниз, вслед за пальцем. Ошибка знака
    // разворачивает страницу после каждого быстрого жеста.
    function fling(velocityY) {
        if (catcher.scrollable === false)
            return;

        // Скорость ограничена как у настоящего жеста: синтетическое движение
        // одним событием даёт тысячи пикселей в секунду, и страница улетала бы
        // к краю от малейшего движения.
        catcher.target.flick(0, Math.max(-catcher.target.maximumFlickVelocity, Math.min(velocityY, catcher.target.maximumFlickVelocity)));
    }
    // Дельта перетаскивания считается по приращению translation: DragHandler
    // отдаёт накопленное смещение, а не шаг.
    property real lastTranslationY: 0

    DragHandler {
        id: drag

        // Только десктоп. На touch-платформах колонка контента занимает всю
        // ширину окна: «пустой» области вокруг неё нет, и вся прокрутка —
        // нативная работа Flickable страницы. Оставлять обработчик здесь было
        // нельзя: нажатия по неинтерактивным элементам страницы (Label, фон
        // карточек) события не принимают и спускаются в окно, после чего два
        // потребителя contentY — Flickable и этот обработчик — двигали страницу
        // одним жестом одновременно (дёрганье), а резкий «тычок» по пустому
        // месту уходил в инерцию от синтетической скорости — страница
        // «перепрыгивала» после быстрого касания-отпускания пальца.
        enabled: !Theme.touchPlatform

        // target не задан: обработчик читает жест, но не двигает сам Item.
        target: null

        onActiveChanged: {
            if (active) {
                // Пользователь начал drag — колесной анимации нельзя мешать:
                // анимация contentY останавливается немедленно.
                wheelAnim.stop();
                // Смещение, накопленное до включения обработчика, — часть жеста:
                // обработчик включается только после порога движения, и о
                // накопленном смещении он сообщает тем же событием. Без этого
                // применения медленный жест по «пустому» месту не прокручивал
                // страницу вовсе — двигал её только быстрый (инерцией).
                catcher.lastTranslationY = 0;
                catcher.scrollBy(drag.translation.y);
                catcher.lastTranslationY = drag.translation.y;
            } else if (catcher.scrollable) {
                catcher.fling(drag.centroid.velocity.y);
            }
        }
        onTranslationChanged: {
            if (active === false)
                return;

            catcher.scrollBy(drag.translation.y - catcher.lastTranslationY);
            catcher.lastTranslationY = drag.translation.y;
        }
    }
    // — Плавное колесо —
    //
    // Нативный Flickable анимирует прокрутку колесом (плавный разгон и
    // доткат), а мгновенные прыжки contentY на каждый тик выглядели рывками —
    // страница «дёргалась» и казалась запаздывающей. Цель накапливается между
    // тиками, а движение до неё анимируется; жест пользователя (drag, flick)
    // останавливает анимацию.
    property real wheelTargetY: 0

    NumberAnimation {
        id: wheelAnim

        duration: 220
        easing.type: Easing.OutCubic
        property: "contentY"
        target: catcher.target
    }

    function wheelBy(delta) {
        if (catcher.scrollable === false)
            return;

        // Повторные тики во время анимации накапливаются от цели, а не от
        // текущей позиции: быстрая прокрутка колесом не «съедает» ход.
        const base = wheelAnim.running ? catcher.wheelTargetY : catcher.target.contentY;

        wheelAnim.stop();
        catcher.wheelTargetY = Math.min(Math.max(base - delta, catcher.minContentY), catcher.maxContentY);
        wheelAnim.from = catcher.target.contentY;
        wheelAnim.to = catcher.wheelTargetY;
        wheelAnim.restart();
    }
    WheelHandler {
        acceptedDevices: PointerDevice.Mouse | PointerDevice.TouchPad
        enabled: catcher.scrollable

        // Колесо вверх показывает более ранний контент — как в списке под
        // курсором; поэтому положительная дельта даёт то же, что палец вниз.
        onWheel: function (event) {
            catcher.wheelBy(event.angleDelta.y);
        }
    }
    Connections {
        target: catcher.target

        // Пользователь начал drag/flick пальцем или мышью — колесная анимация
        // немедленно уступает: два писателя contentY дёргали бы страницу.
        function onMovementStarted() {
            wheelAnim.stop();
        }
    }
}
