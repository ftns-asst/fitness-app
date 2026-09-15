pragma Singleton
import QtQuick

// Демонстрационные данные каталога (§5.1 дизайн-дока).
// Роли намеренно названы так же, как будущие роли C++-моделей (QAbstractListModel),
// поэтому на этапах M2+ страницы перепривяжутся к реальным моделям без правок delegate.
QtObject {
    // — План на сегодня —
    readonly property var todayPlan: ({
            title: qsTr("Сегодня — тренировка A"),
            badges: [qsTr("Full body"), qsTr("новичок")],
            weekIndex: 3,
            weeksTotal: 12,
            exerciseCount: 5,
            durationText: qsTr("около 55 минут"),
            restDay: false
        })

    // — Упражнения плана на сегодня (§5.1) —
    readonly property var todayExercises: [
        {
            title: qsTr("Приседания со штангой"),
            setsText: "4 × 8–10",
            weightText: "62,5 кг"
        },
        {
            title: qsTr("Жим лёжа"),
            setsText: "4 × 8–10",
            weightText: "50 кг"
        },
        {
            title: qsTr("Тяга верхнего блока"),
            setsText: "3 × 10–12",
            weightText: "57,5 кг"
        },
        {
            title: qsTr("Жим гантелей сидя"),
            setsText: "3 × 10–12",
            weightText: "16 кг"
        },
        {
            title: qsTr("Подъём на бицепс"),
            setsText: "3 × 12–15",
            weightText: "14 кг"
        }
    ]

    // — Каталог упражнений (для поиска и фильтров) —
    readonly property var exercises: [
        {
            title: qsTr("Приседания со штангой"),
            subtitle: qsTr("Ноги · штанга"),
            muscleGroup: qsTr("Ноги"),
            equipment: qsTr("Штанга")
        },
        {
            title: qsTr("Жим лёжа"),
            subtitle: qsTr("Грудь · штанга"),
            muscleGroup: qsTr("Грудь"),
            equipment: qsTr("Штанга")
        },
        {
            title: qsTr("Тяга верхнего блока"),
            subtitle: qsTr("Спина · тренажёр"),
            muscleGroup: qsTr("Спина"),
            equipment: qsTr("Тренажёр")
        },
        {
            title: qsTr("Жим гантелей сидя"),
            subtitle: qsTr("Плечи · гантели"),
            muscleGroup: qsTr("Плечи"),
            equipment: qsTr("Гантели")
        },
        {
            title: qsTr("Подъём на бицепс"),
            subtitle: qsTr("Руки · гантели"),
            muscleGroup: qsTr("Руки"),
            equipment: qsTr("Гантели")
        },
        {
            title: qsTr("Становая тяга"),
            subtitle: qsTr("Спина · штанга"),
            muscleGroup: qsTr("Спина"),
            equipment: qsTr("Штанга")
        },
        {
            title: qsTr("Отжимания на брусьях"),
            subtitle: qsTr("Грудь · свой вес"),
            muscleGroup: qsTr("Грудь"),
            equipment: qsTr("Свой вес")
        },
        {
            title: qsTr("Планка"),
            subtitle: qsTr("Кор · свой вес"),
            muscleGroup: qsTr("Кор"),
            equipment: qsTr("Свой вес")
        }
    ]

    // — Базовые планы тренировок (§2.22) —
    readonly property var plans: [
        {
            title: qsTr("Full body для новичка"),
            subtitle: qsTr("3 дня в неделю · 55 мин · 5 упражнений"),
            goal: qsTr("Сила"),
            level: qsTr("Новичок"),
            reviewText: qsTr("34 отзыва"),
            ratingAvg: 4.7,
            ratingCount: 128,
            reviewCount: 34,
            createdAt: "2026-08-02"
        },
        {
            title: qsTr("Сплит 3 дня: тяга / жим / ноги"),
            subtitle: qsTr("3 дня в неделю · 60 мин · 6 упражнений"),
            goal: qsTr("Масса"),
            level: qsTr("Средний"),
            reviewText: qsTr("21 отзыв"),
            ratingAvg: 4.5,
            ratingCount: 96,
            reviewCount: 21,
            createdAt: "2026-07-19"
        },
        {
            title: qsTr("Верх / низ, 4 дня"),
            subtitle: qsTr("4 дня в неделю · 50 мин · 7 упражнений"),
            goal: qsTr("Масса"),
            level: qsTr("Средний"),
            reviewText: qsTr("12 отзывов"),
            ratingAvg: 4.3,
            ratingCount: 58,
            reviewCount: 12,
            createdAt: "2026-08-21"
        },
        {
            title: qsTr("Поддержание формы, 2 дня"),
            subtitle: qsTr("2 дня в неделю · 35 мин · 4 упражнения"),
            goal: qsTr("Тонус"),
            level: qsTr("Новичок"),
            reviewText: qsTr("9 отзывов"),
            ratingAvg: 4.1,
            ratingCount: 37,
            reviewCount: 9,
            createdAt: "2026-06-11"
        }
    ]

    // — Профиль (§5.4) —
    readonly property var profile: ({
            initials: "ИМ",
            name: qsTr("Игорь М."),
            details: qsTr("27 лет · 182 см · 78 кг · цель: Сила")
        })

    // — KPI прогресса (§5.4) —
    readonly property var kpi: [
        {
            label: qsTr("Рабочий вес"),
            valueText: "62,5",
            deltaText: "+12,5 кг",
            deltaKind: "positive"
        },
        {
            label: qsTr("Тоннаж/нед."),
            valueText: "14,8 т",
            deltaText: "+6%",
            deltaKind: "neutral"
        },
        {
            label: qsTr("Частота"),
            valueText: "3,1",
            deltaText: qsTr("цель 3"),
            deltaKind: "neutral"
        }
    ]

    // — Параметры профиля для вкладки «Параметры» —
    readonly property var profileParams: [
        {
            label: qsTr("Пол"),
            value: qsTr("Мужской")
        },
        {
            label: qsTr("Возраст"),
            value: qsTr("27 лет")
        },
        {
            label: qsTr("Рост"),
            value: qsTr("182 см")
        },
        {
            label: qsTr("Вес"),
            value: qsTr("78 кг")
        },
        {
            label: qsTr("Уровень"),
            value: qsTr("Новичок")
        },
        {
            label: qsTr("Цель"),
            value: qsTr("Сила")
        },
        {
            label: qsTr("Тренировок в неделю"),
            value: "3"
        }
    ]

    // — Личный рекорд для экрана «Сегодня» (референс 2081) —
    readonly property var todayRecord: ({
            label: qsTr("Личный рекорд"),
            title: qsTr("Приседания 80 кг × 5"),
            dateText: qsTr("установлен 7 сентября")
        })

    // — Аналитика раздела «Прогресс» (§5.4, референс 2084/2085) —
    readonly property var chartCaption: qsTr("Приседания · 8 недель")
    readonly property var chartPoints: [48.0, 50.0, 50.0, 52.5, 55.0, 57.5, 57.5, 60.0]
    readonly property var heatmapWeeks: [1, 2, 3, 2, 0, 1, 2, 3, 2, 1, 0, 2, 3, 2, 1, 1, 0, 2, 3, 2, 1, 3, 2, 3]

    // — Список личных рекордов (референс 2085) —
    readonly property var records: [
        {
            title: qsTr("Приседания со штангой"),
            valueText: "80 × 5",
            dateText: "07.09"
        },
        {
            title: qsTr("Жим лёжа"),
            valueText: "57,5 × 5",
            dateText: "31.08"
        },
        {
            title: qsTr("Румынская тяга"),
            valueText: "90 × 6",
            dateText: "24.08"
        },
        {
            title: qsTr("Тяга верхнего блока"),
            valueText: "65 × 8",
            dateText: "12.09"
        }
    ]
}
