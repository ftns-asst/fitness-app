import QtQuick
import QtQuick.Controls
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: testCase
    name: "AppInput"
    when: windowShown
    width: 390
    height: 240

    Component {
        id: inputComponent
        AppInput {
            width: 280
            height: 56
        }
    }
    function test_programmaticTextDoesNotEmitUserEdit() {
        const input = createTemporaryObject(inputComponent, testCase);
        const spy = Qt.createQmlObject('import QtTest; SignalSpy {}', testCase);
        spy.target = input;
        spy.signalName = "textEdited";
        input.text = "75,5";
        compare(spy.count, 0);
        input.forceActiveFocus();
        keyClick(Qt.Key_End);
        keyClick(Qt.Key_2);
        compare(spy.count, 1);
        compare(input.text, "75,52");
        spy.destroy();
    }
    function test_errorTakesPriorityOverFocus() {
        const input = createTemporaryObject(inputComponent, testCase);
        input.forceActiveFocus();
        input.invalid = true;
        compare(input.background.border.color, Theme.negative);
        input.invalid = false;
        compare(input.background.border.color, Theme.accent);
    }
    function test_existingInputContractsRemainNative() {
        const input = createTemporaryObject(inputComponent, testCase, {
            maximumLength: 4,
            readOnly: true
        });
        input.text = "123456";
        compare(input.text, "1234");
        input.forceActiveFocus();
        keyClick(Qt.Key_7);
        compare(input.text, "1234");
    }
}
