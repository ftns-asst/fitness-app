import QtQuick
import QtTest
import TrainingAppClient 1.0

// Форма цели редактора программы — по макету `docs/mockups-programs/03-edit.html`.
//
// Проверяются два расхождения, которые были в разметке. Направление цели
// рисовалось двумя равноправными вторичными кнопками: выбранное в них не
// читалось, а нажатие сразу создавало цель, поэтому «Снизить» и «Повысить»
// выглядели одинаково. Шкала «Целевого значения» делила строку с подписью,
// получала половину карточки и уезжала вправо — вместо `.slider` во всю ширину
// с подписью над дорожкой.
TestCase {
    id: testCase

    property var page: null

    function cleanup() {
        page = null;
    }
    // Элементы формы находятся по objectName: id компонента снаружи не видны, а
    // страница лежит внутри каркаса со своими вложенными рядами.
    function findByName(parent, name) {
        if (parent === null || parent === undefined)
            return null;

        const kids = parent.children;

        if (kids === undefined)
            return null;

        for (let i = 0; i < kids.length; ++i) {
            if (kids[i].objectName === name)
                return kids[i];

            const nested = testCase.findByName(kids[i], name);

            if (nested !== null)
                return nested;
        }

        return null;
    }
    function init() {
        // Заглушка модели живёт весь файл, а состояние между проверками не
        // переносится: иначе «модель ещё не трогали» проверялось бы по прошлой
        // проверке, а не по текущей.
        programEditStub.lastTarget = null;
        programEditStub.hasDraft = false;
        programEditStub.goal_targets.clear();
        page = createTemporaryObject(programEditPageComponent, testCase);

        verify(page !== null);

        // Раскладка доводится до конца: ширина линейки проверяется по факту, а не
        // по свойству, которое ещё не посчитано.
        wait(0);
    }
    // Направление — состояние формы, а не аргумент нажатия. Подсвеченный сегмент
    // обязан быть тем же, что уйдёт в модель: иначе выбор «Повысить» создавал бы
    // цель на снижение.
    function test_directionSelectorFollowsSelection() {
        const selector = testCase.findByName(page, "goalDirectionSelector");

        verify(selector !== null, "в форме цели нет сегментированного выбора");

        compare(selector.items.length, 2);
        // Слева «Снизить» и оно же выбрано по умолчанию — как в макете.
        compare(selector.items[0], "↓ Снизить");
        compare(selector.items[1], "↑ Повысить");
        compare(selector.currentIndex, 0);
        compare(page.goalDirection, 1);

        // Выбор подсвечивается: сегмент следует за состоянием страницы.
        page.goalDirection = 0;

        compare(selector.currentIndex, 1);
        compare(page.goalDirections[selector.currentIndex], 0);
    }
    // Нажатие сегмента переводит направление и не создаёт цель само: цель
    // добавляет отдельная кнопка. Проверяется сторона страницы — обработчик
    // сигнала; сама кнопка сегмента общая с экранами входа и «Профиля».
    function test_directionSelectorChangesDirection() {
        const selector = testCase.findByName(page, "goalDirectionSelector");

        verify(selector !== null, "в форме цели нет сегментированного выбора");

        selector.selected(1);

        compare(page.goalDirection, 0);
        compare(selector.currentIndex, 1);
        // Направление — только состояние: до «Добавить» модель не трогали.
        compare(programEditStub.lastTarget, null);

        selector.selected(0);

        compare(page.goalDirection, 1);
        compare(selector.currentIndex, 0);
    }
    // Диапазон шкалы идёт от метрики: у веса тела это ровно 50…130 с шагом 0,5 и
    // единицей «кг» из макета (`input[type=range] min=50 max=130 step=0.5`).
    // Нейтральные 0…300 растягивали шаг до 0,3 px, и подписи сливались в полосу.
    function test_bodyWeightRangeMatchesMockup() {
        const ruler = testCase.findByName(page, "goalRuler");

        verify(ruler !== null, "в форме цели нет линейки");

        compare(page.selectedGoalMetric, 0);
        compare(ruler.from, 50);
        compare(ruler.step, 0.5);
        compare(ruler.to, 130);
        compare(ruler.unit, "кг");
        // Подпись поля стоит над шкалой, а не в строке рядом с ней: в общей
        // строке линейке доставалась половина карточки.
        compare(ruler.title, "Целевое значение");
    }
    // Линейка занимает карточку целиком и начинается от её левого края. Сравнение
    // с родителем ловит обе прежние поломки: ширину от `implicitWidth` (280 px в
    // обычной колонке) и строку, в которой линейка делила место с подписью и
    // уезжала вправо.
    function test_rulerFillsCardFromLeftEdge() {
        const ruler = testCase.findByName(page, "goalRuler");

        verify(ruler !== null, "в форме цели нет линейки");

        verify(Math.abs(ruler.width - ruler.parent.width) < 1.5, "линейка не во всю ширину карточки: " + ruler.width + " из " + ruler.parent.width);
        verify(ruler.width > page.width * 0.6, "линейка сжата: " + ruler.width + " px при ширине страницы " + page.width);
        verify(ruler.x <= 1, "линейка сдвинута вправо: x = " + ruler.x);
    }
    // Смена метрики переносит диапазон, единицу и разрядность и зажимает
    // значение: 75 осмысленны для веса тела и невозможны для процента жира, а
    // ползунок не имеет права показывать значение, до которого нельзя дотянуть.
    function test_metricChangeClampsValueIntoRange() {
        page.goalTargetValue = 75;
        page.setGoalMetric(2);

        const option = page.goalOption;

        compare(option.value, 2);
        compare(option.unit, "%");
        compare(option.to, 60);
        compare(page.goalTargetValue, 60);
    }
    // Каждая метрика списка обязана иметь рабочий диапазон и верхнюю границу на
    // шаге: недостижимая цель — не цель. На шкале 0…300 недостижимым был,
    // например, недельный тоннаж.
    function test_everyMetricHasReachableRange() {
        for (let i = 0; i < page.goalMetricOptions.length; ++i) {
            const option = page.goalMetricOptions[i];

            page.setGoalMetric(option.value);

            // Метрика находится по значению, а не по индексу списка: индекс
            // начнёт врать, как только список переставят или дополнят.
            compare(page.goalOption.value, option.value);
            verify(option.from < option.to, "пустой диапазон у метрики " + option.value);
            verify(option.unit.length > 0, "у метрики " + option.value + " нет единицы");
            verify(option.decimals >= 0 && option.decimals <= 1, "непонятная разрядность у метрики " + option.value);

            const span = option.to - option.from;
            const steps = span / option.step;

            verify(Math.abs(steps - Math.round(steps)) < 0.001, "верхняя граница не кратна шагу у метрики " + option.value);
        }
    }
    // «Добавить» передаёт в модель выбранное направление вместе с метрикой и
    // значением — то же, что делает кнопка формы. Направление по умолчанию —
    // «снизить» (1), как подсвечено в макете.
    function test_addGoalSendsSelectedDirection() {
        page.goalTargetValue = 82;
        page.addGoal(page.goalDirection);

        verify(programEditStub.lastTarget !== null, "цель не дошла до модели");
        compare(programEditStub.lastTarget.direction, 1);
        compare(programEditStub.lastTarget.metric, 0);
        compare(programEditStub.lastTarget.targetValue, 82);

        page.goalDirection = 0;
        page.addGoal(page.goalDirection);

        compare(programEditStub.lastTarget.direction, 0);
    }

    // — Цель в списке: видно, что она добавилась —
    // Строка цели собирается экраном из ролей (`metric`, `direction`,
    // `targetValue`): роли `displayText` у цели нет, и делегат, который её
    // требовал, не рисовал ничего — добавленная цель пропадала из виду.
    function test_goalListShowsAddedGoal() {
        programEditStub.hasDraft = true;
        programEditStub.goal_targets.append({
            "direction": "decrease",
            "metric": "bodyWeightKg",
            "targetValue": 82
        });

        const window = createTemporaryObject(programEditPageInWindowComponent, testCase);

        verify(window !== null);

        window.show();
        waitForRendering(window.page);

        const caption = testCase.findByName(window.page, "goalCardText");
        const count = testCase.findByName(window.page, "goalSectionCount");

        verify(caption !== null, "в списке целей нет подписи цели");
        compare(caption.text, "Вес тела — 82 кг");
        verify(count !== null, "у заголовка целей нет счётчика");
        compare(count.text, "1");
    }
    // Пустой список обязан объясняться подписью из макета, а не выглядеть как
    // секция, которая не загрузилась.
    function test_emptyGoalListShowsHint() {
        programEditStub.hasDraft = true;

        const window = createTemporaryObject(programEditPageInWindowComponent, testCase);

        verify(window !== null);

        window.show();
        waitForRendering(window.page);

        const hint = testCase.findByName(window.page, "goalEmptyHint");

        verify(hint !== null, "нет подписи пустого списка целей");
        compare(hint.visible, true);
        compare(testCase.findByName(window.page, "goalSectionCount").text, "0");
    }
    // Направление цели читается значком: ↓ снижение (акцентом, как в макете),
    // ↑ повышение, = удержание.
    function test_goalDirectionArrowFollowsDirection() {
        compare(page.goalDirectionArrow("decrease"), "↓");
        compare(page.goalDirectionArrow("increase"), "↑");
        compare(page.goalDirectionArrow("maintain"), "=");
        compare(page.goalDirectionHighlighted("decrease"), true);
        compare(page.goalDirectionHighlighted("increase"), false);
    }
    // Подпись берёт имя метрики и разрядность из списка метрик формы:
    // «Процент жира — 22,5 %», а не машинное «bodyFatPercent — 22.5».
    function test_goalCaptionUsesMetricLabels() {
        compare(page.goalCardText("bodyFatPercent", 22.5), "Процент жира — 22,5 %");
        compare(page.goalCardText("workoutsPerWeek", 4), "Тренировок в неделю — 4 раз/нед");
    }

    Component {
        id: programEditPageInWindowComponent

        Window {
            property alias page: pageInWindow

            height: 812
            visible: false
            width: 390

            ProgramEditPage {
                id: pageInWindow

                anchors.fill: parent
                viewModel: programEditStub
            }
        }
    }

    name: "ProgramEditGoal"

    Component {
        id: programEditPageComponent

        ProgramEditPage {
            height: 812
            // Заглушка модели вместо `ProgramEditVM`: в qmltestrunner контекст
            // приложения не задан, и обращение к нему дало бы ReferenceError.
            viewModel: programEditStub
            width: 390
        }
    }

    // Минимальная модель: свойства, которые страница читает в привязках, и
    // `setTarget` — через него проверяется, что форма отправляет выбранное
    // направление, а не своё представление о нём.
    QtObject {
        id: programEditStub

        property bool canCreate: false
        property bool hasDraft: false
        property int trainingDays: 0
        property int weeks: 8
        property string errorKey: ""
        property string startDate: ""
        property string title: ""
        property var goal_targets: ListModel {}
        property var lastTarget: null
        property var program_days: ListModel {}
        property var rules: ListModel {}

        signal changed

        function load(userId, programId) {
            return false;
        }

        function reset(userId) {
            return true;
        }

        function setTarget(metric, startValue, hasStartValue, targetValue, direction) {
            lastTarget = {
                direction: direction,
                metric: metric,
                targetValue: targetValue
            };

            return true;
        }
    }
}
