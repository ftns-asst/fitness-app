import QtQuick
import QtTest
import TrainingAppClient 1.0

// Экран выбора правил — по макету `docs/mockups-programs/08-program-rules.html`.
//
// Проверяется то, что видно и то, что уходит в модель: витрина каталога плитками
// с категорией, два шага экрана (подбор и форма своего правила), условия цели,
// допустимые метрикой, строка превью и счётчик выбранного. Счётчик — не
// украшение: он держится привязкой к `count` модели, и вызов метода вместо
// свойства оставил бы его нулём навсегда.
TestCase {
    id: testCase

    property var addCalls: []
    property bool addSucceeds: true
    property var adviceCalls: []
    property var page: null

    function cleanup() {
        page = null;
    }
    // Элементы ищутся по objectName: id компонента снаружи не видны, а страница
    // лежит внутри каркаса со вложенными колонками.
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
        pickerStub.rules.clear();
        testCase.addCalls = [];
        testCase.addSucceeds = true;
        testCase.adviceCalls = [];

        page = createTemporaryObject(pickerPageComponent, testCase);

        verify(page !== null);

        // Страница переносится в окно теста: его корень — сам TestCase, а его
        // дети невидимы, и `visible` формы было бы не отличить от
        // унаследованного false (тот же приём, что в tst_weight_ruler_snap).
        page.parent = testCase.Window.window.contentItem;

        wait(0);
    }
    // Плитки внутри сетки: делегаты лежат среди детей вместе с самим Repeater,
    // поэтому отбор идёт по признаку компонента, а не по позиции в списке.
    function tilesIn(host, marker) {
        const found = [];
        const kids = host.children === undefined ? [] : host.children;

        for (let i = 0; i < kids.length; ++i) {
            if (kids[i][marker] !== undefined)
                found.push(kids[i]);
        }

        return found;
    }
    // Витрина каталога: имя правила и категория, к которой оно относится.
    // Полноширинные карточки с кнопкой «Добавить» занимали шесть строк экрана —
    // макет (`.preset-grid`) держит каталог в двух колонках.
    function test_catalogRendersTilesWithCategory() {
        const grid = testCase.findByName(page, "rulePresetGrid");

        verify(grid !== null, "в каталоге нет витрины плиток");

        const tiles = testCase.tilesIn(grid, "canAdd");

        compare(tiles.length, 2);
        compare(tiles[0].title, "Белок 140 г");
        compare(tiles[0].categoryTitle, "Питание");
        verify(tiles[0].isAdded === false, "недобавленное правило помечено добавленным");

        // Добавленное правило остаётся в витрине приглушённым: исчезнув, оно
        // читалось бы как потерянное.
        compare(tiles[1].title, "Сон 8 ч");
        compare(tiles[1].categoryTitle, "Восстановление");
        verify(tiles[1].isAdded === true);
        verify(tiles[1].canAdd === false, "добавленный пресет предлагается добавить снова");
    }
    // Форма своего правила — отдельный шаг экрана: подбор и форма вместе не
    // помещаются, и в макете это разные кадры.
    function test_formIsSeparateStep() {
        const entry = testCase.findByName(page, "customRuleEntry");
        const form = testCase.findByName(page, "customRuleForm");

        verify(entry !== null, "нет входа «Своё правило»");
        verify(form !== null, "нет формы своего правила");

        compare(entry.visible, true);
        compare(form.visible, false);
        verify(page.caption.indexOf("действуют весь срок") > 0, "подпись шага подбора: " + page.caption);

        page.openCustomForm();

        compare(form.visible, true);
        compare(page.title, "Своё правило");
        verify(page.caption.indexOf("добавится в программу") > 0, "подпись шага формы: " + page.caption);

        page.closeCustomForm();

        compare(form.visible, false);
        compare(page.title, "Правила программы");
    }
    // Метрика выбирается плитками с единицей измерения: в выпадающем списке
    // единицу приходилось держать в голове, а от неё зависит и порог, и подпись.
    function test_metricTilesCarryUnits() {
        const grid = testCase.findByName(page, "ruleMetricGrid");

        verify(grid !== null, "нет плиток метрик");

        const tiles = testCase.tilesIn(grid, "selected");

        compare(tiles.length, 3);
        compare(tiles[0].title, "Белок");
        compare(tiles[0].unit, "г");
        compare(tiles[1].title, "Сон");
        compare(tiles[1].unit, "ч");
        verify(tiles[0].selected === true, "выбранная метрика не подсвечена");
        verify(tiles[1].selected === false);
    }
    // Условия цели выводятся из метрики: «ровно 7 часов сна» осмысленно, «ровно
    // 8 RPE» — нет.
    function test_conditionsFollowAllowedTargets() {
        page.setMetric(0);

        compare(page.conditionTitles.length, 2);
        compare(page.conditionTitles[0], "выше");
        compare(page.conditionTitles[1], "ниже");

        page.setMetric(1);

        compare(page.conditionTitles.length, 3);
        compare(page.conditionTitles[2], "ровно");
    }
    // Смена метрики приводит условие к её правилам: иначе форма осталась бы с
    // «ровно», которого модель для этой метрики не принимает.
    function test_metricChangeResetsUnsupportedCondition() {
        page.setMetric(1);
        page.customCondition = "exact";

        compare(page.conditionIndex, 2);

        page.setMetric(0);

        compare(page.customCondition, "min");
        compare(page.conditionIndex, 0);
    }
    // Ползунок значения берёт диапазон и разрядность у метрики: у белка граммы с
    // шагом 5, у сна часы с шагом 0,5.
    function test_valueRulerFollowsMetricRange() {
        const ruler = testCase.findByName(page, "ruleValueRuler");

        verify(ruler !== null, "нет ползунка значения");

        compare(ruler.to, 500);
        compare(ruler.step, 5);
        compare(ruler.valueDecimals, 0);

        page.setMetric(1);

        compare(ruler.to, 14);
        compare(ruler.step, 0.5);
        compare(ruler.valueDecimals, 1);
    }
    // В модель уходит ровно один порог: правило и читается одним условием,
    // «не выше 8» — это максимум, а не пара границ.
    function test_addRuleSendsSingleCondition() {
        page.setMetric(0);
        page.customTitle = "Белок для силы";
        page.customValue = 140;
        page.customCondition = "min";

        page.addCustomRule();

        compare(testCase.addCalls.length, 1);
        compare(testCase.addCalls[0].title, "Белок для силы");
        compare(testCase.addCalls[0].metric, 0);
        compare(testCase.addCalls[0].unit, 0);
        compare(testCase.addCalls[0].min, 140);
        compare(testCase.addCalls[0].hasMin, true);
        compare(testCase.addCalls[0].hasMax, false);
        compare(testCase.addCalls[0].hasExact, false);

        page.customCondition = "max";
        page.customValue = 8;

        page.addCustomRule();

        compare(testCase.addCalls[1].max, 8);
        compare(testCase.addCalls[1].hasMax, true);
        compare(testCase.addCalls[1].hasMin, false);

        page.setMetric(1);
        page.customCondition = "exact";
        page.customValue = 7;

        page.addCustomRule();

        compare(testCase.addCalls[2].exact, 7);
        compare(testCase.addCalls[2].hasExact, true);
        compare(testCase.addCalls[2].hasMin, false);
        compare(testCase.addCalls[2].hasMax, false);
    }
    // Отказ модели форму не закрывает: введённое не должно пропасть, а текст
    // ошибки человек читает рядом с полем.
    function test_formClosesOnlyAfterSuccess() {
        page.openCustomForm();
        page.setMetric(1);
        page.customTitle = "Сон не меньше 7";
        page.customValue = 7;
        page.customCondition = "min";

        testCase.addSucceeds = false;
        page.addCustomRule();

        compare(page.customFormOpen, true);

        testCase.addSucceeds = true;
        page.addCustomRule();

        compare(page.customFormOpen, false);
    }
    // Превью — та строка, по которой правило будут узнавать в списке и на
    // «Сегодня»: она собирается из тех же данных, что уйдут в модель, и число в
    // ней записано так же, как в поле («7,0 ч» при шаге 0,5 — как в линейке).
    function test_previewTextFollowsFormState() {
        page.setMetric(1);
        page.customValue = 7;
        page.customCondition = "min";

        compare(page.previewText(), "Сон выше 7,0 ч");
        compare(page.periodicityText(), "каждый день");

        page.setMetric(2);
        page.customCondition = "max";

        compare(page.periodicityText(), "на каждой тренировке");
    }
    // Счётчик выбранного держится привязкой к `count` модели: вызов метода внутри
    // привязки зависимости не создаёт, и число застыло бы на нуле.
    function test_chosenCountFollowsRuleModel() {
        compare(page.chosenCount, 0);

        pickerStub.rules.append({
            category: "nutrition",
            title: "Белок 140 г"
        });

        compare(page.chosenCount, 1);
        compare(page.footerText(), "Готово · 1 правил");
    }

    // «Другое» — отдельный вид правила: у рекомендации нет ни метрики, ни цели,
    // только текст. Раньше «Другое» существовало лишь фильтром категорий, и
    // создать такое правило в форме было нечем.
    function test_adviceModeSendsTextRule() {
        page.openCustomForm();
        page.setAdviceMode(true);
        page.customTitle = "Алкоголь — стоп";
        page.adviceText = "Не пить за 2 дня до соревнования";

        page.addCustomRule();

        compare(testCase.adviceCalls.length, 1, "рекомендация не ушла в addAdvice");
        compare(testCase.adviceCalls[0].title, "Алкоголь — стоп");
        compare(testCase.adviceCalls[0].text, "Не пить за 2 дня до соревнования");
        // Текст не превращается в измеримое правило с нулевым порогом: такие
        // домен отвергает, и отказ выглядел бы «кнопка ничего не делает».
        compare(testCase.addCalls.length, 0, "текст ушёл в измеримый вход");
        // Форма закрывается после успеха и возвращается к измеримому виду.
        compare(page.customFormOpen, false);
        compare(page.adviceMode, false);
        compare(page.adviceText, "");
    }
    // Отказ модели форму не закрывает: введённое не должно пропасть, а текст
    // ошибки человек читает рядом с полем (та же логика, что у измеримого вида).
    function test_adviceModeKeepsFormAfterFailure() {
        page.openCustomForm();
        page.setAdviceMode(true);
        page.customTitle = "Разминка";
        page.adviceText = "   ";

        testCase.addSucceeds = false;
        page.addCustomRule();

        // Пустой текст уходит в домен: проверка непустоты живёт там
        // (`Has_Advice_Text`), и своя копия в QML разошлась бы с CHECK схемы.
        compare(testCase.adviceCalls.length, 1);
        compare(page.customFormOpen, true);
        compare(page.adviceMode, true);
    }
    // Открытая заново форма начинается с измеримого вида: «Отмена» — это отказ от
    // ввода, и остатки «Другого» в полях читались бы как введённые данные.
    function test_adviceModeResetsOnOpen() {
        page.openCustomForm();
        page.setAdviceMode(true);
        page.adviceText = "Что-то";
        page.closeCustomForm();

        page.openCustomForm();

        compare(page.adviceMode, false);
        compare(page.adviceText, "");
        // Превью вернулось к измеримому виду — оно снова про метрику, а не про
        // совет, которого в форме уже нет.
        verify(page.previewText().indexOf("Белок") === 0, "превью осталось от «Другого»: " + page.previewText());
    }
    // Превью «Другого» показывает то, чем станет правило: название и текст совета.
    function test_advicePreviewShowsTitleAndText() {
        page.setAdviceMode(true);

        compare(page.previewText(), "Например: не пить алкоголь за 2 дня до соревнования");

        page.customTitle = "Алкоголь";

        compare(page.previewText(), "Алкоголь");

        page.adviceText = "Стоп за 2 дня";

        compare(page.previewText(), "Алкоголь: Стоп за 2 дня");
    }

    name: "ProgramRulesPicker"

    Component {
        id: pickerPageComponent

        ProgramRulesPickerPage {
            height: 812
            // Заглушка модели вместо `ProgramEditVM`: в qmltestrunner контекст
            // приложения не задан, и обращение к нему дало бы ReferenceError.
            viewModel: pickerStub
            width: 390
        }
    }

    // Минимальная модель: каталог правил, каталог метрик и список выбранного —
    // те же роли, что отдаёт `Program_Rows`. `addCustomRule` записывает вызов:
    // через него проверяется, что форма отправляет ровно один порог.
    QtObject {
        id: pickerStub

        property bool addSucceeds: true
        property bool hasDraft: true
        property string errorKey: ""
        property string title: ""
        property var catalog: ListModel {
            ListElement {
                category: "nutrition"
                code: "protein"
                isAdded: false
                title: "Белок 140 г"
            }
            ListElement {
                category: "recovery"
                code: "sleep"
                isAdded: true
                title: "Сон 8 ч"
            }
        }
        property var metrics: ListModel {
            ListElement {
                allowsExact: false
                allowsMax: true
                allowsMin: true
                metric: 0
                periodicity: 0
                title: "Белок"
                unit: "g"
            }
            ListElement {
                allowsExact: true
                allowsMax: true
                allowsMin: true
                metric: 3
                periodicity: 0
                title: "Сон"
                unit: "hours"
            }
            ListElement {
                allowsExact: false
                allowsMax: true
                allowsMin: true
                metric: 6
                periodicity: 2
                title: "Нагрузка (RPE)"
                unit: "rpe"
            }
        }
        property var rules: ListModel {}

        // «Другое» уходит другим входом: у рекомендации нет ни метрики, ни цели.
        // Вызов записывается, чтобы проверить, что текст не подменяется измеримым
        // правилом с нулевым порогом.
        function addAdvice(title, text) {
            testCase.adviceCalls.push({
                text: text,
                title: title
            });

            return testCase.addSucceeds;
        }

        function addCatalogPreset(code) {
            return true;
        }

        function addCustomRule(title, metric, unit, min, hasMin, max, hasMax, exact, hasExact) {
            testCase.addCalls.push({
                exact: exact,
                hasExact: hasExact,
                hasMax: hasMax,
                hasMin: hasMin,
                max: max,
                metric: metric,
                min: min,
                title: title,
                unit: unit
            });

            return testCase.addSucceeds;
        }

        function removeRule(index) {
            return true;
        }

        function setCatalogFilter(category) {
            return true;
        }
    }
}
