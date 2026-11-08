pragma Singleton
import QtQuick

QtObject {
    id: texts
    readonly property var unitLabels: ({
            g: qsTr("г"),
            gPerKg: qsTr("г/кг"),
            kg: qsTr("кг"),
            kcal: qsTr("ккал"),
            ml: qsTr("мл"),
            hours: qsTr("ч"),
            rpe: "RPE"
        })
    // Неизвестные единицы сохраняют политику конкретного экрана.
    function unitLabel(unit, fallback) {
        return texts.unitLabels[unit] !== undefined ? texts.unitLabels[unit] : fallback;
    }
}
