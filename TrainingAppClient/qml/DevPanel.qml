import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Панель дизайн-ревью (§4.11): акцент, тема, рамка телефона, переходы по экранам.
// Не является частью production UI.
Drawer {
    id: panel

    edge: Qt.BottomEdge
    width: parent === null ? 0 : parent.width
    height: Math.min(480, (parent === null ? 480 : parent.height) * 0.85)
    modal: false
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside

    signal screenRequested(int index)

    background: Rectangle {
        color: Theme.surface
        radius: Theme.radiusLg
        border.width: Theme.borderWidth
        border.color: Theme.border

        // Скруглены только верхние углы: нижняя часть прижата к краю экрана.
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            height: parent.radius
            color: parent.color
        }
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: Spacing.screenPadding
        contentWidth: width
        contentHeight: contentColumn.implicitHeight
        bottomMargin: Spacing.sectionGap
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        Column {
            id: contentColumn

            width: parent.width
            spacing: Spacing.listGap

            Label {
                text: qsTr("Дизайн-ревью")
                font: Typography.overline
                color: Theme.textMuted
            }

            Label {
                text: qsTr("Акцент: ") + Theme.accentNames[Theme.accentIndex]
                font: Typography.caption
                color: Theme.textSecondary
            }

            // Flow: круги акцента переносятся, если панель узкая.
            Flow {
                width: parent.width
                spacing: Spacing.listGap

                Repeater {
                    model: Theme.accentPalette

                    delegate: Rectangle {
                        width: Theme.touchMin
                        height: Theme.touchMin
                        radius: width / 2
                        color: modelData
                        border.width: Theme.accentIndex === index ? 3 : 0
                        border.color: Theme.textPrimary

                        MouseArea {
                            anchors.fill: parent
                            onClicked: Theme.accentIndex = index
                        }
                    }
                }
            }

            Label {
                text: qsTr("Тема")
                font: Typography.caption
                color: Theme.textSecondary
            }

            // Flow: кнопки темы переносятся на узкой панели, ширина адаптивная.
            Flow {
                width: parent.width
                spacing: Spacing.itemGap

                Repeater {
                    model: [qsTr("Светлая"), qsTr("Тёмная")]

                    delegate: Button {
                        id: themeChip

                        readonly property bool active: Theme.darkMode === (index === 1)

                        width: Math.min(160, (panel.width - Spacing.screenPadding * 2 - Spacing.itemGap) / 2)
                        height: Theme.buttonSecondary

                        background: Rectangle {
                            radius: Theme.radiusMd
                            color: themeChip.active ? Theme.accent : "transparent"
                            border.width: themeChip.active ? 0 : Theme.borderWidth
                            border.color: Theme.border

                            Behavior on color {
                                ColorAnimation {
                                    duration: 140
                                }
                            }
                        }

                        contentItem: Label {
                            text: modelData
                            font: Typography.captionStrong
                            color: themeChip.active ? Theme.accentForeground : Theme.textSecondary
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: Theme.darkMode = (index === 1)
                    }
                }
            }

            Item {
                width: parent.width
                height: Theme.touchMin

                Label {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    text: qsTr("Рамка телефона 390×812")
                    font: Typography.body
                    color: Theme.textPrimary
                }

                Switch {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    checked: Demo.phoneFrame

                    onToggled: Demo.phoneFrame = checked
                }
            }

            Label {
                text: qsTr("Экраны")
                font: Typography.caption
                color: Theme.textSecondary
            }

            Grid {
                width: parent.width
                columns: 2
                columnSpacing: Spacing.itemGap
                rowSpacing: Spacing.itemGap

                Repeater {
                    model: [qsTr("Сегодня"), qsTr("Планы"), qsTr("Упражнения"), qsTr("Профиль")]

                    delegate: AppButton {
                        variant: "secondary"
                        text: modelData
                        width: (panel.width - Spacing.screenPadding * 2 - Spacing.itemGap) / 2

                        onClicked: {
                            panel.screenRequested(index);
                            panel.close();
                        }
                    }
                }
            }

            Label {
                width: parent.width
                text: qsTr("Панель — инструмент дизайн-ревью, не часть production UI. " + "Запуск в рамке телефона из командной строки: --frame")
                font: Typography.caption
                color: Theme.textMuted
                wrapMode: Text.WordWrap
            }
        }
    }
}
