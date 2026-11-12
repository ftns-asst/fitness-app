import QtQuick
import TrainingAppClient

// Одноразовый resetToken живёт только до завершения/отмены формы, не сохраняется.
QtObject {
    id: controller

    property var auth: AuthViewModel
    property string step: "email"
    property string email: ""
    property string code: ""
    property string password: ""
    property string confirmation: ""
    property string resetToken: ""
    property string errorText: ""
    property bool busy: false
    property int requestId: -1
    property string operation: ""
    property double resendAt: 0
    property double now: Date.now()
    readonly property int resendSeconds: Math.max(0, Math.ceil((resendAt - now) / 1000))
    property Connections replies: Connections {
        target: controller.auth
        function onRequestFinished(id, operation, ok, data, error) {
            controller.finish(id, operation, ok, data, error);
        }
    }
    property Timer clock: Timer {
        interval: 1000
        repeat: true
        running: controller.resendAt > controller.now
        onTriggered: controller.now = Date.now()
    }

    signal authenticated
    signal closeRequested

    function clearSecrets() {
        code = "";
        password = "";
        confirmation = "";
        resetToken = "";
    }
    function back() {
        // Смена пароля завершает вход: не покидаем форму во время этого POST.
        if (busy && step === "password")
            return;
        const closing = step === "email";
        requestId = -1;
        busy = false;
        operation = "";
        errorText = "";
        step = "email";
        clearSecrets();
        if (closing)
            closeRequested();
    }
    function start(nextOperation) {
        busy = true;
        operation = nextOperation;
        errorText = "";
        if (nextOperation === "requestRecovery")
            requestId = auth.requestRecovery(email);
        else if (nextOperation === "verifyRecovery")
            requestId = auth.verifyRecovery(email, code);
        else
            requestId = auth.resetPassword(resetToken, password);
        if (requestId <= 0) {
            busy = false;
            errorText = qsTr("Не удалось отправить запрос. Попробуйте ещё раз");
        }
    }
    function submit() {
        if (busy || step === "complete")
            return;
        errorText = "";
        if (step === "email") {
            email = email.trim().toLowerCase();
            if (!auth.isValidEmail(email)) {
                errorText = auth.errorText("invalid_email");
                return;
            }
            start("requestRecovery");
        } else if (step === "code") {
            code = code.trim();
            if (!/^[0-9]{6}$/.test(code)) {
                errorText = qsTr("Введите 6 цифр из письма");
                return;
            }
            start("verifyRecovery");
        } else {
            const key = auth.passwordErrorKey(password);
            if (key.length > 0) {
                errorText = auth.errorText(key);
                return;
            }
            if (password !== confirmation) {
                errorText = qsTr("Пароли не совпадают");
                return;
            }
            start("resetPassword");
        }
    }
    function resend() {
        now = Date.now();
        if (busy || step !== "code" || resendSeconds > 0)
            return;
        code = "";
        start("requestRecovery");
    }
    function finish(id, finishedOperation, ok, data, error) {
        if (id !== requestId || finishedOperation !== operation)
            return;
        requestId = -1;
        busy = false;
        if (!ok) {
            if (finishedOperation === "requestRecovery" && error.httpStatus === 404)
                errorText = qsTr("Аккаунт с этим email не найден. Проверьте адрес");
            else
                errorText = auth.errorText(error.key);
            if (error.key === "reset_token_expired" || error.key === "reset_token_invalid") {
                clearSecrets();
                step = "email";
            }
            return;
        }
        if (finishedOperation === "requestRecovery") {
            step = "code";
            code = "";
            now = Date.now();
            resendAt = now + 60000;
        } else if (finishedOperation === "verifyRecovery") {
            if (!data || !data.resetToken) {
                errorText = qsTr("Не удалось проверить код. Запросите новый");
                return;
            }
            resetToken = data.resetToken;
            code = "";
            step = "password";
        } else {
            clearSecrets();
            step = "complete";
            authenticated();
        }
    }
}
