import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Плитка каталога упражнений: картинка техники сверху, название снизу.
// Две плитки в строке — ширину задаёт хост (вкладка «Упражнения», окно выбора).
// Без фото показывается плейсхолдер с первой буквой названия.
Item {
    id: tile

    // URL локального фото техники; пусто — плейсхолдер.
    property string photoUrl: ""
    // Выбранная плитка (окно выбора) подсвечивается акцентной рамкой.
    property bool selected: false
    property string title: ""

    signal clicked

    // Подпись резервирует две строки: плитки в строке и сетке одной высоты.
    implicitHeight: imageBox.height + titleLabel.height + Spacing.listGap * 2 + Spacing.itemGap
    implicitWidth: 150
    // Отклик на касание и мягкое появление плитки при построении списка.
    opacity: 0
    scale: area.pressed ? 0.97 : 1.0

    Behavior on scale {
        NumberAnimation {
            duration: 90
            easing.type: Easing.OutCubic
        }
    }
    Component.onCompleted: opacity = 1
    Behavior on opacity {
        NumberAnimation {
            duration: 220
            easing.type: Easing.OutCubic
        }
    }

    Rectangle {
        anchors.fill: parent
        Behavior on border.color {
            ColorAnimation {
                duration: 140
            }
        }
        border.color: tile.selected ? Theme.accent : Theme.border
        border.width: tile.selected ? 2 : Theme.borderWidth
        color: Theme.surface
        radius: Theme.radiusLg
    }
    // Картинка: 4:3 к ширине плитки, скруглена по верхним углам плитки.
    Rectangle {
        id: imageBox

        anchors.left: parent.left
        anchors.leftMargin: Spacing.listGap
        anchors.right: parent.right
        anchors.rightMargin: Spacing.listGap
        anchors.top: parent.top
        anchors.topMargin: Spacing.listGap
        clip: true
        color: Theme.surfaceMuted
        height: Math.round(width * 1.2)
        radius: Theme.radiusMd

        Image {
            id: photo

            anchors.fill: parent
            asynchronous: true
            fillMode: Image.PreserveAspectCrop
            source: tile.photoUrl
            visible: tile.photoUrl.length > 0 && status === Image.Ready
        }
        Label {
            anchors.centerIn: parent
            color: Theme.textMuted
            font: Typography.screenTitle
            text: tile.title.length > 0 ? tile.title.charAt(0).toUpperCase() : ""
            visible: photo.visible === false
        }
    }
    FontMetrics {
        id: titleMetrics

        font: Typography.bodyStrong
    }
    Label {
        id: titleLabel

        anchors.left: parent.left
        anchors.leftMargin: Spacing.listGap
        anchors.right: parent.right
        anchors.rightMargin: Spacing.listGap
        anchors.top: imageBox.bottom
        anchors.topMargin: Spacing.itemGap
        color: Theme.textPrimary
        elide: Text.ElideRight
        font: Typography.bodyStrong
        height: Math.ceil(titleMetrics.lineSpacing * 2)
        maximumLineCount: 2
        text: tile.title
        wrapMode: Text.WordWrap
    }
    MouseArea {
        id: area

        objectName: "tileArea"
        anchors.fill: parent

        onClicked: tile.clicked()
    }
}
