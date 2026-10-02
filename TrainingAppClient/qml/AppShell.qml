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
    property string initialProgramScreen: ""
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
        if (shell.initialProgramScreen.length > 0) {
            shell.openProgramScreen(shell.initialProgramScreen);
        } else if (shell.initialTab > 0 && shell.initialTab < 4) {
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

                onMyProgramRequested: shell.openProgramCatalog()
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
    // Запрос на открытие экрана программы из «Планов» — своей глобальной
    // программы пользователь не ищет среди шаблонов, он открывает её сам.
    signal myProgramRequested

    // — Экраны глобальной программы (срез W11, issues 41–44) —
    // Отдельный стек поверх вкладок: детали программы живут вне неё, поэтому
    // вкладок не видно, а «назад» возвращает на вкладку, из которой пришли.
    function openProgramCatalog() {
        pages.push(programCatalogComponent);
    }
    function openProgramOverview(id) {
        shell.programId = id === undefined ? "" : id;
        pages.push(programOverviewComponent);
    }

    function openProgramEditor(id) {
        shell.programId = id === undefined ? "" : id;
        pages.push(programEditorComponent);
    }

    function openProgramRules() {
        pages.push(programRulesComponent);
    }

    function openProgramStats(id) {
        shell.programId = id === undefined ? "" : id;
        pages.push(programStatsComponent);
    }

    // Открытие экрана по имени: используется дизайн-ревью (--screen) и
    // восстановление состояния при возврате в приложение.
    function openProgramScreen(name) {
        const map = {
            "program-catalog": openProgramCatalog,
            "program-overview": openProgramOverview,
            "program-edit": openProgramEditor,
            "program-rules": openProgramRules,
            "program-stats": openProgramStats,
            // Экран дня открывается на первом дне недели: так он доступен из
            // --screen для аудита вёрстки, как и остальные экраны программы.
            "program-day": function () {
                openProgramDay(1, 1);
            }
        };
        const opener = map[name];

        if (typeof opener === "function")
            opener();
    }

    // День и вид дня, открываемые на экране дня. Экрану нужны оба значения до его
    // создания, а задать их напрямую нельзя: страница живёт внутри Component, и
    // её идентификатор из оболочки недоступен.
    // Программа, открытая из каталога (срез S2). Пусто — активная: вход из
    // вкладок открывает текущую, а каталог передаёт идентификатор.
    property string programId: ""

    property int programDayWeekday: 1
    property int programDayKind: 1

    // Открытый экран дня недели.
    function openProgramDay(weekday, kind) {
        programDayWeekday = weekday;
        programDayKind = kind;
        pages.push(programDayComponent);
    }

    // Открытый экран программы всегда над вкладками, поэтому «назад» просто
    // снимает верхний элемент стека (см. BackHandler ниже).
    function goBack() {
        if (pages.depth > 1)
            pages.pop();
    }

    Component {
        id: programCatalogComponent

        ProgramCatalogPage {
            bottomInset: shell.bottomInset
            topInset: shell.topInset

            onBackRequested: shell.goBack()
            onCreateRequested: shell.openProgramEditor()
            // Открытие, правка и статистика приходят идентификатором: каталог
            // показывает все программы, а не только активную, поэтому и переход
            // должен нести конкретную программу.
            onOpenRequested: function (programId) {
                shell.openProgramOverview(programId);
            }
            onEditRequested: function (programId) {
                shell.openProgramEditor(programId);
            }
            onStatsRequested: function (programId) {
                shell.openProgramStats(programId);
            }
        }
    }
    Component {
        id: programOverviewComponent

        ProgramOverviewPage {
            bottomInset: shell.bottomInset
            topInset: shell.topInset
            programId: shell.programId

            onBackRequested: shell.goBack()
            onCreateRequested: shell.openProgramEditor()
            onEditRequested: function (programId) {
                shell.openProgramEditor(programId);
            }
            onRulesRequested: shell.openProgramRules()
            onStatsRequested: shell.openProgramStats()
        }
    }
    Component {
        id: programEditorComponent

        ProgramEditPage {
            bottomInset: shell.bottomInset
            topInset: shell.topInset
            programId: shell.programId

            onBackRequested: shell.goBack()
            onCreated: shell.goBack()
            onDayRequested: function (weekday, dayKind) {
                shell.openProgramDay(weekday, dayKind);
            }
        }
    }
    Component {
        id: programDayComponent

        ProgramDayPage {
            bottomInset: shell.bottomInset
            topInset: shell.topInset

            // Экран создаётся в момент открытия, поэтому привязки успевают
            // получить актуальные день и вид дня.
            kind: shell.programDayKind
            weekday: shell.programDayWeekday

            onBackRequested: shell.goBack()
            onCloseRequested: shell.goBack()
        }
    }
    Component {
        id: programRulesComponent

        ProgramRulesPage {
            bottomInset: shell.bottomInset
            topInset: shell.topInset

            onBackRequested: shell.goBack()
        }
    }
    Component {
        id: programStatsComponent

        ProgramStatsPage {
            bottomInset: shell.bottomInset
            topInset: shell.topInset
            programId: shell.programId

            onBackRequested: shell.goBack()
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
