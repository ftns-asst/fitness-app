import QtQuick
import QtTest
import TrainingAppClient 1.0

// Оформление клиента: акцент и тема — настройка пользователя в «Профиль →
// Параметры». Раньше оба переключателя жили только в дев-панели дизайн-ревью
// (§4.11), то есть в инструменте разработчика: пользователь не мог выбрать
// акцент, а панель рядом с экраном выглядела как настройка продукта.
TestCase {
    id: testCase

    property var card: null
    property var profilePage: null

    function cleanup() {
        card = null;
        profilePage = null;
        // Theme — синглтон, общий для всего прогона: без возврата значений
        // следующая проверка (и следующий файл тестов) получили бы чужую тему.
        Theme.accentIndex = 0;
        Theme.darkMode = false;
        Theme.fpsCounter = false;
    }
    // Рекурсивный поиск по objectName: у блока оформления нет своих id снаружи,
    // а кружки акцента лежат в ряду внутри карточки.
    function findAllByName(parent, name, found) {
        if (parent === null || parent === undefined)
            return found;

        const kids = parent.children;

        if (kids === undefined)
            return found;

        for (let i = 0; i < kids.length; ++i) {
            if (kids[i].objectName === name)
                found.push(kids[i]);

            testCase.findAllByName(kids[i], name, found);
        }

        return found;
    }
    // Наличие элемента по objectName: блоку оформления id снаружи не видны.
    function findByName(parent, name) {
        const found = testCase.findAllByName(parent, name, []);

        return found.length > 0 ? found[0] : null;
    }
    function init() {
        Theme.accentIndex = 0;
        Theme.darkMode = false;
        Theme.fpsCounter = false;
        card = null;
        profilePage = null;
    }
    // Кружков ровно столько, сколько акцентов в палитре: список из двух мест
    // разошёлся бы, если бы один из них дополнили.
    function test_accentSwatchesRenderPalette() {
        card = createTemporaryObject(appearanceCardComponent, testCase);

        verify(card !== null, "блок оформления не создался");

        wait(0);

        const swatches = testCase.findAllByName(card, "appearanceAccentSwatch", []);
        const accentName = testCase.findByName(card, "appearanceAccentName");

        compare(swatches.length, Theme.accentPalette.length);
        verify(accentName !== null, "в блоке оформления нет имени акцента");
        compare(accentName.text, "Акцент: " + Theme.accentNames[0]);
    }
    // Нажатие на кружок меняет акцент приложения: значение пишется в тот же
    // токен Theme, что читают все компоненты, — отдельного «локального» акцента
    // у блока нет.
    function test_accentSwatchTapChangesTheme() {
        const window = createTemporaryObject(appearanceCardInWindowComponent, testCase);

        verify(window !== null, "блок оформления не создался");

        window.show();

        const cardInWindow = window.card;

        verify(cardInWindow !== null, "в окне нет блока оформления");

        waitForRendering(cardInWindow);

        const swatches = testCase.findAllByName(cardInWindow, "appearanceAccentSwatch", []);

        compare(swatches.length, Theme.accentPalette.length);

        mouseClick(swatches[2]);

        compare(Theme.accentIndex, 2);
        compare(testCase.findByName(cardInWindow, "appearanceAccentName").text, "Акцент: " + Theme.accentNames[2]);
    }
    // Тема переключается тем же сегментированным контролом, что и разделы: в
    // блоке оформления он один.
    function test_themeSelectorSwitchesTheme() {
        card = createTemporaryObject(appearanceCardComponent, testCase);

        verify(card !== null, "блок оформления не создался");

        const selector = testCase.findByName(card, "appearanceThemeSelector");

        verify(selector !== null, "в блоке оформления нет переключателя темы");
        compare(selector.items.length, 2);

        selector.selected(1);

        compare(Theme.darkMode, true);

        selector.selected(0);

        compare(Theme.darkMode, false);
    }
    // Счётчик кадров включается тумблером в блоке оформления и пишется в тот
    // же токен Theme, что читает оверлей: диагностика «негладкого» скролла —
    // настройка пользователя, а не только флаг запуска --fps.
    function test_fpsCounterTogglesFromSettings() {
        card = createTemporaryObject(appearanceCardComponent, testCase);

        verify(card !== null, "блок оформления не создался");

        const selector = testCase.findByName(card, "appearanceFpsSelector");

        verify(selector !== null, "в блоке оформления нет переключателя счётчика кадров");
        compare(selector.currentIndex, 0);

        selector.selected(1);

        compare(Theme.fpsCounter, true);

        selector.selected(0);

        compare(Theme.fpsCounter, false);
    }
    // Контрол показывает текущее состояние настройки, а не дефолт: счётчик
    // включён до создания блока — подсвечен «Вкл».
    function test_fpsSelectorShowsCurrentState() {
        Theme.fpsCounter = true;
        card = createTemporaryObject(appearanceCardComponent, testCase);

        verify(card !== null, "блок оформления не создался");

        compare(testCase.findByName(card, "appearanceFpsSelector").currentIndex, 1);
    }
    // Контрол показывает текущую тему, а не своё состояние по умолчанию:
    // тёмная тема включена до создания блока — подсвечен «Тёмная».
    function test_themeSelectorShowsCurrentTheme() {
        Theme.darkMode = true;
        card = createTemporaryObject(appearanceCardComponent, testCase);

        verify(card !== null, "блок оформления не создался");

        compare(testCase.findByName(card, "appearanceThemeSelector").currentIndex, 1);
    }
    // Блок живёт в разделе «Параметры» профиля: он стоит рядом с другими
    // элементами раздела и показывается вместе с ним. Проверка идёт в показанном
    // окне: `visible` у элемента читается с учётом предков и окна, и в
    // непоказанном окне он был бы false независимо от раздела.
    function test_profileSettingsContainAppearanceCard() {
        const window = createTemporaryObject(profilePageInWindowComponent, testCase);

        verify(window !== null, "экран профиля не создался");

        window.show();

        const profile = window.profile;

        waitForRendering(profile);

        profile.sectionIndex = 2;

        const appearance = testCase.findByName(profile, "appearanceCard");
        const logoutButton = testCase.findByName(profile, "profileLogoutButton");

        verify(appearance !== null, "в разделе «Параметры» нет блока оформления");
        verify(logoutButton !== null, "в разделе «Параметры» нет кнопки выхода");

        // Общий родитель с известным элементом раздела: так проверяется, что
        // блок стоит именно в «Параметрах», а не в другом разделе профиля.
        compare(appearance.parent, logoutButton.parent);
        compare(appearance.parent.visible, true);

        profile.sectionIndex = 0;

        compare(appearance.parent.visible, false);
    }

    height: 812
    name: "AppearanceSettings"
    width: 390

    Component {
        id: appearanceCardComponent

        AppearanceCard {
            width: 358
        }
    }
    // Нажатия проверяются в показанном окне: TapHandler получает события только
    // у видимого элемента.
    Component {
        id: appearanceCardInWindowComponent

        Window {
            property alias card: cardInWindow

            height: 812
            visible: false
            width: 390

            AppearanceCard {
                id: cardInWindow

                width: 358
                x: 16
                y: 16
            }
        }
    }
    Component {
        id: profilePageInWindowComponent

        Window {
            property alias profile: profileInWindow

            height: 812
            visible: false
            width: 390

            ProfilePage {
                id: profileInWindow

                anchors.fill: parent
                auth: authStub
                viewModel: profileStub
            }
        }
    }

    // Гость без сессии: проверяется раздел «Параметры», а данные профиля к блоку
    // оформления отношения не имеют.
    QtObject {
        id: authStub

        property bool authenticated: false
        property var localProfile: null
        property var user: {
            "email": "guest@example.com",
            "name": "Гость"
        }
    }
    QtObject {
        id: profileStub

        property var chartCaption: ""
        property var chartPoints: []
        property var heatmapWeeks: []
        property var kpi: []
        property var profileParams: []
        property var records: []
        property var workoutHistory: []
    }
}
