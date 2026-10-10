import QtQuick
import QtTest
import TrainingAppClient 1.0

// Подписи и результат сохранения на экране редактора программы.
//
// Раньше экран и в режиме правки называл себя «Новая программа» и предлагал
// «Создать программу»: пользователь читал это как «будет создана вторая». Второе
// расхождение — обратная связь: после успешного сохранения экран закрывался
// молча, и по нему нельзя было понять, сохранилось ли что-то. Теперь наружу
// уходит идентификатор сохранённой программы и режим: оболочка по ним решает,
// открыть обзор созданной программы или вернуться назад.
TestCase {
    id: testCase

    property var page: null

    function cleanup() {
        page = null;
    }
    // Элементы находятся по objectName: id компонентов снаружи не видны, а
    // кнопка лежит в закреплённой панели каркаса, а не в контенте страницы.
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
        // Заглушка живёт весь файл, поэтому её состояние приводится к исходному:
        // иначе «сохранение прошло» проверялось бы по прошлой проверке.
        programEditStub.createCalls = 0;
        programEditStub.createResult = true;
        programEditStub.programId = "";
        programEditStub.startDate = "";
        programEditStub.title = "";
        programEditStub.previousProgramPaused = false;
        createdSpy.clear();

        page = createTemporaryObject(programEditPageComponent, testCase);

        verify(page !== null);

        wait(0);
    }
    // Создание и правка — разные действия, и подписи обязаны их различать.
    function test_creationModeShowsCreateLabels() {
        const button = testCase.findByName(page, "programEditPrimaryButton");

        verify(button !== null, "в редакторе нет кнопки сохранения");

        compare(page.editing, false);
        compare(page.title, "Новая программа");
        compare(button.text, "Создать программу");
    }
    function test_editingModeShowsSaveLabels() {
        // Состояние «страница открыта с идентификатором» задаётся свойствами, а
        // не reset(): он читает AppContext, которого в qmltestrunner нет.
        programEditStub.programId = "p_edit";
        programEditStub.title = "Сила 12 недель";
        page.programId = "p_edit";

        const button = testCase.findByName(page, "programEditPrimaryButton");

        verify(button !== null, "в редакторе нет кнопки сохранения");

        compare(page.editing, true);
        // Заголовок правки — название программы, а не служебная подпись (§5.3).
        compare(page.title, "Сила 12 недель");
        compare(button.text, "Сохранить изменения");
    }
    // Программа не загрузилась (нет названия) — шапка показывает режим, но не
    // остаётся пустой.
    function test_editingWithoutTitleFallsBackToModeLabel() {
        programEditStub.programId = "p_edit";
        page.programId = "p_edit";

        compare(page.editing, true);
        compare(page.title, "Редактирование программы");
    }
    // Подсказка под кнопкой называет то же действие, что и сама кнопка: иначе
    // «Чтобы создать программу» стояло бы под «Сохранить изменения».
    function test_missingHintFollowsMode() {
        compare(page.missingHint().indexOf("Чтобы создать программу"), 0);

        page.programId = "p_edit";

        compare(page.missingHint().indexOf("Чтобы сохранить программу"), 0);
    }
    // Успешное сохранение сообщает наружу идентификатор и режим: без
    // идентификатора оболочка не знает, что открывать, без режима — куда
    // возвращаться. Третий аргумент — «прежняя активная ушла в паузу» (§2.3).
    function test_saveReportsCreatedProgramId() {
        page.create();

        compare(programEditStub.createCalls, 1);
        compare(createdSpy.count, 1);
        compare(createdSpy.signalArguments[0][0], "p_made");
        compare(createdSpy.signalArguments[0][1], false);
        compare(createdSpy.signalArguments[0][2], false);
    }
    // Включение новой программы ставит прежнюю активную на паузу (§2.3): экран
    // обязан отдать этот признак наружу, иначе о смене активной программы
    // пользователь не узнает.
    function test_saveReportsPausedPreviousProgram() {
        programEditStub.previousProgramPaused = true;

        page.create();

        compare(createdSpy.count, 1);
        compare(createdSpy.signalArguments[0][2], true);
    }
    function test_editingSaveReportsSameProgram() {
        // Правка: идентификатор уже есть — ViewModel его сохраняет.
        programEditStub.programId = "p_edit";
        page.programId = "p_edit";
        page.create();

        compare(createdSpy.count, 1);
        compare(createdSpy.signalArguments[0][0], "p_edit");
        compare(createdSpy.signalArguments[0][1], true);
    }
    // Отказ сохранения не должен выглядеть как успех: экран остаётся на месте,
    // а сообщения о сохранении нет — иначе пользователь ушёл бы с экрана, не
    // сохранив программу.
    function test_failedSaveStaysOnScreen() {
        programEditStub.createResult = false;

        page.create();

        compare(programEditStub.createCalls, 1);
        compare(createdSpy.count, 0);
    }

    height: 812
    name: "ProgramEditCta"
    width: 390

    SignalSpy {
        id: createdSpy

        signalName: "created"
        target: testCase.page
    }
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
    // `create` — через него проверяется, что наружу уходит идентификатор
    // сохранённой программы, а не представление экрана о нём.
    QtObject {
        id: programEditStub

        property bool canCreate: false
        property int createCalls: 0
        property bool createResult: true
        property string errorDetail: ""
        property string errorKey: ""
        property bool hasDraft: false
        property bool previousProgramPaused: false
        property string programId: ""
        property string startDate: ""
        property string title: ""
        property int trainingDays: 0
        property int weeks: 0
        property var goal_targets: ListModel {}
        property var program_days: ListModel {}
        property var rules: ListModel {}

        signal changed

        function create(activate) {
            createCalls += 1;

            if (createResult === false)
                return false;

            // Как и настоящая ViewModel: правка сохраняет прежний
            // идентификатор, создание получает новый.
            if (programId.length === 0)
                programId = "p_made";

            return true;
        }

        function load(userId, programCode) {
            programId = programCode;

            return true;
        }

        function reset(userId) {
            programId = "";

            return true;
        }

        function setStartDate(value) {
            startDate = value;
        }

        function setTitle(value) {
            title = value;
        }

        function setWeeks(value) {
            weeks = value;
        }
    }
}
