import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Плитка дня недели — ряд из семи в одну линию (спека §4, §5.1/§5.3). Раньше дни
// были кружками, а в редакторе — крупными карточками сеткой 2/3 колонки: кружок
// тратил место под форму, сетка заставляла читать неделю по диагонали. Теперь это
// прямоугольник со скруглением, у каждого дня своя рамка и радиус, между днями
// зазор — как и на всех остальных экранах модуля.
//
// Плитка одна и та же для обзора и редактора; различаются только заполнение
// (число упражнений) и высота. Имена свойств не совпадают с ролями модели:
// одноимённые затеняли бы роли, и значения из AList_Model не дошли бы сюда.
Item {
    id: tile

    property string tileKind
    property string weekdayName
    property bool isToday
    // Подпись под днём в редакторе. Содержимое вычисляет countFor(): правило
    // «тише у отдыха, «!» у пустого дня» — это про чтение ряда, а не про
    // страницу, значит живёт в компоненте.
    property string countText
    property bool showCount: false
    property bool isEmptyDay: false
    property bool isActive: false
    signal clicked

    readonly property bool isRest: tileKind === "rest"

    // — Геометрия плитки (§3.3) —
    // Числа названы по смыслу, а не разбросаны по разметке и страницам: высоты
    // двух вариантов плитки, отметка «сегодня» и её раскладка — величины
    // одного решения, менять их нужно в одном месте.
    readonly property int heightCompact: 46
    readonly property int heightTall: 64
    readonly property int minWidth: 40
    readonly property int markWidth: 14
    readonly property int markHeight: 3
    readonly property int markInset: 4
    readonly property int contentGap: 4
    readonly property int fadeDuration: 120
    // Дней в неделе и зазоры между ними: из них считается ширина плитки.
    readonly property int daysInWeek: 7
    readonly property int gapsInWeek: daysInWeek - 1
    // Ширина плитки от ширины страницы, а не от ширины ряда: на момент
    // создания делегата ширина ряда ещё не посчитана, и плитки получают ноль.
    function widthFor(availableWidth) {
        const gaps = Spacing.itemGap * gapsInWeek;
        return Math.floor((availableWidth - Spacing.screenPadding * 2 - gaps) / daysInWeek);
    }

    implicitHeight: heightCompact
    implicitWidth: minWidth

    // Цветовое пятно заливкой всей плитки: вид дня читается на расстоянии, а
    // короткая черта внутри карточки — нет. Рамка у залитой плитки убирается:
    // тон сам задаёт форму, серая обводка поверх тона делала плитку вклеенной.
    Rectangle {
        anchors.fill: parent
        // Рамка только там, где заливки нет: у тонированной плитки серая
        // обводка поверх тона делает её вклеенной, тон и сам задаёт форму.
        border.color: tile.colors.fill === Theme.surface ? Theme.border : "transparent"
        border.width: Theme.borderWidth
        color: tile.colors.fill
        radius: Theme.radiusSm

        Behavior on color {
            ColorAnimation {
                duration: tile.fadeDuration
            }
        }
    }
    Column {
        anchors.centerIn: parent
        spacing: tile.contentGap
        width: parent.width

        Label {
            anchors.horizontalCenter: parent.horizontalCenter
            color: tile.colors.label
            font: Typography.captionStrong
            // Плитка узкая (≈40 px на 360 dp), но «Пн» в неё помещается, поэтому
            // подпись не сжимаем и не скрываем: день недели — то, ради чего ряд
            // открывают. Видимость зависит только от данных: слот ротации может
            // быть не привязан к конкретному дню.
            text: tile.weekdayName
            visible: text.length > 0
        }
        Label {
            visible: tile.showCount
            anchors.horizontalCenter: parent.horizontalCenter
            color: tile.colors.count
            font: Typography.bodyStrong
            text: tile.countText
        }
    }
    // «Сегодня» — тонкая черта снизу, а не кольцо вокруг плитки: кольцо
    // конкурировало с заливкой вида дня, а на активной плитке превращалось в
    // бублик. Черта читается как отметка времени и не спорит с заливкой.
    // Без скругления: у Rectangle скругление живёт в border.radius и без рамки
    // не рисуется, а на полоске высотой 3 px скругление и не видно.
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.bottomMargin: tile.markInset
        anchors.horizontalCenter: parent.horizontalCenter
        color: tile.colors.label
        height: tile.markHeight
        visible: tile.isToday
        width: tile.markWidth
    }
    TapHandler {
        onTapped: tile.clicked()
    }

    // Вид дня задаётся таблицей, а не цепочкой if: правила «какой цвет у
    // какого вида дня» должны читаться одним взглядом и дополняться новой
    // строкой, а не новой веткой в четырёх местах сразу.
    //
    // Ключ составной: вид дня + состояние. Состояний два — обычная плитка и
    // активная («сегодня»), — и они важнее вида дня: на активной плитке все
    // подписи светлые, потому что заливка акцентная и тёмная.
    // Подпись под днём в редакторе. Правила читания ряда живут в countMarks:
    // у отдыха вместо цифры тире, иначе ряд из семи цифр выглядит списком
    // одинаковых значений; у дня без упражнений — «!», иначе он обнаруживается
    // только при попытке создать программу.
    readonly property var countMarks: {
        "rest": "—",
        "empty": "!"
    }

    function countFor(kind, exerciseCount) {
        if (kind === "rest")
            return tile.countMarks.rest;
        if (exerciseCount > 0)
            return String(exerciseCount);

        return tile.countMarks.empty;
    }

    // Активная плитка одинакова для всех видов дня: заливка акцентная, все
    // подписи светлые — серый по тёмному акценту не читается. Вид дня поверх
    // активного состояния не работает, поэтому он у всех строк общий.
    readonly property var activePalette: {
        "fill": Theme.accent,
        "label": Theme.accentForeground,
        "count": Theme.accentForeground
    }
    readonly property var palettesByKind: {
        "workout": {
            "plain": {
                "fill": Theme.accentTint,
                "label": Theme.accent,
                "count": Theme.textPrimary
            },
            "active": tile.activePalette
        },
        "cardio": {
            "plain": {
                "fill": Theme.positiveTint,
                "label": Theme.positive,
                "count": Theme.textPrimary
            },
            "active": tile.activePalette
        },
        "mobility": {
            "plain": {
                "fill": Theme.surface,
                "label": Theme.warning,
                "count": Theme.textPrimary
            },
            "active": tile.activePalette
        },
        "rest": {
            "plain": {
                "fill": Theme.surface,
                "label": Theme.textMuted,
                "count": Theme.textMuted
            },
            "active": tile.activePalette
        },
        // День есть, упражнений нет: вид дня ещё не задан, поэтому подпись и
        // заливка предупреждающие, а не акцентные.
        "empty": {
            "plain": {
                "fill": Theme.warningTint,
                "label": Theme.warning,
                "count": Theme.warning
            },
            "active": tile.activePalette
        }
    }
    readonly property bool active: tile.isActive === true || tile.isToday === true
    readonly property string colorsKey: tile.isEmptyDay === true ? "empty" : (tile.tileKind.length > 0 ? tile.tileKind : "rest")
    readonly property var colors: tile.palettesByKind[tile.colorsKey][tile.active ? "active" : "plain"]
}
