import QtQuick
import QtQuick.Controls
import QtTest
import TrainingAppClient 1.0

// T3: значения и вердикты приходят из модели; экран только вводит факты.
TestCase {
    id: testCase

    name: "DiaryPage"
    when: windowShown
    property var page: null

    function init() {
        stub.days.clear();
        stub.days.append({
            date: "2026-10-01",
            dayTitle: "Верх",
            weight: 82,
            average7: 82.5,
            workoutDone: true,
            note: "Старая заметка",
            editable: true,
            values: [
                {
                    ruleId: "protein",
                    title: "Белок",
                    value: 200,
                    verdict: "failed",
                    target: "≥ 180",
                    unit: "г"
                }
            ]
        });
        stub.days.append({
            date: "2099-10-02",
            dayTitle: "Отдых",
            weight: 0,
            average7: 0,
            workoutDone: false,
            note: "",
            editable: false,
            values: [
                {
                    ruleId: "protein",
                    title: "Белок",
                    value: 0,
                    verdict: "unknown",
                    target: "≥ 180",
                    unit: "г"
                }
            ]
        });
        stub.calls = [];
        stub.selectedDate = "";
        stub.selectedNote = "";
        stub.errorKey = "";
        stub.saveSucceeds = true;
        const component = Qt.createComponent("../../qml/Pages/Diary/DiaryPage.qml");
        compare(component.status, Component.Ready, component.errorString());
        page = createTemporaryObject(component, testCase.Window.window.contentItem, {
            width: 390,
            height: 812,
            viewModel: stub,
            programId: "p1"
        });
        verify(page !== null);
        wait(0);
    }

    function cleanup() {
        page = null;
    }

    function test_loadsRequestedProgramAndShowsNormsAndSummary() {
        compare(stub.calls[0].join("|"), "reload|u1|p1");
        compare(stub.selectedDate, "2026-10-01");
        // Программа — подпись в шапке каркаса: строка не дублируется в контенте.
        compare(page.caption, "Сила 4 недели");
        verify(findChild(page, "diaryValue_2026-10-01_protein").text.indexOf("≥ 180") >= 0);
        compare(findChild(page, "diarySummary_protein"), null);
        findChild(page, "diarySummaryToggle").clicked();
        wait(0);
        const summary = findChild(page, "diarySummary_protein");
        verify(summary.text.indexOf("175") >= 0);
        verify(summary.text.indexOf("2") >= 0);
        verify(summary.text.indexOf("1") >= 0);
    }

    function test_dateJumpStartsCollapsedAndWeekFitsNarrowScreen() {
        page.width = 360;
        const jump = findChild(page, "diaryDateJump");
        verify(!jump.visible);
        compare(findChild(page, "diaryDateJumpToggle").text, "Октябрь 2026");
        findChild(page, "diaryDateJumpToggle").clicked();
        verify(jump.visible);
        verify(jump.expanded);
        verify(jump.pickerOnly);
        compare(jump.height, 0);
        jump.edited("2099-10-02");
        verify(!jump.visible);
        tryCompare(jump, "expanded", false);
        compare(page.activeDate, "2099-10-02");
        const past = findChild(page, "diaryDate_2026-10-01");
        const future = findChild(page, "diaryDate_2099-10-02");
        verify(past.width >= 38);
        compare(past.height, future.height);
        compare(page.unitLabel("g"), "г");
        compare(page.unitLabel("hours"), "ч");
    }

    function test_selectingVisibleDatePreservesStripPosition() {
        for (let i = 0; i < 20; ++i)
            stub.days.append({
                date: "2026-11-" + String(i + 1).padStart(2, "0"),
                dayTitle: "Отдых",
                weight: 80,
                average7: 81,
                workoutDone: false,
                note: "",
                editable: false,
                values: []
            });
        const strip = findChild(page, "diaryDaysStrip");
        wait(50);
        strip.contentX = 220;
        wait(50);
        const date = findChild(page, "diaryDate_2026-11-07");
        verify(date !== null);
        const before = strip.contentX;
        date.clicked();
        wait(50);
        compare(page.activeDate, "2026-11-07");
        compare(strip.contentX, before);
    }

    function test_horizontalGestureScrollsDatesWithoutSelectingDay() {
        const previousWidth = testCase.Window.window.width;
        const previousHeight = testCase.Window.window.height;
        testCase.Window.window.width = 390;
        testCase.Window.window.height = 812;
        for (let i = 0; i < 30; ++i)
            stub.days.append({
                date: "2026-11-" + String(i + 1).padStart(2, "0"),
                dayTitle: "Отдых",
                weight: 80,
                average7: 81,
                workoutDone: false,
                note: "",
                editable: true,
                values: []
            });
        page.selectDate("2026-10-01");
        wait(50);
        const strip = findChild(page, "diaryDaysStrip");
        compare(strip.contentX, 0);
        mouseDrag(strip, strip.width - 15, strip.height / 2, -180, 0);
        verify(strip.contentX > 50);
        compare(page.activeDate, "2026-10-01");
        strip.cancelFlick();
        verify(strip.contentX <= strip.contentWidth - strip.width);
        testCase.Window.window.width = previousWidth;
        testCase.Window.window.height = previousHeight;
        wait(50);
    }

    function test_pastValueOpensWithOneTapAndSavesZero() {
        findChild(page, "diaryValue_2026-10-01_protein").clicked();
        compare(stub.selectedDate, "2026-10-01");
        verify(page.valuePopup.visible);
        findChild(page.valuePopup.contentItem, "diaryNumberInput").text = "0";
        findChild(page.valuePopup.contentItem, "diaryNumberSave").clicked();
        compare(stub.calls[stub.calls.length - 1].join("|"), "rule|protein|0");
    }

    function test_futureDayCanPreviewButCannotEdit() {
        const count = stub.calls.length;
        verify(findChild(page, "diaryDate_2099-10-02").enabled);
        findChild(page, "diaryDate_2099-10-02").clicked();
        compare(page.activeDate, "2099-10-02");
        verify(findChild(page, "diaryDay_2099-10-02") !== null);
        compare(findChild(page, "diaryDay_2026-10-01"), null);
        verify(!findChild(page, "diaryValue_2099-10-02_protein").enabled);
        verify(!findChild(page, "diaryWeight_2099-10-02").enabled);
        verify(!findChild(page, "diaryNote_2099-10-02").enabled);
        verify(!findChild(page, "diaryNoteSave_2099-10-02").enabled);
        verify(!page.openDay("2099-10-02", true));
        page.editNumber("2099-10-02", true, null, true);
        findChild(page, "diaryNoteSave_2099-10-02").clicked();
        compare(stub.calls.length, count);
        verify(!page.valuePopup.visible);
        stub.changed();
        wait(0);
        compare(page.activeDate, "2099-10-02");
        page.reload();
        wait(0);
        compare(page.activeDate, "2099-10-02");
        verify(page.selectDate("2026-10-01"));
        compare(stub.selectedDate, "2026-10-01");
    }

    function test_latestEditableIsInitialSelection() {
        stub.days.insert(1, {
            date: "2026-10-02",
            dayTitle: "Отдых",
            weight: 81,
            average7: 82,
            workoutDone: false,
            note: "",
            editable: true,
            values: []
        });
        page.activeDate = "";
        page.refresh();
        compare(page.activeDate, "2026-10-02");
        compare(stub.selectedDate, "2026-10-02");
        compare(findChild(page, "diaryDay_2026-10-01"), null);
        verify(page.selectDate("2026-10-01"));
        verify(findChild(page, "diaryValue_2026-10-01_protein") !== null);
        stub.days.remove(1);
    }

    function test_editorCannotSaveAfterDayBecomesReadOnly() {
        findChild(page, "diaryWeight_2026-10-01").clicked();
        const save = findChild(page.valuePopup.contentItem, "diaryNumberSave");
        const count = stub.calls.length;
        stub.days.setProperty(0, "editable", false);
        stub.changed();
        wait(0);
        save.clicked();
        compare(stub.calls.length, count);
        stub.days.setProperty(0, "editable", true);
    }

    function test_showingPageAgainReloadsCurrentDays() {
        const count = stub.calls.length;
        page.visible = false;
        page.visible = true;
        verify(stub.calls.length > count);
        verify(stub.calls.some(function (call) {
            return call.join("|") === "reload|u1|p1";
        }));
    }

    function test_verdictUsesModelEvenWhenValueContradictsTarget() {
        // 200 >= 180, но модель намеренно говорит failed: QML не считает норму.
        const value = findChild(page, "diaryValue_2026-10-01_protein");
        verify(value.text.indexOf("Мимо нормы") >= 0);
        compare(value.contentItem.color, Theme.negative);
    }

    function editNote(date) {
        verify(page.selectDate(date));
        findChild(page, "diaryNoteEdit_" + date).clicked();
        return findChild(page, "diaryNote_" + date);
    }

    function test_noteStartsAsPreviewAndSaveClosesEditor() {
        const note = findChild(page, "diaryNote_2026-10-01");
        verify(!note.visible);
        verify(!findChild(page, "diaryNoteSave_2026-10-01").visible);
        editNote("2026-10-01").text = "Новая заметка";
        verify(note.visible);
        findChild(page, "diaryNoteSave_2026-10-01").clicked();
        verify(!note.visible);
        compare(stub.days.get(0).note, "Новая заметка");
    }

    function test_dayNoteIsMultilineAndWeightCanBeSaved() {
        const note = editNote("2026-10-01");
        verify(note !== null);
        note.text = "Первая строка\nВторая строка";
        findChild(page, "diaryNoteSave_2026-10-01").clicked();
        compare(stub.calls[stub.calls.length - 1][1], note.text);
        findChild(page, "diaryWeight_2026-10-01").clicked();
        findChild(page.valuePopup.contentItem, "diaryNumberInput").text = "81,4";
        findChild(page.valuePopup.contentItem, "diaryNumberSave").clicked();
        compare(stub.calls[stub.calls.length - 1].join("|"), "weight|81.4");
    }

    function test_invalidNumberDoesNotSaveAndFailureKeepsEditor() {
        findChild(page, "diaryValue_2026-10-01_protein").clicked();
        const input = findChild(page.valuePopup.contentItem, "diaryNumberInput");
        const save = findChild(page.valuePopup.contentItem, "diaryNumberSave");
        input.text = "";
        verify(!save.enabled);
        input.text = "abc";
        verify(!save.enabled);
        input.text = "-1";
        verify(!save.enabled);
        input.text = "123";
        stub.saveSucceeds = false;
        save.clicked();
        verify(page.valuePopup.visible);
        compare(input.text, "123");
        verify(findChild(page.valuePopup.contentItem, "diarySaveError").visible);
    }

    function test_noteCanCollapseWithoutLosingInput() {
        const note = editNote("2026-10-01");
        note.text = "Незаконченная заметка";
        findChild(page, "diaryNoteCollapse_2026-10-01").clicked();
        verify(!note.visible);
        findChild(page, "diaryNoteEdit_2026-10-01").clicked();
        verify(note.visible);
        compare(note.text, "Незаконченная заметка");
    }
    function test_noteFailureKeepsDraftAndShowsLocalError() {
        const note = editNote("2026-10-01");
        note.text = "Несохранённая\nзаметка";
        stub.saveSucceeds = false;
        findChild(page, "diaryNoteSave_2026-10-01").clicked();
        compare(note.text, "Несохранённая\nзаметка");
        const error = findChild(page, "diaryNoteError_2026-10-01");
        verify(error !== null);
        verify(error.visible);
    }

    function countNamedItems(item, prefix) {
        let count = item.objectName && item.objectName.indexOf(prefix) === 0 ? 1 : 0;
        for (let i = 0; i < item.children.length; ++i)
            count += countNamedItems(item.children[i], prefix);
        return count;
    }

    function test_longTermBoundsWeekAndDirectJump() {
        for (let i = 30; i >= 1; --i) {
            stub.days.insert(0, {
                date: "2026-09-" + String(i).padStart(2, "0"),
                dayTitle: "Отдых",
                weight: 80,
                average7: 81,
                workoutDone: false,
                note: "",
                editable: true,
                values: [
                    {
                        ruleId: "protein",
                        title: "Белок",
                        value: 170,
                        verdict: "failed",
                        target: "≥ 180",
                        unit: "г"
                    }
                ]
            });
        }
        stub.changed();
        wait(0);
        verify(page.selectDate("2026-10-01"));
        compare(page.visibleDayCount, 4);
        verify(countNamedItems(page, "diaryDate_") <= 9);
        compare(countNamedItems(page, "diaryDay_"), 1);
        const start = page.weekStart;
        findChild(page, "diaryPreviousWeek").clicked();
        compare(page.weekStart, start - 7);
        compare(stub.selectedDate, "2026-10-01");
        findChild(page, "diaryNextWeek").clicked();
        compare(page.weekStart, start);
        findChild(page, "diaryDateJump").edited("2026-09-01");
        wait(0);
        compare(stub.selectedDate, "2026-09-01");
        compare(findChild(page, "diaryDay_2026-10-01"), null);
        verify(findChild(page, "diaryDay_2026-09-01") !== null);
        verify(!findChild(page, "diaryPreviousWeek").enabled);
        verify(!page.selectDate("2025-01-01"));
        verify(countNamedItems(page, "diaryDate_") <= 9);
        compare(countNamedItems(page, "diaryDay_"), 1);
        verify(page.selectDate("2099-10-02"));
        verify(!findChild(page, "diaryNextWeek").enabled);
        verify(page.visibleDayCount <= 7);
        compare(countNamedItems(page, "diaryDay_"), 1);
        stub.days.remove(0, 30);
    }

    function test_vmChangeRefreshesWeightAndPreservesDraft() {
        const note = editNote("2026-10-01");
        note.text = "Черновик";
        stub.days.setProperty(0, "weight", 83);
        stub.changed();
        wait(0);
        compare(note.text, "Черновик");
        compare(findChild(page, "diaryWeightMetric").valueText, "83");
        compare(stub.selectedDate, "2026-10-01");
        stub.days.setProperty(0, "weight", 82);
    }

    function test_savedNoteSurvivesReload() {
        editNote("2026-10-01").text = "Сохранено\nв дневнике";
        findChild(page, "diaryNoteSave_2026-10-01").clicked();
        page.reload();
        wait(0);
        compare(stub.selectedDate, "2026-10-01");
        compare(findChild(page, "diaryNote_2026-10-01").text, "Сохранено\nв дневнике");
        stub.days.setProperty(0, "note", "Старая заметка");
    }

    function test_ruleSavePreservesDirtyNoteUntilDateChanges() {
        const note = editNote("2026-10-01");
        note.text = "Черновик после ввода белка";
        findChild(page, "diaryValue_2026-10-01_protein").clicked();
        findChild(page.valuePopup.contentItem, "diaryNumberInput").text = "180";
        findChild(page.valuePopup.contentItem, "diaryNumberSave").clicked();
        page.reload();
        wait(0);
        compare(note.text, "Черновик после ввода белка");
        verify(page.selectDate("2099-10-02"));
        compare(findChild(page, "diaryNote_2099-10-02").text, "");
        verify(page.selectDate("2026-10-01"));
        compare(findChild(page, "diaryNote_2026-10-01").text, "Черновик после ввода белка");
    }

    QtObject {
        id: stub

        property string programTitle: "Сила 4 недели"

        signal changed
        property var calls: []
        property bool saveSucceeds: true
        property string selectedDate: ""
        property string selectedNote: ""
        property var selectedWeight: 82
        property var selectedValues: [
            {
                ruleId: "protein",
                title: "Белок",
                value: 200,
                verdict: "failed",
                target: "≥ 180",
                unit: "г"
            }
        ]
        property string errorKey: ""
        property var norms: ListModel {
            ListElement {
                ruleId: "protein"
                title: "Белок"
                target: "≥ 180"
                unit: "г"
            }
        }
        property var summaries: ListModel {
            ListElement {
                ruleId: "protein"
                title: "Белок"
                average: 175
                daysInNorm: 2
                daysOff: 1
            }
        }
        property var days: ListModel {
            ListElement {
                date: "2026-10-01"
                dayTitle: "Верх"
                weight: 82
                average7: 82.5
                workoutDone: true
                note: "Старая заметка"
                editable: true
                values: [
                    ListElement {
                        ruleId: "protein"
                        title: "Белок"
                        value: 200
                        verdict: "failed"
                        target: "≥ 180"
                        unit: "г"
                    }
                ]
            }
            ListElement {
                date: "2099-10-02"
                dayTitle: "Отдых"
                weight: ""
                average7: ""
                workoutDone: false
                note: ""
                editable: false
                values: [
                    ListElement {
                        ruleId: "protein"
                        title: "Белок"
                        value: ""
                        verdict: "unknown"
                        target: "≥ 180"
                        unit: "г"
                    }
                ]
            }
        }

        function reload(userId, programId) {
            calls.push(["reload", userId, programId]);
            selectedDate = "";
            changed();
            return true;
        }
        function openDay(date) {
            calls.push(["open", date]);
            selectedDate = date;
            for (let i = 0; i < days.count; ++i) {
                if (days.get(i).date === date) {
                    selectedNote = days.get(i).note;
                    selectedWeight = days.get(i).weight;
                }
            }
            changed();
            return true;
        }
        function saveRuleValue(id, value) {
            calls.push(["rule", id, value]);
            return result();
        }
        function saveWeight(value) {
            calls.push(["weight", value]);
            return result();
        }
        function saveNote(text) {
            calls.push(["note", text]);
            if (!result())
                return false;
            for (let i = 0; i < days.count; ++i)
                if (days.get(i).date === selectedDate)
                    days.setProperty(i, "note", text);
            selectedNote = text;
            changed();
            return true;
        }
        function result() {
            errorKey = saveSucceeds ? "" : "validation_error";
            return saveSucceeds;
        }
    }
}
