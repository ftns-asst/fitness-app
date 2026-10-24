pragma Singleton
import QtQuick

// Mock-backed presentation contract вкладки «Профиль».
QtObject {
    readonly property string chartCaption: MockCatalog.chartCaption
    readonly property var chartPoints: MockCatalog.chartPoints
    readonly property var heatmapWeeks: MockCatalog.heatmapWeeks
    readonly property var kpi: MockCatalog.kpi
    readonly property var profileParams: MockCatalog.profileParams
    readonly property var records: MockCatalog.records
    readonly property var workoutHistory: MockCatalog.workoutHistory
}
