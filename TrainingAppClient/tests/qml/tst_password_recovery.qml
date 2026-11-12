import QtQuick
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: testCase
    name: "PasswordRecovery"
    property var controller

    Component {
        id: factory
        PasswordRecoveryController {}
    }

    function init() {
        MockApi.reset();
        MockApi.responseDelayMs = 1;
        controller = createTemporaryObject(factory, testCase);
        verify(controller !== null);
    }
    function cleanup() {
        MockApi.reset();
    }
    function requestCode() {
        controller.email = " IGOR@example.com ";
        controller.submit();
        tryCompare(controller, "step", "code");
        compare(controller.email, "igor@example.com");
        verify(!MockSession.authenticated);
    }
    function verifyCode() {
        requestCode();
        controller.code = "123456";
        controller.submit();
        tryCompare(controller, "step", "password");
        verify(!MockSession.authenticated);
    }
    function test_completeRecoveryAuthenticatesAndChangesPassword() {
        verifyCode();
        controller.password = "NewStrong2!";
        controller.confirmation = "NewStrong2!";
        controller.submit();
        tryCompare(controller, "step", "complete");
        verify(MockSession.authenticated);
        compare(MockApi.usersByEmail["igor@example.com"].password, "NewStrong2!");
        compare(controller.resetToken, "");
        compare(controller.password, "");
    }
    function test_invalidEmailDoesNotSend() {
        controller.email = "invalid";
        controller.submit();
        compare(MockApi.pendingCount, 0);
        verify(controller.errorText.length > 0);
    }
    function test_invalidCodeCanBeCorrected() {
        requestCode();
        controller.code = "000000";
        controller.submit();
        tryCompare(controller, "busy", false);
        compare(controller.step, "code");
        compare(controller.errorText, ApiErrorText.textFor("recovery_code_invalid"));
        controller.code = "123456";
        controller.submit();
        tryCompare(controller, "step", "password");
    }
    function test_confirmationRequired() {
        verifyCode();
        controller.password = "NewStrong2!";
        controller.confirmation = "OtherStrong3!";
        controller.submit();
        verify(controller.errorText.length > 0);
        verify(!controller.busy);
        verify(!MockSession.authenticated);
    }
    function test_expiredResetReturnsToCodeRequest() {
        verifyCode();
        MockApi.failMode = "resetPassword:reset_token_expired";
        controller.password = "NewStrong2!";
        controller.confirmation = "NewStrong2!";
        controller.submit();
        tryCompare(controller, "step", "email");
        compare(controller.resetToken, "");
        verify(controller.errorText.length > 0);
    }
    function test_cancelIgnoresPendingReply() {
        MockApi.responseDelayMs = 30;
        controller.email = "igor@example.com";
        controller.submit();
        controller.back();
        tryCompare(MockApi, "pendingCount", 0);
        compare(controller.step, "email");
        verify(!controller.busy);
    }
    function test_resendUsesCooldownAndClearsOldCode() {
        requestCode();
        const nextId = MockApi.nextRequestId;
        controller.resend();
        compare(MockApi.nextRequestId, nextId);
        controller.resendAt = 0;
        controller.code = "000000";
        controller.resend();
        tryCompare(controller, "busy", false);
        compare(controller.code, "");
        compare(controller.step, "code");
    }
}
