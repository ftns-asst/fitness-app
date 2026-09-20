import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Нижняя навигация (§4.4): 4 вкладки, pill вокруг иконки+подписи
// у активной вкладки, safe area снизу.
Item {
    id: bar

    property real bottomInset: 0
    property int currentIndex: 0
    readonly property var iconNames: ["today", "plans", "exercises", "profile"]
    readonly property var titles: [qsTr("Сегодня"), qsTr("Планы"), qsTr("Упражнения"), qsTr("Профиль")]

    signal tabSelected(int index)

    implicitHeight: Theme.tabBarHeight + bottomInset
    implicitWidth: 320

    Rectangle {
        anchors.fill: parent
        color: Theme.surface
    }
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: Theme.border
        height: Theme.borderWidth
    }
    Row {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        height: Theme.tabBarHeight

        Repeater {
            model: 4

            delegate: Button {
                id: tabButton

                readonly property bool active: bar.currentIndex === index

                height: Theme.tabBarHeight
                width: bar.width / 4

                background: Item {
                    Rectangle {
                        anchors.centerIn: parent
                        color: Theme.accentTint
                        height: 52
                        opacity: tabButton.active ? 1.0 : 0.0
                        radius: Theme.radiusLg
                        scale: tabButton.active ? 1.0 : 0.85
                        visible: tabButton.active
                        width: parent.width - 24

                        Behavior on opacity {
                            NumberAnimation {
                                duration: 160
                            }
                        }
                        Behavior on scale {
                            NumberAnimation {
                                duration: 160
                                easing.type: Easing.OutCubic
                            }
                        }
                    }
                }
                contentItem: Item {
                    Column {
                        anchors.centerIn: parent
                        spacing: 3

                        // Ширина подписи ограничена шириной вкладки: на узких
                        // экранах «Упражнения» иначе вылезает за соседние вкладки.
                        width: parent.width

                        AppIcon {
                            anchors.horizontalCenter: parent.horizontalCenter
                            iconColor: tabButton.active ? Theme.accent : Theme.textMuted
                            name: bar.iconNames[index]
                        }
                        Label {
                            color: tabButton.active ? Theme.accent : Theme.textMuted
                            elide: Text.ElideRight
                            font: Typography.tabLabel
                            horizontalAlignment: Text.AlignHCenter
                            text: bar.titles[index]
                            width: parent.width
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
