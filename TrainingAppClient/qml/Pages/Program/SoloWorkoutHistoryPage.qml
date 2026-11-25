import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

PageScaffold {
    id: page

    property var sync: typeof WorkoutSyncVM !== "undefined" ? WorkoutSyncVM : null
    property var recorder: typeof SoloWorkoutVM !== "undefined" ? SoloWorkoutVM : null
    property string selectedId: ""
    property string startError: ""
    property var plan: []
    readonly property bool signedIn: !!sync && sync.userId !== ""
    readonly property var selected: signedIn && sync.detail.id === selectedId ? sync.detail : ({})
    readonly property string today: Qt.formatDate(new Date(), "yyyy-MM-dd")

    signal backRequested
    signal workoutOpened

    showHeader: false
    Component.onCompleted: {
        if (signedIn)
            sync.refreshHistory();
    }
    Connections {
        target: page.sync
        function onAccountChanged() {
            page.selectedId = "";
            page.startError = "";
            page.plan = [];
            picker.close();
        }
    }

    function dateLabel(timestamp) {
        const date = new Date(timestamp);
        return Number.isNaN(date.getTime()) ? qsTr("Тренировка") : Qt.formatDateTime(date, "dd.MM.yyyy · HH:mm");
    }
    function syncLabel(row) {
        switch (row.sync || "synced") {
        case "pending":
            return qsTr("Ожидает отправки");
        case "synced":
            return qsTr("Синхронизировано");
        case "local":
            return qsTr("На устройстве");
        case "workout_conflict":
            return qsTr("На сервере уже есть другая версия");
        default:
            return qsTr("Не удалось отправить");
        }
    }
    function startOrResume() {
        if (!signedIn || !recorder)
            return;
        startError = "";
        if (!recorder.hasPending) {
            if (plan.length === 0) {
                picker.open();
                return;
            }
            if (recorder.startSolo(sync.userId, today, plan)) {
                plan = [];
                workoutOpened();
                return;
            }
        } else if (recorder.startOrResume(sync.userId, "", today)) {
            workoutOpened();
            return;
        }
        startError = recorder.errorDetail;
    }
    function selectWorkout(workoutId) {
        selectedId = workoutId;
        sync.openWorkout(workoutId);
    }

    BackBar {
        text: page.selectedId ? qsTr("История") : qsTr("Назад")
        width: parent.width
        onClicked: {
            if (page.selectedId)
                page.selectedId = "";
            else
                page.backRequested();
        }
    }
    Label {
        text: page.selectedId ? page.dateLabel(page.selected.completed_at) : qsTr("Свободные тренировки")
        font: Typography.screenTitle
        color: Theme.textPrimary
        wrapMode: Text.WordWrap
        width: parent.width
    }
    AppButton {
        text: page.recorder && page.recorder.hasPending ? qsTr("Продолжить тренировку") : page.plan.length > 0 ? qsTr("Начать тренировку") : qsTr("Выбрать упражнения")
        visible: page.signedIn && !page.selectedId
        width: parent.width
        onClicked: page.startOrResume()
    }
    Repeater {
        model: page.signedIn && !page.selectedId ? page.plan : []
        delegate: AppCard {
            id: plannedExercise
            required property var modelData
            width: parent.width
            RowLayout {
                width: parent.width
                Label {
                    Layout.fillWidth: true
                    text: plannedExercise.modelData.title
                    textFormat: Text.PlainText
                    font: Typography.body
                    color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                }
                AppButton {
                    text: qsTr("Убрать")
                    variant: AppButton.Ghost
                    onClicked: page.plan = page.plan.filter(item => item.exerciseId !== plannedExercise.modelData.exerciseId)
                }
            }
        }
    }
    AppButton {
        text: qsTr("Добавить упражнение")
        variant: AppButton.Secondary
        visible: page.signedIn && !page.selectedId && page.plan.length > 0
        width: parent.width
        onClicked: picker.open()
    }
    Label {
        text: !page.signedIn ? qsTr("Войдите, чтобы открыть тренировки аккаунта.") : page.startError
        visible: text.length > 0
        color: Theme.textSecondary
        font: Typography.body
        width: parent.width
        wrapMode: Text.WordWrap
    }
    RowLayout {
        visible: page.signedIn && !page.selectedId
        width: parent.width
        Label {
            Layout.fillWidth: true
            text: qsTr("История")
            font: Typography.sectionTitle
            color: Theme.textPrimary
        }
        AppButton {
            text: qsTr("Обновить")
            variant: AppButton.Secondary
            enabled: page.sync && !page.sync.loading
            onClicked: page.sync.refreshHistory()
        }
    }
    Label {
        text: page.sync && page.sync.loading ? qsTr("Загружаем…") : page.sync && page.sync.errorKey === "network_unavailable" ? qsTr("Нет сети. Доступны записи с устройства.") : qsTr("Не удалось обновить данные")
        visible: page.signedIn && (page.sync.loading || page.sync.errorKey !== "")
        color: Theme.textSecondary
        font: Typography.body
        width: parent.width
    }
    Label {
        text: qsTr("Тренировок пока нет")
        visible: page.signedIn && !page.selectedId && !page.sync.loading && page.sync.errorKey === "" && page.sync.history.length === 0
        color: Theme.textSecondary
        font: Typography.body
        width: parent.width
    }
    AppButton {
        text: qsTr("Повторить загрузку")
        variant: AppButton.Secondary
        visible: page.signedIn && page.selectedId !== "" && !page.sync.loading && page.sync.errorKey !== ""
        width: parent.width
        onClicked: page.sync.openWorkout(page.selectedId)
    }
    Repeater {
        model: page.signedIn && !page.selectedId ? page.sync.history : []
        delegate: WorkoutResultRow {
            required property var modelData
            objectName: "soloWorkoutHistoryRow"
            width: parent.width
            title: page.dateLabel(modelData.completed_at)
            subtitle: page.syncLabel(modelData)
            detailText: modelData.setCount === undefined ? "" : qsTr("Подходы: %1").arg(modelData.setCount)
            onClicked: page.selectWorkout(modelData.id)
        }
    }
    AppButton {
        text: qsTr("Показать ещё")
        variant: AppButton.Secondary
        visible: page.signedIn && !page.selectedId && page.sync.hasMore
        enabled: page.sync && !page.sync.loading
        width: parent.width
        onClicked: page.sync.loadMoreHistory()
    }
    Repeater {
        model: page.selected.exercises || []
        delegate: AppCard {
            id: exerciseCard
            required property int index
            required property var modelData
            width: parent.width
            Label {
                text: exerciseCard.modelData.exercise_name || qsTr("Упражнение %1").arg(exerciseCard.index + 1)
                textFormat: Text.PlainText
                font: Typography.sectionTitle
                color: Theme.textPrimary
                width: parent.width
                wrapMode: Text.WordWrap
            }
            Repeater {
                model: exerciseCard.modelData.sets || []
                delegate: WorkoutResultRow {
                    required property int index
                    required property var modelData
                    interactive: false
                    width: parent.width
                    title: qsTr("Подход %1").arg(index + 1)
                    valueText: modelData.weight_kg === undefined ? "" : qsTr("%1 кг").arg(StatisticsUi.number(modelData.weight_kg, modelData.weight_kg % 1 ? 2 : 0))
                    detailText: qsTr("%1 повт.").arg(modelData.reps)
                    subtitle: modelData.note || ""
                }
            }
        }
    }
    ExercisePickerPopup {
        id: picker
        parent: Overlay.overlay
        confirmText: qsTr("Добавить в тренировку")

        onPicked: function (exerciseId, title, weightKg, sets, repsMin, repsMax) {
            if (!page.signedIn || !page.recorder)
                return;
            close();
            page.startError = "";
            if (page.plan.some(item => item.exerciseId === exerciseId)) {
                page.startError = qsTr("Это упражнение уже добавлено.");
                return;
            }
            page.plan = page.plan.concat([
                {
                    exerciseId: exerciseId,
                    title: title,
                    weightKg: weightKg,
                    sets: sets,
                    repsMin: repsMin,
                    repsMax: repsMax
                }
            ]);
        }
    }
}
