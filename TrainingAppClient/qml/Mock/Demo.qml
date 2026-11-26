pragma Singleton
import QtQuick

// Рамка для дизайн-ревью и короткие сообщения приложения.
QtObject {
    // — Режим «рамка телефона» (390×812) для дизайн-ревью на десктопе —
    property bool phoneFrame: false

    signal toastRequested(string message)

    function notify(message) {
        toastRequested(message);
    }
}
