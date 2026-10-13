import QtQuick
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: testCase

    property var createdObject: null

    function cleanup() {
        createdObject = null;
        MockApi.reset();
    }
    function test_appShellCreatesProductionTree() {
        createdObject = createTemporaryObject(appShellComponent, testCase);

        verify(createdObject !== null);
        compare(createdObject.currentTab, 0);
    }
    function test_authPageCreatesWithPresentationContract() {
        createdObject = createTemporaryObject(authPageComponent, testCase);

        verify(createdObject !== null);
        compare(createdObject.authState, "idle");
        verify(createdObject.isValidEmail("user@example.com"));
    }
    function test_formComponentsExposeExpectedApi() {
        createdObject = createTemporaryObject(textFieldComponent, testCase);

        verify(createdObject !== null);
        createdObject.text = "value";
        compare(createdObject.text, "value");
        compare(createdObject.errorText, "");
    }

    function test_checkinPillMapsEveryStateToLabel() {
        createdObject = createTemporaryObject(checkinPillComponent, testCase);

        verify(createdObject !== null);

        // Подпись и цвет берутся из таблицы по состоянию. Проверяются все
        // состояния модели: раньше здесь стояла цепочка if/return, и состояние
        // могло получить подпись без цвета или наоборот.
        createdObject.checkinState = "done";
        compare(createdObject.labelText(), "Выполнено");
        compare(createdObject.colors.foreground, Theme.accent);

        createdObject.checkinState = "failed";
        compare(createdObject.labelText(), "Не выполнено");
        compare(createdObject.colors.foreground, Theme.negative);

        createdObject.checkinState = "partial";
        compare(createdObject.labelText(), "Частично");

        createdObject.checkinState = "skipped";
        compare(createdObject.labelText(), "Пропущено");

        // Неизвестное состояние не должно оставлять пилюлю без подписи.
        createdObject.checkinState = "что-то новое";
        compare(createdObject.labelText(), "Нет отметки");
    }

    function test_todayMeasurementFormKeepsUnsavedInput() {
        createdObject = createTemporaryObject(todayPageComponent, testCase);

        verify(createdObject !== null);

        // Замер, пришедший из базы, попадает в форму.
        createdObject.syncMeasurementFromModel(true);
        verify(createdObject.measurementDirty === false);

        // Пользователь правит вес — форма становится «грязной».
        createdObject.setMeasurementValue("weight", 82.4);
        verify(createdObject.measurementDirty === true);
        compare(createdObject.measurementValue("weight"), 82.4);
        verify(createdObject.measurementFilled.weight === true);

        // Обновление модели (его эмитит и отметка правила) не имеет права
        // стирать несохранённый ввод: без проверки набор полей исчезал бы на
        // глазах у человека, который отметил сон и потерял введённый вес.
        createdObject.syncMeasurementFromModel();
        compare(createdObject.measurementValue("weight"), 82.4);

        // Принудительная синхронизация (собственное сохранение) — приводит
        // форму к базе и снимает «грязный» флаг.
        createdObject.measurementDirty = false;
        createdObject.syncMeasurementFromModel(true);
        verify(createdObject.measurementDirty === false);

        // Значение 0 — допустимое измерение, и поле считается заполненным:
        // без этого запись молча ушла бы в «не измерял».
        createdObject.setMeasurementValue("arm", 0);
        verify(createdObject.measurementFilled.arm === true);
        compare(createdObject.measurementValue("arm"), 0);
    }

    name: "QmlObjectSmoke"

    Component {
        id: appShellComponent

        AppShell {
            height: 812
            width: 390
        }
    }
    Component {
        id: authPageComponent

        AuthPage {
            height: 812
            width: 390
        }
    }
    Component {
        id: checkinPillComponent

        CheckinStatePill {
            width: 120
        }
    }
    Component {
        id: textFieldComponent

        AppTextField {
            width: 320
        }
    }
    Component {
        id: todayPageComponent

        // Заглушка модели вместо `null`: страница читает свойства ViewModel в
        // привязках, и null заваливал их десятком TypeError. Модель-заглушка
        // нужна, чтобы проверялась логика формы — сохранение несохранённого
        // ввода и флаг «поле задано», — а не устойчивость к отсутствию данных.
        TodayPage {
            height: 812
            viewModel: todayStub
            width: 390
        }
    }

    // Минимальная модель: только те свойства, к которым страница обращается
    // в привязках. Списки — с методами rowCount()/get(), как у AList_Model.
    QtObject {
        id: todayStub

        property bool hasMeasurement: false
        property bool hasProgram: true
        property bool isRestDay: false
        property bool sessionCompleted: false
        property int currentWeek: 3
        property int estimatedMinutes: 55
        property int nextTrainingWeekday: 0
        property int weekday: 1
        property int weeksTotal: 8
        property string dayKindName: "workout"
        property string goal: "strength"
        property string programTitle: "Сила 8 недель"
        property string today: "2026-10-05"
        // Не заполнены: проверяется пустая форма ввода.
        property double weightKg: -1

        property var exercises: ListModel {}
        property var rules: ListModel {}

        function get(row) {
            return {};
        }

        function reload(userId) {
        }

        function saveCheckin(ruleId, state, value, hasValue) {
            return true;
        }

        function saveMeasurement(weight, hasWeight, chest, hasChest, waist, hasWaist, hip, hasHip, arm, hasArm, thigh, hasThigh) {
            return true;
        }

        signal changed
    }
}
