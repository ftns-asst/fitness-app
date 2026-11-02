import QtQuick
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: testCase

    property var createdObject: null

    function cleanup() {
        createdObject = null;
        todayStub.selectedMeasurementKeys = [];
        todayStub.savedMeasurementPayload = ({});
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


    // Ввод веса с главной убран: «В фокусе» только читает модель, а изменение
    // живёт в дневнике — два места записи одного факта расходились.
    function test_todayFocusReadsWeightWithoutInput() {
        todayStub.hasMeasurement = true;
        todayStub.weightKg = 82.4;
        createdObject = createTemporaryObject(todayPageComponent, testCase);

        verify(createdObject !== null);
        compare(createdObject.focusWeightText(), "82,4 кг");
        verify(findChild(createdObject, "todayFocusWeight") !== null);
        verify(findChild(createdObject, "todayMeasurementsCard") === null, "формы замеров быть не должно");
        verify(findChild(createdObject, "todayMeasurementsToggle") === null, "переключателя формы быть не должно");

        todayStub.hasMeasurement = false;
        compare(createdObject.focusWeightText(), "—");
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
        property var selectedMeasurementKeys: []
        property var savedMeasurementPayload: ({})

        function get(row) {
            return {};
        }

        function reload(userId) {
        }

        function saveCheckin(ruleId, state, value, hasValue) {
            return true;
        }

        function saveMeasurement(values) {
            savedMeasurementPayload = values;
            return true;
        }

        function setMeasurementInterval(days) {
            return true;
        }

        signal changed
    }
}
