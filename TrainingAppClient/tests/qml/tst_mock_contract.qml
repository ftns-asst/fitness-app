import QtQuick
import QtTest
import TrainingAppClient 1.0

TestCase {
    id: testCase

    function cleanup() {
        MockApi.reset();
        requestFinishedSpy.clear();
    }
    function init() {
        MockApi.reset();
        MockApi.responseDelayMs = 1;
        requestFinishedSpy.clear();
    }
    function test_checkEmail() {
        const request_id = MockApi.checkEmail("  NEW@example.com ");
        const args = waitForResult();

        compare(args[0], request_id);
        compare(args[1], "checkEmail");
        compare(args[2], true);
        compare(args[3].free, true);
        compare(args[4], null);
    }
    function test_checkEmailInvalid() {
        MockApi.checkEmail("not-an-email");
        const args = waitForResult();

        compare(args[2], false);
        compare(args[3], null);
        compare(args[4].key, "undefined_error");
        compare(args[4].httpStatus, 400);
    }
    function test_checkEmailTaken() {
        MockApi.checkEmail("igor@example.com");
        const args = waitForResult();

        compare(args[2], true);
        compare(args[3].free, false);
        compare(args[4], null);
    }
    function test_errorTexts() {
        const keys = ["email_taken", "email_not_found", "incorrect_password", "invalid_email", "invalid_name", "invalid_password", "password_too_short", "password_too_long", "password_missing_letter", "password_missing_digit", "password_missing_special", "validation_error", "invalid_token", "token_expired", "network_error", "timeout", "rate_limited", "server_error", "recovery_code_invalid", "recovery_code_expired"];

        for (let i = 0; i < keys.length; ++i) {
            verify(ApiErrorText.textFor(keys[i]).length > 0, "Missing text for " + keys[i]);
        }

        compare(ApiErrorText.textFor("email_not_found"), ApiErrorText.textFor("not_found"));
        compare(ApiErrorText.textFor("invalid_token"), ApiErrorText.textFor("token_expired"));
        compare(ApiErrorText.textFor("undefined_error"), ApiErrorText.fallbackText);
        compare(ApiErrorText.textFor("unknown_error"), ApiErrorText.fallbackText);
        compare(ApiErrorText.textFor("constructor"), ApiErrorText.fallbackText);
    }
    function test_incorrectPassword() {
        MockApi.login("igor@example.com", "wrong-password");
        const args = waitForResult();

        compare(args[2], false);
        compare(args[4].key, "incorrect_password");
        compare(args[4].httpStatus, 400);
        verify(!MockSession.authenticated);
        verify(!MockTokenStore.hasTokens);
    }
    function test_loginAndGetUser() {
        MockApi.login("igor@example.com", "Strong1!");
        let args = waitForResult();

        compare(args[2], true);
        compare(args[3].user.id, "2e8cbeec-3528-45bc-908b-cdf76944d9c3");
        verify(args[3].user.createdAt.length > 0);
        verify(args[3].tokens.accessToken.length > 0);
        verify(args[3].tokens.refreshToken.length > 0);
        compare(Object.keys(args[3].tokens).sort().join(","), "accessToken,refreshToken");
        verify(MockSession.authenticated);
        verify(MockTokenStore.hasTokens);
        compare(MockSession.userId, "2e8cbeec-3528-45bc-908b-cdf76944d9c3");
        compare(MockSession.profile, null);
        compare(MockTokenStore.authorizationHeader, "Bearer " + MockTokenStore.accessToken);

        requestFinishedSpy.clear();
        MockApi.getUser(MockSession.userId, true);
        args = waitForResult();

        compare(args[1], "getUser");
        compare(args[2], true);
        compare(args[3].id, MockSession.userId);
        compare(args[3].profile.heightCm, 182);
        compare(args[3].profile.weightKg, 78);
        compare(args[3].profile.gender, "male");

        requestFinishedSpy.clear();
        MockApi.getUser(MockSession.userId, false);
        args = waitForResult();

        compare(args[2], true);
        verify(args[3].profile === undefined);

        requestFinishedSpy.clear();
        MockApi.getUser(MockSession.userId);
        args = waitForResult();

        compare(args[2], true);
        verify(args[3].profile === undefined);
    }
    function test_loginUnknownEmailMatchesCurrentBackend() {
        MockApi.login("missing@example.com", "Strong1!");
        const args = waitForResult();

        compare(args[2], false);
        compare(args[4].key, "undefined_error");
        compare(args[4].httpStatus, 500);
    }
    function test_passwordValidation(data) {
        MockApi.signup({
            email: "validation@example.com",
            name: "Валидатор",
            password: data.password
        });
        const args = waitForResult();

        compare(args[2], false);
        compare(args[4].key, data.key);
        compare(args[4].httpStatus, 400);
    }
    function test_passwordValidation_data() {
        return [
            {
                tag: "too short",
                password: "Aa1!",
                key: "password_too_short"
            },
            {
                tag: "missing letter",
                password: "12345678!",
                key: "password_missing_letter"
            },
            {
                tag: "missing digit",
                password: "Password!",
                key: "password_missing_digit"
            },
            {
                tag: "missing special",
                password: "Password1",
                key: "password_missing_special"
            },
            {
                tag: "non ascii",
                password: "Пароль123!",
                key: "invalid_password"
            }
        ];
    }
    function test_refreshAndForcedExpiration() {
        MockApi.login("igor@example.com", "Strong1!");
        let args = waitForResult();
        const old_access_token = MockTokenStore.accessToken;
        const old_refresh_token = MockTokenStore.refreshToken;

        compare(args[2], true);
        requestFinishedSpy.clear();
        MockApi.refresh(old_refresh_token);
        args = waitForResult();

        compare(args[2], true);
        verify(args[3].tokens.accessToken !== old_access_token);
        verify(args[3].tokens.refreshToken !== old_refresh_token);
        const new_refresh_token = args[3].tokens.refreshToken;
        compare(MockTokenStore.accessToken, args[3].tokens.accessToken);
        compare(MockTokenStore.refreshToken, new_refresh_token);

        requestFinishedSpy.clear();
        MockApi.refresh(old_refresh_token);
        args = waitForResult();

        compare(args[2], false);
        compare(args[4].key, "invalid_token");
        compare(MockSession.state, "sessionExpired");
        verify(!MockTokenStore.hasTokens);

        requestFinishedSpy.clear();
        MockApi.refresh(new_refresh_token);
        args = waitForResult();

        compare(args[2], false);
        compare(args[4].key, "invalid_token");
    }
    function test_requestQueue() {
        const first_id = MockApi.checkEmail("one@example.com");
        const second_id = MockApi.checkEmail("two@example.com");

        tryCompare(requestFinishedSpy, "count", 2, 3000);
        compare(requestFinishedSpy.signalArguments[0][0], first_id);
        compare(requestFinishedSpy.signalArguments[1][0], second_id);
        compare(requestFinishedSpy.signalArguments[0][2], true);
        compare(requestFinishedSpy.signalArguments[1][2], true);
        compare(MockApi.pendingCount, 0);
        verify(!MockApi.busy);
    }
    function test_signupAllowsDuplicateName() {
        MockApi.signup({
            email: "same-name@example.com",
            name: "Игорь Морозов",
            password: "Secure2@"
        });
        const args = waitForResult();

        compare(args[2], true);
        compare(args[3].user.name, "Игорь Морозов");
        compare(args[3].user.email, "same-name@example.com");
    }
    function test_signupAndLogout() {
        MockApi.signup({
            email: "anna@example.com",
            name: "Анна",
            password: "Secure2@"
        });
        const args = waitForResult();

        compare(args[2], true);
        compare(args[3].user.email, "anna@example.com");
        verify(MockSession.authenticated);
        compare(MockSession.profile, null);
        compare(Object.keys(MockTokenStore.snapshot()).sort().join(","), "accessToken,refreshToken");

        requestFinishedSpy.clear();
        MockApi.getUser(MockSession.userId, true);
        const user_args = waitForResult();

        compare(user_args[2], true);
        verify(user_args[3].profile === undefined);

        MockApi.logout();
        compare(MockSession.state, "anonymous");
        verify(!MockSession.authenticated);
        verify(!MockTokenStore.hasTokens);
    }
    function waitForResult() {
        tryCompare(requestFinishedSpy, "count", 1, 2000, "Mock API did not answer in time");
        compare(requestFinishedSpy.count, 1);
        return requestFinishedSpy.signalArguments[0];
    }

    name: "MockContract"

    SignalSpy {
        id: requestFinishedSpy

        signalName: "requestFinished"
        target: MockApi
    }
}
