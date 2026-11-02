import QtQuick
import QtQuick.Controls
import QtTest
import TrainingAppClient 1.0

// Навигация каркаса (отзывы сессии):
// 1. Панель навигации видна всегда, и тап по ней переключает раздел даже
//    с pushed-экрана (сигнал tabSelected раньше никто не слушал — панель
//    меняла только подсветку).
// 2. Последний открытый экран вкладки восстанавливается при возвращении
//    на неё (отзыв: «открыл создание программы, ушёл на главную, вернулся —
//    а там корень»).
// 3. Повторный тап по своей вкладке — к её корню, память снимается.
//
// Оболочка собирается с боевыми страницами, но без контекст-свойств main.cpp:
// ViewModel здесь undefined, и страницы обязаны переживать это guard'ами —
// как и в тесте дымовой сборки. Навигация проверяется по глубине стека
// (shell.pushedDepth) и активной вкладке, а не по содержимому страниц.
TestCase {
    id: testCase

    name: "AppShellTabs"
    when: windowShown

    property var shell: null
    property var shellWindow: null

    ListModel {
        id: emptyExercises
    }
    QtObject {
        id: swipeCatalog
        property var exercise_list: emptyExercises
        property var muscleGroups: ["chest", "legs", "back", "shoulders", "biceps", "triceps", "abs"]
        property var equipments: []
        property bool loading: false
        property bool fromCache: false
        property bool canLoadMore: false
        property string errorKey: ""
        function reload() {
        }
        function loadCatalogs() {
        }
        function setQuery(query) {
        }
        function setMuscleGroups(groups) {
        }
        function setEquipments(equipments) {
        }
    }

    // Поиск панели навигации в дереве оболочки: её идентификатор снаружи
    // недоступен, а для клика нужен сам элемент, а не координаты корня.
    function findTabBar(item) {
        for (var i = 0; i < item.children.length; ++i) {
            var child = item.children[i];

            if (child.tabSelected !== undefined && child.currentIndex !== undefined)
                return child;

            var found = findTabBar(child);

            if (found !== null)
                return found;
        }

        return null;
    }

    // Координаты вкладки панели: 4 равные кнопки по всей ширине.
    function tabCenterX(index) {
        return shell.width * (index + 0.5) / 4;
    }

    function init() {
        // Клики QmlTest доставляются только в показанном окне: TestCase сам
        // по себе невидим, поэтому оболочка живёт в собственном окне (см.
        // tst_appearance_settings.qml), а не в корне теста.
        shellWindow = createTemporaryObject(shellWindowComponent, testCase);
        verify(shellWindow !== null);
        shellWindow.show();
        shell = shellWindow.shell;
        tryVerify(function () {
            return shell !== null && shell.visible;
        });
        compare(shell.currentTab, 0);
        compare(shell.pushedDepth, 1);
    }

    function cleanup() {
        shell = null;
        shellWindow = null;
    }

    function test_overviewSelectedProgramContext_data() {
        return [{ tag: "rules", signal: "rulesRequested" },
                { tag: "stats", signal: "statsRequested" }];
    }

    function test_overviewSelectedProgramContext(data) {
        shell.openProgramOverview("selected-program");
        tryCompare(shell, "stackBusy", false);
        shell.topItem[data.signal]();
        tryCompare(shell, "stackBusy", false);
        compare(shell.topItem.programId, "selected-program");
        shell.showTab(1);
        tryCompare(shell, "stackBusy", false);
        shell.openProgramOverview("other-program");
        tryCompare(shell, "stackBusy", false);
        shell.showTab(0);
        tryCompare(shell, "stackBusy", false);
        compare(shell.topItem.programId, "selected-program");
        shell.goBack();
        tryCompare(shell, "stackBusy", false);
        compare(shell.topItem.programId, "selected-program");
    }

    function test_activeProgramRouteRestoresEmptyContext_data() {
        return [{ tag: "rules", open: "openProgramRules" },
                { tag: "stats", open: "openProgramStats" }];
    }

    function test_activeProgramRouteRestoresEmptyContext(data) {
        shell[data.open]();
        tryCompare(shell, "stackBusy", false);
        compare(shell.topItem.programId, "");
        shell.showTab(1);
        tryCompare(shell, "stackBusy", false);
        shell.openProgramOverview("other-program");
        tryCompare(shell, "stackBusy", false);
        shell.showTab(0);
        tryCompare(shell, "stackBusy", false);
        compare(shell.topItem.programId, "");
    }

    function test_swipeChangesRootTabInBothDirections() {
        const view = findChild(shell, "mainSwipeView");
        tryVerify(() => view.width > 0 && view.currentItem.width > 0);
        waitForRendering(view);
        mouseDrag(shellWindow.contentItem, 320, 80, -250, 0);
        tryCompare(shell, "currentTab", 1);
        tryVerify(() => !view.contentItem.moving);
        mouseDrag(shellWindow.contentItem, 70, 80, 250, 0);
        tryCompare(shell, "currentTab", 0);
        compare(shell.pushedDepth, 1);
    }
    function test_leftEdgeSwipeNavigatesBack() {
        shell.openProgramCatalog();
        tryCompare(shell, "stackBusy", false);
        compare(shell.pushedDepth, 2);
        mouseDrag(shellWindow.contentItem, 8, 80, 180, 0);
        tryCompare(shell, "pushedDepth", 1);
        tryCompare(shell, "stackBusy", false);
    }
    function test_swipeRestoresSavedScreenAfterRelease() {
        shell.showTab(1);
        shell.openProgramCatalog();
        tryCompare(shell, "stackBusy", false);
        shell.showTab(0);
        tryCompare(shell, "stackBusy", false);
        const view = findChild(shell, "mainSwipeView");
        tryVerify(() => Math.abs(view.contentItem.contentX) < 1);
        waitForRendering(view);
        mousePress(shellWindow.contentItem, 320, 80);
        mouseMove(shellWindow.contentItem, 200, 80, 50);
        mouseMove(shellWindow.contentItem, 60, 80, 50);
        compare(shell.pushedDepth, 1, "сохранённый экран не открывается под удерживаемым пальцем");
        mouseRelease(shellWindow.contentItem, 60, 80);
        tryCompare(shell, "currentTab", 1);
        tryCompare(shell, "pushedDepth", 2);
        tryCompare(shell, "stackBusy", false);
        shell.goBack();
        tryCompare(shell, "stackBusy", false);
        tryVerify(() => Math.abs(view.contentItem.contentX - view.width) < 1);
    }
    function test_shortEdgeSwipeKeepsCurrentScreen() {
        shell.openProgramCatalog();
        tryCompare(shell, "stackBusy", false);
        mouseDrag(shellWindow.contentItem, 8, 80, 30, 0);
        compare(shell.pushedDepth, 2);
        compare(shell.stackBusy, false);
    }
    function test_horizontalFilterScrollDoesNotChangeTab() {
        shell.showTab(2);
        const view = findChild(shell, "mainSwipeView");
        view.currentItem.viewModel = swipeCatalog;
        tryVerify(() => Math.abs(view.contentItem.contentX - 2 * view.width) < 1);
        waitForRendering(view);
        const chips = findChild(view.currentItem, "muscleGroupChips");
        verify(chips !== null);
        const flick = chips.children[0];
        mouseDrag(chips, chips.width - 25, chips.height / 2, -220, 0);
        tryVerify(() => flick.contentX > 0);
        compare(shell.currentTab, 2);
    }
    function test_diaryDiagnosticRoute() {
        shell.openProgramScreen("diary");
        tryCompare(shell, "stackBusy", false);
        compare(shell.pushedDepth, 2);
        compare(shell.topItem.objectName, "diaryPage");
    }

    function test_diaryFromTodayAndBack() {
        const button = findChild(shell, "todayDiaryButton");
        verify(button !== null);
        const header = findChild(shell, "todayCheckinsHeader");
        verify(header !== null);
        compare(button.parent, header);
        compare(button.text, "Дневник");
        button.clicked();
        tryCompare(shell, "stackBusy", false);
        compare(shell.topItem.objectName, "diaryPage");
        compare(shell.topItem.programId, "");
        shell.goBack();
        tryCompare(shell, "stackBusy", false);
        compare(shell.pushedDepth, 1);
    }

    function test_diaryActiveProgramRestoredAfterSpecificOtherTab() {
        shell.openDiary();
        tryCompare(shell, "stackBusy", false);
        shell.showTab(1);
        tryCompare(shell, "stackBusy", false);
        shell.openProgramOverview("other-tab-program");
        tryCompare(shell, "stackBusy", false);
        shell.showTab(0);
        tryCompare(shell, "stackBusy", false);
        compare(shell.topItem.objectName, "diaryPage");
        compare(shell.topItem.programId, "");
    }

    function test_diaryOverviewKeepsProgramAndRestoresTabHistory() {
        shell.openProgramOverview("diary-test-program");
        tryCompare(shell, "stackBusy", false);
        shell.topItem.diaryRequested();
        tryCompare(shell, "stackBusy", false);
        compare(shell.topItem.objectName, "diaryPage");
        compare(shell.topItem.programId, "diary-test-program");
        shell.showTab(1);
        tryCompare(shell, "stackBusy", false);
        shell.showTab(0);
        tryCompare(shell, "stackBusy", false);
        compare(shell.pushedDepth, 3);
        compare(shell.topItem.objectName, "diaryPage");
        compare(shell.topItem.programId, "diary-test-program");
        shell.goBack();
        tryCompare(shell, "stackBusy", false);
        verify(shell.topItem instanceof ProgramOverviewPage);
    }

    // Тап по вкладке с pushed-экрана закрывает поток и открывает раздел.
    function test_tabBarClickLeavesPushedScreen() {
        shell.openProgramEditor();
        tryCompare(shell, "stackBusy", false);
        tryCompare(shell, "pushedDepth", 2);

        // Клик реальной кнопкой панели: он проверяет и проводку сигнала
        // tabSelected → showTab, которую раньше никто не слушал. Клик идёт
        // по самому бару, в его локальных координатах.
        var bar = findTabBar(shell);

        verify(bar !== null, "панель навигации не найдена в дереве оболочки");
        mouseClick(bar, tabCenterX(1), bar.height / 2);
        tryCompare(shell, "stackBusy", false);
        tryCompare(shell, "pushedDepth", 1);
        compare(shell.currentTab, 1);
    }

    // Возвращение на вкладку восстанавливает последний открытый экран.
    function test_switchingTabRestoresLastScreen() {
        shell.showTab(1);
        shell.openProgramEditor();
        tryCompare(shell, "stackBusy", false);
        tryCompare(shell, "pushedDepth", 2);

        shell.showTab(0);
        tryCompare(shell, "stackBusy", false);
        tryCompare(shell, "pushedDepth", 1);
        compare(shell.currentTab, 0);

        shell.showTab(1);
        tryCompare(shell, "stackBusy", false);
        // Восстановление — push после завершения pop: глубина снова 2.
        tryCompare(shell, "pushedDepth", 2);
        compare(shell.currentTab, 1);
    }

    // Повторный тап по своей вкладке — к её корню, и память снимается:
    // следующий возврат на вкладку открывает корень, а не редактор.
    function test_sameTabTapReturnsToRootAndForgets() {
        shell.showTab(1);
        shell.openProgramEditor();
        tryCompare(shell, "stackBusy", false);
        tryCompare(shell, "pushedDepth", 2);

        shell.showTab(1);
        tryCompare(shell, "stackBusy", false);
        tryCompare(shell, "pushedDepth", 1);
        compare(shell.currentTab, 1);

        shell.showTab(0);
        tryCompare(shell, "stackBusy", false);
        shell.showTab(1);
        tryCompare(shell, "stackBusy", false);
        compare(shell.pushedDepth, 1);
    }

    // «Назад» проходит по всей цепочке переходов вкладки, а не ведёт сразу
    // в корень: день → редактор → каталог → корень.
    function test_backWalksThroughChain() {
        shell.showTab(1);
        shell.openProgramCatalog();
        tryCompare(shell, "stackBusy", false);
        shell.openProgramEditor();
        tryCompare(shell, "stackBusy", false);
        shell.openProgramDay(2, 0);
        tryCompare(shell, "stackBusy", false);
        compare(shell.pushedDepth, 4);

        shell.goBack();
        tryCompare(shell, "stackBusy", false);
        tryCompare(shell, "pushedDepth", 3);
        verify(shell.topItem instanceof ProgramEditPage, "назад из дня должен открыть редактор");

        shell.goBack();
        tryCompare(shell, "stackBusy", false);
        tryCompare(shell, "pushedDepth", 2);
        verify(shell.topItem instanceof ProgramCatalogPage, "назад из редактора должен открыть каталог");

        shell.goBack();
        tryCompare(shell, "stackBusy", false);
        tryCompare(shell, "pushedDepth", 1);
    }

    // Возврат на вкладку восстанавливает ВСЮ цепочку переходов, а не только
    // последний экран (отзыв: «вернулся — а перед редактором потерялся
    // каталог»: назад из восстановленного экрана не должен вести в корень).
    function test_switchingTabRestoresWholeChain() {
        shell.showTab(1);
        shell.openProgramCatalog();
        tryCompare(shell, "stackBusy", false);
        shell.openProgramEditor();
        tryCompare(shell, "stackBusy", false);
        shell.openProgramDay(2, 0);
        tryCompare(shell, "stackBusy", false);
        compare(shell.pushedDepth, 4);

        shell.showTab(0);
        tryCompare(shell, "stackBusy", false);
        tryCompare(shell, "pushedDepth", 1);

        shell.showTab(1);
        tryCompare(shell, "stackBusy", false);
        tryCompare(shell, "pushedDepth", 4);
        compare(shell.currentTab, 1);
        verify(shell.topItem instanceof ProgramDayPage, "верх цепочки — день, который открыли последним");
    }

    // Сохранение программы заменяет редактор обзором в стеке И в истории:
    // иначе возврат на вкладку восстанавливал бы уже закрытую форму создания.
    function test_saveReplacesEditorWithOverviewInHistory() {
        shell.showTab(1);
        shell.openProgramEditor();
        tryCompare(shell, "stackBusy", false);

        shell.finishProgramSave("test-p1", false, false);
        tryCompare(shell, "stackBusy", false);
        tryCompare(shell, "pushedDepth", 2);
        verify(shell.topItem instanceof ProgramOverviewPage, "после создания программа показывается обзором");

        shell.showTab(0);
        tryCompare(shell, "stackBusy", false);
        shell.showTab(1);
        tryCompare(shell, "stackBusy", false);
        tryCompare(shell, "pushedDepth", 2);
        verify(shell.topItem instanceof ProgramOverviewPage, "восстанавливается обзор, а не закрытая форма создания");
    }

    Component {
        id: shellWindowComponent

        ApplicationWindow {
            width: 390
            height: 812
            visible: false

            property alias shell: shellItem

            AppShell {
                id: shellItem

                anchors.fill: parent
            }
        }
    }
}
