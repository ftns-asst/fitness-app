import QtQuick
import QtTest
import TrainingAppClient 1.0

// Профиль — экран настроек, а не вкладка. Проверяется то, что отличает его от
// прежнего вида: возврат кнопкой, отсутствие сегментов и демонстрационной
// аналитики, наличие самих настроек.
TestCase {
    id: testCase

    when: windowShown

    property var page: null

    function init() {
        page = null;
    }

    function cleanup() {
        page = null;
    }

    // Возврат обязателен: экран открывается поверх вкладок.
    function test_backBarClosesProfile() {
        page = createTemporaryObject(profileComponent, testCase);

        verify(page !== null);

        let closed = false;

        page.backRequested.connect(function () {
            closed = true;
        });

        const bar = findChild(page, "profileBackBar");

        verify(bar !== null, "панель возврата должна быть на экране");
        bar.clicked();
        verify(closed, "«Назад» обязан сообщать о закрытии");
    }

    // Аналитика переехала в программу и дневник: в профиле её нет.
    function test_onlySettingsRemain() {
        page = createTemporaryObject(profileComponent, testCase);

        verify(page !== null);
        verify(findChild(page, "profileSegmentedControl") === null, "сегментов быть не должно");
        verify(findChild(page, "profileKpi") === null, "KPI-плиток быть не должно");
        verify(findChild(page, "profileRecords") === null, "рекордов быть не должно");
        verify(findChild(page, "appearanceCard") !== null, "настройки оформления на месте");
    }

    Component {
        id: profileComponent

        ProfilePage {
            auth: authStub
            height: 812
            viewModel: profileStub
            width: 390
        }
    }

    QtObject {
        id: authStub

        property bool authenticated: true
        property var localProfile: null
        property var user: {
            "name": "Анна Ковалёва",
            "email": "anna@example.com"
        }
    }

    QtObject {
        id: profileStub

        property var profileParams: []
    }
}
