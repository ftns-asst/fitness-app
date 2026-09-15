import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Каркас приложения: 4 вкладки (StackLayout) + стек деталей + панель дизайн-ревью.
// Оверлеи записи тренировки и таймера отдыха добавляются на этапе D2 — они будут
// братями StackView и будут скрывать tab bar (§5.2, §5.3).
Item {
    id: shell

    // Отступы под safe area / status bar mock передаёт Main.qml.
    property real topInset: 0
    property real bottomInset: 0
    property int initialTab: 0

    readonly property int currentTab: tabBar.currentIndex

    Component.onCompleted: {
        if (shell.initialTab > 0 && shell.initialTab < 4) {
            shell.showTab(shell.initialTab);
        }
    }

    function showTab(index) {
        if (pages.depth > 1) {
            pages.pop(null);
        }

        tabBar.currentIndex = index;
    }

    StackView {
        id: pages

        anchors.fill: parent
        initialItem: tabsView
    }

    BackHandler {
        priority: 20
        enabled: devPanel.visible

        onBackPerformed: devPanel.close()
    }

    BackHandler {
        priority: 10
        enabled: pages.depth > 1

        onBackPerformed: pages.pop()
    }

    Component {
        id: tabsView

        StackLayout {
            currentIndex: tabBar.currentIndex

            TodayPage {
                topInset: shell.topInset
                bottomInset: shell.bottomInset + Theme.tabBarHeight
            }

            PlansPage {
                topInset: shell.topInset
                bottomInset: shell.bottomInset + Theme.tabBarHeight
            }

            ExercisesPage {
                topInset: shell.topInset
                bottomInset: shell.bottomInset + Theme.tabBarHeight
            }

            ProfilePage {
                topInset: shell.topInset
                bottomInset: shell.bottomInset + Theme.tabBarHeight
            }
        }
    }

    BottomTabBar {
        id: tabBar

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        bottomInset: shell.bottomInset
        visible: pages.depth === 1
    }

    Button {
        id: devButton

        anchors.top: parent.top
        anchors.right: parent.right
        anchors.topMargin: shell.topInset + Spacing.safeGap
        anchors.rightMargin: Spacing.screenPadding
        width: 80
        height: Theme.touchMin
        visible: pages.depth === 1

        background: Rectangle {
            radius: Theme.radiusLg
            color: Theme.surface
            border.width: Theme.borderWidth
            border.color: Theme.border
        }

        contentItem: Item {
            Row {
                anchors.centerIn: parent
                spacing: 6

                AppIcon {
                    width: 20
                    height: 20
                    name: "settings"
                    iconColor: Theme.accent
                }

                Label {
                    height: 20
                    text: qsTr("Dev")
                    font: Typography.captionStrong
                    color: Theme.accent
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        onClicked: devPanel.open()
    }

    // — Временное сообщение о ещё не реализованном экране —
    Rectangle {
        id: toast

        anchors.horizontalCenter: parent.horizontalCenter
        anchors.bottom: tabBar.top
        anchors.bottomMargin: Spacing.sectionGap
        width: Math.min(parent.width - Spacing.screenPadding * 2, toastLabel.implicitWidth + Spacing.screenPadding * 2)

        height: toastLabel.implicitHeight + Spacing.itemGap * 2
        radius: Theme.radiusLg
        color: Theme.textPrimary
        opacity: 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation {
                duration: 180
            }
        }

        Label {
            id: toastLabel

            anchors.centerIn: parent
            width: Math.min(implicitWidth, parent.width - Spacing.screenPadding * 2)
            text: ""
            font: Typography.captionStrong
            color: Theme.background
            horizontalAlignment: Text.AlignHCenter
            wrapMode: Text.WordWrap
        }

        Timer {
            id: toastTimer

            interval: 2200

            onTriggered: toast.opacity = 0
        }
    }

    Connections {
        target: Demo

        function onToastRequested(message) {
            toastLabel.text = message;
            toast.opacity = 0.94;
            toastTimer.restart();
        }
    }

    DevPanel {
        id: devPanel

        onScreenRequested: function (index) {
            shell.showTab(index);
        }
    }
}
