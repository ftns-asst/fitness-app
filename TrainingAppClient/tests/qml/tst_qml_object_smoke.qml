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
        id: textFieldComponent

        AppTextField {
            width: 320
        }
    }
}
