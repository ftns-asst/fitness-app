import QtQuick
import QtTest
import TrainingAppClient 1.0

// Главный экран по макету 13: крупная шапка, переход акцента в поверхность,
// порядок блоков «hero → план → В фокусе → отметки». Проверяется структура, а не
// данные: она не должна разъезжаться при правках модели.
TestCase {
    id: testCase

    when: windowShown

    property var page: null

    function init() {
        page = createTemporaryObject(todayPageComponent, testCase);
        verify(page !== null);
        waitForRendering(page);
    }

    function cleanup() {
        page = null;
    }

    // Крупная шапка и градиент — то, что отличает экран от остальных вкладок.
    function test_headerIsLargeWithHeroBackground() {
        verify(page.largeTitle, "шапка главной должна быть крупной");
        verify(page.heroBackground, "под шапкой нужен переход акцента в поверхность");
        compare(page.title, "Сегодня");
    }

    // Подложка — четыре ступени акцента, последняя совпадает с фоном страницы:
    // иначе на границе градиента видна полоса. Флаг видимости отдельно не
    // проверяется: у Item `visible` — эффективная видимость, а в тесте дерево
    // не показано в окне.
    function test_heroBackgroundIsGradientToBackground() {
        const gradient = findChild(page, "heroBackgroundGradient");

        verify(gradient !== null, "нет градиентной подложки");
        compare(gradient.gradient.stops.length, 4);
        compare(gradient.gradient.stops[3].color.toString(), Theme.background.toString());
        verify(gradient.height > 0, "подложка схлопнулась по высоте");
        verify(gradient.height <= page.height, "подложка не должна выходить за экран");
    }

    // Порядок блоков: hero → план дня → «В фокусе» → отметки. Проверяется по
    // вертикальным координатам — так тест ловит и перестановку, и случайное
    // «спрятал блок».
    function test_blocksFollowMockupOrder() {
        const hero = findChild(page, "todayHero");
        const plan = findChild(page, "todayPlan");
        const focus = findChild(page, "todayFocus");
        const rules = findChild(page, "todayCheckinsHeader");

        verify(hero !== null, "нет hero-блока");
        verify(plan !== null, "нет плана дня");
        verify(focus !== null, "нет блока «В фокусе»");
        verify(rules !== null, "нет заголовка отметок");

        const heroY = hero.mapToItem(page, 0, 0).y;
        const planY = plan.mapToItem(page, 0, 0).y;
        const focusY = focus.mapToItem(page, 0, 0).y;
        const rulesY = rules.mapToItem(page, 0, 0).y;

        verify(heroY < planY, "план дня должен идти после hero");
        verify(planY < focusY, "«В фокусе» должен идти после плана дня");
        verify(focusY < rulesY, "отметки должны идти после «В фокусе»");
    }

    // Главное действие и переход в программу живут в hero, а не в отдельной
    // карточке: второй карточки-обёртки больше нет.
    function test_heroHoldsPrimaryAction() {
        const start = findChild(page, "todayStartButton");
        const link = findChild(page, "todayProgramLink");

        verify(start !== null, "нет кнопки старта тренировки");
        verify(link !== null, "нет перехода в программу");
        compare(start.text, "Начать тренировку");
    }

    // Формы замеров на главной нет: ввод веса — в дневнике.
    function test_measurementFormIsGone() {
        verify(findChild(page, "todayMeasurementsCard") === null, "форма замеров вернулась на главную");
        verify(findChild(page, "todayFocusWeight") !== null, "вес показывается только для чтения");
    }

    Component {
        id: todayPageComponent

        TodayPage {
            height: 812
            viewModel: todayStub
            width: 390
        }
    }

    QtObject {
        id: todayStub

        property bool hasMeasurement: true
        property bool hasProgram: true
        property bool isRestDay: false
        property bool sessionCompleted: false
        property int currentWeek: 3
        property int estimatedMinutes: 55
        property int nextTrainingWeekday: 5
        property int weekday: 3
        property int weeksTotal: 8
        property string dayKindName: "Тренировка"
        property string dayTitle: "День A · Ноги"
        property string goal: "strength"
        property string programTitle: "Сила и объём"
        property string today: "2026-11-02"
        property double weightKg: 82.4

        property var exercises: ListModel {}
        property var rules: ListModel {}
        property var selectedMeasurementKeys: []

        function get() {
            return {};
        }

        function reload() {
        }

        signal changed
    }
}