import QtQuick
import QtTest
import TrainingAppClient 1.0

// Срок программы в редакторе — по макету `docs/mockups-programs/03-edit.html` и
// решению «Срок программы — ползунок, а не кнопки 4/8/12/16»
// (`docs/mockups-programs/README.md`).
//
// Проверяется то, ради чего ползунок и вводился: достижима любая длительность
// от 1 до 52 недель, частые сроки остались ярлыками, а срок перестал быть
// абстракцией — под шкалой стоит производная дата окончания. На кнопках
// 5, 6, 7, 9, 10 и 11 недель были недостижимы, и это была не мелочь: человек
// выбирал соседнее значение и правил его вручную.
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
        // Состояние между проверками не переносится: иначе «шкала ещё не
        // двигалась» проверялось бы по прошлой проверке, а не по текущей.
        programEditStub.plannedEndDate = "";
        programEditStub.startDate = "";
        programEditStub.weekCalls = [];
        programEditStub.weeks = 0;
        page = createTemporaryObject(programEditPageComponent, testCase);

        verify(page !== null);

        // Раскладка доводится до конца: ширины и позиции проверяются по факту,
        // а не по свойствам, которые ещё не посчитаны.
        wait(0);
    }
    // Шкала покрывает весь допустимый срок, а не четыре значения: ровно столько,
    // сколько допускает CHECK схемы (`weeks_total <= 52`).
    function test_termRulerCoversWholeAllowedTerm() {
        const ruler = testCase.findByName(page, "termRuler");

        verify(ruler !== null, "в редакторе нет ползунка срока");

        compare(ruler.from, 1);
        compare(ruler.to, 52);
        compare(ruler.step, 1);
        compare(ruler.valueDecimals, 0);
        compare(ruler.unit, "нед.");
        // Подпись шкалы — как в макете: строка «Срок программы» осталась от
        // прежнего ряда кнопок и теперь принадлежит ползунку.
        compare(ruler.title, "Срок программы");
    }
    // Длительность, которой нет среди ярлыков, достижима ползунком и уходит в
    // модель. 7 недель — ровно тот случай, ради которого кнопки и заменены.
    function test_termRulerReachesValueWithoutShortcut() {
        const ruler = testCase.findByName(page, "termRuler");

        verify(ruler !== null, "в редакторе нет ползунка срока");

        ruler.setValue(7);

        compare(programEditStub.weekCalls.length, 1);
        compare(programEditStub.weekCalls[0], 7);
        compare(ruler.value, 7);
    }
    // Ярлык частого срока — ускорение, а не ограничение: он двигает ту же шкалу,
    // и подсвечен тот ярлык, чей срок выбран.
    function test_termShortcutMovesRuler() {
        const shortcut = testCase.findByName(page, "termPreset12");

        verify(shortcut !== null, "нет ярлыка 12 недель");

        shortcut.clicked();

        compare(programEditStub.weekCalls[programEditStub.weekCalls.length - 1], 12);

        // Связь «модель → шкала» проверяется через изменение свойства модели:
        // прямая привязка `value: page.weeks` умирала бы на первом же жесте
        // ползунка, и ярлык больше не двигал бы шкалу.
        programEditStub.weeks = 12;
        wait(0);

        compare(testCase.findByName(page, "termRuler").value, 12);

        const highlighted = testCase.findByName(page, "termPreset12");
        const plain = testCase.findByName(page, "termPreset4");

        verify(plain !== null, "нет ярлыка 4 недель");
        verify(highlighted.variant !== plain.variant, "выбранный срок не отличается от невыбранного");
    }
    // Производная дата окончания под шкалой: без неё срок остаётся абстракцией,
    // которую пересчитывают в уме. Формат — «дд.мм.гггг», как даты в продукте.
    function test_termHintShowsPlannedEndDate() {
        programEditStub.plannedEndDate = "2026-12-20";
        programEditStub.startDate = "2026-09-28";
        programEditStub.weeks = 12;

        const ruler = testCase.findByName(page, "termRuler");

        verify(ruler !== null, "в редакторе нет ползунка срока");
        compare(ruler.hint, "Окончание 20.12.2026");
    }
    // Пока не хватает даты старта, подсказка называет причину, а не молчит:
    // пустая строка под шкалой читается как «здесь ничего и не появится».
    function test_termHintNamesMissingStartDate() {
        programEditStub.startDate = "";
        programEditStub.weeks = 8;

        compare(testCase.findByName(page, "termRuler").hint, "Укажите дату старта — окончание посчитается");
    }
    // Разбор ISO-даты не должен уезжать на день: `new Date("2026-09-28")` читает
    // дату как UTC и в отрицательной зоне даёт 27 сентября — подсказка врала бы
    // на день, и это ровно тот класс ошибок, который глазами не виден.
    function test_termHintFormatsIsoDate() {
        compare(page.formatDate("2026-09-28"), "28.09.2026");
        compare(page.formatDate("2027-01-05"), "05.01.2027");
        // Не дата — показывается как есть, а не «undefined».
        compare(page.formatDate(""), "");
    }

    name: "ProgramEditTerm"

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
    // `setWeeks` — через него проверяется, что срок уходит в модель, а не
    // остаётся состоянием шкалы.
    QtObject {
        id: programEditStub

        property bool canCreate: false
        property string errorDetail: ""
        property string errorKey: ""
        property bool hasDraft: false
        property string plannedEndDate: ""
        property bool previousProgramPaused: false
        property string programId: ""
        property string startDate: ""
        property string title: ""
        property int trainingDays: 0
        property var weekCalls: []
        property int weeks: 0
        property var goal_targets: ListModel {}
        property var program_days: ListModel {}
        property var rules: ListModel {}

        signal changed

        function create(activate) {
            return true;
        }

        function load(userId, programCode) {
            return true;
        }

        function reset(userId) {
            return true;
        }

        function setStartDate(value) {
            startDate = value;
        }

        function setTitle(value) {
            title = value;
        }

        function setWeeks(value) {
            weekCalls.push(value);
            weeks = value;
        }
    }
}
