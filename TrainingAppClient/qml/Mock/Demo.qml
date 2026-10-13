pragma Singleton
import QtQuick

// Состояние UI-демо: флаги дизайн-ревью и сообщения о ещё не реализованных экранах.
// Бизнес-логики здесь нет; на этапах M1+ часть состояния заменят ViewModel на C++.
QtObject {
    // — Режим «рамка телефона» (390×812) для дизайн-ревью на десктопе —
    property bool phoneFrame: false

    // — «офлайн» в status bar: включают оверлеи записи тренировки и таймера отдыха
    // (этап D2, референс 2090) —
    property bool sessionOffline: false

    // — Запрошено временное сообщение —
    signal toastRequested(string message)

    function notify(message) {
        toastRequested(message);
    }
}
