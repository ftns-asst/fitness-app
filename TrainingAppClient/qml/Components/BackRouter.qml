pragma Singleton
import QtQuick

// Диспетчер системной кнопки/жеста «назад» (этап M1). Держит реестр
// BackHandler'ов всего приложения и при нажатии вызывает включённый
// обработчик с наибольшим priority (при равенстве — зарегистрированный
// позже). Самостоятельно ничего не перехватывает: клавиши слушают
// сами BackHandler'ы (см. BackHandler.qml).
QtObject {
    id: router

    // Монотонный счётчик изменений набора/состояний обработчиков:
    // на него опираются биндинги `isActive` в BackHandler, чтобы
    // пересчитываться при каждом изменении.
    property int revision: 0

    // Зарегистрированные BackHandler'ы. Свойство var: присваиваем новую
    // ссылку на каждый регистр/снятие, чтобы список не мутировали за спиной QML.
    property var handlers: ([])

    function register(handler) {
        const list = handlers.slice();
        list.push(handler);
        handlers = list;
        ++revision;
    }

    function unregister(handler) {
        const index = handlers.indexOf(handler);
        if (index !== -1) {
            const list = handlers.slice();
            list.splice(index, 1);
            handlers = list;
            ++revision;
        }
    }

    // Вызывается обработчиком при изменении priority/enabled.
    function touch() {
        ++revision;
    }

    // Включённый обработчик с наибольшим priority либо null.
    function activeHandler() {
        let best = null;
        for (let i = 0; i < handlers.length; ++i) {
            const candidate = handlers[i];
            if (!candidate.enabled)
                continue;
            if (best === null || candidate.priority >= best.priority)
                best = candidate;
        }
        return best;
    }

    // Раздача нажатия «назад» активному обработчику.
    function dispatchBack() {
        const best = activeHandler();
        if (best !== null)
            best.backPerformed();
    }
}
