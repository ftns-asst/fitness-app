pragma Singleton
import QtQuick

// Параметры профиля для экрана настроек. Аналитики здесь больше нет: прогресс
// и история живут в программе и дневнике, где для них есть настоящие данные, а
// не демонстрационные значения MockCatalog.
QtObject {
    readonly property var profileParams: MockCatalog.profileParams
}
