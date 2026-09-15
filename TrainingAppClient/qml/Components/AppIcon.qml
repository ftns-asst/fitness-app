import QtQuick
import TrainingAppClient

// Линейные иконки (§3.6). Рисуются примитивами, поэтому мгновенно перекрашиваются
// вместе с темой и не требуют растровых ассетов.
Item {
    id: icon

    width: 24
    height: 24

    // today | plans | exercises | profile | settings
    property string name: "today"
    property color iconColor: Theme.textSecondary
    property real strokeWidth: 2

    // — Сегодня: календарь —
    Item {
        anchors.fill: parent
        visible: icon.name === "today"

        Rectangle {
            anchors.fill: parent
            anchors.margins: 3
            radius: 4
            color: "transparent"
            border.width: icon.strokeWidth
            border.color: icon.iconColor
        }

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.topMargin: 8
            anchors.leftMargin: 3
            anchors.rightMargin: 3
            height: icon.strokeWidth
            color: icon.iconColor
        }

        Rectangle {
            anchors.top: parent.top
            anchors.left: parent.left
            anchors.leftMargin: 7
            width: icon.strokeWidth
            height: 5
            radius: width / 2
            color: icon.iconColor
        }

        Rectangle {
            anchors.top: parent.top
            anchors.right: parent.right
            anchors.rightMargin: 7
            width: icon.strokeWidth
            height: 5
            radius: width / 2
            color: icon.iconColor
        }
    }

    // — Планы: список —
    Item {
        anchors.fill: parent
        visible: icon.name === "plans"

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 3
            anchors.rightMargin: 3
            spacing: 5

            Repeater {
                model: 3

                Rectangle {
                    width: parent.width
                    height: icon.strokeWidth
                    radius: height / 2
                    color: icon.iconColor
                    opacity: 1.0 - index * 0.2
                }
            }
        }
    }

    // — Упражнения: гантель —
    Item {
        anchors.fill: parent
        visible: icon.name === "exercises"

        Rectangle {
            anchors.centerIn: parent
            width: parent.width - 6
            height: icon.strokeWidth
            radius: height / 2
            color: icon.iconColor
        }

        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            width: 4
            height: 14
            radius: 2
            color: icon.iconColor
        }

        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            width: 4
            height: 14
            radius: 2
            color: icon.iconColor
        }
    }

    // — Профиль: человек —
    Item {
        anchors.fill: parent
        visible: icon.name === "profile"

        Rectangle {
            anchors.top: parent.top
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.topMargin: 3
            width: 9
            height: 9
            radius: width / 2
            color: "transparent"
            border.width: icon.strokeWidth
            border.color: icon.iconColor
        }

        Item {
            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: 3
            width: 16
            height: 7
            clip: true

            Rectangle {
                width: 16
                height: 14
                radius: 7
                color: "transparent"
                border.width: icon.strokeWidth
                border.color: icon.iconColor
            }
        }
    }

    // — Настройки: слайдеры —
    Item {
        anchors.fill: parent
        visible: icon.name === "settings"

        Column {
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            anchors.leftMargin: 4
            anchors.rightMargin: 4
            spacing: 6

            Repeater {
                model: 3

                Item {
                    width: parent.width
                    height: 6

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        width: parent.width
                        height: icon.strokeWidth
                        radius: height / 2
                        color: icon.iconColor
                    }

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter

                        // Ограничиваем справа: иначе крайний регулятор выезжает
                        // за границы иконки.
                        x: Math.min(parent.width - width, parent.width * (index === 0 ? 0.2 : (index === 1 ? 0.65 : 0.4)))
                        width: 6
                        height: 6
                        radius: 3
                        color: icon.iconColor
                    }
                }
            }
        }
    }
}
