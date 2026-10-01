pragma Singleton
import QtQuick

// Стабильная presentation-граница auth для QML-экранов. Без флага --real-auth
// адаптирует presentation-mock; при --real-auth операции уходят на настоящий
// backend через AuthBackend (Avm_Auth_Bridge), а события зеркалируются в
// MockSession — реальный auth transparentен для роутинга Main/AppShell.
// Реальные токены остаются в C++ (SQLite): в MockTokenStore попадают заглушки.
// Страницы не должны обращаться к MockApi/MockSession напрямую.
QtObject {
    id: viewModel

    // qmltestrunner не получает контекст-свойство → тесты всегда на mock.
    readonly property bool realMode: typeof AuthBackend !== "undefined" && AuthBackend !== null
    // Заглушки вместо реальных токенов: настоящие пары остаются в C++ SQLite.
    readonly property var realSessionTokens: ({
            accessToken: "real-session",
            refreshToken: "real-session"
        })
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
    property Connections backendConnections: Connections {
        target: viewModel.realMode ? AuthBackend : null

        function onRequestFinished(requestId, operation, ok, data, error) {
            viewModel.requestFinished(requestId, operation, ok, data, error);
            if (ok && (operation === "login" || operation === "signup"))
                MockSession.authenticate(data.user, undefined, viewModel.realSessionTokens);
        }
        function onLoggedOut() {
            MockSession.logout();
        }
        function onSessionExpired() {
            MockSession.expire();
        }
        function onSessionRestored(user) {
            MockSession.authenticate(user, undefined, viewModel.realSessionTokens);
        }
    }

    signal loggedOut
    signal requestFinished(int requestId, string operation, bool ok, var data, var error)
    signal sessionExpired

    function checkEmail(email) {
        if (realMode)
            return AuthBackend.checkEmail(email);
        return MockApi.checkEmail(email);
    }
    function clearLocalProfile() {
        if (realMode)
            AuthBackend.clearLocalProfile();
        MockSession.clearLocalProfile();
    }
    function errorText(key) {
        return ApiErrorText.textFor(key);
    }
    function isValidEmail(email) {
        return /^[A-Z0-9._%+-]+@[A-Z0-9.-]+\.[A-Z]{2,}$/i.test(String(email).trim());
    }
    function login(email, password) {
        if (realMode)
            return AuthBackend.login(email, password);
        return MockApi.login(email, password);
    }
    function logout() {
        if (realMode) {
            AuthBackend.logout();
            return;
        }
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
        if (realMode) {
            const saved = AuthBackend.saveLocalProfile(localProfileDto);
            MockSession.saveLocalProfile(localProfileDto);
            return saved;
        }
        return MockSession.saveLocalProfile(localProfileDto);
    }
    function signup(request) {
        if (realMode)
            return AuthBackend.signup(request);
        return MockApi.signup(request);
    }

    Component.onCompleted: {
        if (realMode)
            AuthBackend.restoreSession();
    }
}
