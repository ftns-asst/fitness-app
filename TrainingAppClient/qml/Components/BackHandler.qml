import QtQuick
import TrainingAppClient

// Не-визуальный обработчик системной кнопки/жеста «назад» (этап M1):
// на Android перехватывается Key_Back, на десктопе — Esc для ручной
// проверки навигации. Обработчики упорядочены по priority: срабатывает
// включённый обработчик с наибольшим приоритетом (реестр — в BackRouter).
// Если активных обработчиков нет, нажатие не перехватывается и система
// выполняет действие по умолчанию (на Android — выход из приложения).
//
// Приоритеты (§5.2, §5.3): панель дизайн-ревью — 20, стек деталей — 10;
// оверлеи записи тренировки и таймера отдыха (D2) добавят свои значения
// выше этих.
Item {
    id: root

    // Активен = именно этот обработчик получит нажатие. Shortcut включён
    // только у активного, чтобы одинаковые последовательности у нескольких
    // обработчиков не становились неоднозначными.
    readonly property bool isActive: BackRouter.revision >= 0 && BackRouter.activeHandler() === root
    property int priority: 0

    signal backPerformed

    Component.onCompleted: BackRouter.register(root)
    Component.onDestruction: BackRouter.unregister(root)
    onEnabledChanged: BackRouter.touch()
    onPriorityChanged: BackRouter.touch()

    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: root.enabled && root.isActive
        sequence: "Back"

        onActivated: BackRouter.dispatchBack()
    }

    // Десктоп-дубликат: Esc закрывает панель/стек так же, как Back на Android.
    Shortcut {
        context: Qt.ApplicationShortcut
        enabled: root.enabled && root.isActive
        sequence: "Esc"

        onActivated: BackRouter.dispatchBack()
    }
}
