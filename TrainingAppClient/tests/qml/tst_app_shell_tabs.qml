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
        shell.openProgramOverview();
        tryCompare(shell, "stackBusy", false);
        compare(shell.pushedDepth, 2);
        mouseDrag(shellWindow.contentItem, 8, 80, 180, 0);
        tryCompare(shell, "pushedDepth", 1);
        tryCompare(shell, "stackBusy", false);
    }
    function test_swipeRestoresSavedScreenAfterRelease() {
        shell.showTab(1);
        shell.openProgramOverview();
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
        shell.openProgramOverview();
        tryCompare(shell, "stackBusy", false);
        mouseDrag(shellWindow.contentItem, 8, 80, 30, 0);
        compare(shell.pushedDepth, 2);
        compare(shell.stackBusy, false);
    }
    function test_horizontalFilterScrollDoesNotChangeTab() {
        shell.showTab(3);
        const view = findChild(shell, "mainSwipeView");
        view.currentItem.viewModel = swipeCatalog;
        tryVerify(() => Math.abs(view.contentItem.contentX - 3 * view.width) < 1);
        waitForRendering(view);
        const chips = findChild(view.currentItem, "muscleGroupChips");
        verify(chips !== null);
        const flick = chips.children[0];
        mouseDrag(chips, chips.width - 25, chips.height / 2, -220, 0);
        tryVerify(() => flick.contentX > 0);
        compare(shell.currentTab, 3);
    }
    // Дневник — вкладка, а не экран поверх: диагностический переход приводит
    // на неё же и не увеличивает глубину стека.
    function test_diaryDiagnosticRoute() {
        shell.openProgramScreen("diary");
        tryCompare(shell, "stackBusy", false);
        compare(shell.pushedDepth, 1);
        compare(shell.currentTab, 1);
    }

    // Кнопка «Дневник» в шапке отметок больше не открывает экран поверх,
    // а переключает на вкладку с историей по датам.
    function test_diaryFromTodayOpensTab() {
        const button = findChild(shell, "todayDiaryButton");
        verify(button !== null);
        const header = findChild(shell, "todayCheckinsHeader");
        verify(header !== null);
        compare(button.parent, header);
        compare(button.text, "Дневник");
        button.clicked();
        tryCompare(shell, "stackBusy", false);
        compare(shell.currentTab, 1);
        compare(shell.pushedDepth, 1);
        verify(findChild(shell, "mainSwipeView").currentItem.objectName === "diaryPage");
    }

    // Контекст программы у дневника сохраняется при переключении вкладок:
    // вкладка живёт всё время, поэтому ей достаточно собственного свойства.
    function test_diaryKeepsProgramContextAcrossTabs() {
        shell.openDiary("other-tab-program");
        tryCompare(shell, "stackBusy", false);
        compare(shell.currentTab, 1);
        shell.showTab(0);
        tryCompare(shell, "stackBusy", false);
        shell.showTab(1);
        tryCompare(shell, "stackBusy", false);
        compare(shell.currentTab, 1);
        compare(shell.pushedDepth, 1);
        compare(findChild(shell, "mainSwipeView").currentItem.programId, "other-tab-program");
    }

    // Панель — четыре назначения; профиль вышел из неё.
    function test_tabBarShowsFourSections() {
        const bar = findTabBar(shell);

        verify(bar !== null);
        compare(bar.iconNames.length, 4);
        compare(bar.titles[0], "Сегодня");
        compare(bar.titles[1], "Дневник");
        compare(bar.titles[2], "Программы");
        compare(bar.titles[3], "Упражнения");
        verify(bar.iconNames.indexOf("profile") < 0, "профиль больше не вкладка");
        verify(bar.iconNames.indexOf("diary") >= 0, "у дневника своя иконка");
    }

    // Профиль — экран поверх вкладок: вход кнопкой в шапке, выход «Назад»,
    // активная вкладка при этом не меняется.
    function test_profileOpensFromHeaderAndReturns() {
        const view = findChild(shell, "mainSwipeView");
        const tabBefore = shell.currentTab;

        view.currentItem.profileActionRequested();
        tryCompare(shell, "stackBusy", false);
        compare(shell.pushedDepth, 2);
        compare(shell.topItem.objectName, "profilePage");
        compare(shell.currentTab, tabBefore);

        shell.goBack();
        tryCompare(shell, "stackBusy", false);
        compare(shell.pushedDepth, 1);
        compare(shell.currentTab, tabBefore);
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
        shell.openProgramOverview();
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
        verify(shell.topItem instanceof ProgramOverviewPage, "назад из редактора должен открыть обзор");

        shell.goBack();
        tryCompare(shell, "stackBusy", false);
        tryCompare(shell, "pushedDepth", 1);
    }

    // Возврат на вкладку восстанавливает ВСЮ цепочку переходов, а не только
    // последний экран (отзыв: «вернулся — а перед редактором потерялся
    // каталог»: назад из восстановленного экрана не должен вести в корень).
    function test_switchingTabRestoresWholeChain() {
        shell.showTab(1);
        shell.openProgramOverview();
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

    // Экран «Программы» — два сегмента: свои программы и шаблоны.
    function test_programsTabHasTwoSegments() {
        shell.showTab(2);
        tryCompare(shell, "stackBusy", false);
        compare(shell.currentTab, 2);

        const segments = findChild(shell, "programsSegments");

        verify(segments !== null, "на вкладке нет сегментов «Мои / Шаблоны»");
        compare(segments.items.length, 2);
    }

    // Обзор программы держит сегменты и не плодит стек: переключение меняет
    // содержимое, а не открывает новый экран.
    function test_programOverviewSegmentsDoNotGrowStack() {
        shell.openProgramOverview();
        tryCompare(shell, "stackBusy", false);
        compare(shell.pushedDepth, 2);

        const segments = findChild(shell.topItem, "programSegments");

        verify(segments !== null, "в обзоре нет сегментов «Состав / Прогресс / Правила»");
        compare(segments.items.length, 3);

        segments.selected(1);
        tryCompare(shell, "stackBusy", false);
        compare(shell.pushedDepth, 2, "прогресс открывается внутри обзора");
        compare(shell.topItem.segment, 1);

        segments.selected(2);
        tryCompare(shell, "stackBusy", false);
        compare(shell.pushedDepth, 2, "правила открываются внутри обзора");
        compare(shell.topItem.segment, 2);

        segments.selected(0);
        tryCompare(shell, "stackBusy", false);
        compare(shell.topItem.segment, 0);
    }

    // «Каталог программ» больше не отдельный экран: свои программы показывает
    // вкладка «Программы».
    function test_programCatalogOpensProgramsTab() {
        shell.showTab(0);
        tryCompare(shell, "stackBusy", false);

        shell.openProgramCatalog();
        tryCompare(shell, "stackBusy", false);
        compare(shell.currentTab, 2);
        compare(shell.pushedDepth, 1);
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
