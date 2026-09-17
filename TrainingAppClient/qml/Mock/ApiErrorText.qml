pragma Singleton
import QtQuick

// Единое отображение ключей API в локализованный текст. В фазе V реализация
// переедет в AApi_Error, а QML-контракт textFor(key) останется прежним.
QtObject {
    readonly property string fallbackText: qsTr("Не удалось выполнить операцию")
    readonly property var errorTexts: ({
            email_taken: qsTr("Этот email уже используется"),
            email_not_found: qsTr("Проверьте email"),
            not_found: qsTr("Проверьте email"),
            incorrect_password: qsTr("Неверный пароль"),
            invalid_email: qsTr("Введите корректный email"),
            invalid_name: qsTr("Введите имя от 2 до 80 символов"),
            invalid_password: qsTr("Пароль содержит недопустимые символы"),
            password_too_short: qsTr("Пароль должен содержать не менее 8 символов"),
            password_too_long: qsTr("Пароль должен содержать не более 64 символов"),
            password_missing_letter: qsTr("Добавьте латинскую букву"),
            password_missing_digit: qsTr("Добавьте цифру"),
            password_missing_special: qsTr("Добавьте специальный символ"),
            validation_error: qsTr("Проверьте введённые данные"),
            invalid_token: qsTr("Сессия истекла. Войдите снова"),
            token_expired: qsTr("Сессия истекла. Войдите снова"),
            network_error: qsTr("Нет соединения с сервером"),
            timeout: qsTr("Сервер не ответил вовремя"),
            rate_limited: qsTr("Слишком много попыток. Повторите позже"),
            server_error: qsTr("Сервис временно недоступен"),
            recovery_code_invalid: qsTr("Неверный код восстановления"),
            recovery_code_expired: qsTr("Срок действия кода истёк"),
            undefined_error: fallbackText
        })

    function textFor(key) {
        const text = errorTexts[key];

        if (key === "undefined_error") {
            console.warn("ApiErrorText: undefined_error");
        }
        if (typeof text === "string") {
            return text;
        }

        console.warn("ApiErrorText: неизвестный ключ ошибки:", key);
        return fallbackText;
    }
}
