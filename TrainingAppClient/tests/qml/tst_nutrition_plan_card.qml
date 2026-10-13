import QtQuick
import QtTest
import TrainingAppClient 1.0

// Карточка КБЖУ на главной (`qml/Components/NutritionPlanCard.qml`).
//
// Проверяется главное требование: после расчёта на главной видны числа, а не
// приглашение рассчитать, и пересчёт остаётся доступным. Расчёт — действие
// человека (§9.2 доклада), поэтому кнопка не исчезает вместе с результатом.
TestCase {
    id: testCase

    // Карточка под сигналом: привязка SignalSpy идёт к свойству, а не к
    // конкретному экземпляру из Component (см. tst_date_field.qml).
    property var card: null

    function cleanup() {
        card = null;
    }
    // Элементы карточки ищутся по objectName: id компонента снаружи не виден.
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
    // Карточка в показанном окне: `visible` зависит от видимости предков, и в
    // дереве без окна проверка видимости блока ничего не проверяла бы.
    function cardInWindow() {
        const window = createTemporaryObject(cardInWindowComponent, testCase);

        verify(window !== null);

        window.show();
        waitForRendering(window.card);

        card = window.card;

        return card;
    }
    // До расчёта карточка приглашает: чисел нет, потому что их неоткуда взять.
    function test_withoutPlanInvitesToCalculate() {
        const card = testCase.cardInWindow();

        verify(card !== null);

        compare(card.hasPlan, false);
        // Элементы скрытого блока остаются в дереве, поэтому проверяется его
        // видимость, а не отсутствие: пустых нулей человек не видит.
        compare(testCase.findByName(card, "nutritionNumbers").visible, false);
        compare(testCase.findByName(card, "nutritionRecalculateButton").text, "Рассчитать");
    }
    // После расчёта карточка показывает числа и даёт пересчитать: числа зависят
    // от веса, и вернуться к расчёту нужно из главной.
    function test_withPlanShowsNumbers() {
        const card = testCase.cardInWindow();

        verify(card !== null);

        card.caloriesKcal = 1843;
        card.proteinG = 132;
        card.fatG = 51;
        card.carbsG = 180;
        card.hasPlan = true;

        compare(testCase.findByName(card, "nutritionNumbers").visible, true);
        compare(testCase.findByName(card, "nutritionCalories").text, "1843 ккал");
        compare(testCase.findByName(card, "nutritionProtein").text, "132 г");
        compare(testCase.findByName(card, "nutritionFat").text, "51 г");
        compare(testCase.findByName(card, "nutritionCarbs").text, "180 г");
        compare(testCase.findByName(card, "nutritionRecalculateButton").text, "Пересчитать");
    }
    // Дата расчёта показывается подписью: без неё старые числа выглядели бы
    // свежими, хотя вес с тех пор изменился.
    function test_captionShowsCalculationDate() {
        const card = testCase.cardInWindow();

        verify(card !== null);

        card.calculatedAt = "2026-10-09";
        card.hasPlan = true;

        verify(testCase.findByName(card, "nutritionCaption").text.indexOf("9") >= 0);
    }
    // Нажатие кнопки уходит наружу сигналом: карточка не знает про навигацию.
    function test_recalculateEmitsSignal() {
        const card = testCase.cardInWindow();

        verify(card !== null);

        const button = testCase.findByName(card, "nutritionRecalculateButton");

        verify(button !== null, "в карточке нет кнопки пересчёта");

        mouseClick(button);

        compare(requestedSpy.count, 1);
    }

    height: 812
    name: "NutritionPlanCard"
    width: 390

    SignalSpy {
        id: requestedSpy

        signalName: "recalculateRequested"
        target: testCase.card
    }

    Component {
        id: cardInWindowComponent

        Window {
            property alias card: cardInWindow

            height: 812
            visible: false
            width: 390

            NutritionPlanCard {
                id: cardInWindow

                width: 358
                x: 16
                y: 16
            }
        }
    }
}
