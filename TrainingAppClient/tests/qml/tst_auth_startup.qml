import QtQuick
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: testCase
    name: "AuthStartup"

    QtObject {
        id: backend
        property int restoreCalls: 0
        property bool hasSession: true
        signal requestFinished(int requestId, string operation, bool ok, var data, var error)
        signal sessionRestored(var user)
        signal sessionExpired
        signal loggedOut

        function restoreSession() {
            ++restoreCalls;
            if (hasSession)
                sessionRestored({
                    id: "persisted-user",
                    name: "Saved user"
                });
            return hasSession;
        }
    }
    function init() {
        MockApi.reset();
        AuthViewModel.backend = null;
        AuthViewModel.sessionInitialized = false;
        backend.restoreCalls = 0;
        backend.hasSession = true;
    }
    function cleanup() {
        AuthViewModel.backend = null;
        AuthViewModel.sessionInitialized = false;
        MockApi.reset();
    }
    function test_realSessionSurvivesStartupAndRepeatedInitialization() {
        AuthViewModel.backend = backend;
        AuthViewModel.initializeSession(false, false);
        compare(backend.restoreCalls, 1);
        verify(AuthViewModel.authenticated);
        compare(AuthViewModel.user.id, "persisted-user");
        AuthViewModel.initializeSession(false, false);
        compare(backend.restoreCalls, 1);
        verify(AuthViewModel.authenticated);
    }
    function test_demoFlagsNeverReplaceRealSession() {
        AuthViewModel.backend = backend;
        AuthViewModel.initializeSession(true, false);
        verify(AuthViewModel.authenticated);
        compare(AuthViewModel.user.id, "persisted-user");
    }
    function test_realStartupDoesNotResetPendingRecoveryRequests() {
        AuthViewModel.backend = backend;
        MockApi.nextRequestId = 42;
        AuthViewModel.initializeSession(false, false);
        compare(MockApi.nextRequestId, 42);
    }
    function test_noStoredSessionStaysAnonymous() {
        backend.hasSession = false;
        AuthViewModel.backend = backend;
        AuthViewModel.initializeSession(false, false);
        compare(backend.restoreCalls, 1);
        verify(!AuthViewModel.authenticated);
    }
    function test_mockGuestStartupClearsDemoSession() {
        MockApi.startDemoSession();
        AuthViewModel.initializeSession(false, false);
        verify(!AuthViewModel.authenticated);
    }
    function test_demoStartupStillWorks() {
        AuthViewModel.initializeSession(true, false);
        verify(AuthViewModel.authenticated);
        compare(AuthViewModel.user.email, "igor@example.com");
    }
    function test_forcedGuestDoesNotStartDemoSession() {
        AuthViewModel.initializeSession(true, true);
        verify(!AuthViewModel.authenticated);
    }
}
