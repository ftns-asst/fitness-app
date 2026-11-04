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
        onClicked: page.backRequested()
        text: qsTr("Назад")
        visible: true
        width: parent.width
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

    // Идентификатор берётся из AppContext: литерал «u1» повторялся в четырёх
    // экранах, и любое другое значение делало программу невидимой.
    function currentUserId() {
        return typeof AppContext !== "undefined" && AppContext !== null ? AppContext.currentUserId : "u1";
    }

    function reload() {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        // ProgramVM загружается здесь же: экран открыт напрямую (например, по
        // --screen), минуя обзор, поэтому активная программа ещё не искалась.
        // Без этого resolvedProgramId() давал пустую строку и статистика
        // оставалась пустой при непустой базе.
        if (page.programViewModel !== undefined && page.programViewModel !== null && page.programId.length === 0)
            page.programViewModel.reload(page.currentUserId());

        page.viewModel.refresh(page.resolvedProgramId());
    }

    // Ширина каждого ряда задаётся явно: колонка контента PageScaffold — это
    // Column, а в ней `Layout.fillWidth` не действует, и без `width` ряд
    // схлопывался бы до суммы implicitWidth карточек (то есть заметно уже
    // области контента). Внутри ряда `Layout.fillWidth` уже работает: там
    // настоящий layout.
    RowLayout {
        spacing: Spacing.listGap
        width: parent.width

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
        spacing: Spacing.listGap
        width: parent.width

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
        spacing: Spacing.listGap
        width: parent.width

        MetricCard {
            Layout.fillWidth: true
            label: qsTr("Тело, кг")
            valueText: page.bodyWeightText()
        }
        // Выполнение по правилам стоит в паре с весом: одинокая карточка на всю
        // ширину ряда читалась как пустое место. Метрика не серия недель (она
        // равна нулю, пока неделю не выполнили целиком) и не дубль «Выполнения»:
        // «Выполнение» считается по расписанию, это — по отметкам правил (§5.1).
        MetricCard {
            Layout.fillWidth: true
            label: qsTr("Правила, %")
            valueText: page.ruleAdherenceText()
        }
    }
    Label {
        color: Theme.textMuted
        font: Typography.sectionTitle
        text: qsTr("Тоннаж по неделям")
        visible: page.hasData
        width: parent.width
    }
    LineChartCard {
        captionText: page.chartCaption()
        title: qsTr("Тоннаж, кг")
        values: page.chartValues()
        visible: page.hasData
        width: parent.width
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
    // по осям. Значения берутся из строки недельного среза по имени роли:
    // числового индекса роли в QML нет (`roleNames` отдаёт имена, а значения
    // начинаются с Qt::UserRole + 1), поэтому чтение среза по числу возвращало
    // пустые значения — сетка рисовалась, а линии тоннажа не было.
    function chartValues() {
        const values = [];
        const model = page.hasData ? page.viewModel.weeks : null;

        if (model === undefined || model === null)
            return values;

        for (let i = 0; i < model.rowCount(); ++i) {
            const row = model.get(i);

            if (row !== undefined && row !== null && row.tonnageKg !== undefined)
                values.push(Number(row.tonnageKg));
        }

        return values;
    }

    function percent(value) {
        if (value === null || value === undefined)
            return "—";

        return qsTr("%1 %").arg(Math.round(value));
    }

    // Процент по правилам без данных остаётся прочерком, а не нулём: ноль
    // выполненных отметок и их отсутствие — разные состояния (§5.5).
    function ruleAdherenceText() {
        const vm = page.viewModel;

        if (vm === undefined || vm === null || vm.hasRuleAdherencePercent === false)
            return "—";

        return String(Math.round(vm.ruleAdherencePercent));
    }

    // Подпись под заголовком графика: сколько недель в окне и каков пик тоннажа.
    // Без неё линия не читается — по ней видно только форму, но не масштаб.
    function chartCaption() {
        const model = page.hasData ? page.viewModel.weeks : null;

        if (model === undefined || model === null || model.rowCount() === 0)
            return "";

        let peak = 0;

        for (let i = 0; i < model.rowCount(); ++i) {
            const value = Number(model.get(i).tonnageKg);

            peak = Math.max(peak, value);
        }

        return qsTr("%1 нед. · пик %2 кг").arg(model.rowCount()).arg(Math.round(peak));
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
