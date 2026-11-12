import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

PageScaffold {
    id: page
    property var viewModel: typeof WorkoutVM !== "undefined" ? WorkoutVM : null
    property int exerciseIndex: 0
    property int setNumber: 1
    property bool editingSet: false
    property bool extrasExpanded: false
    property real rpeValue: -1
    property int expandedResult: -1
    readonly property var exerciseRows: viewModel ? viewModel.exercises : []
    readonly property var exercise: exerciseRows[exerciseIndex] || ({})
    readonly property bool finished: viewModel && viewModel.completed
    readonly property bool allRecorded: viewModel && viewModel.totalSets > 0 && viewModel.completedSets >= viewModel.totalSets
    readonly property var recordedSets: (exercise.facts || []).filter(fact => fact.done)
    readonly property var rpeOptions: [
        {
            id: -1,
            title: qsTr("Не указывать")
        },
        {
            id: 5,
            title: qsTr("5 · Легко")
        },
        {
            id: 6,
            title: qsTr("6 · Умеренно")
        },
        {
            id: 7,
            title: qsTr("7 · Ощутимо")
        },
        {
            id: 8,
            title: qsTr("8 · Тяжело")
        },
        {
            id: 9,
            title: qsTr("9 · Очень тяжело")
        },
        {
            id: 10,
            title: qsTr("10 · Максимальное усилие")
        }
    ]
    signal backRequested
    showHeader: false
    spacing: Spacing.itemGap
    title: finished ? qsTr("Тренировка завершена") : qsTr("Тренировка")

    function choose(index, number, edit = true) {
        exerciseIndex = index;
        setNumber = number;
        const item = exerciseRows[index] || {};
        const fact = item.facts && item.facts[number - 1] ? item.facts[number - 1] : {};
        const recorded = (item.facts || []).filter(set => set.done);
        const previous = recorded.length ? recorded[recorded.length - 1] : null;
        editingSet = edit && fact.done === true;
        weightInput.setValue(fact.done ? fact.weightKg : previous ? previous.weightKg : item.weightKg || 0);
        repsInput.setValue(fact.done ? fact.reps : previous ? previous.reps : item.repsMin || 1);
        rpeValue = fact.done && StatisticsUi.has(fact.rpe) ? fact.rpe : -1;
        noteInput.text = fact.done ? fact.note || "" : "";
    }
    function selectExercise(index) {
        const facts = exerciseRows[index].facts || [];
        let pending = facts.findIndex(fact => !fact.done);
        choose(index, pending >= 0 ? pending + 1 : 1);
        scrollToEntry.restart();
    }
    function nextPending() {
        for (let offset = 0; offset < exerciseRows.length; ++offset) {
            const index = (exerciseIndex + offset) % exerciseRows.length;
            const facts = exerciseRows[index].facts || [];
            const pending = facts.findIndex(fact => !fact.done);
            if (pending >= 0) {
                choose(index, pending + 1, false);
                return;
            }
        }
        editingSet = false;
    }
    function saveCurrent() {
        if (!viewModel || !weightInput.valid || !repsInput.valid || submitCooldown.running)
            return;
        if (viewModel.saveSet(exerciseIndex, setNumber, weightInput.numberValue, repsInput.numberValue, rpeValue, noteInput.text)) {
            submitCooldown.start();
            nextPending();
            extrasExpanded = false;
            scrollToEntry.restart();
        }
    }
    function finishWorkout() {
        if (!viewModel || finished || submitCooldown.running || !viewModel.finish())
            return false;
        submitCooldown.start();
        scrollToEntry.restart();
        return true;
    }
    function requestFinish() {
        if (!viewModel || viewModel.completedSets === 0)
            return;
        if (viewModel.completedSets < viewModel.totalSets)
            partialConfirmation.open();
        else
            finishWorkout();
    }
    function repsTarget() {
        return exercise.repsMin === exercise.repsMax ? String(exercise.repsMin || 1) : (exercise.repsMin || 1) + "–" + (exercise.repsMax || 1);
    }
    function clock(seconds) {
        return Math.floor(seconds / 60) + ":" + String(seconds % 60).padStart(2, "0");
    }
    function exerciseVolume(item) {
        return (item.facts || []).filter(fact => fact.done).reduce((sum, fact) => sum + fact.weightKg * fact.reps, 0);
    }
    function recordedCount(item) {
        return (item.facts || []).filter(fact => fact.done).length;
    }
    Component.onCompleted: nextPending()

    Timer {
        id: submitCooldown
        objectName: "workoutSubmitCooldown"
        interval: 350
    }
    Timer {
        interval: 1000
        repeat: true
        running: page.visible && !page.finished && page.viewModel && page.viewModel.restRemaining > 0
        onTriggered: page.viewModel.refreshClock()
    }
    NumberAnimation {
        id: scrollToEntry
        target: page.pageFlickable
        property: "contentY"
        to: 0
        duration: Theme.pageTransitionDuration
        easing.type: Easing.OutCubic
    }
    Connections {
        target: Qt.application
        function onStateChanged() {
            if (page.viewModel && page.visible)
                page.viewModel.refreshClock();
        }
    }

    RowLayout {
        width: parent.width
        visible: !page.finished
        AppButton {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            text: qsTr("‹ Сохранить и выйти")
            variant: AppButton.Ghost
            labelFont: Typography.detail
            onClicked: page.backRequested()
        }
        AppButton {
            visible: page.viewModel && page.viewModel.completedSets > 0 && !page.allRecorded && !page.editingSet
            text: qsTr("Завершить")
            variant: AppButton.Ghost
            labelFont: Typography.detail
            enabled: !submitCooldown.running
            onClicked: page.requestFinish()
        }
    }
    Column {
        id: entry
        width: parent.width
        spacing: Spacing.itemGap
        visible: !page.finished
        RowLayout {
            width: parent.width
            Label {
                Layout.fillWidth: true
                text: page.editingSet ? qsTr("Правка подхода %1").arg(page.setNumber) : page.allRecorded ? qsTr("Все подходы записаны") : qsTr("Упражнение %1 из %2 · подход %3 из %4").arg(page.exerciseIndex + 1).arg(page.exerciseRows.length).arg(page.setNumber).arg(page.exercise.sets || 0)
                font: Typography.detail
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
            AppButton {
                visible: page.editingSet
                text: qsTr("Отмена")
                variant: AppButton.Ghost
                labelFont: Typography.detail
                onClicked: {
                    page.nextPending();
                    page.extrasExpanded = false;
                }
            }
        }
        Rectangle {
            width: parent.width
            height: 3
            radius: 1.5
            color: Theme.borderSubtle
            Rectangle {
                height: parent.height
                radius: parent.radius
                color: Theme.accent
                width: parent.width * (page.viewModel && page.viewModel.totalSets > 0 ? page.viewModel.completedSets / page.viewModel.totalSets : 0)
                Behavior on width {
                    NumberAnimation {
                        duration: Theme.selectionDuration
                        easing.type: Easing.OutCubic
                    }
                }
            }
        }
        AbstractButton {
            objectName: "workoutExercisePicker"
            width: parent.width
            implicitHeight: contentItem.implicitHeight
            Accessible.name: qsTr("Выбрать упражнение: %1").arg(page.exercise.title || "")
            onClicked: exercisePicker.open()
            contentItem: RowLayout {
                spacing: Spacing.itemGap
                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: page.exercise.title || qsTr("Упражнение")
                    font: Typography.workoutTitle
                    color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                }
                AppIcon {
                    Layout.preferredWidth: 20
                    Layout.preferredHeight: 20
                    name: "chevronDown"
                    visible: page.exerciseRows.length > 1
                }
            }
        }
        Label {
            width: parent.width
            text: page.exercise.previous ? qsTr("В прошлый раз: %1").arg(page.exercise.previous) + (page.exercise.previousDate ? " · " + StatisticsUi.date(page.exercise.previousDate) : "") : qsTr("Первый результат в этой программе")
            color: Theme.textSecondary
            font: Typography.detail
            wrapMode: Text.WordWrap
        }
        Item {
            width: parent.width
            height: page.viewModel && page.viewModel.restRemaining > 0 ? restCard.implicitHeight : 0
            visible: height > 0
            clip: true
            Behavior on height {
                NumberAnimation {
                    duration: Theme.pageTransitionDuration
                    easing.type: Easing.OutCubic
                }
            }
            AppCard {
                id: restCard
                objectName: "workoutRestCard"
                width: parent.width
                elevated: false
                surfaceColor: Theme.accentSurface
                paddingVertical: 12
                RowLayout {
                    width: parent.width
                    spacing: 8
                    Column {
                        Layout.fillWidth: true
                        Label {
                            text: qsTr("Отдых")
                            font: Typography.caption
                            color: Theme.textSecondary
                        }
                        Label {
                            text: page.clock(page.viewModel ? page.viewModel.restRemaining : 0)
                            font: Typography.figure
                            color: Theme.textPrimary
                        }
                    }
                    AppButton {
                        text: qsTr("+15 с")
                        compact: true
                        variant: AppButton.Secondary
                        labelFont: Typography.detail
                        implicitWidth: 64
                        onClicked: page.viewModel.adjustRest(15)
                    }
                    AppButton {
                        text: qsTr("Пропустить")
                        compact: true
                        variant: AppButton.Secondary
                        labelFont: Typography.detail
                        onClicked: page.viewModel.skipRest()
                    }
                }
            }
        }
        AppCard {
            width: parent.width
            paddingHorizontal: 0
            paddingVertical: 0
            spacing: 0
            elevated: false
            WorkoutValueRow {
                id: weightInput
                objectName: "workoutWeightInput"
                width: parent.width
                label: qsTr("Вес")
                caption: qsTr("кг")
                decimals: 2
                step: 2.5
            }
            WorkoutValueRow {
                id: repsInput
                objectName: "workoutRepsInput"
                width: parent.width
                label: qsTr("Повторы")
                caption: qsTr("Цель %1").arg(page.repsTarget())
                from: 1
                showDivider: false
            }
        }
        DisclosureCard {
            objectName: "workoutExtras"
            width: parent.width
            title: qsTr("Усилие и заметка")
            summary: page.rpeValue >= 5 ? qsTr("RPE %1").arg(page.rpeValue) + (noteInput.text.length ? qsTr(" · есть заметка") : "") : noteInput.text.length ? qsTr("Есть заметка") : qsTr("По желанию")
            showIcon: false
            titleFont: Typography.bodyMedium
            summaryFont: Typography.detail
            doneText: qsTr("Свернуть")
            expanded: page.extrasExpanded
            onToggleRequested: page.extrasExpanded = !page.extrasExpanded
            onDoneRequested: page.extrasExpanded = false
            body: Column {
                width: parent.width
                spacing: Spacing.itemGap
                Label {
                    width: parent.width
                    text: qsTr("Насколько тяжёлым был подход?")
                    font: Typography.body
                    color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                }
                AppComboBox {
                    width: parent.width
                    model: page.rpeOptions
                    currentIndex: Math.max(0, page.rpeOptions.findIndex(option => option.id === page.rpeValue))
                    onActivated: page.rpeValue = page.rpeOptions[currentIndex].id
                }
                AppTextArea {
                    id: noteInput
                    objectName: "workoutNoteInput"
                    width: parent.width
                    label: qsTr("Заметка к подходу")
                    placeholderText: qsTr("Например, изменил хват")
                }
            }
        }
        Label {
            width: parent.width
            visible: page.recordedSets.length > 0
            text: qsTr("Записано")
            font: Typography.bodyMedium
            color: Theme.textPrimary
        }
        AppCard {
            width: parent.width
            visible: page.recordedSets.length > 0
            paddingHorizontal: 0
            paddingVertical: 0
            spacing: 0
            elevated: false
            Repeater {
                model: page.recordedSets
                delegate: WorkoutResultRow {
                    required property var modelData
                    required property int index
                    width: parent.width
                    title: qsTr("Подход %1").arg(modelData.number)
                    subtitle: modelData.note || (StatisticsUi.has(modelData.rpe) ? "RPE " + modelData.rpe : "")
                    valueText: StatisticsUi.number(modelData.weightKg, modelData.weightKg % 1 ? 2 : 0) + " " + qsTr("кг")
                    detailText: qsTr("%1 повт.").arg(modelData.reps)
                    symbol: "✓"
                    symbolColor: Theme.positive
                    showDivider: index < page.recordedSets.length - 1
                    onClicked: {
                        page.choose(page.exerciseIndex, modelData.number);
                        scrollToEntry.restart();
                    }
                }
            }
        }
    }
    Column {
        width: parent.width
        spacing: Spacing.listGap
        visible: page.finished
        Label {
            width: parent.width
            text: qsTr("Тренировка завершена")
            font: Typography.workoutTitle
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
        }
        Label {
            width: parent.width
            text: page.viewModel ? page.viewModel.title : ""
            font: Typography.body
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
        }
        AppCard {
            width: parent.width
            elevated: false
            RowLayout {
                width: parent.width
                StatisticValue {
                    Layout.fillWidth: true
                    label: qsTr("Подходы")
                    valueText: page.viewModel ? String(page.viewModel.completedSets) : "—"
                }
                StatisticValue {
                    Layout.fillWidth: true
                    label: qsTr("Упражнения")
                    valueText: page.viewModel && page.viewModel.completedExercises !== undefined ? String(page.viewModel.completedExercises) : "—"
                }
                StatisticValue {
                    Layout.fillWidth: true
                    label: qsTr("Объём, кг")
                    valueText: page.viewModel ? StatisticsUi.number(page.viewModel.tonnageKg, 0) : "—"
                }
            }
            Label {
                width: parent.width
                text: page.viewModel && page.viewModel.durationSeconds >= 60 ? qsTr("Время: %1 мин").arg(Math.floor(page.viewModel.durationSeconds / 60)) : qsTr("Меньше минуты")
                font: Typography.detail
                color: Theme.textSecondary
            }
        }
        Label {
            width: parent.width
            text: qsTr("Результаты сохранены в дневнике")
            font: Typography.detail
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
        }
        AppCard {
            width: parent.width
            paddingHorizontal: 0
            paddingVertical: 0
            spacing: 0
            elevated: false
            Repeater {
                model: page.exerciseRows
                delegate: Column {
                    id: summaryExercise
                    required property var modelData
                    required property int index
                    width: parent.width
                    visible: page.recordedCount(modelData) > 0
                    WorkoutResultRow {
                        width: parent.width
                        title: summaryExercise.modelData.title
                        subtitle: qsTr("Подходы: %1").arg(page.recordedCount(summaryExercise.modelData))
                        valueText: StatisticsUi.number(page.exerciseVolume(summaryExercise.modelData), 0) + " " + qsTr("кг")
                        expandable: true
                        expanded: page.expandedResult === summaryExercise.index
                        onClicked: page.expandedResult = page.expandedResult === summaryExercise.index ? -1 : summaryExercise.index
                    }
                    Item {
                        width: parent.width
                        height: page.expandedResult === summaryExercise.index ? summarySets.implicitHeight : 0
                        visible: height > 0
                        clip: true
                        Behavior on height {
                            NumberAnimation {
                                duration: Theme.pageTransitionDuration
                                easing.type: Easing.OutCubic
                            }
                        }
                        Column {
                            id: summarySets
                            width: parent.width
                            Repeater {
                                model: summaryExercise.modelData.facts.filter(fact => fact.done)
                                delegate: WorkoutResultRow {
                                    required property var modelData
                                    width: parent.width
                                    interactive: false
                                    title: qsTr("Подход %1").arg(modelData.number)
                                    valueText: StatisticsUi.number(modelData.weightKg, modelData.weightKg % 1 ? 2 : 0) + " " + qsTr("кг")
                                    detailText: qsTr("%1 повт.").arg(modelData.reps)
                                    subtitle: modelData.note || ""
                                }
                            }
                        }
                    }
                }
            }
        }
    }
    footer: ColumnLayout {
        spacing: 8
        Label {
            Layout.fillWidth: true
            visible: text.length > 0
            text: page.viewModel ? page.viewModel.errorDetail : ""
            font: Typography.detail
            color: Theme.negative
            wrapMode: Text.WordWrap
        }
        AppButton {
            objectName: "workoutSaveSetButton"
            Layout.fillWidth: true
            visible: !page.finished && (!page.allRecorded || page.editingSet)
            enabled: weightInput.valid && repsInput.valid && !submitCooldown.running
            text: page.editingSet ? qsTr("Сохранить изменения") : qsTr("Записать подход")
            onClicked: page.saveCurrent()
        }
        AppButton {
            objectName: "workoutFinishButton"
            Layout.fillWidth: true
            visible: !page.finished && page.allRecorded && !page.editingSet
            enabled: page.viewModel && page.viewModel.completedSets > 0 && !submitCooldown.running
            text: qsTr("Завершить тренировку")
            onClicked: page.requestFinish()
        }
        AppButton {
            Layout.fillWidth: true
            visible: page.finished
            text: qsTr("Готово")
            enabled: !submitCooldown.running
            onClicked: page.backRequested()
        }
    }
    SwipePopup {
        id: exercisePicker
        parent: Overlay.overlay
        dragHandle: exerciseGrip
        width: parent ? Math.min(480, parent.width - Spacing.screenPadding * 2) : 328
        height: Math.min(implicitHeight, parent ? parent.height - 64 : implicitHeight)
        modal: true
        dim: true
        padding: Spacing.cardPadding
        contentItem: ColumnLayout {
            spacing: Spacing.itemGap
            SwipeDismissHandle {
                id: exerciseGrip
                Layout.fillWidth: true
                onDismissed: exercisePicker.close()
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Упражнения тренировки")
                font: Typography.sectionTitle
                color: Theme.textPrimary
            }
            ScrollView {
                Layout.fillWidth: true
                Layout.preferredHeight: Math.min(exerciseList.implicitHeight, 360)
                clip: true
                Column {
                    id: exerciseList
                    width: parent.width
                    Repeater {
                        model: page.exerciseRows
                        delegate: WorkoutResultRow {
                            required property var modelData
                            required property int index
                            width: parent.width
                            title: modelData.title
                            subtitle: qsTr("Подходы: %1 из %2").arg(page.recordedCount(modelData)).arg(modelData.sets)
                            symbol: index === page.exerciseIndex ? "✓" : ""
                            symbolColor: Theme.accent
                            onClicked: {
                                page.selectExercise(index);
                                exercisePicker.close();
                            }
                        }
                    }
                }
            }
            AppButton {
                Layout.fillWidth: true
                text: qsTr("Закрыть")
                variant: AppButton.Ghost
                onClicked: exercisePicker.close()
            }
        }
    }
    SwipePopup {
        id: partialConfirmation
        parent: Overlay.overlay
        dragHandle: partialGrip
        width: parent ? Math.min(360, parent.width - 32) : 328
        modal: true
        dim: true
        padding: Spacing.cardPadding
        contentItem: ColumnLayout {
            spacing: Spacing.itemGap
            SwipeDismissHandle {
                id: partialGrip
                Layout.fillWidth: true
                onDismissed: partialConfirmation.close()
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Завершить тренировку?")
                font: Typography.sectionTitle
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Записано %1 из %2 подходов. В дневник попадут выполненные.").arg(page.viewModel ? page.viewModel.completedSets : 0).arg(page.viewModel ? page.viewModel.totalSets : 0)
                font: Typography.body
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
            AppButton {
                Layout.fillWidth: true
                text: qsTr("Завершить")
                onClicked: {
                    if (page.finishWorkout())
                        partialConfirmation.close();
                }
            }
            AppButton {
                Layout.fillWidth: true
                text: qsTr("Продолжить тренировку")
                variant: AppButton.Secondary
                onClicked: partialConfirmation.close()
            }
        }
    }
}
