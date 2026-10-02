import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Создание программы (экран 42, UC-1/UC-2). Настраивается срок, цель и
// КАЖДЫЙ день недели отдельно — это ядро продукта (§3.4).
//
// Экран работает через Avm_Program_Edit: правила создания (рабочий день
// требует упражнения, измеримая цель — цели) проверяет ViewModel, а экран
// лишь показывает errorKey и не даёт отправить заведомо плохое.
PageScaffold {
    id: page

    // Каталог упражнений на этом этапе минимален: полный каталог наполняется
    // в фазе B (issue 23), поэтому здесь два упражнения-заглушки.
    readonly property var exerciseOptions: [
        {
            id: "ex_squat",
            title: qsTr("Присед")
        },
        {
            id: "ex_bench",
            title: qsTr("Жим лёжа")
        }
    ]
    readonly property var weekdayNames: [qsTr("Пн"), qsTr("Вт"), qsTr("Ср"), qsTr("Чт"), qsTr("Пт"), qsTr("Сб"), qsTr("Вс")]
    property var viewModel: ProgramEditVM
    // Программа для правки. Пусто — создание новой; каталог и обзор передают
    // идентификатор существующей (срез S2).
    property string programId: ""
    // Панель возврата обязательна: экраны программы открыты поверх вкладок, где
    // таб-бара нет.
    BackBar {
        Layout.fillWidth: true
        onClicked: page.backRequested()
        text: qsTr("Назад")
        visible: true
    }

    caption: qsTr("%1 тренировочных дней").arg(page.trainingDays)
    title: qsTr("Новая программа")

    readonly property bool canCreate: page.viewModel !== undefined && page.viewModel !== null && page.viewModel.canCreate
    // Черновик, а не созданная программа: reset() уже наполняет семь дней недели,
    readonly property bool hasProgram: page.viewModel !== undefined && page.viewModel !== null && page.viewModel.hasDraft
    readonly property int trainingDays: page.viewModel !== undefined && page.viewModel !== null ? page.viewModel.trainingDays : 0

    Component.onCompleted: page.reset()

    function reset() {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        // С идентификатором открывается существующая программа, без него —
        // пустой редактор создания. Создание всегда начинается с reset(),
        // иначе семь дней прежней программы остались бы в форме.
        if (page.programId.length > 0)
            page.viewModel.load("u1", page.programId);
        else
            page.viewModel.reset("u1");
    }

    AppCard {
        width: parent.width
        ColumnLayout {
            // Внутри AppCard ширину задаёт родитель, а не Layout: содержимое
            // карточки лежит в обычной Column, где Layout-свойства не работают.
            width: parent.width
            spacing: Spacing.itemGap

            AppTextField {
                Layout.fillWidth: true
                label: qsTr("Название программы")
                onTextChanged: page.viewModel.setTitle(text)
                text: page.hasProgram ? page.viewModel.title : ""
            }
            DateField {
                Layout.fillWidth: true
                label: qsTr("Дата старта")
                onEdited: function (isoDate) {
                    page.viewModel.setStartDate(isoDate);
                }
                value: page.hasProgram ? page.viewModel.startDate : ""
            }
            Label {
                Layout.fillWidth: true
                color: Theme.textSecondary
                font: Typography.caption
                text: qsTr("Срок программы")
                wrapMode: Text.WordWrap
            }
            // Срок выбирается кнопками, а не вводится текстом: на узком экране
            // поле с числом обрезалось, а пустое значение давало NaN в модели.
            RowLayout {
                Layout.fillWidth: true
                spacing: Spacing.itemGap

                Repeater {
                    model: page.weekOptions

                    delegate: AppButton {
                        required property int modelData

                        Layout.fillWidth: true
                        labelFont: Typography.captionStrong
                        height: Theme.buttonSecondary
                        onClicked: page.viewModel.setWeeks(modelData)
                        text: qsTr("%1").arg(modelData)
                        variant: page.weeks === modelData ? AppButton.Primary : AppButton.Secondary
                    }
                }
            }
        }
    }
    // — Неделя-карта —
    // Семь дней показаны сеткой, а не списком: список из семи крупных карточек
    // прокручивается и не даёт увидеть неделю целиком, а карта помещается в экран
    // и отвечает на вопрос «какая у меня неделя» одним взглядом (§3.4).
    Label {
        color: Theme.textMuted
        font: Typography.sectionTitle
        text: qsTr("Неделя")
    }
    Label {
        color: Theme.textMuted
        font: Typography.caption
        // Подсказка ведёт прямо к следующему действию, а не пересказывает
        // то, что уже видно на карте.
        text: qsTr("Нажмите на день, чтобы настроить его упражнения")
        width: parent.width
        wrapMode: Text.WordWrap
    }
    Flow {
        // Сетка строится через Flow с явной шириной карточки: GridLayout
        // распределял колонки по неявной ширине и уводил вторую за край.
        id: weekMap

        width: parent.width
        spacing: Spacing.itemGap

        Repeater {
            model: page.hasProgram ? page.viewModel.program_days : []

            delegate: DayMapCell {
                // Роли берутся через model.имя, а не через обязательные свойства:
                // объявление required для AList_Model не срабатывает, делегат не
                // создаётся, и неделя остаётся пустой.
                cellExerciseCount: model.exerciseCount
                cellKind: model.kind
                cellWeekday: model.weekday
                cellWeekdayName: page.weekdayNames[model.weekday - 1]
                height: 96
                onClicked: page.dayRequested(model.weekday, page.kindValue(model.kind))
                // Ширина считается от ширины страницы, а не от ширины сетки: в
                // момент создания делегата ширина самой сетки ещё не посчитана,
                // карточки получали ноль, и неделя не отображалась.
                width: Math.floor((page.width - Spacing.screenPadding * 2 - Spacing.itemGap) / (page.width >= 600 ? 3 : 2))
            }
        }
    }
    // — Цели —
    // Цели вводятся на создании, а не после: без измеримой цели программу нельзя
    // активировать (инвариант 8), и пользователь упирался в ошибку в самом конце.
    Label {
        color: Theme.textMuted
        font: Typography.sectionTitle
        text: qsTr("Цели")
    }
    ColumnLayout {
        id: goalList

        width: parent.width
        spacing: Spacing.itemGap

        Repeater {
            model: page.hasProgram ? page.viewModel.goal_targets : []

            delegate: AppCard {
                id: goalRow

                required property string displayText
                required property int index

                Layout.fillWidth: true
                borderColor: Theme.borderSubtle

                RowLayout {
                    width: parent.width
                    spacing: Spacing.itemGap

                    Label {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        color: Theme.textPrimary
                        font: Typography.body
                        text: goalRow.displayText
                        wrapMode: Text.WordWrap
                    }
                    AppButton {
                        // Цель обязана удаляться: ошибочно выбранную метрику
                        // иначе не исправить, и активация была бы заблокирована.
                        Layout.minimumWidth: 0
                        onClicked: page.viewModel.removeTarget(goalRow.index)
                        padding: Spacing.itemGap
                        text: qsTr("Убрать")
                        variant: AppButton.Secondary
                    }
                }
            }
        }
    }
    AppCard {
        width: parent.width

        ColumnLayout {
            width: parent.width
            spacing: Spacing.itemGap

            Label {
                Layout.fillWidth: true
                color: Theme.textMuted
                font: Typography.caption
                text: qsTr("Добавить цель")
            }
            AppComboBox {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                model: page.goalMetricOptions
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Spacing.itemGap

                AppButton {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    labelFont: Typography.captionStrong
                    onClicked: page.addGoal(1)
                    padding: Spacing.itemGap
                    text: qsTr("Снизить")
                    variant: AppButton.Secondary
                }
                AppButton {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    labelFont: Typography.captionStrong
                    onClicked: page.addGoal(0)
                    padding: Spacing.itemGap
                    text: qsTr("Повысить")
                    variant: AppButton.Secondary
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Spacing.itemGap

                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    color: Theme.textSecondary
                    font: Typography.caption
                    text: qsTr("Целевое значение")
                    wrapMode: Text.WordWrap
                }
                AppNumberField {
                    decimals: 1
                    from: 0
                    Layout.preferredWidth: 152
                    onEdited: newValue => page.goalTargetValue = newValue
                    step: 0.5
                    to: 500
                    value: page.goalTargetValue
                }
            }
        }
    }
    // — Правила —
    // Правила набираются до создания программы: они и есть «что соблюдать»,
    // а проверяются по отметкам уже во время выполнения (§3.6).
    Label {
        color: Theme.textMuted
        font: Typography.sectionTitle
        text: qsTr("Правила")
    }
    ColumnLayout {
        id: ruleList

        width: parent.width
        spacing: Spacing.itemGap

        Repeater {
            model: page.hasProgram ? page.viewModel.rules : []

            delegate: AppCard {
                id: ruleRow

                required property string displayText
                required property int index

                Layout.fillWidth: true
                borderColor: Theme.borderSubtle

                RowLayout {
                    width: parent.width
                    spacing: Spacing.itemGap

                    Label {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        color: Theme.textPrimary
                        font: Typography.body
                        text: ruleRow.displayText
                        wrapMode: Text.WordWrap
                    }
                    AppButton {
                        // Правило обязано удаляться: опечатку в названии иначе не
                        // исправить, и программа осталась бы с мусором.
                        Layout.minimumWidth: 0
                        onClicked: page.viewModel.removeRule(ruleRow.index)
                        padding: Spacing.itemGap
                        text: qsTr("Убрать")
                        variant: AppButton.Secondary
                    }
                }
            }
        }
    }
    AppCard {
        width: parent.width

        ColumnLayout {
            width: parent.width
            spacing: Spacing.itemGap

            Label {
                Layout.fillWidth: true
                color: Theme.textMuted
                font: Typography.caption
                text: qsTr("Типовые правила")
            }
            // Типовые наборы добавляются одним нажатием: заставлять вводить
            // «белок 1,6 г на кг» вручную — способ отпугнуть от настройки.
            Flow {
                Layout.fillWidth: true
                spacing: Spacing.itemGap

                Repeater {
                    model: page.rulePresets

                    delegate: AppButton {
                        required property var modelData

                        Layout.minimumWidth: 0
                        labelFont: Typography.captionStrong
                        onClicked: page.applyPreset(modelData)
                        padding: Spacing.itemGap
                        text: modelData.title
                        variant: AppButton.Secondary
                    }
                }
            }
        }
    }
    AppCard {
        width: parent.width
        visible: page.errorKey.length > 0

        Label {
            color: Theme.negative
            font: Typography.body
            text: page.errorText()
            width: parent.width
            wrapMode: Text.WordWrap
        }
    }
    // Кнопка всегда активна и сама объясняет, чего не хватает. Раньше она
    // была disabled при незаполненной форме, и нажатие просто ничего не
    // делало — пользователь не понимал, что именно нужно дозаполнить.
    AppButton {
        Layout.fillWidth: true
        onClicked: page.create()
        text: qsTr("Создать программу")
        variant: AppButton.Primary
    }
    Label {
        // Ширина задаётся родителем: страница лежит в обычной Column
        // PageScaffold, где Layout-свойства не действуют, и текст уезжал
        // за край экрана.
        width: parent.width
        color: Theme.negative
        font: Typography.caption
        text: page.missingHint()
        visible: page.canCreate === false
        wrapMode: Text.WordWrap
    }

    // Варианты срока: конечный список вместо свободного ввода, иначе на
    // узком экране поле обрезалось, а пустое значение давало NaN в модели.
    readonly property var weekOptions: [4, 8, 12, 16]
    readonly property int weeks: viewModel !== undefined && viewModel !== null ? viewModel.weeks : 0

    // Метрики целей. Значения — EGoal_Metric (§7: в QML числа, не enum).
    readonly property var goalMetricOptions: [
        {
            title: qsTr("Вес тела, кг"),
            value: 0
        },
        {
            title: qsTr("Рабочий вес, кг"),
            value: 1
        },
        {
            title: qsTr("Процент жира"),
            value: 2
        },
        {
            title: qsTr("Обхваты, см"),
            value: 3
        },
        {
            title: qsTr("Тренировок в неделю"),
            value: 4
        },
        {
            title: qsTr("Тоннаж в неделю, кг"),
            value: 5
        }
    ]
    // Типовые правила: «белок», «сон», «шаги» закрывают большинство запросов
    // пользователя без ввода чисел руками.
    readonly property var rulePresets: [
        {
            title: qsTr("Белок 140 г"),
            code: "protein",
            metric: 0,
            unit: 0,
            min: 140,
            max: 0,
            hasMin: true,
            hasMax: false,
            category: 0,
            periodicity: 0
        },
        {
            title: qsTr("Сон 8 ч"),
            code: "sleep",
            metric: 3,
            unit: 4,
            min: 8,
            max: 0,
            hasMin: true,
            hasMax: false,
            category: 2,
            periodicity: 0
        },
        {
            title: qsTr("10 000 шагов"),
            code: "steps",
            metric: 4,
            unit: 5,
            min: 10000,
            max: 0,
            hasMin: true,
            hasMax: false,
            category: 3,
            periodicity: 0
        },
        {
            title: qsTr("Вода 2 л"),
            code: "water",
            metric: 2,
            unit: 3,
            min: 2000,
            max: 0,
            hasMin: true,
            hasMax: false,
            category: 0,
            periodicity: 0
        }
    ]
    property double goalTargetValue: 75
    property int selectedGoalMetric: 0
    // Открытый день недели: карта не меняет состояние сама, она только
    // сообщает, какой день нажали.
    signal dayRequested(int weekday, int kind)

    function addGoal(direction) {
        if (viewModel === undefined || viewModel === null)
            return;

        // Стартовое значение не спрашивается: его уточняют замеры позже, а пустое
        // поле в форме цели отпугивает сильнее, чем лишний вопрос.
        viewModel.setTarget(selectedGoalMetric, -1, false, goalTargetValue, direction);
    }

    function applyPreset(preset) {
        if (viewModel === undefined || viewModel === null)
            return;

        viewModel.addRuleDetailed(preset.code, preset.title, preset.category, preset.metric, preset.periodicity, preset.min, preset.hasMin, preset.max, preset.hasMax, preset.unit, 0, 0);
    }

    // Вид дня приходит из модели строкой, а экран дня ждёт число (§7).
    function kindValue(kind) {
        if (kind === "workout")
            return 0;
        if (kind === "cardio")
            return 2;
        if (kind === "mobility")
            return 3;

        return 1;
    }

    // Запрос возврата: экран открыт поверх вкладок, где таб-бара нет.
    signal backRequested
    signal created

    // Вид дня приходит в C++ числом (EDay_Kind), поэтому QML переводит его в
    // строку один раз здесь, а не разматывает по всему экрану.
    function setKind(weekday, kind) {
        page.viewModel.setDayKind(weekday, kind === "workout" ? 0 : 1);
    }

    function addExercise(weekday) {
        page.viewModel.addDayExercise(weekday, "ex_squat", 4, 5, 8, 100, true);
    }

    function create() {
        if (page.viewModel.create(true) === false)
            return;

        page.created();
    }

    function errorText() {
        const texts = {
            empty_training_day: qsTr("В рабочем дне нужно хотя бы одно упражнение"),
            goal_required: qsTr("Добавьте цель программы"),
            invalid_start_date: qsTr("Проверьте дату старта"),
            invalid_draft: qsTr("Заполните название, дату старта и срок"),
            invalid_term: qsTr("Срок должен быть от 1 до 104 недель"),
            save_failed: qsTr("Не удалось сохранить программу")
        };

        // Техническая причина (FOREIGN KEY, занятый id и т.п.) показывается
        // второй строкой: без неё отказ записи выглядел как «кнопка ничего не
        // делает».
        const detail = page.viewModel !== undefined && page.viewModel !== null ? page.viewModel.errorDetail : "";

        if (page.errorKey === "save_failed" && detail !== "")
            return texts["save_failed"] + ": " + detail;

        return texts[page.errorKey] !== undefined ? texts[page.errorKey] : qsTr("Не удалось создать программу");
    }

    // Что ещё нужно заполнить для создания программы. Условия те же, что у
    // canCreate в C++, но перечисляются здесь: кнопка сама говорит, чего не
    // хватает, вместо молчаливого отказа.
    function missingHint() {
        const missing = [];

        if (page.viewModel === undefined || page.viewModel === null)
            return "";

        if (page.viewModel.startDate === "")
            missing.push(qsTr("укажите дату старта"));
        if (page.viewModel.weeks < 1)
            missing.push(qsTr("укажите срок"));
        if (page.trainingDays === 0)
            missing.push(qsTr("добавьте хотя бы один тренировочный день"));

        if (missing.length === 0)
            return "";

        return qsTr("Чтобы создать программу: %1.").arg(missing.join(qsTr(", ")));
    }

    readonly property string errorKey: page.viewModel !== undefined && page.viewModel !== null ? page.viewModel.errorKey : ""
}
