import QtQuick
import QtQuick.Window
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: testCase
    name: "PasswordRecoveryUi"
    when: windowShown
    property var host: null

    Component {
        id: factory
        Window {
            property alias page: authPage
            visible: true
            width: 390
            height: 780
            AuthPage {
                id: authPage
                anchors.fill: parent
            }
        }
    }
    function init() {
        MockApi.reset();
        MockApi.responseDelayMs = 1;
    }
    function cleanup() {
        if (host !== null)
            host.destroy();
        host = null;
        Theme.darkMode = false;
        MockApi.reset();
    }
    function field(name, text) {
        const item = findChild(host.page, name);
        verify(item !== null);
        item.text = text;
        item.userEdited();
    }
    function click(name) {
        const item = findChild(host.page, name);
        verify(item !== null);
        verify(item.visible);
        verify(item.enabled);
        mouseClick(item, item.width / 2, item.height / 2);
    }
    function test_recovery_data() {
        return [
            {
                tag: "360-light",
                size: 360,
                dark: false
            },
            {
                tag: "390-light",
                size: 390,
                dark: false
            },
            {
                tag: "480-dark",
                size: 480,
                dark: true
            }
        ];
    }
    function test_recovery(data) {
        Theme.darkMode = data.dark;
        host = createTemporaryObject(factory, testCase, {
            width: data.size
        });
        verify(host !== null);
        waitForRendering(host.contentItem);
        host.page.loginEmail = "igor@example.com";
        click("forgotPasswordButton");
        const recovery = findChild(host.page, "passwordRecoveryPage");
        verify(recovery !== null);
        compare(recovery.email, "igor@example.com");
        click("recoverySubmitButton");
        tryCompare(recovery, "step", "code");
        field("recoveryCodeField", "123456");
        click("recoverySubmitButton");
        tryCompare(recovery, "step", "password");
        field("recoveryPasswordField", "NewStrong2!");
        field("recoveryConfirmationField", "NewStrong2!");
        click("recoverySubmitButton");
        tryCompare(host.page, "recoveryOpen", false);
        verify(MockSession.authenticated);
        compare(MockSession.user.email, "igor@example.com");
    }
}
