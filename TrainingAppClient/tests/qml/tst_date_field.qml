import QtQuick
import QtTest
import TrainingAppClient 1.0

// Календарь выбора даты (`qml/Components/DateField.qml`).
//
// Год и месяц листаются отдельно: календарь открывается на текущей дате, а
// программу планируют на год вперёд, поэтому шаг только по месяцу заставлял
// щёлкать по двенадцать раз за каждый год. Двойные стрелки дают шаг по году с
// сохранением месяца — в том числе 29 февраля, иначе календарь показывал бы март.
TestCase {
    id: testCase

    property var field: null

    function cleanup() {
        field = null;
    }
    // Элементы календаря находятся по objectName: id компонентов снаружи не видны.
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
    // Ячейка дня ищется по подписи: у дней нет своих objectName, а номер дня —
    // ровно то, что видит пользователь.
    function findByText(parent, text) {
        if (parent === null || parent === undefined)
            return null;

        const kids = parent.children;

        if (kids === undefined)
            return null;

        for (let i = 0; i < kids.length; ++i) {
            if (kids[i].text !== undefined && String(kids[i].text) === text)
                return kids[i];

            const nested = testCase.findByText(kids[i], text);

            if (nested !== null)
                return nested;
        }

        return null;
    }
    function init() {
        field = createTemporaryObject(dateFieldComponent, testCase);

        verify(field !== null);

        pickedSpy.clear();
    }
    // Поле показывает месяц выбранной даты: это точка отсчёта для обеих стрелок.
    function test_calendarOpensOnSelectedDate() {
        field.value = "2026-11-02";

        compare(field.shownYear, 2026);
        compare(field.shownMonth, 10);
    }
    // Шаг по году сохраняет месяц: «+ год» из ноября даёт ноябрь следующего года,
    // а не тот же день через двенадцать месяцев.
    function test_yearStepKeepsMonth() {
        field.value = "2026-11-02";

        field.shiftYear(1);

        compare(field.shownYear, 2027);
        compare(field.shownMonth, 10);

        field.shiftYear(-2);

        compare(field.shownYear, 2025);
        compare(field.shownMonth, 10);
    }
    // Шаг по месяцу переходит через границу года.
    function test_monthStepRollsYear() {
        field.value = "2026-12-05";

        field.shiftMonth(1);

        compare(field.shownYear, 2027);
        compare(field.shownMonth, 0);

        field.shiftMonth(-1);

        compare(field.shownYear, 2026);
        compare(field.shownMonth, 11);
    }
    // 29 февраля: в невисокосном году такого дня нет, и календарь обязан
    // остаться в феврале, а не уехать в март.
    function test_leapDayKeepsMonth() {
        field.value = "2028-02-29";

        compare(field.shownMonth, 1);

        field.shiftYear(1);

        compare(field.shownYear, 2029);
        compare(field.shownMonth, 1);
    }
    // Кнопка года переводит год целиком: проверяется сама кнопка, а не только
    // функция — стрелка без обработчика выглядела бы рабочей.
    function test_yearButtonStepsYear() {
        const window = createTemporaryObject(dateFieldInWindowComponent, testCase);

        verify(window !== null);

        window.show();

        field = window.field;

        verify(field !== null);

        field.value = "2026-11-02";
        field.openPicker();

        waitForRendering(field);
        // Содержимое календаря — Popup: его элементы живут в оверлее окна, а не
        // в children поля, поэтому искать нужно по дереву окна.
        const nextYear = testCase.findByName(window.contentItem, "dateNextYearButton");

        verify(nextYear !== null, "в календаре нет кнопки следующего года");

        mouseClick(nextYear);

        compare(field.shownYear, 2027);
        compare(field.shownMonth, 10);
    }
    // Выбор дня отдаёт дату в ISO и закрывает календарь: раскрытое поле
    // перекрывало бы следующую карточку формы.
    function test_pickingDayEmitsIsoDate() {
        const window = createTemporaryObject(dateFieldInWindowComponent, testCase);

        verify(window !== null);

        window.show();

        field = window.field;

        verify(field !== null);

        field.value = "2026-11-02";
        field.openPicker();

        waitForRendering(field);

        const dayCell = testCase.findByText(window.contentItem, "15");

        verify(dayCell !== null, "в календаре нет дня 15");

        mouseClick(dayCell);

        compare(pickedSpy.count, 1);
        compare(pickedSpy.signalArguments[0][0], "2026-11-15");
        compare(field.expanded, false);
    }

    height: 812
    name: "DateField"
    width: 390

    SignalSpy {
        id: pickedSpy

        signalName: "edited"
        target: testCase.field
    }
    Component {
        id: dateFieldComponent

        DateField {
            label: "Дата старта"
            width: 292
        }
    }
    // Нажатия проверяются в показанном окне: календарь — Popup, и события
    // доходят до кнопок только когда окно показано.
    Component {
        id: dateFieldInWindowComponent

        Window {
            property alias field: fieldInWindow

            height: 812
            visible: false
            width: 390

            DateField {
                id: fieldInWindow

                label: "Дата старта"
                width: 292
                x: 16
                y: 16
            }
        }
    }
}
