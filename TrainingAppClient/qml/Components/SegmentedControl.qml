import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Сегментированный контрол (§4.5): активный сегмент — pill fill accent, белый текст.
Item {
    id: control

    implicitWidth: 300
    implicitHeight: Theme.touchMin

    property var items: []
    property int currentIndex: 0

    signal selected(int index)

    Rectangle {
        anchors.fill: parent
        radius: Theme.radiusLg
        color: Theme.surfaceMuted
    }

    Row {
        anchors.fill: parent
        anchors.margins: 4
        spacing: 4

        Repeater {
            model: control.items

            delegate: Button {
                id: segment

                readonly property bool active: control.currentIndex === index

                width: (control.width - 8 - Math.max(0, control.items.length - 1) * 4) / Math.max(1, control.items.length)
                height: control.height - 8

                background: Rectangle {
                    radius: Theme.radiusLg - 4
                    color: segment.active ? Theme.accent : "transparent"

                    Behavior on color {
                        ColorAnimation {
                            duration: 140
                        }
                    }
                }

                contentItem: Label {
                    text: modelData
                    font: Typography.captionStrong
                    color: segment.active ? Theme.accentForeground : Theme.textMuted
                    horizontalAlignment: Text.AlignHCenter
                    verticalAlignment: Text.AlignVCenter
                    elide: Text.ElideRight
                }

                onClicked: {
                    control.currentIndex = index;
                    control.selected(index);
                }
            }
        }
    }
}
