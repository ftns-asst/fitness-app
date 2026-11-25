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
    // Multi-режим: массив активных индексов; null — обычный одиночный выбор
    // по currentIndex. Нажатие по-прежнему приходит сигналом selected(index),
    // а набор ведёт владелец.
    property var selectedIndices: null
    property bool keepCurrentVisible: false
    property bool fadeEdge: true
    property real maximumItemWidth: 0

    signal selected(int index)

    implicitHeight: Theme.touchMin
    implicitWidth: 320

    onCurrentIndexChanged: {
        if (keepCurrentVisible)
            Qt.callLater(revealCurrent);
    }

    function revealCurrent() {
        const item = delegates.itemAt(currentIndex);
        if (!item)
            return;
        if (item.x < chipsFlick.contentX)
            chipsFlick.contentX = item.x;
        else if (item.x + item.width > chipsFlick.contentX + chipsFlick.width)
            chipsFlick.contentX = item.x + item.width - chipsFlick.width;
    }

    Flickable {
        id: chipsFlick

        anchors.fill: parent
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        contentHeight: height
        contentWidth: chipsRow.width + Spacing.screenPadding
        flickableDirection: Flickable.HorizontalFlick

        Row {
            id: chipsRow

            spacing: Spacing.itemGap

            Repeater {
                id: delegates
                model: chips.items

                delegate: Button {
                    id: chip

                    readonly property bool active: chips.selectedIndices !== null ? chips.selectedIndices.indexOf(index) >= 0 : chips.currentIndex === index

                    // Hit-area — Theme.touchMin, визуальная pill остаётся 36 dp.
                    hoverEnabled: !Theme.touchPlatform
                    height: chips.height
                    width: chips.maximumItemWidth > 0 ? Math.min(chips.maximumItemWidth, chipLabel.implicitWidth + Spacing.screenPadding * 2) : chipLabel.implicitWidth + Spacing.screenPadding * 2
                    Accessible.name: chipLabel.text

                    background: Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        border.color: Theme.border
                        border.width: chip.active ? 0 : Theme.borderWidth
                        color: chip.active ? (chip.down ? Theme.accentPressed : Theme.accent) : chip.down || chip.hovered ? Theme.surfaceMuted : Theme.surface
                        height: 36
                        radius: Theme.radiusLg
                        width: parent.width

                        Behavior on color {
                            ColorAnimation {
                                duration: Theme.pressFeedbackDuration
                                easing.type: Easing.OutCubic
                            }
                        }
                    }
                    contentItem: Label {
                        id: chipLabel

                        color: chip.active ? Theme.accentForeground : Theme.textSecondary

                        Behavior on color {
                            ColorAnimation {
                                duration: Theme.pressFeedbackDuration
                                easing.type: Easing.OutCubic
                            }
                        }
                        font: Typography.captionStrong
                        elide: Text.ElideRight
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
        visible: chips.fadeEdge && chipsFlick.contentWidth > chipsFlick.width && chipsFlick.contentX + chipsFlick.width < chipsFlick.contentWidth - 1
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
