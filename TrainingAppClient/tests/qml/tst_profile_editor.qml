import QtQuick
import QtQuick.Window
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: testCase
    name: "ProfileEditor"
    when: windowShown
    property var host
    QtObject {
        id: model
        property var profile: ({
                age: 30,
                gender: "male",
                heightCm: 180,
                weightKg: 70
            })
        property bool loading: false
        property bool saving: false
        property bool offline: false
        property string errorText: ""
        property var changes: null
        property string lastReloadId: ""
        property var profileParams: []
        function reload(userId) {
            lastReloadId = userId;
        }
        signal profileSaved
        function saveProfile(values) {
            changes = values;
            saving = true;
            return true;
        }
    }
    Component {
        id: factory
        Window {
            property alias editor: editor
            visible: true
            width: 390
            height: 812
            ProfileEditorPopup {
                id: editor
                viewModel: model
            }
        }
    }
    Component {
        id: profileFactory
        Window {
            visible: true
            width: 390
            height: 812
            property alias account: account
            QtObject {
                id: account
                property bool authenticated: false
                property var user: null
                property var localProfile: null
            }
            ProfilePage {
                anchors.fill: parent
                auth: account
                viewModel: model
            }
        }
    }
    function init() {
        model.changes = null;
        model.saving = false;
        model.offline = false;
        model.errorText = "";
        host = createTemporaryObject(factory, testCase);
        host.editor.openForProfile();
        tryCompare(host.editor, "opened", true);
    }
    function cleanup() {
        host.destroy();
    }
    function test_onlyChangedFieldsAreSent() {
        findChild(host.editor.contentItem, "profileWeightField").text = "71,5";
        host.editor.save();
        compare(Object.keys(model.changes).length, 1);
        compare(model.changes.weightKg, 71.5);
        verify(host.editor.visible);
        model.saving = false;
        model.profileSaved();
        tryCompare(host.editor, "visible", false);
    }
    function test_invalidAgeDoesNotSend() {
        findChild(host.editor.contentItem, "profileAgeField").text = "151";
        host.editor.save();
        compare(model.changes, null);
        verify(host.editor.formError.length > 0);
    }
    function test_offlineSavesLocally() {
        findChild(host.editor.contentItem, "profileWeightField").text = "72";
        model.offline = true;
        host.editor.save();
        compare(model.changes.weightKg, 72);
        compare(findChild(host.editor.contentItem, "profileWeightField").text, "72");
        verify(host.editor.visible);
    }
    function test_profileReloadsAfterLoginCompletes() {
        host.editor.close();
        const profileHost = createTemporaryObject(profileFactory, testCase);
        profileHost.account.user = {
            id: "signed-in-user",
            name: "User",
            email: "user@example.com"
        };
        profileHost.account.authenticated = true;
        compare(model.lastReloadId, "signed-in-user");
    }
    function test_focusedFieldStaysVisibleWhenWindowShrinks() {
        host.height = 440;
        const field = findChild(host.editor.contentItem, "profileWeightField");
        const flick = findChild(host.editor.contentItem, "profileEditorFlick");
        field.focusInput();
        tryVerify(function () {
            const bottom = field.mapToItem(flick.contentItem, 0, field.height).y;
            return bottom <= flick.contentY + flick.height + 1;
        });
        tryVerify(function () {
            return host.editor.height <= 408;
        });
    }
}
