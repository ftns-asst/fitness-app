import QtQuick
import TrainingAppClient

// Оверлей счётчика кадров: отвечает на вопрос «почему скролл не гладкий»
// числами прямо на экране телефона — без логов и adb.
//
// Включается настройкой `Theme.fpsCounter` («Профиль → Параметры») рядом с
// флагом запуска --fps, который пишет числа в лог. Измерение заставляет окно
// рисовать непрерывно (FrameAnimation), поэтому оверлей выключен по умолчанию
// и показывается только пока настройка включена. Нажатия не перехватывает —
// диагностическая плашка не должна мешать работать со страницей.
//
// Рядом с фактической частотой показывается частота панели, о которой
// знает окно (`Screen.refreshRate`): если панель 90/120 Гц, а счётчик
// показывает вдвое меньше — окно не попало в высокий режим (запрос
// `preferredDisplayModeId` из `main.cpp` не сработал либо система
// экономит батарею), и «негладкость» объясняется герцовкой.
Rectangle {
    id: overlay

    // Читается тестами: работает ли измерение при данном состоянии настройки.
    readonly property alias meterRunning: meter.running
    // Строка вида «59.8 fps · 120 Гц».
    readonly property alias fpsText: label.text

    // Подложка тёмная и полупрозрачная: плашка лежит поверх контента любого
    // цвета, и белый текст обязан читаться (дефолтный белый Rectangle с белым
    // текстом читался как сплошное белое пятно).
    color: "#CC101420"
    height: label.implicitHeight + 12
    radius: height / 2
    visible: Theme.fpsCounter
    width: label.implicitWidth + 20
    z: 1000

    FrameRateMeter {
        id: meter

        running: overlay.visible
    }
    Text {
        id: label

        anchors.centerIn: parent
        color: "#FFFFFF"
        font.pixelSize: 13
        text: "… fps"
    }
    Timer {
        interval: 500
        repeat: true
        running: overlay.visible

        onTriggered: label.text = meter.framesPerSecond.toFixed(1) + " fps · " + Math.round(overlay.Screen.refreshRate) + " Гц"
    }
}
