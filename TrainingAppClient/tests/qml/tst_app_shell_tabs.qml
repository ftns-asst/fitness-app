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
