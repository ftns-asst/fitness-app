import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

PageScaffold {
    id: page
    property var viewModel: typeof StrengthVM !== "undefined" ? StrengthVM : null
    property string userId: typeof AppContext !== "undefined" ? AppContext.currentUserId : ""
    property string programId: ""
    property bool embedded: false
    property int dataRevision: 0
    property int selectedSplitIndex: 0
    property int exerciseIndex: 0
    property int periodIndex: 2
    property int metricIndex: 0
    property int sectionIndex: 0
    property var selectedHistoryRecord: ({})
    property int historyPage: 0
    readonly property var historyWindow: historySlice(historyRows, historyPage)
    property int statusPage: 0
    readonly property var statusRows: StatisticsUi.rows(viewModel ? viewModel.status_plot : null, dataRevision).reverse()
    readonly property var statusWindow: statusRows.slice(statusPage * 8, statusPage * 8 + 8)
    property string referenceDate: Qt.formatDate(new Date(), Qt.ISODate)
    readonly property bool hasData: viewModel !== null && viewModel !== undefined && viewModel.hasData
    readonly property var indexData: viewModel ? viewModel.programIndex : ({
            percent: null,
            better: 0,
            worse: 0,
            evaluated: 0
        })
    readonly property var exerciseRows: StatisticsUi.rows(viewModel ? viewModel.exercises : null, dataRevision).filter(row => (row.currentSets > 0 || (row.currentSets === undefined && StatisticsUi.has(row.currentWeightKg))) && includesSplit(row.splitId, dataRevision))
    readonly property var selectedExercise: exerciseRows[Math.min(exerciseIndex, exerciseRows.length - 1)] || ({})
    readonly property var range: StatisticsUi.period(referenceDate, periodIndex)
    property var historyRows: []
    onSelectedExerciseChanged: Qt.callLater(rebuildHistory)
    onRangeChanged: Qt.callLater(rebuildHistory)
    onDataRevisionChanged: Qt.callLater(rebuildHistory)
    readonly property var chartPoints: historyRows.filter(row => StatisticsUi.has(metricIndex === 0 ? row.currentWeightKg : row.currentReps)).map(row => ({
                date: row.date,
                value: metricIndex === 0 ? row.currentWeightKg : row.currentReps
            }))
    readonly property var splitRows: StatisticsUi.rows(viewModel ? viewModel.splits : null, dataRevision).filter(row => row.evaluated > 0)
    onSectionIndexChanged: {
        pageFlickable.cancelFlick();
        pageFlickable.contentY = 0;
        sectionFade.restart();
    }
    signal backRequested
    title: embedded ? "" : qsTr("Сила")
    caption: embedded ? "" : qsTr("Динамика выбранного упражнения")
    showHeader: false
    spacing: Spacing.itemGap
    Component.onCompleted: {
        reload();
        Qt.callLater(rebuildHistory);
    }
    onUserIdChanged: reload()
    onProgramIdChanged: reload()
    onViewModelChanged: reload()
    onVisibleChanged: {
        if (visible)
            reload();
    }
    onExerciseIndexChanged: {
        historyPage = 0;
    }
    onPeriodIndexChanged: {
        historyPage = 0;
    }
    onSelectedSplitIndexChanged: {
        exerciseIndex = 0;
        historyPage = 0;
        statusPage = 0;
    }
    Connections {
        target: page.viewModel
        function onChanged() {
            page.dataRevision += 1;
        }
    }
    // Snapshot model roles outside bindings: delegates never read live ListModel proxies.
    function rebuildHistory() {
        const rows = StatisticsUi.rows(viewModel ? viewModel.status_rows : null, dataRevision);
        const result = [];
        for (const row of rows) {
            if (row.exerciseId !== selectedExercise.exerciseId || row.splitId !== selectedExercise.splitId || (range.from && row.date < range.from) || (range.to && row.date > range.to))
                continue;
            result.push({
                exerciseId: row.exerciseId,
                splitId: row.splitId,
                sessionId: row.sessionId,
                date: row.date,
                currentWeightKg: row.currentWeightKg,
                currentReps: row.currentReps,
                currentSets: row.currentSets,
                symbol: row.symbol,
                status: row.status,
                reasonText: row.reasonText,
                deltaKg: row.deltaKg,
                deltaPercent: row.deltaPercent
            });
        }
        result.sort((a, b) => a.date.localeCompare(b.date));
        historyRows = result;
        historyPage = Math.min(historyPage, Math.max(0, Math.ceil(result.length / 6) - 1));
    }
    function historySlice(rows, index) {
        const result = [];
        for (let i = rows.length - 1 - index * 6; i >= Math.max(0, rows.length - (index + 1) * 6); --i)
            result.push(rows[i]);
        return result;
    }
    function reload() {
        if (viewModel && userId.length > 0)
            viewModel.reload(userId, programId);
    }
    function numberText(value) {
        return StatisticsUi.number(value, Number.isInteger(Number(value)) ? 0 : 1);
    }
    function signedText(value) {
        return StatisticsUi.signed(value, Number.isInteger(Number(value)) ? 0 : 1);
    }
    function workloadText(weight, reps) {
        return StatisticsUi.has(weight) ? qsTr("%1 кг × %2 повт.").arg(numberText(weight)).arg(numberText(reps)) : qsTr("Нет данных");
    }
    function statusColor(status) {
        return status === "better" ? Theme.positive : status === "worse" ? Theme.negative : status === "attention" ? Theme.warning : Theme.textSecondary;
    }
    function splitNames(revision) {
        return [qsTr("Все сплиты")].concat(StatisticsUi.rows(viewModel ? viewModel.splits : null, revision).map(row => row.splitTitle));
    }
    function includesSplit(splitId, revision) {
        const splits = StatisticsUi.rows(viewModel ? viewModel.splits : null, revision);
        return selectedSplitIndex === 0 || selectedSplitIndex > splits.length || splits[selectedSplitIndex - 1].splitId === splitId;
    }

    NumberAnimation {
        id: sectionFade
        target: page.pageFlickable
        property: "opacity"
        from: 0.35
        to: 1
        duration: Theme.selectionDuration
        easing.type: Easing.OutCubic
    }
    RowLayout {
        width: parent.width
        visible: !page.embedded
        AppButton {
            objectName: "strengthBack"
            Layout.preferredWidth: Theme.touchMin
            Layout.minimumWidth: Theme.touchMin
            Layout.maximumWidth: Theme.touchMin
            text: "‹"
            variant: AppButton.Ghost
            Accessible.name: qsTr("Назад")
            onClicked: page.backRequested()
        }
        Label {
            Layout.fillWidth: true
            text: page.title
            font: Typography.workoutTitle
            color: Theme.textPrimary
        }
    }
    AppCard {
        width: parent.width
        visible: page.viewModel && page.viewModel.errorKey.length > 0
        Label {
            width: parent.width
            text: qsTr("Не удалось загрузить результаты")
            font: Typography.body
            color: Theme.negative
            wrapMode: Text.WordWrap
        }
        AppButton {
            width: parent.width
            text: qsTr("Повторить")
            variant: AppButton.Secondary
            onClicked: page.reload()
        }
    }
    SegmentedControl {
        objectName: "strengthSections"
        width: parent.width
        implicitWidth: 0
        visible: page.hasData && page.sectionIndex < 3
        items: [qsTr("Динамика"), qsTr("История"), qsTr("Сводка")]
        currentIndex: Math.min(2, page.sectionIndex)
        onSelected: index => page.sectionIndex = index
    }
    AppCard {
        width: parent.width
        visible: (!page.hasData || (page.exerciseRows.length === 0 && page.sectionIndex < 2)) && (!page.viewModel || !page.viewModel.errorKey)
        Label {
            width: parent.width
            text: page.hasData ? qsTr("В этом сплите пока нет записей") : qsTr("Первая тренировка — точка старта")
            font: Typography.sectionTitle
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
        }
        Label {
            width: parent.width
            text: page.hasData ? qsTr("Выберите другой тренировочный день.") : qsTr("Запишите рабочие подходы. Со следующей тренировки здесь появится сравнение результатов.")
            font: Typography.body
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
        }
    }
    AppComboBox {
        objectName: "strengthSplitPicker"
        width: parent.width
        visible: page.hasData && page.sectionIndex < 2 && page.splitNames(page.dataRevision).length > 2
        model: page.splitNames(page.dataRevision).map((title, index) => ({
                    id: index,
                    title: title
                }))
        currentIndex: page.selectedSplitIndex
        Accessible.name: qsTr("Тренировочный день")
        onActivated: page.selectedSplitIndex = currentIndex
    }
    AppComboBox {
        objectName: "strengthExercisePicker"
        width: parent.width
        visible: page.hasData && page.sectionIndex < 2 && page.exerciseRows.length > 0
        model: page.exerciseRows.map((row, index) => ({
                    id: index,
                    title: row.exerciseTitle
                }))
        currentIndex: Math.min(page.exerciseIndex, count - 1)
        Accessible.name: qsTr("Упражнение")
        onActivated: page.exerciseIndex = currentIndex
    }
    FilterChips {
        width: parent.width
        visible: page.hasData && page.sectionIndex < 2
        items: [qsTr("7 дней"), qsTr("28 дней"), qsTr("Программа")]
        currentIndex: page.periodIndex
        onSelected: index => page.periodIndex = index
    }
    AppCard {
        objectName: "strengthExerciseCard"
        width: parent.width
        visible: page.hasData && page.exerciseRows.length > 0 && page.sectionIndex === 0
        elevated: false
        RowLayout {
            width: parent.width
            Label {
                Layout.fillWidth: true
                text: page.metricIndex === 0 ? qsTr("Рабочий вес") : qsTr("Средние повторы")
                color: Theme.textPrimary
                font: Typography.bodyMedium
            }
        }
        Label {
            width: parent.width
            text: page.numberText(strengthPlot.selected ? strengthPlot.selected.value : null) + " " + (page.metricIndex === 0 ? qsTr("кг") : qsTr("повт."))
            font: Typography.figure
            color: Theme.textPrimary
        }
        Label {
            width: parent.width
            text: strengthPlot.selected ? StatisticsUi.date(strengthPlot.selected.date) : qsTr("Нет тренировок за этот период")
            color: Theme.textSecondary
            font: Typography.caption
        }
        SegmentedControl {
            implicitWidth: 0
            width: parent.width
            items: [qsTr("Вес"), qsTr("Повторы")]
            currentIndex: page.metricIndex
            onSelected: index => page.metricIndex = index
        }
        StatisticsPlot {
            id: strengthPlot
            width: parent.width
            points: page.chartPoints
            unit: page.metricIndex === 0 ? qsTr("кг") : qsTr("повт.")
            showSelectionHeader: false
        }
        Label {
            width: parent.width
            text: page.historyRows.length ? page.historyRows[page.historyRows.length - 1].reasonText : ""
            visible: text.length > 0
            font: Typography.body
            color: page.statusColor(page.historyRows.length ? page.historyRows[page.historyRows.length - 1].status : "")
            wrapMode: Text.WordWrap
        }
        Label {
            width: parent.width
            text: page.selectedExercise.nextGoal || ""
            visible: text.length > 0 && page.historyRows.length > 0
            font: Typography.captionStrong
            color: Theme.accent
            wrapMode: Text.WordWrap
        }
    }

    AppButton {
        width: parent.width
        visible: page.hasData && page.exerciseRows.length > 0 && page.sectionIndex === 0
        text: qsTr("Сравнить со стартом программы")
        variant: AppButton.Ghost
        labelFont: Typography.detail
        onClicked: startComparison.open()
    }
    Column {
        objectName: "exerciseHistorySection"
        width: parent.width
        visible: page.hasData && page.sectionIndex === 1
        spacing: 16
        Label {
            width: parent.width
            text: page.historyRows.length ? qsTr("Тренировок за период: %1").arg(page.historyRows.length) : qsTr("За этот период нет записей")
            font: Typography.detail
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
        }
        AppCard {
            width: parent.width
            visible: page.historyWindow.length > 0
            paddingHorizontal: 0
            paddingVertical: 0
            spacing: 0
            elevated: false
            Repeater {
                model: page.historyWindow
                delegate: WorkoutResultRow {
                    required property var modelData
                    required property int index
                    objectName: "exerciseHistoryRow" + index
                    width: parent.width
                    topPadding: 18
                    bottomPadding: 18
                    title: page.numberText(modelData.currentWeightKg) + " " + qsTr("кг")
                    titleFont: Typography.inputNumber
                    subtitle: qsTr("%1 повт. в среднем").arg(page.numberText(modelData.currentReps))
                    valueText: StatisticsUi.date(modelData.date)
                    valueFont: Typography.detail
                    valueColor: Theme.textSecondary
                    navigation: true
                    showDivider: index < page.historyWindow.length - 1
                    onClicked: {
                        page.selectedHistoryRecord = modelData;
                        historyDetail.open();
                    }
                }
            }
        }
        RowLayout {
            width: parent.width
            visible: page.historyRows.length > 6
            AppButton {
                Layout.fillWidth: true
                compact: true
                text: qsTr("Новее")
                variant: AppButton.Ghost
                enabled: page.historyPage > 0
                onClicked: page.historyPage -= 1
            }
            Label {
                text: (page.historyPage + 1) + " / " + Math.ceil(page.historyRows.length / 6)
                font: Typography.detail
                color: Theme.textSecondary
            }
            AppButton {
                Layout.fillWidth: true
                compact: true
                text: qsTr("Раньше")
                variant: AppButton.Ghost
                enabled: (page.historyPage + 1) * 6 < page.historyRows.length
                onClicked: page.historyPage += 1
            }
        }
    }
    Column {
        objectName: "programStrengthSummary"
        width: parent.width
        visible: page.hasData && page.sectionIndex === 2
        spacing: 24
        Label {
            width: parent.width
            text: page.viewModel ? page.viewModel.programTitle : ""
            font: Typography.sectionTitle
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
        }
        AppCard {
            width: parent.width
            elevated: false
            spacing: 20
            Column {
                width: parent.width
                spacing: 8
                Label {
                    text: qsTr("Баланс прогресса")
                    font: Typography.body
                    color: Theme.textSecondary
                }
                Label {
                    objectName: "strengthProgramIndex"
                    text: page.numberText(page.indexData.percent) + (StatisticsUi.has(page.indexData.percent) ? " %" : "")
                    font: Typography.figure
                    color: Theme.textPrimary
                }
                Label {
                    width: parent.width
                    text: page.indexData.evaluated > 0 ? qsTr("На основе %1 сравнений тренировок").arg(page.indexData.evaluated) : qsTr("Появится после второй тренировки")
                    font: Typography.detail
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
            }
            Rectangle {
                width: parent.width
                height: 1
                color: Theme.borderSubtle
            }
            RowLayout {
                width: parent.width
                spacing: 24
                StatisticValue {
                    Layout.fillWidth: true
                    label: qsTr("Улучшений")
                    valueText: String(page.indexData.better)
                    valueFont: Typography.inputNumber
                    valueColor: Theme.positive
                    labelFont: Typography.detail
                }
                StatisticValue {
                    Layout.fillWidth: true
                    label: qsTr("Ухудшений")
                    valueText: String(page.indexData.worse)
                    valueFont: Typography.inputNumber
                    valueColor: Theme.negative
                    labelFont: Typography.detail
                }
            }
        }
        Column {
            width: parent.width
            spacing: 12
            visible: page.splitRows.length > 1
            Label {
                text: qsTr("По тренировочным дням")
                font: Typography.bodyMedium
                color: Theme.textPrimary
            }
            AppCard {
                width: parent.width
                elevated: false
                paddingHorizontal: 0
                paddingVertical: 0
                spacing: 0
                Repeater {
                    model: page.splitRows
                    delegate: WorkoutResultRow {
                        required property var modelData
                        required property int index
                        width: parent.width
                        title: modelData.splitTitle
                        valueText: page.numberText(modelData.percent) + " %"
                        navigation: true
                        showDivider: index < page.splitRows.length - 1
                        onClicked: {
                            page.selectedSplitIndex = StatisticsUi.rows(page.viewModel.splits, page.dataRevision).findIndex(row => row.splitId === modelData.splitId) + 1;
                            page.sectionIndex = 0;
                        }
                    }
                }
            }
        }
        AppCard {
            width: parent.width
            visible: page.statusRows.length > 0
            paddingHorizontal: 0
            paddingVertical: 0
            elevated: false
            WorkoutResultRow {
                width: parent.width
                title: qsTr("Оценки по тренировкам")
                navigation: true
                showDivider: false
                onClicked: page.sectionIndex = 3
            }
        }
        AppButton {
            width: parent.width
            text: qsTr("Как считается прогресс")
            variant: AppButton.Ghost
            labelFont: Typography.detail
            onClicked: assessmentSheet.open()
        }
    }
    Column {
        width: parent.width
        visible: page.hasData && page.sectionIndex === 3
        spacing: 20
        AppButton {
            text: qsTr("‹ К сводке")
            variant: AppButton.Ghost
            onClicked: page.sectionIndex = 2
        }
        Label {
            width: parent.width
            text: qsTr("Оценки по тренировкам")
            font: Typography.sectionTitle
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
        }
        Label {
            width: parent.width
            text: qsTr("Улучшения, без изменений, внимание и ухудшения")
            font: Typography.detail
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
        }
        AppCard {
            width: parent.width
            elevated: false
            spacing: 20
            Repeater {
                model: page.statusWindow
                delegate: Column {
                    id: statusRow
                    required property var modelData
                    width: parent.width
                    spacing: 10
                    RowLayout {
                        width: parent.width
                        Label {
                            Layout.fillWidth: true
                            text: StatisticsUi.date(statusRow.modelData.date)
                            font: Typography.bodyMedium
                            color: Theme.textPrimary
                        }
                        Label {
                            text: qsTr("%1 лучше · %2 хуже").arg(statusRow.modelData.better).arg(statusRow.modelData.worse)
                            font: Typography.detail
                            color: Theme.textSecondary
                        }
                    }
                    Row {
                        width: parent.width
                        height: 6
                        Repeater {
                            model: [
                                {
                                    count: statusRow.modelData.better || 0,
                                    tone: Theme.positive
                                },
                                {
                                    count: (statusRow.modelData.same || 0) + (statusRow.modelData.start || 0),
                                    tone: Theme.textMuted
                                },
                                {
                                    count: statusRow.modelData.attention || 0,
                                    tone: Theme.warning
                                },
                                {
                                    count: statusRow.modelData.worse || 0,
                                    tone: Theme.negative
                                }
                            ]
                            delegate: Rectangle {
                                required property var modelData
                                height: parent.height
                                width: parent.width * modelData.count / Math.max(1, statusRow.modelData.total || 0)
                                color: modelData.tone
                            }
                        }
                    }
                }
            }
        }
        RowLayout {
            width: parent.width
            visible: page.statusRows.length > 8
            AppButton {
                Layout.fillWidth: true
                text: qsTr("Новее")
                variant: AppButton.Ghost
                enabled: page.statusPage > 0
                onClicked: page.statusPage -= 1
            }
            AppButton {
                Layout.fillWidth: true
                text: qsTr("Раньше")
                variant: AppButton.Ghost
                enabled: (page.statusPage + 1) * 8 < page.statusRows.length
                onClicked: page.statusPage += 1
            }
        }
    }
    SwipePopup {
        id: historyDetail
        objectName: "exerciseHistoryDetail"
        parent: Overlay.overlay
        dragHandle: historyGrip
        width: parent ? Math.min(420, parent.width - 32) : 328
        modal: true
        dim: true
        padding: 20
        contentItem: ColumnLayout {
            spacing: 20
            SwipeDismissHandle {
                id: historyGrip
                Layout.fillWidth: true
                onDismissed: historyDetail.close()
            }
            Column {
                Layout.fillWidth: true
                spacing: 8
                Label {
                    width: parent.width
                    text: StatisticsUi.fullDate(page.selectedHistoryRecord.date)
                    font: Typography.sectionTitle
                    color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                }
                Label {
                    width: parent.width
                    text: page.selectedExercise.exerciseTitle || ""
                    font: Typography.detail
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 20
                StatisticValue {
                    Layout.fillWidth: true
                    label: qsTr("Рабочий вес")
                    valueText: page.numberText(page.selectedHistoryRecord.currentWeightKg) + " " + qsTr("кг")
                    valueFont: Typography.inputNumber
                    labelFont: Typography.detail
                }
                StatisticValue {
                    Layout.fillWidth: true
                    label: qsTr("Средние повторы")
                    valueText: page.numberText(page.selectedHistoryRecord.currentReps)
                    valueFont: Typography.inputNumber
                    labelFont: Typography.detail
                }
            }
            Label {
                Layout.fillWidth: true
                text: StatisticsUi.setsText(page.selectedHistoryRecord.currentSets || 0)
                font: Typography.detail
                color: Theme.textSecondary
            }
            Rectangle {
                Layout.fillWidth: true
                implicitHeight: 1
                color: Theme.borderSubtle
            }
            Label {
                Layout.fillWidth: true
                text: page.selectedHistoryRecord.reasonText || qsTr("Первый результат")
                font: Typography.body
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                visible: StatisticsUi.has(page.selectedHistoryRecord.deltaKg)
                text: qsTr("К прошлой тренировке: %1 кг · %2 %").arg(page.signedText(page.selectedHistoryRecord.deltaKg)).arg(page.signedText(page.selectedHistoryRecord.deltaPercent))
                font: Typography.detail
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
            AppButton {
                Layout.fillWidth: true
                text: qsTr("Готово")
                variant: AppButton.Secondary
                onClicked: historyDetail.close()
            }
        }
    }
    SwipePopup {
        id: startComparison
        parent: Overlay.overlay
        dragHandle: comparisonGrip
        width: parent ? Math.min(420, parent.width - 32) : 328
        modal: true
        dim: true
        padding: 20
        contentItem: ColumnLayout {
            spacing: 20
            SwipeDismissHandle {
                id: comparisonGrip
                Layout.fillWidth: true
                onDismissed: startComparison.close()
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("С начала программы")
                font: Typography.sectionTitle
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                text: page.selectedExercise.exerciseTitle || ""
                font: Typography.body
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 20
                StatisticValue {
                    Layout.fillWidth: true
                    label: qsTr("В начале")
                    valueText: page.numberText(page.selectedExercise.startWeightKg) + " " + qsTr("кг")
                    detail: qsTr("%1 повт. в среднем").arg(page.numberText(page.selectedExercise.startReps))
                    valueFont: Typography.inputNumber
                    labelFont: Typography.detail
                }
                StatisticValue {
                    Layout.fillWidth: true
                    label: qsTr("Сейчас")
                    valueText: page.numberText(page.selectedExercise.currentWeightKg) + " " + qsTr("кг")
                    detail: qsTr("%1 повт. в среднем").arg(page.numberText(page.selectedExercise.currentReps))
                    valueFont: Typography.inputNumber
                    labelFont: Typography.detail
                }
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Изменение: %1 кг · %2 %").arg(page.signedText(page.selectedExercise.startDeltaKg)).arg(page.signedText(page.selectedExercise.startDeltaPercent))
                font: Typography.bodyMedium
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                visible: text.length > 0
                text: page.selectedExercise.rangeNote || ""
                font: Typography.detail
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
            AppButton {
                Layout.fillWidth: true
                text: qsTr("Готово")
                variant: AppButton.Secondary
                onClicked: startComparison.close()
            }
        }
    }
    SwipePopup {
        id: assessmentSheet
        parent: Overlay.overlay
        dragHandle: assessmentGrip
        width: parent ? Math.min(400, parent.width - 32) : 328
        modal: true
        dim: true
        padding: Spacing.cardPadding
        contentItem: ColumnLayout {
            spacing: Spacing.itemGap
            SwipeDismissHandle {
                id: assessmentGrip
                Layout.fillWidth: true
                onDismissed: assessmentSheet.close()
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Как оцениваем прогресс")
                font: Typography.sectionTitle
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Сравниваем максимальный рабочий вес и средние повторы. Разминка не учитывается. Допуск снижения повторов: %1.").arg(page.viewModel ? page.viewModel.repTolerance : 2)
                font: Typography.body
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("▲ Лучше\n● Без изменения или первый результат\n◆ Стоит проверить нагрузку\n▼ Хуже")
                font: Typography.body
                color: Theme.textPrimary
                lineHeight: 1.5
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Баланс = (улучшения − ухудшения) / сравнения. Первая тренировка задаёт старт и не оценивается.")
                font: Typography.detail
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
            AppButton {
                Layout.fillWidth: true
                text: qsTr("Понятно")
                variant: AppButton.Secondary
                onClicked: assessmentSheet.close()
            }
        }
    }
}
