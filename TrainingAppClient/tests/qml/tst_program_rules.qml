import QtQuick
import QtTest
import TrainingAppClient 1.0

// Экран «Правила и отметки» (ProgramRulesPage, §3.6–3.7).
//
// Кнопка «Создать правила из расчёта» была спрятана за hasMacroResult: человек
// без сохранённого расчёта КБЖУ не находил функцию вовсе. Здесь проверяется, что
// кнопка видима при активной программе независимо от расчёта, исчезает без
// программы и что нажатие без расчёта всё равно доходит до модели (отказ с
// ошибкой — на стороне ViewModel).
TestCase {
    id: testCase

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
        stub.hasProgram = true;
        stub.hasMacroResult = false;
        stub.generateCalls = [];

        page = createTemporaryObject(rulesPageComponent, testCase);

        verify(page !== null);

        // Страница переносится в окно теста: его корень — сам TestCase, а его
        // дети невидимы, и `visible` кнопки было бы не отличить от
        // унаследованного false (тот же приём, что в tst_weight_ruler_snap).
        page.parent = testCase.Window.window.contentItem;

        wait(0);
    }

    // Кнопка видна при активной программе, даже когда расчёта КБЖУ ещё нет:
    // спрятанная за hasMacroResult она не находилась бы вовсе.
    function test_buttonVisibleWithProgramWithoutMacroResult() {
        const button = testCase.findByName(page, "macroRulesButton");

        verify(button !== null, "на экране правил нет кнопки «Создать правила из расчёта»");
        verify(button.visible === true, "кнопка правил скрыта при наличии программы без расчёта");
    }

    // Без активной программы кнопка не обещает работу, которую нечем наполнить.
    function test_buttonHiddenWithoutProgram() {
        stub.hasProgram = false;

        const button = testCase.findByName(page, "macroRulesButton");

        verify(button !== null, "на экране правил нет кнопки «Создать правила из расчёта»");
        verify(button.visible === false, "кнопка правил видна без активной программы");
    }

    // Нажатие без расчёта всё равно доходит до ViewModel: отказ с понятной
    // ошибкой происходит там, а не молчаливой невидимостью кнопки.
    function test_clickWithoutMacroResultStillInvokesModel() {
        const button = testCase.findByName(page, "macroRulesButton");

        verify(button !== null);
        verify(button.visible === true);

        button.clicked();

        compare(stub.generateCalls.length, 1, "нажатие не дошло до ViewModel");
    }

    name: "ProgramRules"

    Component {
        id: rulesPageComponent

        ProgramRulesPage {
            height: 812
            // Заглушка модели вместо `ProgramRulesVM`: в qmltestrunner контекст
            // приложения не задан, и обращение к нему дало бы ReferenceError.
            viewModel: stub
            width: 390
        }
    }

    // Минимальная модель экрана: программе достаточно наличия активной программы
    // и пустого списка правил — группы просто не строятся.
    QtObject {
        id: stub

        property bool hasProgram: true
        property bool hasMacroResult: false
        property string errorKey: ""
        property string rangeFrom: ""
        property string rangeTo: ""
        property string today: ""
        property bool hasAdherencePercent: false
        property double adherencePercent: 0
        property int periodDays: 7
        property int checkinCount: 0
        property var rules: ruleStub
        property var generateCalls: []

        function reload(userId) {
            return true;
        }

        function generateRulesFromMacro(userId) {
            stub.generateCalls.push(userId);

            return true;
        }

        function setPeriodDays(days) {
        }

        function saveCheckin(ruleId, state, value, hasValue, note) {
            return true;
        }
    }

    QtObject {
        id: ruleStub

        property int count: 0

        function rowCount() {
            return 0;
        }

        function get(index) {
            return null;
        }
    }
}
