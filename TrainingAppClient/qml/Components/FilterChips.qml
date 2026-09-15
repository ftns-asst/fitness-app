import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Ряд chips (§4.5): горизонтально прокручиваемые pill-кнопки
// фильтров и сортировки; активная — accent fill, неактивные — surface + border.
// currentIndex управляется внешней страницей, поэтому внутри не переопределяется.
Item {
    id: chips

    implicitWidth: 320
    implicitHeight: Theme.touchMin

    property var items: []
    property int currentIndex: 0

    signal selected(int index)

    Flickable {
        id: chipsFlick

        anchors.fill: parent
        contentWidth: chipsRow.width + Spacing.screenPadding
        contentHeight: height
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Row {
            id: chipsRow

            spacing: Spacing.itemGap

            Repeater {
                model: chips.items

                delegate: Button {
                    id: chip

                    readonly property bool active: chips.currentIndex === index

                    // Hit-area — Theme.touchMin, визуальная pill остаётся 36 dp.
                    height: chips.height
                    width: chipLabel.implicitWidth + Spacing.screenPadding * 2

                    background: Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        height: 36
                        radius: Theme.radiusLg
                        color: chip.active ? Theme.accent : Theme.surface
                        border.width: chip.active ? 0 : Theme.borderWidth
                        border.color: Theme.border

                        Behavior on color {
                            ColorAnimation {
                                duration: 140
                            }
                        }
                    }

                    contentItem: Label {
                        id: chipLabel

                        text: modelData
                        font: Typography.captionStrong
                        color: chip.active ? Theme.accentForeground : Theme.textSecondary
                        horizontalAlignment: Text.AlignHCenter
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: chips.selected(index)
                }
            }
        }
    }

    // Подсказка прокручиваемости ряда: правый fade, пока есть контент за краем.
    Rectangle {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.bottom: parent.bottom
        width: 24
        visible: chipsFlick.contentWidth > chipsFlick.width && chipsFlick.contentX + chipsFlick.width < chipsFlick.contentWidth - 1
        gradient: Gradient {
            GradientStop {
                position: 0.0
                color: "transparent"
            }
            GradientStop {
                position: 1.0
                color: Theme.background
            }
        }
    }
}
