import QtQuick

// Измеритель частоты кадров: отвечает на вопрос «работают ли 120 Гц».
//
// Qt Quick рисует по vsync, поэтому на 120-герцевом экране кадры идут каждые
// 8,3 мс, на 60-герцевом — 16,7 мс. Проверить это можно только там, где
// приложение запущено: числа уходят в лог (`--fps`), и по ним же видно просадки —
// частота кадров падает раньше, чем это заметно глазом.
//
// Компонент невидим и по умолчанию выключен: измерение заставляет окно рисовать
// непрерывно, а в обычном запуске это только расход батареи.
Item {
    id: meter

    // Число кадров с момента включения измерения.
    property int frameCount: animation.currentFrame
    // Сглаженное время кадра (секунды) из Qt.
    readonly property real frameSeconds: animation.smoothFrameTime
    // Фактическая частота: 120 на 120 Гц, 60 на 60 Гц, меньше — просадки.
    readonly property real framesPerSecond: meter.frameSeconds > 0 ? 1 / meter.frameSeconds : 0
    property bool running: false

    FrameAnimation {
        id: animation

        running: meter.running
    }
}
