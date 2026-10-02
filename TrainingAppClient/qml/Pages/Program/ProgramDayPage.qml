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

    // Имена дней недели в ISO-порядке 1…7 (§3.4).
    readonly property var fullWeekdayNames: [qsTr("Понедельник"), qsTr("Вторник"), qsTr("Среда"), qsTr("Четверг"), qsTr("Пятница"), qsTr("Суббота"), qsTr("Воскресенье")]

    // Каталог упражнений пока минимальный: без него день нельзя наполнить.
    readonly property var exerciseOptions: [
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
    readonly property int exerciseCount: viewModel !== undefined && viewModel !== null ? viewModel.day_exercises.rowCount() : 0
    caption: qsTr("%1 упражнений").arg(page.exerciseCount)
    title: page.weekdayName

    signal backRequested
    signal closeRequested

    Component.onCompleted: page.viewModel.selectDay(page.weekday)

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
            AppComboBox {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                model: page.exerciseOptions
                onActivated: page.selectedExerciseId = currentValue
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: Spacing.itemGap

                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    color: Theme.textSecondary
                    font: Typography.caption
                    text: qsTr("Подходы")
                    wrapMode: Text.WordWrap
                }
                AppNumberField {
                    from: 1
                    Layout.preferredWidth: 152
                    onEdited: newValue => page.setsCount = newValue
                    step: 1
                    to: 20
                    value: page.setsCount
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
                    text: qsTr("Вес, кг")
                    wrapMode: Text.WordWrap
                }
                AppNumberField {
                    from: 0
                    // Шаг 2,5 кг: штанга собирается из дисков по 1,25 кг, поэтому
                    // целые значения недостижимы. Раньше шаг был целым — только
                    // потому, что штатный SpinBox дробные шаги терял.
                    decimals: 1
                    Layout.preferredWidth: 152
                    onEdited: newValue => page.targetWeight = newValue
                    step: 2.5
                    suffix: qsTr(" кг")
                    to: 500
                    value: page.targetWeight
                }
            }
            AppButton {
                Layout.fillWidth: true
                onClicked: page.addExercise()
                text: qsTr("Добавить в день")
                variant: AppButton.Primary
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
    // — Действия с днём —
    RowLayout {
        width: parent.width
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

    function addExercise() {
        if (viewModel === undefined || viewModel === null)
            return;

        viewModel.addDayExercise(weekday, selectedExerciseId, setsCount, 5, 8, targetWeight, targetWeight > 0);
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

    // Название упражнения ищется в каталоге по идентификатору: в модели лежит
    // только ex_squat, а пользователю нужно видеть «Присед».
    function exerciseTitle(id) {
        for (let i = 0; i < exerciseOptions.length; ++i) {
            if (exerciseOptions[i].id === id)
                return exerciseOptions[i].title;
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
