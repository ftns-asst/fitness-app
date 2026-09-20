import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Панель дизайн-ревью (§4.11): акцент, тема, рамка телефона, переходы по экранам.
// Не является частью production UI.
Drawer {
    id: panel

    signal screenRequested(int index)

    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    edge: Qt.BottomEdge
    height: Math.min(480, (parent === null ? 480 : parent.height) * 0.85)
    modal: false
    width: parent === null ? 0 : parent.width

    background: Rectangle {
        border.color: Theme.border
        border.width: Theme.borderWidth
        color: Theme.surface
        radius: Theme.radiusLg

        // Скруглены только верхние углы: нижняя часть прижата к краю экрана.
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            color: parent.color
            height: parent.radius
        }
    }

    Flickable {
        anchors.fill: parent
        anchors.margins: Spacing.screenPadding
        bottomMargin: Spacing.sectionGap
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        contentHeight: contentColumn.implicitHeight
        contentWidth: width

        Column {
            id: contentColumn

            spacing: Spacing.listGap
            width: parent.width

            Label {
                color: Theme.textMuted
                font: Typography.overline
                text: qsTr("Дизайн-ревью")
            }
            Label {
                color: Theme.textSecondary
                font: Typography.caption
                text: qsTr("Акцент: ") + Theme.accentNames[Theme.accentIndex]
            }

            // Flow: круги акцента переносятся, если панель узкая.
            Flow {
                spacing: Spacing.listGap
                width: parent.width

                Repeater {
                    model: Theme.accentPalette

                    delegate: Rectangle {
                        border.color: Theme.textPrimary
                        border.width: Theme.accentIndex === index ? 3 : 0
                        color: modelData
                        height: Theme.touchMin
                        radius: width / 2
                        width: Theme.touchMin

                        MouseArea {
                            anchors.fill: parent

                            onClicked: Theme.accentIndex = index
                        }
                    }
                }
            }
            Label {
                color: Theme.textSecondary
                font: Typography.caption
                text: qsTr("Тема")
            }

            // Flow: кнопки темы переносятся на узкой панели, ширина адаптивная.
            Flow {
                spacing: Spacing.itemGap
                width: parent.width

                Repeater {
                    model: [qsTr("Светлая"), qsTr("Тёмная")]

                    delegate: Button {
                        id: themeChip

                        readonly property bool active: Theme.darkMode === (index === 1)

                        height: Theme.buttonSecondary
                        width: Math.min(160, (panel.width - Spacing.screenPadding * 2 - Spacing.itemGap) / 2)

                        background: Rectangle {
                            border.color: Theme.border
                            border.width: themeChip.active ? 0 : Theme.borderWidth
                            color: themeChip.active ? Theme.accent : "transparent"
                            radius: Theme.radiusMd

                            Behavior on color {
                                ColorAnimation {
                                    duration: 140
                                }
                            }
                        }
                        contentItem: Label {
                            color: themeChip.active ? Theme.accentForeground : Theme.textSecondary
                            font: Typography.captionStrong
                            horizontalAlignment: Text.AlignHCenter
                            text: modelData
                            verticalAlignment: Text.AlignVCenter
                        }

                        onClicked: Theme.darkMode = (index === 1)
                    }
                }
            }
            Item {
                height: Theme.touchMin
                width: parent.width

                Label {
                    anchors.left: parent.left
                    anchors.verticalCenter: parent.verticalCenter
                    color: Theme.textPrimary
                    font: Typography.body
                    text: qsTr("Рамка телефона 390×812")
                }
                Switch {
                    anchors.right: parent.right
                    anchors.verticalCenter: parent.verticalCenter
                    checked: Demo.phoneFrame

                    onToggled: Demo.phoneFrame = checked
                }
            }
            Label {
                color: Theme.textSecondary
                font: Typography.caption
                text: qsTr("Экраны")
            }
            Grid {
                columnSpacing: Spacing.itemGap
                columns: 2
                rowSpacing: Spacing.itemGap
                width: parent.width

                Repeater {
                    model: [qsTr("Сегодня"), qsTr("Планы"), qsTr("Упражнения"), qsTr("Профиль")]

                    delegate: AppButton {
                        text: modelData
                        variant: AppButton.Secondary
                        width: (panel.width - Spacing.screenPadding * 2 - Spacing.itemGap) / 2

                        onClicked: {
                            panel.screenRequested(index);
                            panel.close();
                        }
                    }
                }
            }
            Label {
                color: Theme.textMuted
                font: Typography.caption
                text: qsTr("Панель — инструмент дизайн-ревью, не часть production UI. " + "Запуск в рамке телефона из командной строки: --frame")
                width: parent.width
                wrapMode: Text.WordWrap
            }
        }
    }
}
