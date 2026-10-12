pragma Singleton
import QtQuick

// Перевод справочных значений упражнений на русский. Ключи приходят с
// бэкенда (`GET /muscle-groups`, `GET /equipments`, поля muscle_groups/
// equipment в Exercise); справочники могут расти — неизвестный ключ
// показывается как есть (ключ читаем), а не ломает интерфейс.
QtObject {
    readonly property var equipmentTitles: ({
        ab_wheel: qsTr("Ролик для пресса"),
        barbell: qsTr("Штанга"),
        bench: qsTr("Скамья"),
        bodyweight: qsTr("Собственный вес"),
        cable_machine: qsTr("Кроссовер"),
        calf_raise_machine: qsTr("Тренажёр для икр"),
        chest_press_machine: qsTr("Жим от груди"),
        dip_bars: qsTr("Брусья"),
        dumbbell: qsTr("Гантели"),
        elliptical: qsTr("Эллипсоид"),
        exercise_bike: qsTr("Велотренажёр"),
        ez_bar: qsTr("EZ-гриф"),
        hack_squat_machine: qsTr("Гакк-машина"),
        incline_bench: qsTr("Наклонная скамья"),
        jump_rope: qsTr("Скакалка"),
        kettlebell: qsTr("Гиря"),
        lat_pulldown_machine: qsTr("Верхняя тяга"),
        leg_curl_machine: qsTr("Сгибание ног"),
        leg_extension_machine: qsTr("Разгибание ног"),
        leg_press_machine: qsTr("Жим ногами"),
        mat: qsTr("Коврик"),
        medicine_ball: qsTr("Медбол"),
        pec_deck_machine: qsTr("Бабочка"),
        pull_up_bar: qsTr("Турник"),
        resistance_band: qsTr("Резиновая лента"),
        roman_chair: qsTr("Римский стул"),
        rowing_machine: qsTr("Гребной тренажёр"),
        seated_row_machine: qsTr("Горизонтальная тяга"),
        smith_machine: qsTr("Машина Смита"),
        squat_rack: qsTr("Стойка для приседа"),
        treadmill: qsTr("Беговая дорожка")
    })
    readonly property var muscleGroupTitles: ({
        abs: qsTr("Пресс"),
        abductors: qsTr("Отводящие"),
        adductors: qsTr("Приводящие"),
        biceps: qsTr("Бицепс"),
        calves: qsTr("Икры"),
        cardio: qsTr("Кардио"),
        chest: qsTr("Грудь"),
        forearms: qsTr("Предплечья"),
        front_delts: qsTr("Передние дельты"),
        glutes: qsTr("Ягодицы"),
        hamstrings: qsTr("Бицепс бедра"),
        lats: qsTr("Широчайшие"),
        lower_back: qsTr("Поясница"),
        obliques: qsTr("Косые"),
        quads: qsTr("Квадрицепсы"),
        rear_delts: qsTr("Задние дельты"),
        side_delts: qsTr("Средние дельты"),
        traps: qsTr("Трапеции"),
        triceps: qsTr("Трицепс"),
        upper_back: qsTr("Верх спины")
    })

    function equipmentTitle(key) {
        return equipmentTitles[key] !== undefined ? equipmentTitles[key] : key;
    }
    // Подзаголовок карточки каталога: группы мышц через запятую, по-русски.
    function groupsSubtitle(groups) {
        const titles = [];

        for (let i = 0; i < groups.length; ++i)
            titles.push(muscleGroupTitle(groups[i]));

        return titles.join(", ");
    }
    function muscleGroupTitle(key) {
        return muscleGroupTitles[key] !== undefined ? muscleGroupTitles[key] : key;
    }
}
