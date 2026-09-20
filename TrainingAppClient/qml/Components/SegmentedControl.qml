import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Сегментированный контрол (§4.5): активный сегмент — pill fill accent, белый текст.
Item {
    id: control

    property int currentIndex: 0
    property var items: []

    signal selected(int index)

    implicitHeight: Theme.touchMin
    implicitWidth: 300

    Rectangle {
        anchors.fill: parent
        color: Theme.surfaceMuted
        radius: Theme.radiusLg
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

                height: control.height - 8
                width: (control.width - 8 - Math.max(0, control.items.length - 1) * 4) / Math.max(1, control.items.length)

                background: Rectangle {
                    color: segment.active ? Theme.accent : "transparent"
                    radius: Theme.radiusLg - 4

                    Behavior on color {
                        ColorAnimation {
                            duration: 140
                        }
                    }
                }
                contentItem: Label {
                    color: segment.active ? Theme.accentForeground : Theme.textMuted
                    elide: Text.ElideRight
                    font: Typography.captionStrong
                    horizontalAlignment: Text.AlignHCenter
                    text: modelData
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    control.currentIndex = index;
                    control.selected(index);
                }
            }
        }
    }
}
