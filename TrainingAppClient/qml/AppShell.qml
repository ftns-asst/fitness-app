import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Каркас приложения: 4 вкладки (StackLayout) + стек деталей + панель дизайн-ревью.
// Оверлеи записи тренировки и таймера отдыха добавляются на этапе D2 — они будут
// братями StackView и будут скрывать tab bar (§5.2, §5.3).
Item {
    id: shell

    property var auth: AuthViewModel
    property real bottomInset: 0
    readonly property int currentTab: tabBar.currentIndex
    property var exercisesViewModel: ExercisesViewModel
    property int initialTab: 0
    property var plansViewModel: PlansViewModel
    property var profileViewModel: ProfileViewModel
    property var todayViewModel: TodayViewModel

    // Отступы под safe area / status bar mock передаёт Main.qml.
    property real topInset: 0

    // Запрос на открытие экрана входа из «Профиля» (гость): Main.qml решает,
    // показывать ли экран и куда вернуться после входа.
    signal authPageRequested(int originTab)

    function showTab(index) {
        if (pages.depth > 1) {
            pages.pop(null);
        }

        tabBar.currentIndex = index;
    }

    Component.onCompleted: {
        if (shell.initialTab > 0 && shell.initialTab < 4) {
            shell.showTab(shell.initialTab);
        }
    }

    StackView {
        id: pages

        anchors.fill: parent
        initialItem: tabsView
    }
    BackHandler {
        enabled: devPanel.visible
        priority: 20

        onBackPerformed: devPanel.close()
    }
    BackHandler {
        enabled: pages.depth > 1
        priority: 10

        onBackPerformed: pages.pop()
    }
    Component {
        id: tabsView

        StackLayout {
            currentIndex: tabBar.currentIndex

            TodayPage {
                bottomInset: shell.bottomInset + Theme.tabBarHeight
                topInset: shell.topInset
                viewModel: shell.todayViewModel
            }
            PlansPage {
                bottomInset: shell.bottomInset + Theme.tabBarHeight
                topInset: shell.topInset
                viewModel: shell.plansViewModel
            }
            ExercisesPage {
                bottomInset: shell.bottomInset + Theme.tabBarHeight
                topInset: shell.topInset
                viewModel: shell.exercisesViewModel
            }
            ProfilePage {
                auth: shell.auth
                bottomInset: shell.bottomInset + Theme.tabBarHeight
                topInset: shell.topInset
                viewModel: shell.profileViewModel

                onLoginRequested: shell.authPageRequested(3)
                onLogoutRequested: shell.auth.logout()
            }
        }
    }
    BottomTabBar {
        id: tabBar

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        bottomInset: shell.bottomInset
        visible: pages.depth === 1
    }
    Button {
        id: devButton

        anchors.right: parent.right
        anchors.rightMargin: Spacing.screenPadding
        anchors.top: parent.top
        anchors.topMargin: shell.topInset + Spacing.safeGap
        height: Theme.touchMin
        visible: pages.depth === 1
        width: 80

        background: Rectangle {
            border.color: Theme.border
            border.width: Theme.borderWidth
            color: Theme.surface
            radius: Theme.radiusLg
        }
        contentItem: Item {
            Row {
                anchors.centerIn: parent
                spacing: 6

                AppIcon {
                    height: 20
                    iconColor: Theme.accent
                    name: "settings"
                    width: 20
                }
                Label {
                    color: Theme.accent
                    font: Typography.captionStrong
                    height: 20
                    text: qsTr("Dev")
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        onClicked: devPanel.open()
    }

    // — Временное сообщение о ещё не реализованном экране —
    Rectangle {
        id: toast

        anchors.bottom: tabBar.top
        anchors.bottomMargin: Spacing.sectionGap
        anchors.horizontalCenter: parent.horizontalCenter
        color: Theme.textPrimary
        height: toastLabel.implicitHeight + Spacing.itemGap * 2
        opacity: 0
        radius: Theme.radiusLg
        visible: opacity > 0
        width: Math.min(parent.width - Spacing.screenPadding * 2, toastLabel.implicitWidth + Spacing.screenPadding * 2)

        Behavior on opacity {
            NumberAnimation {
                duration: 180
            }
        }

        Label {
            id: toastLabel

            anchors.centerIn: parent
            color: Theme.background
            font: Typography.captionStrong
            horizontalAlignment: Text.AlignHCenter
            text: ""
            width: Math.min(implicitWidth, parent.width - Spacing.screenPadding * 2)
            wrapMode: Text.WordWrap
        }
        Timer {
            id: toastTimer

            interval: 2200

            onTriggered: toast.opacity = 0
        }
    }
    Connections {
        function onToastRequested(message) {
            toastLabel.text = message;
            toast.opacity = 0.94;
            toastTimer.restart();
        }

        target: Demo
    }
    DevPanel {
        id: devPanel

        onScreenRequested: function (index) {
            shell.showTab(index);
        }
    }
}
