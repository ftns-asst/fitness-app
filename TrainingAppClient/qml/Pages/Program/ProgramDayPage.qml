import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Настройка одного дня недели (экран 42, §3.4). Открывается поверх карты недели
// и отвечает на вопросы только про этот день: вид дня и состав упражнений.
//
// Состав дня редактируется здесь, а не на карте: семь дней не помещаются в один
// экран вместе с составом, и настроить всё с карты невозможно.
PageScaffold {
    id: page

    property var viewModel: ProgramEditVM
    property int weekday: 1
    // Вид дня передаётся при открытии: обращаться к строке модели по индексу
    // из QML нельзя, а экран создаётся заново на каждый переход.
    property int kind: 1

    // Ссылка на окно выбора для тестов: Popup живёт в Overlay, а не в дереве
    // детей страницы, и поиском по objectName из страницы не находится.
    readonly property var exercisePickerItem: exercisePicker

    // Имена дней недели в ISO-порядке 1…7 (§3.4).
    readonly property var fullWeekdayNames: [qsTr("Понедельник"), qsTr("Вторник"), qsTr("Среда"), qsTr("Четверг"), qsTr("Пятница"), qsTr("Суббота"), qsTr("Воскресенье")]

    // Каталог упражнений: серверные названия из ExercisesVM (спринт U4, кэш
    // зеркалится в локальный exercises — FK дня программы удовлетворён).
    // Без VM (qmltestrunner) — минимальный локальный набор.
    readonly property var fallbackExerciseOptions: [
        {
            id: "ex_squat",
            title: qsTr("Присед")
        },
        {
            id: "ex_bench",
            title: qsTr("Жим лёжа")
        },
        {
            id: "ex_row",
            title: qsTr("Тяга")
        },
        {
            id: "ex_deadlift",
            title: qsTr("Становая")
        }
    ]
    // Четыре вида дня, а не два: модель различает кардио и мобилити, и прятать
    // их значит запрещать пользователю обычный вид недели.
    readonly property var kindOptions: [
        {
            title: qsTr("Тренировка"),
            value: 0
        },
        {
            title: qsTr("Отдых"),
            value: 1
        },
        {
            title: qsTr("Кардио"),
            value: 2
        },
        {
            title: qsTr("Мобилити"),
            value: 3
        }
    ]

    // Выбор упражнения хранится в свойстве страницы, а не берётся из ComboBox в
    // момент нажатия: иначе кнопка добавляла первое упражнение списка.
    property string selectedExerciseId: "ex_squat"
    property int setsCount: 4
    property double targetWeight: 100

    readonly property string weekdayName: weekday >= 1 && weekday <= 7 ? fullWeekdayNames[weekday - 1] : ""
    // Счётчик — property `count` модели, а не вызов `rowCount()`: вызов метода
    // внутри привязки не создаёт зависимости, счётчик замирает на значении
    // первого вычисления, и добавленное упражнение не показывается без
    // переоткрытия экрана (тот же класс ошибки, что был в RulesPicker).
    readonly property int exerciseCount: viewModel !== undefined && viewModel !== null ? viewModel.day_exercises.count : 0
    caption: qsTr("%1 упражнений").arg(page.exerciseCount)
    title: page.weekdayName

    signal backRequested
    signal closeRequested

    // Строка упражнения дня — самостоятельный объект: тап по карточке
    // открывает его обзор, а не только «Убрать» рядом. Экран сообщает
    // намерение, путь — стек поверх вкладок — выбирает каркас.
    signal exerciseOpenRequested(string exerciseId)

    Component.onCompleted: {
        // Автосохранение включается и здесь: экран дня можно открыть напрямую
        // (--screen program-day), минуя редактор программы.
        if (viewModel !== undefined && viewModel !== null && viewModel.autoSaveDraft !== undefined)
            viewModel.autoSaveDraft = true;

        // Каталог упражнений для окна выбора: без сети отдаёт кэш, а пустой кэш
        // засевается базовым каталогом (Ensure_Offline_Base).
        if (typeof ExercisesVM !== "undefined" && ExercisesVM !== null && ExercisesVM.exercise_list.count === 0)
            ExercisesVM.reload();

        viewModel.selectDay(page.weekday);
    }

    BackBar {
        onClicked: page.backRequested()
        text: qsTr("Назад")
    }
    // — Вид дня —
    AppCard {
        width: parent.width
        ColumnLayout {
            width: parent.width
            spacing: Spacing.itemGap

            Label {
                Layout.fillWidth: true
                color: Theme.textMuted
                font: Typography.caption
                text: qsTr("Вид дня")
            }
            GridLayout {
                Layout.fillWidth: true
                // У GridLayout нет общего spacing, только направленные зазоры:
                // иначе свойство молча игнорировалось бы и кнопки слипались.
                columnSpacing: Spacing.itemGap
                columns: 2
                rowSpacing: Spacing.itemGap

                Repeater {
                    model: page.kindOptions

                    delegate: AppButton {
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        labelFont: Typography.captionStrong
                        onClicked: page.setKind(modelData.value)
                        padding: Spacing.itemGap
                        text: modelData.title
                        variant: page.kind === modelData.value ? AppButton.Primary : AppButton.Secondary
                    }
                }
            }
        }
    }
    // Блок добавления стоит выше списка: добавление — действие по ходу
    // настройки, и после каждого добавления список меняется под формой.
    // Если форма внизу, к ней приходится возвращаться прокруткой.
    // — Добавление упражнения —
    AppCard {
        width: parent.width
        // В день отдыха упражнения не добавляются: такой день задаётся своим
        // видом, и форма здесь только сбивала бы с толку.
        visible: page.kind !== 1

        ColumnLayout {
            width: parent.width
            spacing: Spacing.itemGap

            Label {
                Layout.fillWidth: true
                color: Theme.textMuted
                font: Typography.caption
                text: qsTr("Добавить упражнение")
            }
            // Выбор — в мини-окне, а не в выпадающем списке: настоящий каталог
            // большой, и искать его перебором закрытого ComboBox неудобно. Окно
            // даёт поиск, фильтр по группе и раскрытие строки с подходами и весом.
            AppButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                objectName: "exercisePickerButton"
                onClicked: exercisePicker.open()
                text: qsTr("Выбрать упражнение…")
                variant: AppButton.Primary
            }
            ExercisePickerPopup {
                id: exercisePicker
                // Родитель окна — корень экрана, а не карточка, в которой окно
                // объявлено: Popup без явного родителя позиционируется
                // относительно item-объявления, окно сжималось до ширины
                // карточки «Добавить упражнение» (~290×99) и список в него
                // не помещался. Родитель-страница стабилен с момента создания
                // (id виден внутри всего компонента), в отличие от
                // Overlay.overlay, который при создании окна мог быть null.
                parent: page

                onPicked: function (exerciseId, title, weightKg, sets) {
                    page.addExercise(exerciseId, sets, weightKg);
                    exercisePicker.close();
                }
            }
            // Заготовка выбора готовой тренировки на день: каталога дней в
            // модели пока нет (планы появятся в фазе B), поэтому кнопка
            // только обозначает место. Она заведена сейчас, чтобы будущая
            // функция не ломала вёрстку экрана.
            AppButton {
                Layout.fillWidth: true
                onClicked: page.pickFromTemplates()
                text: qsTr("Из готовых программ")
                variant: AppButton.Secondary
            }
        }
    }
    // — Состав дня —
    AppCard {
        width: parent.width
        ColumnLayout {
            width: parent.width
            spacing: Spacing.itemGap

            Label {
                Layout.fillWidth: true
                color: Theme.textMuted
                font: Typography.caption
                text: qsTr("Упражнения дня")
            }
            ColumnLayout {
                Layout.fillWidth: true
                // objectName для теста: список обязан появляться сразу после
                // добавления, а не после переоткрытия экрана.
                objectName: "dayExerciseList"
                spacing: Spacing.itemGap
                visible: page.exerciseCount > 0

                Repeater {
                    model: viewModel !== undefined && viewModel !== null ? viewModel.day_exercises : []

                    delegate: AppCard {
                        id: exerciseRow

                        required property string exerciseId
                        required property string repsText
                        required property int orderIndex
                        required property int sets
                        required property double targetWeightKg

                        // Упражнение — самостоятельный объект, поэтому у него
                        // своя карточка со своими действиями.
                        Layout.fillWidth: true
                        borderColor: Theme.borderSubtle
                        surfaceColor: Theme.surfaceMuted

                        // Тап по карточке — обзор упражнения (вход из дня
                        // программы, как и с главной). TapHandler, а не
                        // MouseArea поверх: MouseArea перехватил бы нажатия
                        // кнопки «Убрать», а handler видит только события,
                        // не принятые детьми.
                        TapHandler {
                            onTapped: page.exerciseOpenRequested(exerciseRow.exerciseId)
                        }

                        RowLayout {
                            width: parent.width
                            spacing: Spacing.itemGap

                            Label {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                color: Theme.textPrimary
                                font: Typography.bodyStrong
                                text: page.exerciseTitle(exerciseRow.exerciseId)
                                wrapMode: Text.WordWrap
                            }
                            AppButton {
                                // Удаление обязательно: иначе ошибочно
                                // добавленное упражнение осталось бы навсегда.
                                Layout.minimumWidth: 0
                                onClicked: page.removeExercise(exerciseRow.orderIndex - 1)
                                padding: Spacing.itemGap
                                text: qsTr("Убрать")
                                variant: AppButton.Secondary
                            }
                        }
                        Label {
                            width: parent.width
                            color: Theme.textSecondary
                            font: Typography.caption
                            text: page.prescription(exerciseRow.sets, exerciseRow.repsText, exerciseRow.targetWeightKg)
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
            Label {
                Layout.fillWidth: true
                color: Theme.textMuted
                font: Typography.body
                text: qsTr("В этом дне пока нет упражнений")
                visible: page.exerciseCount === 0
                wrapMode: Text.WordWrap
            }
        }
    }
    // — Действия с днём —
    // Панель закреплена внизу: «Готово» завершает работу с днём, и искать его
    // прокруткой вниз длинного списка упражнений незачем. Каркас держит панель
    // на экране и сам резервирует под неё место в прокрутке.
    footer: RowLayout {
        spacing: Spacing.itemGap

        AppButton {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            // Копирование экономит время: семь дней настраиваются вручную, а
            // разница между соседними обычно мала.
            onClicked: page.copyFromPrevious()
            padding: Spacing.itemGap
            text: qsTr("Из предыдущего")
            variant: AppButton.Secondary
        }
        AppButton {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            onClicked: page.closeRequested()
            padding: Spacing.itemGap
            text: qsTr("Готово")
            variant: AppButton.Primary
        }
    }

    function setKind(value) {
        kind = value;

        if (viewModel !== undefined && viewModel !== null)
            viewModel.setDayKind(weekday, value);
    }

    function addExercise(exerciseId, setsCount, weightKg) {
        if (viewModel === undefined || viewModel === null)
            return;

        // Значения по умолчанию — прежние заготовки экрана (тесты и быстрое
        // добавление без окна), выбор из окна передаёт их явно.
        const id = exerciseId !== undefined ? exerciseId : page.selectedExerciseId;
        const sets = setsCount !== undefined ? setsCount : page.setsCount;
        const weight = weightKg !== undefined ? weightKg : page.targetWeight;

        viewModel.addDayExercise(weekday, id, sets, 5, 8, weight, weight > 0);
    }

    function pickFromTemplates() {
        // Каталог готовых тренировок появится вместе с планами (issue 35):
        // таблица plans/plan_days и чтение из репозитория. Пока выбирать
        // нечего, и нажатие намеренно ничего не меняет.
    }

    function removeExercise(index) {
        if (viewModel !== undefined && viewModel !== null)
            viewModel.removeDayExercise(weekday, index);
    }

    function copyFromPrevious() {
        // Из первого дня копировать нечего: без проверки запрос ушёл бы в день 0
        // и показал бы ошибку вместо понятного отказа.
        if (weekday <= 1 || viewModel === undefined || viewModel === null)
            return;

        viewModel.copyDay(weekday - 1, weekday);
    }

    // Название упражнения ищется в каталоге ExercisesVM по идентификатору: в
    // модели дня лежит только id, а пользователю нужно видеть «Присед». Без
    // каталога (qmltestrunner, офлайн до первого засева) — локальный набор.
    function exerciseTitle(id) {
        if (typeof ExercisesVM !== "undefined" && ExercisesVM !== null) {
            const list = ExercisesVM.exercise_list;

            for (let i = 0; i < list.count; ++i) {
                const row = list.get(i);

                if (row.id === id)
                    return row.title;
            }
        }

        for (let j = 0; j < fallbackExerciseOptions.length; ++j) {
            if (fallbackExerciseOptions[j].id === id)
                return fallbackExerciseOptions[j].title;
        }

        return id;
    }

    // Предписание собирается одной строкой «4 × 5–8 · 100 кг»: отдельные поля
    // пользователю ничего не говорят.
    function prescription(sets, reps, weightKg) {
        const parts = [];

        if (sets > 0)
            parts.push(sets + " × " + reps);
        if (weightKg > 0)
            parts.push(weightKg + " " + qsTr("кг"));

        return parts.join(" · ");
    }
}
