import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Статистика программы (экран 44, §5.1). Данные считает AsProgress_Calculator
// по фактам тренировок, ViewModel только отдаёт их и ведёт кэш program_stats.
//
// Метрика без данных показывается как «—», а не нулём: ноль выполненных
// подходов и отсутствие тренировок — разные вещи (§5.5).
PageScaffold {
    id: page

    property var viewModel: ProgramStatsVM
    property var programViewModel: ProgramVM
    property string programId: ""
    // Панель возврата обязательна: экраны программы открыты поверх вкладок, где
    // таб-бара нет.
    BackBar {
        Layout.fillWidth: true
        onClicked: page.backRequested()
        text: qsTr("Назад")
        visible: true
    }

    caption: page.hasData ? qsTr("По завершённым тренировкам") : qsTr("Пока нет данных")
    title: qsTr("Статистика")

    // Запрос возврата: экраны программы открыты поверх вкладок, где таб-бара нет.
    signal backRequested
    readonly property bool hasData: page.viewModel !== undefined && page.viewModel !== null && page.viewModel.hasStats

    // Идентификатор программы для статистики. Пустой programId означает
    // «активная программа»: тогда он берётся из ProgramVM, который умеет её
    // находить. Раньше здесь передавался идентификатор пользователя, из-за чего
    // refresh() читал несуществующую программу и статистика была пустой.
    function resolvedProgramId() {
        if (page.programId.length > 0)
            return page.programId;

        return page.programViewModel !== undefined && page.programViewModel !== null && page.programViewModel.hasProgram ? page.programViewModel.programId : "";
    }

    // Загрузка идёт при создании и при каждом показе: у страницы, открытой
    // прямо в StackView (например, по --screen), visible не меняется.
    Component.onCompleted: page.reload()
    onVisibleChanged: {
        if (visible)
            page.reload();
    }

    function reload() {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        page.viewModel.refresh(page.resolvedProgramId());
    }

    RowLayout {
        Layout.fillWidth: true
        spacing: Spacing.listGap

        MetricCard {
            Layout.fillWidth: true
            highlight: true
            label: qsTr("Выполнение")
            valueText: page.percent(page.viewModel !== undefined && page.viewModel !== null && page.viewModel.hasAdherencePercent ? page.viewModel.adherencePercent : null)
        }
        MetricCard {
            Layout.fillWidth: true
            label: qsTr("Тоннаж, т")
            valueText: page.tonnageText()
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: Spacing.listGap

        MetricCard {
            Layout.fillWidth: true
            label: qsTr("Тренировок")
            valueText: page.viewModel !== undefined && page.viewModel !== null ? String(page.viewModel.completedSessions) : "—"
        }
        MetricCard {
            Layout.fillWidth: true
            label: qsTr("Подходов")
            valueText: page.viewModel !== undefined && page.viewModel !== null ? String(page.viewModel.setsCount) : "—"
        }
    }
    RowLayout {
        Layout.fillWidth: true
        spacing: Spacing.listGap

        MetricCard {
            Layout.fillWidth: true
            label: qsTr("Тело, кг")
            valueText: page.bodyWeightText()
        }
    }
    Label {
        color: Theme.textMuted
        font: Typography.sectionTitle
        text: qsTr("Тоннаж по неделям")
        visible: page.hasData
    }
    LineChartCard {
        Layout.fillWidth: true
        title: qsTr("Тоннаж, кг")
        values: page.chartValues()
        visible: page.hasData
    }
    Label {
        color: Theme.textSecondary
        font: Typography.body
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("Запишите первую тренировку — здесь появятся тоннаж, подходы и динамика по неделям.")
        visible: page.hasData === false
        width: parent.width
        wrapMode: Text.WordWrap
    }

    // Точки графика — простой массив чисел: LineChartCard сам раскладывает их
    // по осям. Значения берутся из роли weeks недельных срезов.
    function chartValues() {
        const values = [];
        const model = page.hasData ? page.viewModel.weeks : null;

        if (model === undefined || model === null)
            return values;

        for (let i = 0; i < model.rowCount(); ++i) {
            values.push(model.data(model.index(i, 0), 256));
        }

        return values;
    }

    function percent(value) {
        if (value === null || value === undefined)
            return "—";

        return qsTr("%1 %").arg(Math.round(value));
    }

    function tonnageText() {
        if (page.hasData === false)
            return "—";

        return (page.viewModel.tonnageKg / 1000).toFixed(1);
    }

    // Дельта веса показывается со знаком: минус означает снижение (§5.1).
    function bodyWeightText() {
        const vm = page.viewModel;

        if (vm === undefined || vm === null || vm.hasBodyWeightDeltaKg === false)
            return "—";

        const delta = vm.bodyWeightDeltaKg;

        return (delta > 0 ? "+" : "") + delta.toFixed(1);
    }
}
