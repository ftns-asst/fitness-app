pragma Singleton
import QtQuick

// Подписи параметров замеров тела: вес и девять обхватов книги-эталона
// (docs/09, FR-T5). Ключи совпадают с EMeasurement_Param в C++; подписи нужны и
// форме замера на «Сегодня», и динамике на экране статистики, поэтому живут в
// одном месте. Неизвестный ключ показывается как есть, а не ломает экран.
QtObject {
    // Порядок — как в книге-эталоне и в EMeasurement_Param (вес, затем обхваты).
    readonly property var keys: ["weight", "chest", "waist", "hip", "arm", "thigh", "belly", "shoulders", "calves", "neck"]
    readonly property var titles: ({
            weight: qsTr("Вес"),
            chest: qsTr("Грудь"),
            waist: qsTr("Талия"),
            hip: qsTr("Ягодицы"),
            arm: qsTr("Бицепс"),
            thigh: qsTr("Ноги"),
            belly: qsTr("Живот"),
            shoulders: qsTr("Плечи"),
            calves: qsTr("Икры"),
            neck: qsTr("Шея")
        })

    // Единица параметра: вес — килограммы, всё остальное — сантиметры.
    function unit(key) {
        return key === "weight" ? qsTr("кг") : qsTr("см");
    }
    function title(key) {
        return titles[key] !== undefined ? titles[key] : key;
    }
}
