pragma Singleton
import QtQuick

// Mock-backed presentation contract экрана «Сегодня».
QtObject {
    readonly property var plan: MockCatalog.todayPlan
    readonly property var record: MockCatalog.todayRecord
    readonly property var exercises: MockCatalog.todayExercises
}
