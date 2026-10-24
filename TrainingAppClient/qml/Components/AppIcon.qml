import QtQuick
import TrainingAppClient

// Линейные иконки (§3.6). Рисуются примитивами, поэтому мгновенно перекрашиваются
// вместе с темой и не требуют растровых ассетов.
Item {
    id: icon

    property color iconColor: Theme.textSecondary

    // today | plans | exercises | profile | settings
    property string name: "today"
    property real strokeWidth: 2

    height: 24
    width: 24

    // — Сегодня: календарь —
    Item {
        anchors.fill: parent
        visible: icon.name === "today"

        Rectangle {
            anchors.fill: parent
            anchors.margins: 3
            border.color: icon.iconColor
            border.width: icon.strokeWidth
            color: "transparent"
            radius: 4
        }
        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 3
            anchors.right: parent.right
            anchors.rightMargin: 3
            anchors.top: parent.top
            anchors.topMargin: 8
            color: icon.iconColor
            height: icon.strokeWidth
        }
        Rectangle {
            anchors.left: parent.left
            anchors.leftMargin: 7
            anchors.top: parent.top
            color: icon.iconColor
            height: 5
            radius: width / 2
            width: icon.strokeWidth
        }
        Rectangle {
            anchors.right: parent.right
            anchors.rightMargin: 7
            anchors.top: parent.top
            color: icon.iconColor
            height: 5
            radius: width / 2
            width: icon.strokeWidth
        }
    }

    // — Планы: список —
    Item {
        anchors.fill: parent
        visible: icon.name === "plans"

        Column {
            anchors.left: parent.left
            anchors.leftMargin: 3
            anchors.right: parent.right
            anchors.rightMargin: 3
            anchors.verticalCenter: parent.verticalCenter
            spacing: 5

            Repeater {
                model: 3

                Rectangle {
                    color: icon.iconColor
                    height: icon.strokeWidth
                    opacity: 1.0 - index * 0.2
                    radius: height / 2
                    width: parent.width
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
            color: icon.iconColor
            height: icon.strokeWidth
            radius: height / 2
            width: parent.width - 6
        }
        Rectangle {
            anchors.left: parent.left
            anchors.verticalCenter: parent.verticalCenter
            color: icon.iconColor
            height: 14
            radius: 2
            width: 4
        }
        Rectangle {
            anchors.right: parent.right
            anchors.verticalCenter: parent.verticalCenter
            color: icon.iconColor
            height: 14
            radius: 2
            width: 4
        }
    }

    // — Профиль: человек —
    Item {
        anchors.fill: parent
        visible: icon.name === "profile"

        Rectangle {
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.top: parent.top
            anchors.topMargin: 3
            border.color: icon.iconColor
            border.width: icon.strokeWidth
            color: "transparent"
            height: 9
            radius: width / 2
            width: 9
        }
        Item {
            anchors.bottom: parent.bottom
            anchors.bottomMargin: 3
            anchors.horizontalCenter: parent.horizontalCenter
            clip: true
            height: 7
            width: 16

            Rectangle {
                border.color: icon.iconColor
                border.width: icon.strokeWidth
                color: "transparent"
                height: 14
                radius: 7
                width: 16
            }
        }
    }

    // — Настройки: слайдеры —
    Item {
        anchors.fill: parent
        visible: icon.name === "settings"

        Column {
            anchors.left: parent.left
            anchors.leftMargin: 4
            anchors.right: parent.right
            anchors.rightMargin: 4
            anchors.verticalCenter: parent.verticalCenter
            spacing: 6

            Repeater {
                model: 3

                Item {
                    height: 6
                    width: parent.width

                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        color: icon.iconColor
                        height: icon.strokeWidth
                        radius: height / 2
                        width: parent.width
                    }
                    Rectangle {
                        anchors.verticalCenter: parent.verticalCenter
                        color: icon.iconColor
                        height: 6
                        radius: 3
                        width: 6

                        // Ограничиваем справа: иначе крайний регулятор выезжает
                        // за границы иконки.
                        x: Math.min(parent.width - width, parent.width * (index === 0 ? 0.2 : (index === 1 ? 0.65 : 0.4)))
                    }
                }
            }
        }
    }
}
