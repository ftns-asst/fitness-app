pragma Singleton
import QtQuick

// Демонстрационные данные каталога (§5.1 дизайн-дока).
// Роли намеренно названы так же, как будущие роли C++-моделей (QAbstractListModel),
// поэтому на этапах M2+ страницы перепривяжутся к реальным моделям без правок delegate.
QtObject {

    // — Аналитика раздела «Прогресс» (§5.4, референс 2084/2085) —
    readonly property var chartCaption: qsTr("Приседания · 8 недель")
    readonly property var chartPoints: [48.0, 50.0, 50.0, 52.5, 55.0, 57.5, 57.5, 60.0]

    // — Карточка упражнения (issues 16–17) —
    readonly property var exerciseDetails: ({
            id: "exercise-barbell-squat",
            title: qsTr("Приседания со штангой"),
            muscleGroup: qsTr("Ноги"),
            equipment: qsTr("Штанга"),
            description: qsTr("Базовое упражнение для развития силы ног и мышц корпуса."),
            tags: [qsTr("Ноги"), qsTr("Штанга"), qsTr("Базовое")],
            technique: [qsTr("Поставьте стопы немного шире плеч."), qsTr("Сохраняйте нейтральное положение спины."), qsTr("Опускайтесь до параллели бёдер с полом."), qsTr("Поднимайтесь, направляя колени по линии носков.")],
            recordTiles: [
                {
                    id: "best-weight",
                    label: qsTr("Лучший вес"),
                    valueText: "80 кг"
                },
                {
                    id: "best-set",
                    label: qsTr("Лучший подход"),
                    valueText: "80 × 5"
                },
                {
                    id: "last-workout",
                    label: qsTr("Последний"),
                    valueText: "62,5 × 9"
                }
            ],
            progressPoints: [
                {
                    id: "progress-01",
                    date: "2026-07-24",
                    value: 48.0
                },
                {
                    id: "progress-02",
                    date: "2026-07-31",
                    value: 50.0
                },
                {
                    id: "progress-03",
                    date: "2026-08-07",
                    value: 52.5
                },
                {
                    id: "progress-04",
                    date: "2026-08-14",
                    value: 55.0
                },
                {
                    id: "progress-05",
                    date: "2026-08-21",
                    value: 57.5
                },
                {
                    id: "progress-06",
                    date: "2026-09-12",
                    value: 62.5
                }
            ],
            isCustom: false
        })

    // — Каталог упражнений (для поиска и фильтров) —
    readonly property var exercises: [
        {
            id: "exercise-barbell-squat",
            title: qsTr("Приседания со штангой"),
            subtitle: qsTr("Ноги · штанга"),
            muscleGroup: qsTr("Ноги"),
            equipment: qsTr("Штанга"),
            isCustom: false
        },
        {
            id: "exercise-bench-press",
            title: qsTr("Жим лёжа"),
            subtitle: qsTr("Грудь · штанга"),
            muscleGroup: qsTr("Грудь"),
            equipment: qsTr("Штанга"),
            isCustom: false
        },
        {
            id: "exercise-lat-pulldown",
            title: qsTr("Тяга верхнего блока"),
            subtitle: qsTr("Спина · тренажёр"),
            muscleGroup: qsTr("Спина"),
            equipment: qsTr("Тренажёр"),
            isCustom: false
        },
        {
            id: "exercise-seated-dumbbell-press",
            title: qsTr("Жим гантелей сидя"),
            subtitle: qsTr("Плечи · гантели"),
            muscleGroup: qsTr("Плечи"),
            equipment: qsTr("Гантели"),
            isCustom: false
        },
        {
            id: "exercise-biceps-curl",
            title: qsTr("Подъём на бицепс"),
            subtitle: qsTr("Руки · гантели"),
            muscleGroup: qsTr("Руки"),
            equipment: qsTr("Гантели"),
            isCustom: false
        },
        {
            id: "exercise-deadlift",
            title: qsTr("Становая тяга"),
            subtitle: qsTr("Спина · штанга"),
            muscleGroup: qsTr("Спина"),
            equipment: qsTr("Штанга"),
            isCustom: false
        },
        {
            id: "exercise-dips",
            title: qsTr("Отжимания на брусьях"),
            subtitle: qsTr("Грудь · свой вес"),
            muscleGroup: qsTr("Грудь"),
            equipment: qsTr("Свой вес"),
            isCustom: false
        },
        {
            id: "exercise-plank",
            title: qsTr("Планка"),
            subtitle: qsTr("Кор · свой вес"),
            muscleGroup: qsTr("Кор"),
            equipment: qsTr("Свой вес"),
            isCustom: false
        }
    ]
    readonly property var heatmapWeeks: [1, 2, 3, 2, 0, 1, 2, 3, 2, 1, 0, 2, 3, 2, 1, 1, 0, 2, 3, 2, 1, 3, 2, 3]

    // — KPI прогресса (§5.4) —
    readonly property var kpi: [
        {
            id: "working-weight",
            label: qsTr("Рабочий вес"),
            valueText: "62,5",
            deltaText: "+12,5 кг",
            deltaKind: "positive"
        },
        {
            id: "weekly-tonnage",
            label: qsTr("Тоннаж/нед."),
            valueText: "14,8 т",
            deltaText: "+6%",
            deltaKind: "neutral"
        },
        {
            id: "frequency",
            label: qsTr("Частота"),
            valueText: "3,1",
            deltaText: qsTr("цель 3"),
            deltaKind: "neutral"
        }
    ]
    readonly property var planDayExercises: [
        {
            id: "day-exercise-001",
            planDayId: "plan-day-a",
            exerciseId: "exercise-barbell-squat",
            title: qsTr("Приседания со штангой"),
            orderIndex: 0,
            sets: 4,
            repsText: "8–10",
            restSeconds: 120
        },
        {
            id: "day-exercise-002",
            planDayId: "plan-day-a",
            exerciseId: "exercise-bench-press",
            title: qsTr("Жим лёжа"),
            orderIndex: 1,
            sets: 4,
            repsText: "8–10",
            restSeconds: 120
        },
        {
            id: "day-exercise-003",
            planDayId: "plan-day-a",
            exerciseId: "exercise-lat-pulldown",
            title: qsTr("Тяга верхнего блока"),
            orderIndex: 2,
            sets: 3,
            repsText: "10–12",
            restSeconds: 90
        }
    ]

    // — Детали плана и отзывы (issues 14–15) —
    readonly property var planDetails: ({
            id: "plan-full-body-beginner",
            title: qsTr("Full body для новичка"),
            authorName: qsTr("Команда Фитнес-помощника"),
            description: qsTr("Сбалансированная программа на всё тело для первых 12 недель регулярных тренировок."),
            goal: qsTr("Сила"),
            level: qsTr("Новичок"),
            ratingAvg: 4.7,
            ratingCount: 128,
            reviewCount: 34,
            isCurrent: true,
            infoTiles: [
                {
                    id: "duration",
                    label: qsTr("Длительность"),
                    valueText: qsTr("12 недель")
                },
                {
                    id: "frequency",
                    label: qsTr("Частота"),
                    valueText: qsTr("3 дня/нед.")
                },
                {
                    id: "workout-time",
                    label: qsTr("Тренировка"),
                    valueText: qsTr("около 55 мин")
                }
            ],
            days: [
                {
                    id: "plan-day-a",
                    title: qsTr("День A"),
                    subtitle: qsTr("Ноги · грудь · спина"),
                    exerciseCount: 5
                },
                {
                    id: "plan-day-b",
                    title: qsTr("День B"),
                    subtitle: qsTr("Спина · плечи · кор"),
                    exerciseCount: 5
                },
                {
                    id: "plan-day-c",
                    title: qsTr("День C"),
                    subtitle: qsTr("Ноги · грудь · руки"),
                    exerciseCount: 6
                }
            ]
        })
    readonly property var planReviews: [
        {
            id: "review-001",
            planId: "plan-full-body-beginner",
            authorName: qsTr("Анна"),
            rating: 5,
            text: qsTr("Понятный темп и хорошая нагрузка для старта."),
            createdAt: "2026-09-11T15:40:00Z",
            isOwn: false
        },
        {
            id: "review-002",
            planId: "plan-full-body-beginner",
            authorName: qsTr("Михаил"),
            rating: 4,
            text: qsTr("Удобно заниматься трижды в неделю, но заменил одно упражнение."),
            createdAt: "2026-09-08T08:20:00Z",
            isOwn: false
        },
        {
            id: "review-003",
            planId: "plan-full-body-beginner",
            authorName: qsTr("Елена"),
            rating: 5,
            text: "",
            createdAt: "2026-09-03T19:05:00Z",
            isOwn: false
        }
    ]

    // — Анкета и результат подбора плана (issue 18) —
    readonly property var planWizard: ({
            steps: [
                {
                    id: "experience",
                    title: qsTr("Ваш опыт"),
                    options: [
                        {
                            id: "beginner",
                            label: qsTr("Начинаю")
                        },
                        {
                            id: "intermediate",
                            label: qsTr("Есть опыт")
                        },
                        {
                            id: "advanced",
                            label: qsTr("Опытный")
                        }
                    ]
                },
                {
                    id: "days",
                    title: qsTr("Сколько дней в неделю?"),
                    options: [
                        {
                            id: "2",
                            label: qsTr("2 дня")
                        },
                        {
                            id: "3",
                            label: qsTr("3 дня")
                        },
                        {
                            id: "4",
                            label: qsTr("4 дня")
                        }
                    ]
                },
                {
                    id: "equipment",
                    title: qsTr("Доступный инвентарь"),
                    options: [
                        {
                            id: "gym",
                            label: qsTr("Тренажёрный зал")
                        },
                        {
                            id: "dumbbells",
                            label: qsTr("Гантели")
                        },
                        {
                            id: "bodyweight",
                            label: qsTr("Свой вес")
                        }
                    ]
                },
                {
                    id: "goal",
                    title: qsTr("Главная цель"),
                    options: [
                        {
                            id: "strength",
                            label: qsTr("Сила")
                        },
                        {
                            id: "muscle",
                            label: qsTr("Масса")
                        },
                        {
                            id: "fitness",
                            label: qsTr("Тонус")
                        }
                    ]
                }
            ],
            defaultAnswers: ({
                    experience: "beginner",
                    days: "3",
                    equipment: "gym",
                    goal: "strength"
                }),
            resultPlanId: "plan-full-body-beginner"
        })

    // — Базовые планы тренировок (§2.22) —
    readonly property var plans: [
        {
            id: "plan-full-body-beginner",
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
            id: "plan-push-pull-legs",
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
            id: "plan-upper-lower",
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
            id: "plan-maintenance",
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
            id: "2e8cbeec-3528-45bc-908b-cdf76944d9c3",
            initials: "ИМ",
            name: qsTr("Игорь М."),
            email: "igor@example.com",
            age: 27,
            gender: "male",
            heightCm: 182,
            weightKg: 78.0,
            level: "beginner",
            goal: "strength",
            workoutsPerWeek: 3,
            details: qsTr("27 лет · 182 см · 78 кг · цель: Сила"),
            syncState: "synced"
        })

    // — Редактируемые параметры профиля (issue 20) —
    readonly property var profileEditFields: [
        {
            id: "age",
            section: "server",
            label: qsTr("Возраст"),
            value: 27,
            valueType: "integer",
            minimum: 14,
            maximum: 100,
            unit: qsTr("лет")
        },
        {
            id: "gender",
            section: "server",
            label: qsTr("Пол"),
            value: "male",
            valueType: "choice",
            options: ["male", "female"],
            minimum: 0,
            maximum: 0,
            unit: ""
        },
        {
            id: "heightCm",
            section: "server",
            label: qsTr("Рост"),
            value: 182,
            valueType: "integer",
            minimum: 100,
            maximum: 250,
            unit: qsTr("см")
        },
        {
            id: "weightKg",
            section: "server",
            label: qsTr("Вес"),
            value: 78,
            valueType: "integer",
            minimum: 30,
            maximum: 300,
            unit: qsTr("кг")
        },
        {
            id: "level",
            section: "local",
            label: qsTr("Уровень"),
            value: "beginner",
            valueType: "choice",
            options: ["beginner", "intermediate", "advanced"],
            minimum: 0,
            maximum: 0,
            unit: ""
        },
        {
            id: "goal",
            section: "local",
            label: qsTr("Цель"),
            value: "strength",
            valueType: "choice",
            options: ["strength", "muscle", "fitness"],
            minimum: 0,
            maximum: 0,
            unit: ""
        },
        {
            id: "workoutsPerWeek",
            section: "local",
            label: qsTr("Тренировок в неделю"),
            value: 3,
            valueType: "integer",
            minimum: 1,
            maximum: 7,
            unit: ""
        }
    ]

    // — Параметры профиля для вкладки «Параметры» —
    readonly property var profileParams: [
        {
            id: "gender",
            label: qsTr("Пол"),
            value: qsTr("Мужской")
        },
        {
            id: "age",
            label: qsTr("Возраст"),
            value: qsTr("27 лет")
        },
        {
            id: "heightCm",
            label: qsTr("Рост"),
            value: qsTr("182 см")
        },
        {
            id: "weightKg",
            label: qsTr("Вес"),
            value: qsTr("78 кг")
        },
        {
            id: "level",
            label: qsTr("Уровень"),
            value: qsTr("Новичок")
        },
        {
            id: "goal",
            label: qsTr("Цель"),
            value: qsTr("Сила")
        },
        {
            id: "workoutsPerWeek",
            label: qsTr("Тренировок в неделю"),
            value: "3"
        }
    ]
    readonly property var profileProgressPoints: [
        {
            id: "profile-progress-01",
            date: "2026-07-24",
            value: 48.0
        },
        {
            id: "profile-progress-02",
            date: "2026-07-31",
            value: 50.0
        },
        {
            id: "profile-progress-03",
            date: "2026-08-07",
            value: 50.0
        },
        {
            id: "profile-progress-04",
            date: "2026-08-14",
            value: 52.5
        },
        {
            id: "profile-progress-05",
            date: "2026-08-21",
            value: 55.0
        },
        {
            id: "profile-progress-06",
            date: "2026-08-28",
            value: 57.5
        },
        {
            id: "profile-progress-07",
            date: "2026-09-04",
            value: 57.5
        },
        {
            id: "profile-progress-08",
            date: "2026-09-12",
            value: 60.0
        }
    ]

    // — Список личных рекордов (референс 2085) —
    readonly property var records: [
        {
            id: "record-barbell-squat",
            exerciseId: "exercise-barbell-squat",
            title: qsTr("Приседания со штангой"),
            valueText: "80 × 5",
            dateText: "07.09"
        },
        {
            id: "record-bench-press",
            exerciseId: "exercise-bench-press",
            title: qsTr("Жим лёжа"),
            valueText: "57,5 × 5",
            dateText: "31.08"
        },
        {
            id: "record-romanian-deadlift",
            exerciseId: "exercise-romanian-deadlift",
            title: qsTr("Румынская тяга"),
            valueText: "90 × 6",
            dateText: "24.08"
        },
        {
            id: "record-lat-pulldown",
            exerciseId: "exercise-lat-pulldown",
            title: qsTr("Тяга верхнего блока"),
            valueText: "65 × 8",
            dateText: "12.09"
        }
    ]
    readonly property var regularityWeeks: [
        {
            id: "regularity-01",
            weekStart: "2026-03-30",
            workoutCount: 1
        },
        {
            id: "regularity-02",
            weekStart: "2026-04-06",
            workoutCount: 2
        },
        {
            id: "regularity-03",
            weekStart: "2026-04-13",
            workoutCount: 3
        },
        {
            id: "regularity-04",
            weekStart: "2026-04-20",
            workoutCount: 2
        },
        {
            id: "regularity-05",
            weekStart: "2026-04-27",
            workoutCount: 0
        },
        {
            id: "regularity-06",
            weekStart: "2026-05-04",
            workoutCount: 1
        },
        {
            id: "regularity-07",
            weekStart: "2026-05-11",
            workoutCount: 2
        },
        {
            id: "regularity-08",
            weekStart: "2026-05-18",
            workoutCount: 3
        },
        {
            id: "regularity-09",
            weekStart: "2026-05-25",
            workoutCount: 2
        },
        {
            id: "regularity-10",
            weekStart: "2026-06-01",
            workoutCount: 1
        },
        {
            id: "regularity-11",
            weekStart: "2026-06-08",
            workoutCount: 0
        },
        {
            id: "regularity-12",
            weekStart: "2026-06-15",
            workoutCount: 2
        },
        {
            id: "regularity-13",
            weekStart: "2026-06-22",
            workoutCount: 3
        },
        {
            id: "regularity-14",
            weekStart: "2026-06-29",
            workoutCount: 2
        },
        {
            id: "regularity-15",
            weekStart: "2026-07-06",
            workoutCount: 1
        },
        {
            id: "regularity-16",
            weekStart: "2026-07-13",
            workoutCount: 1
        },
        {
            id: "regularity-17",
            weekStart: "2026-07-20",
            workoutCount: 0
        },
        {
            id: "regularity-18",
            weekStart: "2026-07-27",
            workoutCount: 2
        },
        {
            id: "regularity-19",
            weekStart: "2026-08-03",
            workoutCount: 3
        },
        {
            id: "regularity-20",
            weekStart: "2026-08-10",
            workoutCount: 2
        },
        {
            id: "regularity-21",
            weekStart: "2026-08-17",
            workoutCount: 1
        },
        {
            id: "regularity-22",
            weekStart: "2026-08-24",
            workoutCount: 3
        },
        {
            id: "regularity-23",
            weekStart: "2026-08-31",
            workoutCount: 2
        },
        {
            id: "regularity-24",
            weekStart: "2026-09-07",
            workoutCount: 3
        }
    ]
    readonly property var restTimer: ({
            durationSeconds: 90,
            remainingSeconds: 74,
            state: "running",
            autoStart: true,
            addSecondsStep: 15,
            defaultSeconds: 90
        })

    // — Упражнения плана на сегодня (§5.1) —
    readonly property var todayExercises: [
        {
            id: "exercise-barbell-squat",
            title: qsTr("Приседания со штангой"),
            setsText: "4 × 8–10",
            weightText: "62,5 кг"
        },
        {
            id: "exercise-bench-press",
            title: qsTr("Жим лёжа"),
            setsText: "4 × 8–10",
            weightText: "50 кг"
        },
        {
            id: "exercise-lat-pulldown",
            title: qsTr("Тяга верхнего блока"),
            setsText: "3 × 10–12",
            weightText: "57,5 кг"
        },
        {
            id: "exercise-seated-dumbbell-press",
            title: qsTr("Жим гантелей сидя"),
            setsText: "3 × 10–12",
            weightText: "16 кг"
        },
        {
            id: "exercise-biceps-curl",
            title: qsTr("Подъём на бицепс"),
            setsText: "3 × 12–15",
            weightText: "14 кг"
        }
    ]
    // — План на сегодня —
    readonly property var todayPlan: ({
            id: "plan-full-body-beginner",
            title: qsTr("Сегодня — тренировка A"),
            badges: [qsTr("Full body"), qsTr("новичок")],
            weekIndex: 3,
            weeksTotal: 12,
            exerciseCount: 5,
            durationText: qsTr("около 55 минут"),
            restDay: false
        })

    // — Личный рекорд для экрана «Сегодня» (референс 2081) —
    readonly property var todayRecord: ({
            label: qsTr("Личный рекорд"),
            title: qsTr("Приседания 80 кг × 5"),
            dateText: qsTr("установлен 7 сентября")
        })
    readonly property var workoutDetails: ({
            id: "workout-2026-09-12",
            planId: "plan-full-body-beginner",
            title: qsTr("Тренировка A · Full body"),
            completedAt: "2026-09-12T19:22:00Z",
            durationMinutes: 52,
            tonnageKg: 12400,
            note: qsTr("Хорошая техника, в следующий раз добавить 2,5 кг в приседания."),
            exercises: [
                {
                    id: "workout-exercise-001",
                    exerciseId: "exercise-barbell-squat",
                    title: qsTr("Приседания со штангой"),
                    orderIndex: 0,
                    sets: [
                        {
                            id: "history-set-001",
                            setNumber: 1,
                            weight: 60.0,
                            reps: 10,
                            rpe: 7.0,
                            note: "",
                            done: true
                        },
                        {
                            id: "history-set-002",
                            setNumber: 2,
                            weight: 62.5,
                            reps: 9,
                            rpe: 8.0,
                            note: "",
                            done: true
                        },
                        {
                            id: "history-set-003",
                            setNumber: 3,
                            weight: 62.5,
                            reps: 8,
                            rpe: 8.5,
                            note: qsTr("Последний повтор тяжёлый"),
                            done: true
                        }
                    ]
                },
                {
                    id: "workout-exercise-002",
                    exerciseId: "exercise-bench-press",
                    title: qsTr("Жим лёжа"),
                    orderIndex: 1,
                    sets: [
                        {
                            id: "history-set-004",
                            setNumber: 1,
                            weight: 50.0,
                            reps: 10,
                            rpe: 7.5,
                            note: "",
                            done: true
                        },
                        {
                            id: "history-set-005",
                            setNumber: 2,
                            weight: 50.0,
                            reps: 9,
                            rpe: 8.0,
                            note: "",
                            done: true
                        }
                    ]
                }
            ]
        })

    // — История и детали завершённой тренировки (issue 19) —
    readonly property var workoutHistory: [
        {
            id: "workout-2026-09-12",
            planId: "plan-full-body-beginner",
            title: qsTr("Тренировка A · Full body"),
            completedAt: "2026-09-12T19:22:00Z",
            dateText: qsTr("12 сентября"),
            durationMinutes: 52,
            durationText: qsTr("52 мин"),
            tonnageKg: 12400,
            tonnageText: "12,4 т",
            exerciseCount: 5
        },
        {
            id: "workout-2026-09-10",
            planId: "plan-full-body-beginner",
            title: qsTr("Тренировка B · Спина"),
            completedAt: "2026-09-10T18:48:00Z",
            dateText: qsTr("10 сентября"),
            durationMinutes: 48,
            durationText: qsTr("48 мин"),
            tonnageKg: 9800,
            tonnageText: "9,8 т",
            exerciseCount: 5
        },
        {
            id: "workout-2026-09-08",
            planId: "plan-full-body-beginner",
            title: qsTr("Тренировка A · Full body"),
            completedAt: "2026-09-08T11:55:00Z",
            dateText: qsTr("8 сентября"),
            durationMinutes: 55,
            durationText: qsTr("55 мин"),
            tonnageKg: 11900,
            tonnageText: "11,9 т",
            exerciseCount: 5
        }
    ]

    // — Активная запись тренировки (issues 10–13) —
    readonly property var workoutSession: ({
            id: "workout-draft-001",
            planId: "plan-full-body-beginner",
            planDayId: "plan-day-a",
            title: qsTr("Тренировка A · Full body"),
            state: "draft",
            startedAt: "2026-09-17T17:30:00Z",
            currentExerciseIndex: 0,
            exerciseCount: 5,
            offline: true
        })
    readonly property var workoutSessionExercises: [
        {
            id: "session-exercise-001",
            exerciseId: "exercise-barbell-squat",
            title: qsTr("Приседания со штангой"),
            orderIndex: 0,
            targetSets: 4,
            targetRepsText: "8–10",
            suggestedWeight: 62.5,
            completedSets: 2
        },
        {
            id: "session-exercise-002",
            exerciseId: "exercise-bench-press",
            title: qsTr("Жим лёжа"),
            orderIndex: 1,
            targetSets: 4,
            targetRepsText: "8–10",
            suggestedWeight: 50.0,
            completedSets: 0
        },
        {
            id: "session-exercise-003",
            exerciseId: "exercise-lat-pulldown",
            title: qsTr("Тяга верхнего блока"),
            orderIndex: 2,
            targetSets: 3,
            targetRepsText: "10–12",
            suggestedWeight: 57.5,
            completedSets: 0
        },
        {
            id: "session-exercise-004",
            exerciseId: "exercise-seated-dumbbell-press",
            title: qsTr("Жим гантелей сидя"),
            orderIndex: 3,
            targetSets: 3,
            targetRepsText: "10–12",
            suggestedWeight: 16.0,
            completedSets: 0
        },
        {
            id: "session-exercise-005",
            exerciseId: "exercise-biceps-curl",
            title: qsTr("Подъём на бицепс"),
            orderIndex: 4,
            targetSets: 3,
            targetRepsText: "12–15",
            suggestedWeight: 14.0,
            completedSets: 0
        }
    ]
    readonly property var workoutSets: [
        {
            id: "set-001",
            sessionExerciseId: "session-exercise-001",
            setNumber: 1,
            weight: 60.0,
            reps: 10,
            rpe: 7.5,
            note: "",
            done: true
        },
        {
            id: "set-002",
            sessionExerciseId: "session-exercise-001",
            setNumber: 2,
            weight: 62.5,
            reps: 9,
            rpe: 8.0,
            note: qsTr("Держать колени по линии носков"),
            done: true
        },
        {
            id: "set-003",
            sessionExerciseId: "session-exercise-001",
            setNumber: 3,
            weight: 62.5,
            reps: 0,
            rpe: 0.0,
            note: "",
            done: false
        }
    ]
}
