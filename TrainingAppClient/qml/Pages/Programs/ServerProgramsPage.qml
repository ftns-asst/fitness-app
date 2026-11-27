import QtQuick
import QtQuick.Controls
import TrainingAppClient

PageScaffold {
    id: page

    property string initialProgramId: ""
    property string selectedId: initialProgramId
    property var viewModel: typeof ServerProgramsVM !== "undefined" ? ServerProgramsVM : null
    property var inputs: ({})
    property bool workoutPending: false
    readonly property bool signedIn: !!viewModel && viewModel.authenticated
    readonly property bool busy: !!viewModel && viewModel.busy
    readonly property var draft: signedIn && selectedId !== "" && viewModel.draft.id === selectedId ? viewModel.draft : ({})
    readonly property var measurements: draft.required_measurements || []
    readonly property var validRecords: recordsForSave(measurements, inputs)
    readonly property bool activeProgram: draft.state === "in_progress"
    readonly property var weeks: activeProgram ? (draft.weeks || []).filter(week => week.days.some(day => day.exercises.length > 0)) : []
    readonly property var weekdays: [qsTr("Понедельник"), qsTr("Вторник"), qsTr("Среда"), qsTr("Четверг"), qsTr("Пятница"), qsTr("Суббота"), qsTr("Воскресенье")]

    signal backRequested
    signal loginRequested
    signal workoutRequested(string programId, int week, int day, string title, var plan)

    title: selectedId === "" ? qsTr("Серверные программы") : activeProgram ? qsTr("Активная программа") : qsTr("Черновик программы")

    Component.onCompleted: {
        if (!draft.id)
            reload();
    }
    Connections {
        target: page.viewModel

        function onAccountChanged() {
            page.workoutPending = false;
            page.inputs = {};
            page.selectedId = "";
            page.reload();
        }

        function onWorkoutReady(programId, week, day, title, plan) {
            if (page.workoutPending && page.selectedId === programId) {
                page.workoutPending = false;
                page.workoutRequested(programId, week, day, title, plan);
            }
        }
    }

    function reload() {
        if (!signedIn)
            return;
        if (selectedId === "")
            viewModel.loadDrafts();
        else
            viewModel.loadDraft(selectedId);
    }

    function recordsForSave(required, values) {
        const records = [];
        for (const exercise of required) {
            const input = values[exercise.exercise_id];
            if (!input || (!input.weight && !input.reps))
                continue;
            const weight = Number((input.weight || "").replace(",", "."));
            const reps = input.reps ? Number(input.reps) : 1;
            if (!(weight > 0 && weight < 10000) || !Number.isInteger(reps) || reps < 1 || reps > 2147483647)
                return [];
            records.push({
                exercise_id: exercise.exercise_id,
                weight_kg: weight,
                reps: reps
            });
            if (records.length === 100)
                break;
        }
        return records;
    }

    function editInput(exerciseId, field, text) {
        const next = Object.assign({}, inputs);
        const entry = Object.assign({}, next[exerciseId]);
        entry[field] = text.trim();
        next[exerciseId] = entry;
        inputs = next;
    }

    BackBar {
        text: page.selectedId !== "" && page.initialProgramId === "" ? qsTr("Программы") : qsTr("Назад")
        width: parent.width

        onClicked: {
            if (page.selectedId !== "" && page.initialProgramId === "" && !page.busy) {
                page.selectedId = "";
                page.inputs = {};
                page.reload();
            } else {
                page.backRequested();
            }
        }
    }
    AppButton {
        text: qsTr("Войти")
        variant: AppButton.Primary
        visible: !page.signedIn
        width: parent.width

        onClicked: page.loginRequested()
    }
    Label {
        color: Theme.textSecondary
        font: Typography.body
        text: qsTr("Обновляем данные…")
        visible: page.signedIn && page.busy
        width: parent.width
    }
    AppCard {
        visible: page.signedIn && !page.busy && page.viewModel.errorKey !== ""
        width: parent.width

        Label {
            color: Theme.textSecondary
            font: Typography.body
            text: qsTr("Не удалось выполнить запрос. Обновите данные перед повторной попыткой.")
            width: parent.width
            wrapMode: Text.WordWrap
        }
        AppButton {
            text: qsTr("Обновить")
            variant: AppButton.Secondary
            width: parent.width

            onClicked: page.reload()
        }
    }
    Column {
        spacing: Spacing.listGap
        visible: page.signedIn && page.selectedId === ""
        width: parent.width

        Label {
            color: Theme.textSecondary
            font: Typography.body
            text: qsTr("Серверных программ пока нет")
            visible: page.signedIn && !page.busy && page.viewModel.errorKey === "" && page.viewModel.drafts.length === 0
            width: parent.width
            wrapMode: Text.WordWrap
        }
        Repeater {
            model: page.signedIn && page.selectedId === "" ? page.viewModel.drafts : []

            delegate: AppCard {
                id: draftCard

                required property var modelData

                width: parent.width

                Label {
                    color: Theme.textPrimary
                    font: Typography.sectionTitle
                    text: draftCard.modelData.name
                    textFormat: Text.PlainText
                    width: parent.width
                    wrapMode: Text.WordWrap
                }
                Label {
                    color: Theme.textSecondary
                    font: Typography.caption
                    text: draftCard.modelData.state === "in_progress" ? qsTr("Активная") : draftCard.modelData.state === "needs_measurement" ? qsTr("Нужны личные рекорды") : qsTr("Черновик")
                }
                AppButton {
                    enabled: !page.busy
                    text: qsTr("Открыть")
                    variant: AppButton.Secondary
                    width: parent.width

                    onClicked: {
                        page.selectedId = draftCard.modelData.id;
                        page.inputs = {};
                        page.reload();
                    }
                }
            }
        }
    }
    Label {
        color: Theme.textPrimary
        font: Typography.sectionTitle
        text: page.draft.name || ""
        textFormat: Text.PlainText
        visible: !!page.draft.id
        width: parent.width
        wrapMode: Text.WordWrap
    }
    AppCard {
        visible: page.draft.state === "draft" && page.measurements.length === 0
        width: parent.width

        Label {
            color: Theme.textSecondary
            font: Typography.body
            text: qsTr("При запуске текущая активная серверная программа завершится.")
            width: parent.width
            wrapMode: Text.WordWrap
        }
        AppButton {
            enabled: !page.busy && page.viewModel.errorKey === ""
            text: qsTr("Запустить программу")
            variant: AppButton.Primary
            width: parent.width

            onClicked: page.viewModel.startDraft(page.selectedId)
        }
    }
    AppCard {
        id: workoutCard

        readonly property var week: page.weeks[weekPicker.currentIndex]
        readonly property var days: week ? week.days.filter(day => day.exercises.length > 0) : []
        readonly property var day: days[dayPicker.currentIndex]

        visible: page.activeProgram && page.weeks.length > 0
        width: parent.width

        AppComboBox {
            id: weekPicker

            Accessible.name: qsTr("Неделя программы")
            model: page.weeks.map(week => ({
                        id: week.week_number,
                        title: qsTr("Неделя %1").arg(week.week_number)
                    }))
            width: parent.width

            onCurrentIndexChanged: dayPicker.currentIndex = 0
        }
        AppComboBox {
            id: dayPicker

            Accessible.name: qsTr("День тренировки")
            model: workoutCard.days.map(day => ({
                        id: day.day_of_week,
                        title: page.weekdays[day.day_of_week - 1] || ""
                    }))
            width: parent.width
        }
        AppButton {
            enabled: !page.busy && !!workoutCard.day
            objectName: "openServerWorkoutButton"
            text: qsTr("Открыть тренировку")
            variant: AppButton.Primary
            width: parent.width

            onClicked: {
                page.workoutPending = true;
                page.viewModel.prepareWorkout(workoutCard.week.week_number, workoutCard.day.day_of_week);
            }
        }
    }
    Label {
        color: Theme.textSecondary
        font: Typography.body
        text: page.measurements.length > 0 ? qsTr("Для расчёта нагрузки введите вес на один повтор или результат рабочего подхода. Можно сохранить часть рекордов и вернуться позже.") : qsTr("Черновик сохранён. Недостающих рекордов нет.")
        visible: !!page.draft.id && !page.activeProgram
        width: parent.width
        wrapMode: Text.WordWrap
    }
    Repeater {
        model: page.measurements

        delegate: AppCard {
            id: exercise

            required property var modelData
            readonly property string exerciseId: modelData.exercise_id

            width: parent.width

            Label {
                color: Theme.textPrimary
                font: Typography.sectionTitle
                text: exercise.modelData.exercise_name
                textFormat: Text.PlainText
                width: parent.width
                wrapMode: Text.WordWrap
            }
            AppTextField {
                id: weightField

                enabled: !page.busy
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                label: qsTr("Вес, кг")
                text: (page.inputs[exercise.exerciseId] || {}).weight || ""
                width: parent.width

                onUserEdited: page.editInput(exercise.exerciseId, "weight", weightField.text)
            }
            AppTextField {
                id: repsField

                enabled: !page.busy
                inputMethodHints: Qt.ImhDigitsOnly
                label: qsTr("Повторы (по умолчанию 1)")
                text: (page.inputs[exercise.exerciseId] || {}).reps || ""
                width: parent.width

                onUserEdited: page.editInput(exercise.exerciseId, "reps", repsField.text)
            }
        }
    }
    AppButton {
        enabled: page.validRecords.length > 0 && !page.busy && page.viewModel.errorKey === ""
        text: qsTr("Сохранить рекорды (%1)").arg(page.validRecords.length)
        variant: AppButton.Primary
        visible: page.measurements.length > 0
        width: parent.width

        onClicked: page.viewModel.saveRecords(page.selectedId, page.validRecords)
    }
}
