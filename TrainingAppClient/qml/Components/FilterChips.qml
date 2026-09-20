import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Ряд chips (§4.5): горизонтально прокручиваемые pill-кнопки
// фильтров и сортировки; активная — accent fill, неактивные — surface + border.
// currentIndex управляется внешней страницей, поэтому внутри не переопределяется.
Item {
    id: chips

    property int currentIndex: 0
    property var items: []

    signal selected(int index)

    implicitHeight: Theme.touchMin
    implicitWidth: 320

    Flickable {
        id: chipsFlick

        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        contentHeight: height
        contentWidth: chipsRow.width + Spacing.screenPadding

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
                        border.color: Theme.border
                        border.width: chip.active ? 0 : Theme.borderWidth
                        color: chip.active ? Theme.accent : Theme.surface
                        height: 36
                        radius: Theme.radiusLg
                        width: parent.width

                        Behavior on color {
                            ColorAnimation {
                                duration: 140
                            }
                        }
                    }
                    contentItem: Label {
                        id: chipLabel

                        color: chip.active ? Theme.accentForeground : Theme.textSecondary
                        font: Typography.captionStrong
                        horizontalAlignment: Text.AlignHCenter
                        text: modelData
                        verticalAlignment: Text.AlignVCenter
                    }

                    onClicked: chips.selected(index)
                }
            }
        }
    }

    // Подсказка прокручиваемости ряда: правый fade, пока есть контент за краем.
    Rectangle {
        anchors.bottom: parent.bottom
        anchors.right: parent.right
        anchors.top: parent.top
        visible: chipsFlick.contentWidth > chipsFlick.width && chipsFlick.contentX + chipsFlick.width < chipsFlick.contentWidth - 1
        width: 24

        gradient: Gradient {
            GradientStop {
                color: "transparent"
                position: 0.0
            }
            GradientStop {
                color: Theme.background
                position: 1.0
            }
        }
    }
}
