import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Линейка ввода веса (§4.9, спринт S8 / issue 11): перетаскивание с привязкой к
// шагу инвентаря, шкала из пяти равномерных подписей, как в макетах.
//
// Почему линейка, а не поле с кнопками «−/+»: в зале это одна рука и взгляд на
// штангу, и каждый шаг ввода кнопкой — десятки нажатий за подход. Но линейка
// **обязана** выдавать только кратные шагу значения: ближайший инвентарь — плитки
// по 0,1/0,25/0,5/1/2 кг, поэтому промежуточные «54,37 кг» не существуют, и
// запись с таким весом бессмысленна. Проверяет это `tst_weight_ruler_snap.qml`.
//
// Значение — свойство компонента, наружу отдаётся сигналом `edited`, как у
// AppNumberField: ползунок не знает про подходы и тренировки.
Item {
    id: ruler

    // Диапазон и шаг инвентаря, кг. Границы совпадают с пределами AppNumberField
    // на других экранах — иначе одно и то же поле давало бы разные числа в
    // зависимости от того, где им заполняли.
    property real from: 0
    property real to: 300
    property real step: 2.5
    property real value: 0
    // Показывать подпись значения: на записи подхода вес — главное число, и
    // искать его среди мелких меток бессмысленно.
    property bool showValue: true
    // Разрешить ввод числа с клавиатуры по нажатию на значение. Ползунок
    // удобен на глазок, но шаг 0,1 кг на шкале 35–200 — это сотни делений, и
    // поставить вес точно пальцем невозможно: ровно 5 шагов на десятую долю
    // уходит в субпиксельные расстояния. Без ручного ввода вес 74,3 кг был бы
    // недостижим в принципе, а не «неудобен».
    property bool manualInputEnabled: true
    // Подпись поля слева от значения («Вес», «Целевой вес»). На экране дня
    // линейка стоит внутри карточки, где заголовок поля уже не нужен, а в
    // макете подпись идёт слева от числа на одной строке.
    property string title: ""
    // Знаков после запятой в подписи значения: в макете экрана дня вес показан
    // как «100,0», потому что шаг 2,5 кг — и «100» читалось бы как другой вес.
    property int valueDecimals: 1
    // Единица измерения рядом со значением. У ползунка подходов она пустая: тот же
    // контрол показывает и повторы, а «9 кг подходов» звучало бы как опечатка.
    property string unit: qsTr("кг")
    // Число подписей шкалы. Во всех макетах (`04-day.html`, `03-edit.html`,
    // `05-rules.html`, `07-states.html`, `08-program-rules.html`) у линейки
    // ровно пять равномерных подписей — «20 65 110 155 200», «50 70 90 110
    // 130», «1 14 27 40 52» — и ни одного мелкого штриха между ними: масштаб
    // читается по числам, дорожка — тонкая. Прежняя версия рисовала плотную
    // линейку из десятков делений с прореживанием, и рядом с макетом разница
    // видна сразу: не «гравированная шкала», а тонкая дорожка с пятью
    // короткими штрихами.
    property int labelCount: 5
    // Подсказка под шкалой (`.slider-hint` в макете): «Шаг 2,5 кг · как в
    // записи тренировки». Пустая строка места не занимает.
    property string hint: ""

    // Минимально допустимое расстояние между подписями, px. Ширина трёхзначного
    // числа («300») около 22 px, и на узкой шкале подписи слипались бы в
    // «0 20 40 60». Вместо наезда лишние подписи убираются: на узкой шкале
    // четыре читаемых числа лучше, чем пять слипшихся.
    readonly property real labelMinGap: 34

    // Фактическое число подписей: не больше labelCount и не ближе друг к другу,
    // чем labelMinGap. На ширине карточки (292–456 px) помещаются все пять, как
    // в макетах; сужение работает на самых узких местах и не меняет вид
    // обычных экранов.
    readonly property int scaleLabelCount: Math.max(2, Math.min(ruler.labelCount, Math.floor(ruler.scaleWidth / ruler.labelMinGap) + 1))

    // Высота делений шкалы: внутренние — 8 px цветом рамки, краевые — 12 px и
    // заметнее (`.slider-scale span::before` и `:first/:last-child::before`).
    // Наружу — чтобы тест проверял, что края шкалы выделены.
    readonly property real minorTickHeight: 8
    readonly property real edgeTickHeight: 12

    signal edited(real value)

    // Открыт ли ввод с клавиатуры. Состояние компонента, а не аргумент
    // наверх: экрану знать о нём нечего, а потерять его при пересоздании
    // объекта нельзя — нажатая клавиатура должна остаться открытой.
    property bool manualInputActive: false
    // Значение, которое пользователь набирает. Держится отдельно от `value`:
    // промежуточный текст («74,», «74.3») не является весом и не должен
    // попадать в модель до подтверждения.
    property string manualText: ""
    // Минимальная ширина зоны нажатия на значение, px. Число «100,0» вместе с
    // единицей занимает около 90 px, но короткое «80» — вдвое меньше, и
    // целиться в него пальцем становится заметно труднее, чем в длинное.
    readonly property real minimumTapWidth: 110
    // Минимальная высота любой зоны нажатия, px. Android считает надёжной
    // целью для пальца 48 dp, а макет просит 44 (`.slider-input`); берём 44
    // как меньшее из двух, чтобы не раздувать компонент без нужды.
    readonly property real minimumTouchPx: 44

    // Приводит набранный текст к числу и зажимает в диапазон. Разбирается в
    // единственном месте, поэтому и запятая, и точка приводят к одному
    // результату, а «74,3 кг» не превращается в NaN.
    function parseManualText(text) {
        // Пробелы и неразрывные пробелы — обычное дело после автозаполнения
        // из буфера обмена, а «кг» человек дописывает по привычке из поля.
        const cleaned = String(text).replace(/[\s\u00a0]/g, "").replace(/[^\d.,-]/g, "").replace(",", ".");
        const parsed = Number.parseFloat(cleaned);

        if (!isFinite(parsed))
            return null;

        return parsed;
    }

    // Геометрия взята из `docs/mockups-programs/theme.css` (`.slider-*`), который
    // повторяет референс 2090: трек 6 px, ползунок-черта 4×22 px, зона захвата
    // 44 px, подписи 11 px.
    //
    // Высота полос вынесена в свойства, потому что по ней считается
    // implicitHeight: раньше полосы стояли по месту, и лишняя высота уезжала в
    // пустоту над значением.
    readonly property real valueHeight: 36
    // Отступ от строки значения до трека (`.slider-track { margin: 16px … 0 }`).
    readonly property real headGap: 16
    readonly property real trackHeight: 6
    // Полоса под треком: деления и подписи шкалы растут вниз от трека, как в
    // референсе.
    readonly property real ticksHeight: 14
    readonly property real labelsHeight: 16
    // Подсказка под шкалой (`.slider-hint`: отступ 10 px сверху, кегль 12).
    readonly property real hintHeight: 26

    implicitHeight: ruler.valueHeight + ruler.headGap + ruler.trackHeight + ruler.ticksHeight + ruler.labelsHeight + (ruler.hint.length > 0 ? ruler.hintHeight : 0)
    // Ширина — во всю ширину родителя. Раньше ширина не задавалась вовсе, и в
    // обычной Column (каркас страницы и внутренности AppCard, см.
    // PageScaffold.qml: «Layout.* у детей этой колонки не работает») ребёнок
    // берёт свой implicitWidth: шкала выходила 280 px при 346 доступных и
    // стояла сжатой полосой у левого края — ползунок «смещён влево» вместо
    // `.slider-track` из макета во всю ширину карточки с отступами 12 px.
    //
    // Привязка повторяет `width: parent.width`, который в этом проекте ставят
    // детям обычной Column вручную (кнопки, поля, подписи), и остаётся запасом
    // на случай нового места использования. В ColumnLayout/RowLayout она не
    // мешает: layout выставляет ширину сам и перекрывает привязку, поэтому
    // `Layout.fillWidth` у вызывающего остаётся решающим.
    width: parent ? parent.width : implicitWidth
    // Запас по ширине для соло-использования: значение 320 давало SHRUNK-WIDTH —
    // карточка сжимала линейку до 292, и аудит читал это как потерю содержимого.
    implicitWidth: 280

    // Боковые поля шкалы (`.slider-track { margin: … 12px … }`). Они равны
    // половине невидимого thumb, из-за чего у краёв значения 0 и `to` ползунок
    // не обрезается краем карточки, а подписи стоят не под самым краем.
    readonly property real scaleMargin: 12

    // Ширина рабочей части шкалы, без боковых полей.
    readonly property real scaleWidth: Math.max(1, ruler.width - ruler.scaleMargin * 2)

    // Ширина одного шага в пикселях: от неё зависят метки, длина движения и
    // длина заливки.
    readonly property real stepPx: ruler.scaleWidth / Math.max(1, (ruler.to - ruler.from) / ruler.step)

    // Позиция значения на шкале, px от левого края компонента. Единая точка
    // расчёта для меток, заливки и ползунка: раньше каждая считала координату
    // по-своему, и ползунок уезжал относительно делений на половину шага.
    readonly property real valueX: ruler.scaleMargin + (ruler.value - ruler.from) / ruler.step * ruler.stepPx

    // Доля пройденного пути, 0…1 — длина заливки трека (`.slider-fill`).
    readonly property real fillRatio: Math.max(0, Math.min(1, (ruler.value - ruler.from) / Math.max(0.001, ruler.to - ruler.from)))

    // Геометрия наружу. Нужна тестам (`tst_weight_ruler_snap.qml`) и любому
    // следующему коду, который обязан знать, где ползунок и какого размера зона
    // захвата: без этих свойств «крупный палец» нечем проверить, а сузить зону
    // можно молча, не сломав ни одного теста.
    readonly property alias touchArea: touch
    readonly property alias valueTap: valueTap
    // Шкала и поле ручного ввода — наружу для тестов: без этого «пять подписей,
    // как в макете» и «поле получило фокус» нечем проверять, а сломать можно
    // молча, не задев ни одного теста.
    // Идентификатор — rulerScale, а не scale: `scale` — встроенное свойство
    // Item (double), и ссылки вида `scale.width` разрешались бы в него.
    readonly property alias scaleArea: rulerScale
    readonly property alias manualField: inputField
    // Черта ползунка наружу: рост под пальцем иначе можно выключить молча, не
    // сломав ни одного теста.
    readonly property alias thumbBar: thumbBar
    // Палец на шкале прямо сейчас. Отдельное имя, а не `touch.pressed`: разметка
    // и тесты читают состояние «ползунок в руке», а не то, каким обработчиком
    // оно набрано, — иначе замена MouseArea на DragHandler трогала бы разметку.
    readonly property bool gripActive: touch.pressed
    readonly property real trackTop: ruler.valueHeight + ruler.headGap
    // Центр видимой черты ползунка, px от левого края компонента.
    readonly property real thumbCenter: ruler.valueX

    // Трек. В макете это скруглённая полоса 6 px с цветной заливкой до значения
    // (`.slider-track` + `.slider-fill`), а не тонкая линия: заливка показывает
    // долю пути взглядом, не читая число. Раньше трек был линией в 2 px без
    // заливки, и шкала читалась только по делениям.
    Item {
        id: track

        anchors.left: parent.left
        anchors.leftMargin: ruler.scaleMargin
        anchors.right: parent.right
        anchors.rightMargin: ruler.scaleMargin
        anchors.top: parent.top
        anchors.topMargin: ruler.valueHeight + ruler.headGap
        height: ruler.trackHeight

        Rectangle {
            anchors.fill: parent
            // Рамка внутри, как в макете (`box-shadow: inset`): полоса должна
            // читаться и на светлом фоне, где она почти сливается с карточкой.
            border.color: Theme.border
            border.width: Theme.borderWidth
            color: Theme.surfaceMuted
            radius: height / 2
        }
        // Заливка до текущего значения. Скругление то же, иначе правый край
        // заливки выглядит срезанным углом внутри скруглённого трека.
        Rectangle {
            anchors.left: parent.left
            anchors.top: parent.top
            anchors.bottom: parent.bottom
            color: Theme.accent
            radius: height / 2
            width: parent.width * ruler.fillRatio
        }
    }

    // Ползунок-черта: 4×22 px со скруглением (`.slider-thumb`). Высота больше
    // трека вчетверо, поэтому он читается и служит ориентиром на метках.
    //
    // Видимая часть — узкая черта, а Item шириной 44 dp: попадать пальцем
    // приходится в движении, и по черте в 4 px это невозможно. Зона захвата
    // шире риска в 11 раз, поэтому промахнуться мимо ползунка почти нельзя.
    Item {
        id: indicator

        // Черта поднимается над треком на 8 px (`.slider-thumb { top: -8px }`):
        // в макете она торчит над дорожкой на 8 px и под ней на столько же,
        // а целиком под треком она наезжала нижним краем на деления шкалы.
        anchors.top: track.top
        // −14, а не −8: контейнер стал выше (34 вместо 22), и без сдвига черта
        // в покое уехала бы на 6 px ниже — ровно на столько, на сколько она
        // растёт под пальцем. Центр контейнера обязан совпадать с центром трека
        // в обоих состояниях.
        anchors.topMargin: -14
        height: 34
        width: 44
        // Позиция — по центру зоны захвата, а не по её левому краю: иначе при
        // захвате за середину черты ползунок уезжал бы на половину зоны.
        x: Math.max(0, Math.min(ruler.width - width, ruler.valueX - width / 2))

        // Ореол при зажатии: палец закрывает саму черту, и один только рост
        // высоты читался бы лишь после отрыва руки. Ореол шире пальца и мягкий —
        // он говорит «ползунок в руке», а не «значение изменилось».
        Rectangle {
            anchors.centerIn: parent
            color: Theme.accentTint
            height: ruler.gripActive ? 40 : 0
            opacity: ruler.gripActive ? 1 : 0
            radius: height / 2
            width: ruler.gripActive ? 40 : 0

            // Ореол и рост черты — отклик на зажатие: короткий, но мягкий переход.
            Behavior on height {
                NumberAnimation {
                    duration: Theme.pressFeedbackDuration
                    easing.type: Easing.OutCubic
                }
            }
            Behavior on opacity {
                NumberAnimation {
                    duration: Theme.pressFeedbackDuration
                    easing.type: Easing.OutCubic
                }
            }
            Behavior on width {
                NumberAnimation {
                    duration: Theme.pressFeedbackDuration
                    easing.type: Easing.OutCubic
                }
            }
        }
        // Черта растёт под пальцем — это и есть визуальный отклик на зажатие.
        // В покое размеры прежние (4×22, скругление width / 2 совпадает с
        // прежним radius: 2), поэтому выросший ползунок читается как состояние,
        // а не как другой компонент.
        Rectangle {
            id: thumbBar

            anchors.centerIn: parent
            color: Theme.accent
            height: ruler.gripActive ? 30 : 22
            radius: width / 2
            width: ruler.gripActive ? 12 : 4

            Behavior on height {
                NumberAnimation {
                    duration: Theme.pressFeedbackDuration
                    easing.type: Easing.OutCubic
                }
            }
            Behavior on width {
                NumberAnimation {
                    duration: Theme.pressFeedbackDuration
                    easing.type: Easing.OutCubic
                }
            }
        }
    }
    // Шкала — как в макетах: под треком несколько равномерных делений с
    // числами, по одному на подпись, и ничего между ними. Плотная линейка из
    // десятков штрихов с прореживанием смотрелась «гравировкой» и не совпадала
    // ни с одним макетом: в `04-day.html`, `03-edit.html`, `07-states.html` у
    // шкалы пять чисел и пять коротких штрихов.
    //
    // Деление стоит в точке своего значения на дорожке; крайние — выше и
    // ярче (`:first/:last-child::before` в макете: 12 px и text-muted против
    // 8 px и border). Число под делением: первое прижато к левому краю шкалы,
    // последнее — к правому, центральные центрированы по своему делению
    // (`justify-content: space-between`).
    Item {
        id: rulerScale

        anchors.left: parent.left
        anchors.leftMargin: ruler.scaleMargin
        anchors.right: parent.right
        anchors.rightMargin: ruler.scaleMargin
        anchors.top: track.bottom
        height: ruler.ticksHeight + ruler.labelsHeight

        Repeater {
            model: ruler.scaleLabelCount

            delegate: Item {
                id: scaleMark

                required property int index

                readonly property int lastIndex: ruler.scaleLabelCount - 1
                readonly property bool isEdge: scaleMark.index === 0 || scaleMark.index === scaleMark.lastIndex
                // Деление обязано стоять в точке своего значения: подпись «65»
                // под другим местом дорожки — шкала, которая врёт.
                readonly property real markX: scaleMark.index / Math.max(1, scaleMark.lastIndex) * rulerScale.width
                readonly property real markValue: ruler.scaleMarkValue(scaleMark.index)
                // Высота деления наружу — для теста: «края выделены» иначе
                // нечем проверять.
                readonly property real tickHeight: scaleMark.isEdge ? ruler.edgeTickHeight : ruler.minorTickHeight
                // Подпись деления наружу — для теста: наезд подписей
                // проверяется по самой подписи, а не по расчётной ширине.
                readonly property alias markLabel: scaleLabel

                // Делегат — на всю ширину шкалы, а не в 1 px деления: подпись
                // шире деления, и в узком делегате она вылезает за родителя
                // (OVERFLOW-X в аудите вёрстки). Позиции детей — абсолютные,
                // от точки значения деления.
                anchors.top: parent.top
                height: parent.height
                width: parent.width

                Rectangle {
                    // Деления выровнены нижним краем по кромке подписей, как
                    // ::before в макете: крайние тянутся выше, к самой дорожке.
                    color: scaleMark.isEdge ? Theme.textMuted : Theme.border
                    height: scaleMark.tickHeight
                    width: 1
                    x: Math.max(0, Math.min(parent.width - width, scaleMark.markX - width / 2))
                    y: ruler.ticksHeight - height
                }

                Label {
                    id: scaleLabel

                    anchors.top: parent.top
                    anchors.topMargin: ruler.ticksHeight
                    color: Theme.textMuted
                    // Кегль 11 px — как `.slider-scale` в макете: числа шкалы —
                    // второстепенный текст и не спорят со значением над дорожкой.
                    font: Typography.tabLabel
                    text: ruler.formatScaleMark(scaleMark.markValue)
                    // Краевые подписи прижимаются к краям шкалы, иначе «200»
                    // вылезает за карточку и обрезается.
                    x: Math.max(0, Math.min(parent.width - width, scaleMark.markX - width / 2))
                }
            }
        }
    }

    // Подсказка под шкалой (`.slider-hint`): «Шаг 2,5 кг · как в записи
    // тренировки». Пустая — не показывается и не занимает места.
    Label {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: rulerScale.bottom
        anchors.topMargin: 10
        color: Theme.textMuted
        font: Typography.caption
        text: ruler.hint
        visible: ruler.hint.length > 0
    }
    // Подпись поля слева: в макете экрана дня («Вес») и макете записи подхода
    // («Вес») она стоит в одной строке со значением.
    Label {
        anchors.left: parent.left
        anchors.top: parent.top
        color: Theme.textSecondary
        // Кегль подписи — как в макете (`.slider-label`, 12 px), а не кегль
        // основного текста: подпись не должна спорить со значением.
        font: Typography.captionStrong
        height: ruler.valueHeight
        text: ruler.title
        verticalAlignment: Text.AlignVCenter
        visible: ruler.title.length > 0
    }
    // Значение: крупно и справа, как в референсе 2090 и макете экрана дня — по
    // нему выставляют блины.
    //
    // Это Item, а не Row: зона нажатия должна быть шириной не меньше
    // minimumTapWidth независимо от длины числа, а Row раскладывает детей сам
    // и запрещает им якоря («Row will not function»).
    Item {
        id: valueRow

        anchors.right: parent.right
        anchors.top: parent.top
        // Число и единица скрываются на время ввода: поле занимает их место, и
        // два числа рядом читались бы как два разных значения.
        height: ruler.valueHeight
        visible: ruler.showValue && !ruler.manualInputActive
        width: Math.max(ruler.minimumTapWidth, ruler.minimumTouchPx)

        // Нажатие на значение открывает ввод. Сама строка кликабельна, а не
        // только число: попадать пальцем нужно в крупную цель, и «62,5 кг»
        // шириной в несколько пикселей такой целью не является.
        MouseArea {
            id: valueTap

            // Высота задана явно, а не через `anchors.fill`: строка значения по высоте
            // текста — 36 px, и зона нажатия оказалась бы на 8 px ниже
            // минимальной цели для пальца.
            anchors.horizontalCenter: parent.horizontalCenter
            height: Math.max(ruler.valueHeight, ruler.minimumTouchPx)
            width: parent.width

            cursorShape: Qt.PointingHandCursor
            enabled: ruler.manualInputEnabled && !ruler.manualInputActive

            onClicked: ruler.openManualInput()
        }
        Row {
            anchors.bottom: parent.bottom
            anchors.right: parent.right
            height: ruler.valueHeight
            spacing: 5

            Label {
                color: Theme.textPrimary
                font: Typography.metric
                height: ruler.valueHeight
                horizontalAlignment: Text.AlignRight
                text: ruler.formatWeight(ruler.value, ruler.valueDecimals)
                verticalAlignment: Text.AlignVCenter
            }
            Label {
                color: Theme.textMuted
                font: Typography.captionStrong
                height: ruler.valueHeight
                text: ruler.unit
                verticalAlignment: Text.AlignVCenter
                visible: ruler.unit.length > 0
            }
        }
    }

    // Поле ручного ввода поверх строки значения. Показывается вместо числа и
    // единицы, а не рядом с ними: рядом строка разъезжалась бы, и при
    // открытой клавиатуре оба числа читались бы как два разных значения.
    TextField {
        id: inputField

        // Поле привязано к правому верхнему углу компонента, а не к строке
        // значения: строка скрывается на время ввода, и поле, привязанное к
        // скрытому родителю, схлопнулось бы в ноль вместе с ней.
        anchors.top: parent.top
        anchors.right: parent.right
        // Поле шире строки значения: иначе в «100,0» не влез бы курсор, а
        // двухзначное «80» выглядело бы пустым широким боксом. Ширина не
        // зависит от строки значения: та скрыта, пока открыто поле.
        width: Math.max(ruler.minimumTapWidth, 96)
        height: ruler.valueHeight
        visible: ruler.manualInputActive

        // Числовая клавиатура: без неё пришлось бы переключать раскладку, чтобы
        // набрать цифру. Decimal — из-за запятой в русской раскладке.
        inputMethodHints: Qt.ImhFormattedNumbersOnly
        color: Theme.textPrimary
        font: Typography.metric
        horizontalAlignment: Text.AlignRight
        // Отступы обнулены: стандартные 6+6 px съедали высоту поля, и текст
        // кеглем 28 px переставал влезать в 36 px (TEXT-HEIGHT в аудите).
        // Поле не выше строки значения, поэтому и отступов ему не нужно.
        bottomPadding: 0
        leftPadding: 6
        rightPadding: 6
        topPadding: 0
        // Без плейсхолдера: пустая подсказка в поле, которое и так открыто
        // поверх числа, только мешает, а audit считает её за переполнение.
        placeholderText: ""
        // Текст кладётся нативному полю, а не подменяется Label: системная
        // клавиатура пишет прямо в TextField, и курсор должен быть там же.
        selectByMouse: true
        // Поле и разбор текста связаны явно, а не через `text` в одну строку:
        // пока пользователь набирает «74,», значение меняться не должно, иначе
        // ползунок дёргался бы под клавиатурой на каждом символе.
        text: ruler.manualText
        onTextEdited: ruler.manualText = text

        background: Rectangle {
            border.color: Theme.accent
            border.width: Theme.borderWidth
            color: Theme.surface
            radius: Theme.radiusSm
        }

        // Поле берёт фокус в момент, когда оно уже видно, и не сразу, а через
        // Qt.callLater: на Android (целевая платформа — телефоны) фокус в
        // поле, чья привязка visible ещё не пересчиталась, не поднимает
        // системную клавиатуру — ввод «открывался», а набирать было некуда.
        // callLater даёт сцене закончить пересчёт и только потом берёт фокус;
        // на десктопе поведение то же самое, только без гонки.
        onVisibleChanged: {
            if (visible)
                Qt.callLater(inputField.forceActiveFocus);
        }

        // Подтверждение по Enter и по «Готово» на клавиатуре ведёт себя
        // одинаково: иначе на телефоне ввод закрывался бы, но не применялся.
        onAccepted: ruler.commitManualInput()

        // Потеря фокуса (прокрутка, смена экрана) применяет введённое, а не
        // отменяет его молча: человек уже показал, что хотел это число.
        onActiveFocusChanged: {
            if (!activeFocus && ruler.manualInputActive)
                ruler.commitManualInput();
        }
    }

    function openManualInput() {
        if (!ruler.manualInputEnabled || ruler.manualInputActive)
            return;

        // Текст стартует пустым, а не с текущим значением: набранное поверх
        // «74,3» без выделения дописывалось бы в конец и давало «74,3743».
        ruler.manualText = "";
        ruler.manualInputActive = true;
        // Фокус здесь не берётся: он привязан к появлению поля
        // (onVisibleChanged ниже) — фокус в ещё не показавшееся поле на
        // Android клавиатуру не поднимает.
    }

    function commitManualInput() {
        const parsed = ruler.parseManualText(ruler.manualText);

        ruler.manualInputActive = false;
        // Пустой или нечитаемый ввод отменяет правку, а не обнуляет вес:
        // обнуление молча отправило бы в запись 0 кг.
        if (parsed === null) {
            ruler.manualText = "";
            return;
        }

        ruler.setValue(parsed);
        ruler.manualText = "";
    }
    // Значение k-й подписи шкалы: k-я доля диапазона, включая оба края.
    // Равномерность взята из макета (`space-between`), а не подбор «круглых»
    // чисел: круглые шаги у разных диапазонов то слипаются, то оставляют
    // полшкалы без подписей.
    function scaleMarkValue(index) {
        const last = Math.max(1, ruler.scaleLabelCount - 1);

        return ruler.from + (ruler.to - ruler.from) * index / last;
    }

    // Подписи шкалы — целые числа, как во всех макетах («20 65 110 155 200»,
    // «1 7 14 20»). Дробь остаётся только там, где без неё подпись врала бы
    // сильнее округления: у RPE «3» вместо «2,5» — уже другое число.
    function formatScaleMark(value) {
        const rounded = Math.round(value);

        if (Math.abs(value - rounded) < 0.25)
            return String(rounded);

        return Number(value).toFixed(1).replace(".", ",");
    }

    // поэтому разделитель меняется руками. Разрядность задаёт вызывающий: на
    // экране дня в макете «100,0», а на записи подхода целые веса идут без
    // знаков, и «60,0 кг» читалось бы как другой вес.
    function formatWeight(number, decimals) {
        const digits = decimals === undefined ? (Math.abs(number) % 1 < 0.001 ? 0 : 1) : decimals;

        return Number(number).toFixed(digits).replace(".", ",");
    }

    // Номер шага под точкой x — абсолютная привязка: по ней ползунок встаёт под
    // палец при нажатии. Отдельно от snapTo, потому что тот считает смещение от
    // текущего значения и годится только для движения, а не для «поставить здесь».
    function indexAt(xPosition) {
        const index = Math.round((xPosition - ruler.scaleMargin) / ruler.stepPx);
        const lastIndex = Math.floor((ruler.to - ruler.from) / ruler.step);

        return Math.max(0, Math.min(lastIndex, index));
    }

    // Позиция курсора → номер шага. Расстояние считается от центра ползунка, а не
    // от левого края: иначе при захвате ползунок уезжал бы на половину шага.
    function snapTo(xPosition) {
        const center = indicator.x + indicator.width / 2;
        const currentIndex = Math.round((ruler.value - ruler.from) / ruler.step);
        const index = Math.round((xPosition - center) / ruler.stepPx) + currentIndex;
        const lastIndex = Math.floor((ruler.to - ruler.from) / ruler.step);

        return Math.max(0, Math.min(lastIndex, index));
    }

    // Значение по номеру шага. Округление до кратного шага — единственное место,
    // где появляется вес: снаружи «54,37 кг» взяться не может.
    function snappedValue(index) {
        const steps = Math.round((ruler.from + index * ruler.step) / ruler.step);

        return Math.round(steps * ruler.step * 100) / 100;
    }

    function commitValue(index) {
        const snapped = ruler.snappedValue(index);

        if (ruler.value === snapped)
            return;

        ruler.value = snapped;
        ruler.edited(snapped);
    }

    // Программная установка (например, вес из последнего подхода) проходит через
    // ту же решётку: иначе на экране появилось бы значение, до которого нельзя
    // дотянуть ползунком.
    function setValue(newValue) {
        const clamped = Math.max(ruler.from, Math.min(ruler.to, newValue));

        ruler.commitValue(Math.round((clamped - ruler.from) / ruler.step));
    }

    // — Сосуществование с прокруткой страницы —
    //
    // Линейка лежит внутри Flickable страницы (`PageScaffold`), и одна полоса
    // экрана делится двумя жестами: вертикальной прокруткой страницы и
    // горизонтальным ведением ползунка. Прокрутка бывает «в моменте» касания:
    // страница едет по инерции, палец приземляется на линейку, и нажатие в
    // этот миг не должно менять значение — иначе значение «перепрыгивало» при
    // быстром касании и отпускании пальца.
    //
    // Состояние прокрутки читается у ближайшего Flickable-предка: компонент не
    // хочет знать про PageScaffold, поэтому предок ищется обходом родителей.
    property var pageScroll: null
    // Прокрутка заблокирована активным ползунком прямо сейчас: черта в руке —
    // страница стоит (interactive=false гасит и движение, и отбор жеста).
    // Отпускание всегда возвращает прокрутку, включая страховку onDestruction.
    property bool pageScrollLocked: false
    // Радиус явного захвата черты ползунка, px. Ореол зажатия — 40 px, черта —
    // 4–12 px: пальцу нужна полоса вокруг черты, чтобы «взять» её и вести как
    // есть; дальше от неё полоса остаётся тап-зоной и отдаёт вертикаль
    // прокрутке страницы.
    readonly property real gripZonePx: 24

    Component.onCompleted: {
        let ancestor = ruler.parent;

        while (ancestor !== null && ancestor !== undefined) {
            if (ancestor instanceof Flickable) {
                ruler.pageScroll = ancestor;
                return;
            }

            ancestor = ancestor.parent;
        }
    }
    // Прокрутка едет прямо сейчас (drag или инерция). Прежнее «мёртвое окно»
    // 200 мс после остановки откатано: оно заставляло линейку игнорировать
    // касания сразу после скролла, и страница ощущалась запаздывающей. Тап по
    // ползунку в момент докатившейся инерции переставляет значение — как в
    // нативных списках, где тап по элементу останавливает прокрутку.
    function pageScrollBusy() {
        return ruler.pageScroll !== null && ruler.pageScroll !== undefined && ruler.pageScroll.moving === true;
    }

    // Блокировка прокрутки на время активного ползунка: interactive=false
    // гасит и текущее движение, и отбор жеста — черта в руке всегда достаётся
    // ползунку, что бы палец ни делал по вертикали.
    function lockPageScroll() {
        if (ruler.pageScrollLocked || ruler.pageScroll === null || ruler.pageScroll === undefined)
            return;

        ruler.pageScrollLocked = true;
        ruler.pageScroll.interactive = false;
    }

    function unlockPageScroll() {
        if (!ruler.pageScrollLocked || ruler.pageScroll === null || ruler.pageScroll === undefined)
            return;

        ruler.pageScrollLocked = false;
        ruler.pageScroll.interactive = true;
    }

    // Страховка: ползунок разрушен посреди жеста (смена экрана) — прокрутка
    // обязана вернуться, иначе страница остаётся заблокированной навсегда.
    Component.onDestruction: ruler.unlockPageScroll()

    // Жест перетаскивания. Отдельный MouseArea, а не Drag: значение должно
    // меняться по движению, а Drag сжимает шкалу под курсором.
    //
    // Зона захвата — вся полоса шкалы высотой 44 px по центру трека
    // (`.slider-input { height: 44px }` в макете): это минимум, который
    // Android считает надёжной целью для пальца. Раньше зона была высотой во
    // весь компонент и начиналась от его верха, то есть попадать в неё нужно
    // было целясь выше трека, и на длинном экране замеров промах был частым.
    MouseArea {
        id: touch

        anchors.horizontalCenter: parent.horizontalCenter
        // 44 px, а не произвольные 44: значение берётся из minimumTouchPx, чтобы
        // требование «крупная цель» жило в одном месте, а не в двух числах,
        // которые разъедутся при правке одного из них.
        height: ruler.minimumTouchPx
        width: parent.width

        // Полоса выровнена по центру трека, а не по верху компонента: трек — то,
        // куда целится палец, и зона должна быть вокруг него.
        y: track.y + track.height / 2 - height / 2

        // preventStealing решается на каждое нажатие (см. onPressed), а не стоит
        // константой. Постоянный захват делал вертикальные свайпы по полосе
        // линейки — она стоит в карточках самого прокручиваемого экрана дня —
        // «мёртвыми»: страница не ехала, хотя горизонтального ведения не было.
        // Конфликт снимает осесознание Qt: вертикальный Flickable отбирает жест
        // у ребёнка только по превышению порога на своей оси
        // (QQuickFlickablePrivate::handleMoveEvent), поэтому горизонтальное
        // ведение ползунка остаётся у линейки и без preventStealing, а
        // вертикальный свайп честно прокручивает страницу.
        preventStealing: false

        // Значение на момент нажатия: если жест перехватит прокрутка страницы,
        // тап-позиционирование откатывается (см. onCanceled) — иначе свайп по
        // дорожке прыгал бы значением ещё до начала самой прокрутки.
        property real pressValue: 0
        // Точка нажатия: от неё считается намерение жеста (горизонталь —
        // ползунок, вертикаль — страница).
        property real pressX: 0
        property real pressY: 0
        // Нажатие — явный захват черты (касание рядом с ней): значение не
        // перескакивает к пальцу и не откатывается при перехвате жеста.
        property bool grabPress: false
        // Ползунок активен: захват черты или подтверждённое горизонтальное
        // намерение. В этом состоянии прокрутка страницы заблокирована.
        property bool dragMode: false

        onPressed: function (event) {
            // Касание во время прокрутки — жест прокрутки: значение не
            // меняем и прокрутку не перехватываем. Отказ от события оставляет
            // палец странице.
            if (ruler.pageScrollBusy()) {
                event.accepted = false;
                return;
            }

            touch.pressValue = ruler.value;
            touch.pressX = event.x;
            touch.pressY = event.y;
            // Явный захват — только возле самой черты ползунка: её берут как
            // есть, без перескока значения к пальцу и без отката при отмене
            // жеста — иначе вертикальное движение при удержании черты выглядело
            // как «ползунок резко сбрасывается» (отзыв о тач-проверке).
            touch.grabPress = Math.abs(event.x - ruler.thumbCenter) <= ruler.gripZonePx;
            // Захват черты — активный ползунок: прокрутка блокируется целиком
            // (отзыв: «блокировать вертикальный прокрут, когда активен
            // слайдер»), иначе диагональный жест уводил страницу из-под черты.
            touch.dragMode = touch.grabPress;
            touch.preventStealing = touch.dragMode;

            if (touch.dragMode)
                ruler.lockPageScroll();

            // Тап вне ползунка сразу переставляет его под палец (контракт
            // tst_weight_ruler_snap.qml: «нажатие не поставило ползунок под
            // палец»). Без этого значение менялось только от движения, и тап
            // «поставить здесь» не работал — на сенсорном экране это выглядело
            // как «ползунок не отзывается». Если жест уйдёт прокрутке,
            // onCanceled вернёт pressValue.
            if (!touch.grabPress)
                ruler.commitValue(ruler.indexAt(event.x));
        }

        onPositionChanged: function (event) {
            if (!pressed)
                return;

            if (!touch.dragMode) {
                // Развязка жестов: подтверждённое горизонтальное намерение
                // (|dx| ≥ 8 и больше вертикали) забирает ползунок и блокирует
                // прокрутку; вертикальное ведение остаётся странице — свайп
                // «по касательной» через дорожку по-прежнему листает экран,
                // иначе полоса линейки во всю ширину карточки стала бы
                // мёртвой зоной прокрутки.
                const dx = event.x - touch.pressX;
                const dy = event.y - touch.pressY;

                if (Math.abs(dx) < 8 || Math.abs(dx) <= Math.abs(dy))
                    return;

                touch.dragMode = true;
                touch.preventStealing = true;
                ruler.lockPageScroll();
            }

            ruler.commitValue(ruler.snapTo(event.x));
        }

        onCanceled: {
            // Прокрутка отобрала жест. Для тап-позиционирования (вне черты)
            // значение возвращается к нажатому — иначе свайп через дорожку
            // «дёргал» бы ползунок мимо воли пользователя. Явный захват черты
            // НЕ откатывается: пользователь вёл её сам, откат выглядел бы как
            // «резкий сброс».
            if (!touch.grabPress && ruler.value !== touch.pressValue)
                ruler.setValue(touch.pressValue);

            ruler.unlockPageScroll();
            touch.preventStealing = false;
            touch.pressValue = 0;
            touch.grabPress = false;
            touch.dragMode = false;
        }
        onReleased: {
            ruler.unlockPageScroll();
            touch.preventStealing = false;
            touch.pressValue = 0;
            touch.grabPress = false;
            touch.dragMode = false;
        }
    }
}
