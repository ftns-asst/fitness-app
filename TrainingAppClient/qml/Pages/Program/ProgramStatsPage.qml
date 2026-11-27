import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

PageScaffold {
    id: page
    property var viewModel: typeof ProgramStatsVM !== "undefined" ? ProgramStatsVM : null
    property var programViewModel: typeof ProgramVM !== "undefined" ? ProgramVM : null
    property string programId: ""
    property bool embedded: false
    property int sectionIndex: 0
    property int periodIndex: 2
    property int measurementIndex: 0
    property int nutritionIndex: 0
    property int dataRevision: 0
    property string forecastMeasurement: "weight"
    readonly property var forecastTrend: trendRows.find(row => row.param === forecastMeasurement) || ({})
    readonly property string forecastDate: dashboard.endDate || ""
    readonly property bool forecastAvailable: forecastTrend.points >= 2 && StatisticsUi.has(forecastTrend.ratePerWeek) && StatisticsUi.has(forecastTrend.forecastAtEnd) && Number(forecastTrend.forecastAtEnd) > 0 && forecastDate > referenceDate && !isNaN(Date.parse(forecastDate))
    readonly property string forecastMessage: explainForecast()
    property bool changeMapExpanded: false
    property bool detailsExpanded: false
    property string referenceDate: Qt.formatDate(new Date(), Qt.ISODate)
    readonly property bool hasData: viewModel !== null && viewModel !== undefined && viewModel.hasStats
    readonly property var dashboard: hasData && viewModel.dashboard !== undefined ? viewModel.dashboard : ({})
    readonly property var trendRows: StatisticsUi.rows(hasData ? viewModel.trends : null, dataRevision)
    readonly property var nutritionRows: StatisticsUi.rows(hasData ? viewModel.nutritionSummary : null, dataRevision)
    property var measurementOptions: [
        {
            id: "weight",
            title: qsTr("Вес")
        }
    ]
    readonly property string selectedMeasurement: measurementOptions[Math.min(measurementIndex, measurementOptions.length - 1)].id
    readonly property var selectedTrend: trendRows.find(row => row.param === selectedMeasurement) || ({})
    readonly property int selectedDigits: ["kcal", "ml", "count"].indexOf(selectedNutrition.unit) >= 0 ? 0 : 1
    readonly property string selectedUnit: RuleTexts.unitLabel(selectedNutrition.unit, selectedNutrition.unit === "count" ? "" : (selectedNutrition.unit || ""))
    readonly property var selectedNutrition: nutritionRows[Math.min(nutritionIndex, nutritionRows.length - 1)] || ({})
    readonly property var bodyPoints: selectedMeasurement === "weight" ? StatisticsUi.points(hasData ? viewModel.weight_series : null, "weightKg", "average7Kg", dataRevision) : StatisticsUi.points(hasData ? viewModel.measurements : null, selectedMeasurement + "Cm", "", dataRevision)
    readonly property var bounds: StatisticsUi.period(referenceDate, periodIndex)
    readonly property string periodCaption: bounds.from ? StatisticsUi.date(bounds.from) + " — " + StatisticsUi.date(bounds.to) : qsTr("За всю программу")
    signal backRequested
    signal strengthRequested

    title: embedded ? "" : qsTr("Прогресс")
    caption: embedded ? "" : qsTr("Изменения по вашим записям")
    showHeader: false
    spacing: Spacing.itemGap
    Component.onCompleted: {
        reload();
        updateMeasurementOptions();
    }
    onVisibleChanged: {
        if (visible)
            reload();
    }
    onProgramIdChanged: {
        detailsExpanded = false;
        changeMapExpanded = false;
        forecastMeasurement = "weight";
        if (forecastPopup)
            forecastPopup.close();
        measurementIndex = 0;
        measurementOptions = [
            {
                id: "weight",
                title: qsTr("Вес")
            }
        ];
        reload();
    }
    onPeriodIndexChanged: {
        detailsExpanded = false;
        reload();
    }
    onSectionIndexChanged: {
        detailsExpanded = false;
        pageFlickable.contentY = 0;
    }
    Connections {
        target: page.viewModel
        function onChanged() {
            page.dataRevision += 1;
            page.updateMeasurementOptions();
        }
    }

    function openForecast(param) {
        forecastMeasurement = param;
        forecastPopup.open();
    }
    function explainForecast() {
        if (!(forecastTrend.points > 0))
            return qsTr("За выбранный период нет замеров этого показателя.");
        if (forecastTrend.points < 2 || !StatisticsUi.has(forecastTrend.ratePerWeek))
            return qsTr("Нужны хотя бы два замера в разные дни. Добавьте ещё один замер или выберите более длинный период.");
        if (!forecastDate)
            return qsTr("У программы открытый срок. Темп можно рассчитать, но даты для прогноза на конец нет.");
        if (forecastDate <= referenceDate)
            return qsTr("Программа завершилась %1. Показан темп по записям; прогноз на прошедшую дату не строится.").arg(StatisticsUi.fullDate(forecastDate));
        if (StatisticsUi.has(forecastTrend.forecastAtEnd) && forecastTrend.forecastAtEnd <= 0)
            return qsTr("Линейный прогноз неприменим: продолжение текущего темпа даёт нулевое или отрицательное значение. Проверьте записи и выбранный период.");
        if (!forecastAvailable)
            return qsTr("Для этих данных прогноз недоступен.");
        return qsTr("Это продолжение темпа между первым и последним замером до %1. При изменении темпа результат будет другим.").arg(StatisticsUi.fullDate(forecastDate));
    }
    function updateMeasurementOptions() {
        const known = measurementOptions.slice();
        for (const row of StatisticsUi.rows(viewModel ? viewModel.trends : null))
            if (row.points > 0 && !known.some(option => option.id === row.param))
                known.push({
                    id: row.param,
                    title: MeasurementTexts.title(row.param)
                });
        measurementOptions = known;
    }
    function resolvedProgramId() {
        return programId || (programViewModel && programViewModel.hasProgram ? programViewModel.programId : "");
    }
    function reload() {
        if (!viewModel || !bounds)
            return;
        const userId = typeof AppContext !== "undefined" && AppContext ? AppContext.currentUserId : "u1";
        if (programViewModel && !programId)
            programViewModel.reload(userId);
        const period = StatisticsUi.period(referenceDate, periodIndex);
        viewModel.refresh(resolvedProgramId(), period.from, period.to);
    }
    function number(value, digits = 1) {
        return StatisticsUi.number(value, digits);
    }
    function trainingPoints(revision) {
        const start = new Date(String(dashboard.startDate || "") + "T12:00:00");
        if (isNaN(start.getTime()))
            return [];
        return StatisticsUi.rows(hasData ? viewModel.weeks : null, revision).filter(row => row.completedSessions > 0).map(row => {
            const date = new Date(start.getTime());
            date.setDate(date.getDate() + (row.weekIndex - 1) * 7);
            return {
                date: Qt.formatDate(date, Qt.ISODate),
                value: row.tonnageKg
            };
        });
    }

    BackBar {
        objectName: "statisticsBackBar"
        width: parent.width
        visible: !page.embedded
        onClicked: page.backRequested()
    }
    Label {
        width: parent.width
        visible: !page.embedded
        text: page.title
        font: Typography.workoutTitle
        color: Theme.textPrimary
    }
    SegmentedControl {
        objectName: "statisticsSections"
        width: parent.width
        items: [qsTr("Тело"), qsTr("Тренировки"), qsTr("Режим")]
        currentIndex: page.sectionIndex
        onSelected: index => page.sectionIndex = index
    }
    FilterChips {
        objectName: "statisticsPeriod"
        width: parent.width
        items: [qsTr("7 дней"), qsTr("28 дней"), qsTr("Программа")]
        currentIndex: page.periodIndex
        onSelected: index => page.periodIndex = index
    }
    RowLayout {
        width: parent.width
        Label {
            Layout.fillWidth: true
            text: page.periodCaption
            font: Typography.caption
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
        }
        Label {
            text: StatisticsUi.has(page.dashboard.currentDay) ? (StatisticsUi.has(page.dashboard.totalDays) ? qsTr("День %1 / %2").arg(page.dashboard.currentDay).arg(page.dashboard.totalDays) : qsTr("День %1").arg(page.dashboard.currentDay)) : ""
            font: Typography.captionStrong
            color: Theme.textSecondary
        }
    }
    AppCard {
        visible: page.viewModel && page.viewModel.errorKey.length > 0
        width: parent.width
        Label {
            width: parent.width
            text: qsTr("Не удалось загрузить статистику")
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

    Column {
        objectName: "bodyStatistics"
        width: parent.width
        spacing: Spacing.itemGap
        visible: page.sectionIndex === 0
        AppComboBox {
            objectName: "bodyMetricSelector"
            width: parent.width
            visible: page.measurementOptions.length > 1
            model: page.measurementOptions
            currentIndex: Math.min(page.measurementIndex, count - 1)
            Accessible.name: qsTr("Параметр тела")
            onActivated: page.measurementIndex = currentIndex
        }
        AppCard {
            width: parent.width
            elevated: false
            Label {
                text: MeasurementTexts.title(page.selectedMeasurement)
                font: Typography.bodyStrong
                color: Theme.textPrimary
            }
            RowLayout {
                width: parent.width
                Label {
                    Layout.fillWidth: true
                    text: page.number(bodyPlot.selected ? bodyPlot.selected.value : null) + " " + MeasurementTexts.unit(page.selectedMeasurement)
                    font: Typography.largeTitle
                    color: Theme.textPrimary
                }
                Label {
                    text: StatisticsUi.signed(page.selectedTrend.delta) + " " + MeasurementTexts.unit(page.selectedMeasurement)
                    visible: StatisticsUi.has(page.selectedTrend.delta)
                    font: Typography.bodyStrong
                    color: page.selectedTrend.favorable === false ? Theme.warning : Theme.textSecondary
                }
            }
            Label {
                width: parent.width
                text: bodyPlot.selected ? StatisticsUi.date(bodyPlot.selected.date) + (StatisticsUi.has(page.selectedTrend.delta) ? qsTr(" · изменение за выбранный период") : qsTr(" · первый замер")) : qsTr("Нет замеров за этот период")
                font: Typography.caption
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
            StatisticsPlot {
                id: bodyPlot
                objectName: "bodyStatisticsPlot"
                width: parent.width
                points: page.bodyPoints
                unit: MeasurementTexts.unit(page.selectedMeasurement)
                showSelectionHeader: false
                showAverage: page.selectedMeasurement === "weight"
            }
            RowLayout {
                width: parent.width
                visible: page.selectedMeasurement === "weight" && StatisticsUi.has(page.dashboard.targetWeightKg)
                StatisticValue {
                    Layout.fillWidth: true
                    label: qsTr("Цель")
                    valueText: page.number(page.dashboard.targetWeightKg) + " " + qsTr("кг")
                    detail: StatisticsUi.has(page.dashboard.remainingKg) ? qsTr("Осталось %1 кг").arg(page.number(page.dashboard.remainingKg)) : ""
                }
                StatisticValue {
                    Layout.fillWidth: true
                    label: qsTr("В начале периода")
                    valueText: page.number(page.selectedTrend.start) + " " + qsTr("кг")
                }
            }
        }
        DisclosureCard {
            objectName: "measurementDetails"
            width: parent.width
            title: qsTr("Сравнение замеров")
            summary: qsTr("Начало, последний замер и изменение")
            showIcon: false
            titleFont: Typography.bodyMedium
            summaryFont: Typography.detail
            doneText: qsTr("Свернуть")
            expanded: page.detailsExpanded
            onToggleRequested: page.detailsExpanded = !page.detailsExpanded
            onDoneRequested: page.detailsExpanded = false
            body: Column {
                width: parent.width
                spacing: 20
                Column {
                    objectName: "measurementFacts"
                    width: parent.width
                    spacing: 0
                    Label {
                        width: parent.width
                        text: qsTr("Изменения за выбранный период")
                        font: Typography.detail
                        color: Theme.textSecondary
                        bottomPadding: 12
                        wrapMode: Text.WordWrap
                    }
                    Repeater {
                        model: page.trendRows.filter(row => row.points > 0)
                        delegate: WorkoutResultRow {
                            required property var modelData
                            required property int index
                            width: parent.width
                            leftPadding: 0
                            rightPadding: 0
                            title: MeasurementTexts.title(modelData.param)
                            subtitle: qsTr("Начало: %1 %2").arg(page.number(modelData.start)).arg(MeasurementTexts.unit(modelData.param))
                            valueText: page.number(modelData.latest) + " " + MeasurementTexts.unit(modelData.param)
                            detailText: StatisticsUi.has(modelData.delta) ? StatisticsUi.signed(modelData.delta) + " " + MeasurementTexts.unit(modelData.param) : qsTr("Первый замер")
                            navigation: true
                            onClicked: {
                                page.openForecast(modelData.param);
                            }
                        }
                    }
                    Label {
                        width: parent.width
                        visible: !page.trendRows.some(row => row.points > 0)
                        text: qsTr("Добавьте замеры в дневник")
                        font: Typography.body
                        color: Theme.textSecondary
                        wrapMode: Text.WordWrap
                    }
                    AppButton {
                        width: parent.width
                        visible: page.hasData && page.viewModel.change_map !== undefined && page.viewModel.change_map.count > 0
                        text: page.changeMapExpanded ? qsTr("Скрыть карту изменений") : qsTr("Карта изменений по датам")
                        variant: AppButton.Ghost
                        labelFont: Typography.detail
                        onClicked: page.changeMapExpanded = !page.changeMapExpanded
                    }
                    MeasurementChangeMap {
                        width: parent.width
                        visible: page.changeMapExpanded
                        model: page.hasData && page.viewModel.change_map !== undefined ? page.viewModel.change_map : null
                    }
                }
                AppButton {
                    width: parent.width
                    text: qsTr("Темп и прогноз")
                    variant: AppButton.Secondary
                    onClicked: page.openForecast(page.selectedMeasurement)
                }
            }
        }
    }
    Column {
        objectName: "trainingStatistics"
        visible: page.sectionIndex === 1
        width: parent.width
        spacing: Spacing.itemGap
        AppCard {
            width: parent.width
            elevated: false
            Label {
                text: qsTr("За выбранный период")
                font: Typography.bodyStrong
                color: Theme.textPrimary
            }
            RowLayout {
                width: parent.width
                StatisticValue {
                    Layout.fillWidth: true
                    label: qsTr("Тренировок")
                    valueText: page.hasData ? String(page.viewModel.completedSessions) : "—"
                }
                StatisticValue {
                    Layout.fillWidth: true
                    label: qsTr("Подходов")
                    valueText: page.hasData ? String(page.viewModel.setsCount) : "—"
                }
                StatisticValue {
                    Layout.fillWidth: true
                    label: qsTr("Минут")
                    valueText: page.hasData ? page.number(page.viewModel.durationMinutes, 0) : "—"
                }
            }
            Label {
                width: parent.width
                text: qsTr("Объём нагрузки по неделям")
                font: Typography.caption
                color: Theme.textSecondary
            }
            StatisticsPlot {
                width: parent.width
                points: page.trainingPoints(page.dataRevision)
                unit: qsTr("кг")
                decimals: 0
            }
            Label {
                width: parent.width
                text: page.hasData ? qsTr("Всего: %1 кг · %2 повторов").arg(page.number(page.viewModel.tonnageKg, 0)).arg(page.viewModel.repsCount) : ""
                font: Typography.caption
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
        }
        AppButton {
            objectName: "strengthStatisticsLink"
            width: parent.width
            text: qsTr("Сила и рабочие веса  ›")
            variant: AppButton.Secondary
            onClicked: page.strengthRequested()
        }
        Label {
            width: parent.width
            text: qsTr("В объём входят вес × повторы выполненных рабочих подходов. Разминка не учитывается.")
            font: Typography.caption
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
        }
    }
    Column {
        objectName: "nutritionStatistics"
        visible: page.sectionIndex === 2
        width: parent.width
        spacing: Spacing.itemGap
        AppComboBox {
            objectName: "nutritionMetricSelector"
            width: parent.width
            visible: page.nutritionRows.length > 1
            model: page.nutritionRows.map((row, index) => ({
                        id: index,
                        title: row.title
                    }))
            Accessible.name: qsTr("Показатель питания или привычек")
            currentIndex: Math.min(page.nutritionIndex, count - 1)
            onActivated: page.nutritionIndex = currentIndex
        }
        AppCard {
            width: parent.width
            elevated: false
            Label {
                text: page.selectedNutrition.title || qsTr("Питание и привычки")
                font: Typography.bodyStrong
                color: Theme.textPrimary
            }
            Label {
                width: parent.width
                text: page.number(page.selectedNutrition.average, page.selectedDigits) + (page.selectedNutrition.unit ? " " + page.selectedUnit : "")
                font: Typography.largeTitle
                color: Theme.textPrimary
            }
            Label {
                width: parent.width
                text: page.nutritionRows.length ? qsTr("Среднее по записям · цель %1").arg(page.selectedNutrition.targetText) : qsTr("Добавьте числовые правила в программу")
                font: Typography.caption
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
            StatisticsPlot {
                width: parent.width
                points: page.selectedNutrition.points || []
                unit: page.selectedUnit
                decimals: page.selectedDigits
            }
            RowLayout {
                width: parent.width
                visible: StatisticsUi.has(page.selectedNutrition.average)
                StatisticValue {
                    Layout.fillWidth: true
                    label: qsTr("Дней в норме")
                    valueText: String(page.selectedNutrition.daysInNorm || 0)
                }
                StatisticValue {
                    Layout.fillWidth: true
                    label: qsTr("Вне нормы")
                    valueText: String(page.selectedNutrition.daysOff || 0)
                }
            }
        }
        Label {
            width: parent.width
            text: qsTr("Пропущенные дни не считаются нулями. Показаны только заполненные значения.")
            font: Typography.caption
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
        }
    }
    SwipePopup {
        id: forecastPopup
        objectName: "measurementForecastPopup"
        parent: Overlay.overlay
        dragHandle: forecastGrip
        width: parent ? Math.min(440, parent.width - 32) : 328
        height: parent ? Math.min(620, parent.height - 48) : 560
        modal: true
        dim: true
        padding: 20
        contentItem: ColumnLayout {
            objectName: "measurementForecast"
            spacing: 16
            SwipeDismissHandle {
                id: forecastGrip
                Layout.fillWidth: true
                onDismissed: forecastPopup.close()
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Темп и прогноз")
                font: Typography.sectionTitle
                color: Theme.textPrimary
            }
            AppComboBox {
                objectName: "forecastMetricSelector"
                Layout.fillWidth: true
                model: page.measurementOptions
                currentIndex: Math.max(0, page.measurementOptions.findIndex(option => option.id === page.forecastMeasurement))
                onActivated: page.forecastMeasurement = page.measurementOptions[currentIndex].id
                Accessible.name: qsTr("Показатель для прогноза")
            }
            ScrollView {
                id: forecastScroll
                Layout.fillWidth: true
                Layout.fillHeight: true
                clip: true
                contentWidth: availableWidth
                Column {
                    width: forecastScroll.availableWidth
                    spacing: 20
                    Label {
                        width: parent.width
                        text: page.forecastTrend.points === 1 && page.forecastTrend.latestDate ? qsTr("Замер от %1").arg(StatisticsUi.fullDate(page.forecastTrend.latestDate)) : page.forecastTrend.startDate && page.forecastTrend.latestDate ? qsTr("По замерам: %1 — %2").arg(StatisticsUi.fullDate(page.forecastTrend.startDate)).arg(StatisticsUi.fullDate(page.forecastTrend.latestDate)) : page.periodCaption
                        font: Typography.detail
                        color: Theme.textSecondary
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        width: parent.width
                        visible: page.forecastTrend.points > 0
                        StatisticValue {
                            Layout.fillWidth: true
                            visible: page.forecastTrend.points > 1
                            label: qsTr("Первый замер")
                            valueText: page.number(page.forecastTrend.start) + " " + MeasurementTexts.unit(page.forecastMeasurement)
                            valueFont: Typography.inputNumber
                        }
                        StatisticValue {
                            Layout.fillWidth: true
                            label: page.forecastTrend.points > 1 ? qsTr("Последний") : qsTr("Замер")
                            valueText: page.number(page.forecastTrend.latest) + " " + MeasurementTexts.unit(page.forecastMeasurement)
                            valueFont: Typography.inputNumber
                        }
                    }
                    StatisticValue {
                        objectName: "forecastRate"
                        width: parent.width
                        visible: StatisticsUi.has(page.forecastTrend.ratePerWeek)
                        label: qsTr("Темп в неделю")
                        valueText: StatisticsUi.signed(page.forecastTrend.ratePerWeek) + " " + MeasurementTexts.unit(page.forecastMeasurement)
                        valueFont: Typography.inputNumber
                    }
                    StatisticValue {
                        objectName: "forecastEstimate"
                        width: parent.width
                        visible: page.forecastAvailable
                        label: qsTr("Если темп сохранится · %1").arg(StatisticsUi.date(page.forecastDate))
                        valueText: page.number(page.forecastTrend.forecastAtEnd) + " " + MeasurementTexts.unit(page.forecastMeasurement)
                        valueFont: Typography.figure
                    }
                    Label {
                        objectName: "forecastExplanation"
                        width: parent.width
                        text: page.forecastMessage
                        font: Typography.body
                        color: Theme.textSecondary
                        wrapMode: Text.WordWrap
                    }
                }
            }
            AppButton {
                Layout.fillWidth: true
                objectName: "forecastCloseButton"
                text: qsTr("Закрыть")
                variant: AppButton.Secondary
                onClicked: forecastPopup.close()
            }
        }
    }
}
