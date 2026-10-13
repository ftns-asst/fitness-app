import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Плитка пресета в витрине каталога правил (макет `08-program-rules`, `.preset`).
// Тап по всей плитке добавляет правило: кнопка «Добавить» в каждой строке
// превращала шесть пресетов в список на весь экран, а выбор из каталога — это
// быстрый путь, а не единственный.
//
// Добавленный пресет остаётся в витрине приглушённым: он отвечает на вопрос
// «это я уже взял?», а не предлагает действие, которое модель отвергнет.
// Пунктирной рамки из макета здесь нет: у `Rectangle` нет стиля рамки
// (`border-style` в CSS), а рисовать пунктир линиями дороже, чем сказать то же
// цветом фона и текста.
Item {
    id: tile

    // Пара «подпись категории + её цвет точки» приходит от страницы: словарь
    // категорий живёт там же, где фильтр, — иначе каталог и фильтр называли бы
    // одну категорию по-разному.
    property color categoryColor: "transparent"
    property string categoryTitle: ""
    property bool isAdded: false
    property string title: ""

    signal clicked

    readonly property bool canAdd: tile.isAdded === false
    readonly property int contentGap: 3
    readonly property int minHeight: 44
    readonly property int paddingH: 12
    readonly property int paddingV: 7

    implicitHeight: Math.max(tile.minHeight, content.implicitHeight + tile.paddingV * 2)
    implicitWidth: 150

    Rectangle {
        anchors.fill: parent
        border.color: tile.isAdded ? Theme.borderSubtle : Theme.border
        border.width: Theme.borderWidth
        color: tile.isAdded ? Theme.surfaceMuted : Theme.surface
        radius: Theme.radiusMd

        Behavior on color {
            ColorAnimation {
                duration: Theme.pressFeedbackDuration
                easing.type: Easing.OutCubic
            }
        }
    }
    Column {
        id: content

        anchors.left: parent.left
        anchors.leftMargin: tile.paddingH
        anchors.right: parent.right
        anchors.rightMargin: tile.paddingH
        anchors.verticalCenter: parent.verticalCenter
        spacing: tile.contentGap

        Label {
            color: tile.isAdded ? Theme.textSecondary : Theme.textPrimary
            font: Typography.captionStrong
            text: tile.title
            width: parent.width
            wrapMode: Text.WordWrap
        }
        Row {
            spacing: 5
            width: parent.width

            // Точка категории — цветом, а не словом: в сетке из шести плиток
            // подпись категории читается фоном быстрее, чем текстом.
            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                color: tile.categoryColor
                height: 8
                radius: 4
                width: 8
            }
            Label {
                color: Theme.textMuted
                font: Typography.caption
                text: tile.isAdded ? qsTr("добавлено") : tile.categoryTitle
            }
        }
    }
    TapHandler {
        enabled: tile.canAdd

        onTapped: tile.clicked()
    }
}
