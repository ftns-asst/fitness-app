import QtQuick
import TrainingAppClient

// Состояние формы входа/регистрации и координация асинхронного auth API.
// Визуальный AuthPage только собирает значения полей и отображает эти свойства.
QtObject {
    id: controller

    property var auth: AuthViewModel
    property int authRequestId: -1
    property string checkedEmail: ""
    property string emailError: ""
    property bool emailFree: false
    property int emailRequestId: -1
    property bool emailTaken: false
    property string globalError: ""
    property string initialMode: "login"
    property string mode: initialMode === "signup" ? "signup" : "login"
    property string passwordError: ""
    property var pendingLocalProfile: null
    property var pendingRegistration: null
    property bool pendingSignup: false
    property string signupEmail: ""
    property string state: auth.sessionState === "sessionExpired" ? "sessionExpired" : "idle"

    readonly property bool busy: state === "checkingEmail" || state === "loggingIn" || state === "signingUp"
    property Connections requestConnections: Connections {
        target: controller.auth

        function onSessionExpired() {
            controller.state = "sessionExpired";
        }
        function onRequestFinished(requestId, operation, ok, data, error) {
            controller.finishRequest(requestId, operation, ok, data, error);
        }
    }
    property Timer emailDebounce: Timer {
        interval: 450

        onTriggered: controller.checkEmailNow(controller.signupEmail, false, null)
    }

    signal authenticated

    function checkEmailNow(email, forSubmit, registration) {
        const normalizedEmail = normalizeEmail(email);

        emailDebounce.stop();
        signupEmail = normalizedEmail;
        pendingSignup = Boolean(forSubmit);
        pendingRegistration = forSubmit ? registration : null;
        emailError = "";
        emailTaken = false;
        emailFree = false;

        if (!auth.isValidEmail(normalizedEmail)) {
            clearPendingSignup();
            emailError = auth.errorText("invalid_email");
            state = "error";
            return;
        }

        checkedEmail = normalizedEmail;
        state = "checkingEmail";
        emailRequestId = auth.checkEmail(normalizedEmail);
    }
    function clearPendingSignup() {
        pendingSignup = false;
        pendingRegistration = null;
    }
    function finishRequest(requestId, operation, ok, data, error) {
        if (requestId === emailRequestId && operation === "checkEmail") {
            emailRequestId = -1;
            if (normalizeEmail(signupEmail) !== checkedEmail) {
                clearPendingSignup();
                state = "idle";
                return;
            }
            if (!ok) {
                clearPendingSignup();
                state = "error";
                emailError = auth.errorText(error.key);
                return;
            }

            emailFree = data.free;
            emailTaken = !data.free;
            state = data.free ? "idle" : "emailTaken";
            emailError = data.free ? "" : auth.errorText("email_taken");
            if (data.free && pendingSignup)
                submitSignupRequest();
            else
                clearPendingSignup();
            return;
        }

        if (requestId !== authRequestId)
            return;

        authRequestId = -1;
        if (!ok) {
            pendingLocalProfile = null;
            state = "error";
            globalError = auth.errorText(error.key);
            if (operation === "login" && error.key === "email_not_found")
                emailError = auth.errorText(error.key);
            if (operation === "login" && error.key === "incorrect_password")
                passwordError = auth.errorText(error.key);
            return;
        }

        if (operation === "signup" && pendingLocalProfile !== null)
            auth.saveLocalProfile(pendingLocalProfile);
        pendingLocalProfile = null;
        state = "authenticated";
        authenticated();
    }
    function normalizeEmail(email) {
        return String(email).trim().toLowerCase();
    }
    function passwordErrorKey(password) {
        return auth.passwordErrorKey(password);
    }
    function registrationProfile(registration) {
        return {
            age: Number(registration.age),
            gender: registration.gender,
            heightCm: Number(registration.height),
            weightKg: Number(registration.weight),
            level: "beginner",
            goal: registration.goal,
            workoutsPerWeek: 3
        };
    }
    function resetMessages() {
        globalError = "";
        emailError = "";
        passwordError = "";
        if (state !== "sessionExpired")
            state = "idle";
    }
    function signupEmailEdited(email) {
        signupEmail = String(email);
        emailFree = false;
        emailTaken = false;
        checkedEmail = "";
        emailError = "";
        clearPendingSignup();

        if (auth.isValidEmail(signupEmail))
            emailDebounce.restart();
        else
            emailDebounce.stop();
    }
    function submitLogin(email, password) {
        const normalizedEmail = normalizeEmail(email);

        resetMessages();
        if (!auth.isValidEmail(normalizedEmail)) {
            emailError = auth.errorText("invalid_email");
            state = "error";
            return;
        }
        if (password.length === 0) {
            passwordError = qsTr("Введите пароль");
            state = "error";
            return;
        }

        state = "loggingIn";
        authRequestId = auth.login(normalizedEmail, password);
    }
    function submitRegistration(registration) {
        const passwordKey = passwordErrorKey(registration.password);
        const normalizedEmail = normalizeEmail(registration.email);

        resetMessages();
        signupEmail = normalizedEmail;
        if (!auth.isValidEmail(normalizedEmail)) {
            emailError = auth.errorText("invalid_email");
            state = "error";
            return;
        }
        if (registration.name.trim().length < 2 || registration.name.trim().length > 80) {
            globalError = auth.errorText("invalid_name");
            state = "error";
            return;
        }
        if (passwordKey.length > 0) {
            passwordError = auth.errorText(passwordKey);
            state = "error";
            return;
        }
        if (Number(registration.age) < 14 || Number(registration.age) > 100 || Number(registration.height) < 100 || Number(registration.height) > 250 || Number(registration.weight) < 30 || Number(registration.weight) > 300) {
            globalError = qsTr("Проверьте возраст, рост и вес");
            state = "error";
            return;
        }

        if (!emailFree || checkedEmail !== normalizedEmail) {
            checkEmailNow(normalizedEmail, true, registration);
            return;
        }

        pendingRegistration = registration;
        submitSignupRequest();
    }
    function submitSignupRequest() {
        const registration = pendingRegistration;

        clearPendingSignup();
        if (registration === null)
            return;

        pendingLocalProfile = registrationProfile(registration);
        state = "signingUp";
        authRequestId = auth.signup({
            email: normalizeEmail(registration.email),
            name: registration.name.trim(),
            password: registration.password
        });
    }
    function switchMode(nextMode) {
        if (mode === nextMode)
            return;

        emailDebounce.stop();
        clearPendingSignup();
        emailRequestId = -1;
        authRequestId = -1;
        mode = nextMode;
        resetMessages();
    }
}
