import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Нижняя навигация (§4.4): 4 вкладки, pill вокруг иконки+подписи
// у активной вкладки, safe area снизу.
Item {
    id: bar

    implicitWidth: 320
    implicitHeight: Theme.tabBarHeight + bottomInset

    property int currentIndex: 0
    property real bottomInset: 0

    signal tabSelected(int index)

    readonly property var titles: [qsTr("Сегодня"), qsTr("Планы"), qsTr("Упражнения"), qsTr("Профиль")]
    readonly property var iconNames: ["today", "plans", "exercises", "profile"]

    Rectangle {
        anchors.fill: parent
        color: Theme.surface
    }

    Rectangle {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.borderWidth
        color: Theme.border
    }

    Row {
        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        height: Theme.tabBarHeight

        Repeater {
            model: 4

            delegate: Button {
                id: tabButton

                readonly property bool active: bar.currentIndex === index

                width: bar.width / 4
                height: Theme.tabBarHeight

                background: Item {
                    Rectangle {
                        anchors.centerIn: parent
                        width: parent.width - 24
                        height: 52
                        radius: Theme.radiusLg
                        color: Theme.accentTint
                        visible: tabButton.active
                        scale: tabButton.active ? 1.0 : 0.85
                        opacity: tabButton.active ? 1.0 : 0.0

                        Behavior on scale {
                            NumberAnimation {
                                duration: 160
                                easing.type: Easing.OutCubic
                            }
                        }

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 160
                            }
                        }
                    }
                }

                contentItem: Item {
                    Column {
                        anchors.centerIn: parent

                        // Ширина подписи ограничена шириной вкладки: на узких
                        // экранах «Упражнения» иначе вылезает за соседние вкладки.
                        width: parent.width
                        spacing: 3

                        AppIcon {
                            anchors.horizontalCenter: parent.horizontalCenter
                            name: bar.iconNames[index]
                            iconColor: tabButton.active ? Theme.accent : Theme.textMuted
                        }

                        Label {
                            width: parent.width
                            text: bar.titles[index]
                            font: Typography.tabLabel
                            color: tabButton.active ? Theme.accent : Theme.textMuted
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                    }
                }

                onClicked: {
                    bar.currentIndex = index;
                    bar.tabSelected(index);
                }
            }
        }
    }
}
