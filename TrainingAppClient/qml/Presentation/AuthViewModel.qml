pragma Singleton
import QtQuick

// Стабильная presentation-граница auth для QML-экранов. Сейчас адаптирует
// mock-реализацию, а на фазе V её публичный контракт заменит Avm_Auth.
// Страницы не должны обращаться к MockApi/MockSession напрямую.
QtObject {
    id: viewModel

    readonly property bool authenticated: MockSession.authenticated
    readonly property var localProfile: MockSession.localProfile
    readonly property var profile: MockSession.profile
    readonly property string sessionState: MockSession.state
    readonly property var user: MockSession.user
    property Connections apiConnections: Connections {
        target: MockApi

        function onRequestFinished(requestId, operation, ok, data, error) {
            viewModel.requestFinished(requestId, operation, ok, data, error);
        }
    }
    property Connections sessionConnections: Connections {
        target: MockSession

        function onLoggedOut() {
            viewModel.loggedOut();
        }
        function onSessionExpired() {
            viewModel.sessionExpired();
        }
    }

    signal loggedOut
    signal requestFinished(int requestId, string operation, bool ok, var data, var error)
    signal sessionExpired

    function checkEmail(email) {
        return MockApi.checkEmail(email);
    }
    function clearLocalProfile() {
        MockSession.clearLocalProfile();
    }
    function errorText(key) {
        return ApiErrorText.textFor(key);
    }
    function isValidEmail(email) {
        return /^[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}$/i.test(String(email).trim());
    }
    function login(email, password) {
        return MockApi.login(email, password);
    }
    function logout() {
        MockApi.logout();
    }
    function passwordErrorKey(password) {
        if (password.length < 8)
            return "password_too_short";
        if (password.length > 64)
            return "password_too_long";
        if (!/^[\x21-\x7E]+$/.test(password))
            return "invalid_password";
        if (!/[A-Za-z]/.test(password))
            return "password_missing_letter";
        if (!/[0-9]/.test(password))
            return "password_missing_digit";
        if (!/[^A-Za-z0-9]/.test(password))
            return "password_missing_special";
        return "";
    }
    function saveLocalProfile(localProfileDto) {
        return MockSession.saveLocalProfile(localProfileDto);
    }
    function signup(request) {
        return MockApi.signup(request);
    }
}
