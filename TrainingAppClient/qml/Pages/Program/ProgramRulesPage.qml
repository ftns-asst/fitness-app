import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Правила программы и отметки пользователя (экран 43, §3.6–3.7).
//
// Состояние за сегодня приходит ролью todayState из Avm_Program_Rules, окно
// отметок — periodDays. Расчётные правила (computedFrom*) показывают
// «неизвестно»: их проверяет движок правил по фактам тренировок, а не сам
// пользователь, поэтому отметить их здесь нельзя (§3.6).
PageScaffold {
    id: page

    property var viewModel: ProgramRulesVM
    // Панель возврата обязательна: экраны программы открыты поверх вкладок, где
    // таб-бара нет.
    BackBar {
        Layout.fillWidth: true
        onClicked: page.backRequested()
        text: qsTr("Назад")
        visible: true
    }

    caption: page.hasProgram ? qsTr("%1 · окно %2…%3").arg(page.today).arg(page.rangeFrom).arg(page.rangeTo) : qsTr("Нет активной программы")
    title: qsTr("Правила и отметки")

    readonly property bool hasProgram: page.viewModel !== undefined && page.viewModel !== null && page.viewModel.hasProgram
    readonly property string rangeFrom: page.hasProgram ? page.viewModel.rangeFrom : ""
    readonly property string rangeTo: page.hasProgram ? page.viewModel.rangeTo : ""
    readonly property string today: page.hasProgram ? page.viewModel.today : ""

    onVisibleChanged: {
        if (visible)
            page.reload();
    }

    function reload() {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        page.viewModel.reload("u1");
    }

    Label {
        color: Theme.textMuted
        font: Typography.sectionTitle
        text: qsTr("Выполнено за сегодня")
        visible: page.hasProgram
    }
    AppCard {
        visible: page.hasProgram

        RowLayout {
            width: parent.width
            spacing: Spacing.itemGap

            Label {
                Layout.fillWidth: true
                color: Theme.textPrimary
                font: Typography.metric
                text: page.adherenceText()
            }
            AppButton {
                onClicked: page.cyclePeriod()
                text: page.periodText()
                variant: AppButton.Secondary
            }
        }
    }
    Repeater {
        model: page.hasProgram ? page.viewModel.rules : []

        delegate: AppCard {
            id: ruleCard

            required property string code
            required property string displayText
            required property string ruleId
            required property string todayState
            required property string title

            readonly property bool isDone: todayState === "done"
            readonly property bool isManual: code !== undefined

            ColumnLayout {
                width: parent.width
                spacing: Spacing.itemGap

                RowLayout {
                    Layout.fillWidth: true
                    spacing: Spacing.itemGap

                    Label {
                        Layout.fillWidth: true
                        color: Theme.textPrimary
                        font: Typography.bodyStrong
                        text: ruleCard.title
                        wrapMode: Text.WordWrap
                    }
                    CheckinStatePill {
                        state: ruleCard.todayState
                    }
                }
                Label {
                    color: Theme.textSecondary
                    font: Typography.caption
                    text: ruleCard.displayText
                    wrapMode: Text.WordWrap
                }
                // Отметка вручную: чужой сессией это не считается, поэтому
                // кнопки есть только у правил, которые пользователь ведёт сам.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Spacing.itemGap
                    visible: ruleCard.todayState !== "computed"

                    AppButton {
                        Layout.fillWidth: true
                        onClicked: page.save(ruleCard.ruleId, "done")
                        text: qsTr("Выполнено")
                        variant: ruleCard.isDone ? AppButton.Primary : AppButton.Secondary
                    }
                    AppButton {
                        Layout.fillWidth: true
                        onClicked: page.save(ruleCard.ruleId, "failed")
                        text: qsTr("Не выполнено")
                        variant: ruleCard.todayState === "failed" ? AppButton.Primary : AppButton.Secondary
                    }
                }
            }
        }
    }
    Label {
        color: Theme.textSecondary
        font: Typography.body
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("Отметок за это окно нет")
        visible: page.hasProgram && page.checkinCount === 0
        width: parent.width
        wrapMode: Text.WordWrap
    }

    // Запрос возврата: экраны программы открыты поверх вкладок, где таб-бара нет.
    signal backRequested
    readonly property int checkinCount: page.hasProgram ? page.viewModel.checkinCount : 0
    readonly property int periodDays: page.hasProgram ? page.viewModel.periodDays : 7

    function save(ruleId, state) {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        page.viewModel.saveCheckin(ruleId, state, 0, false, "");
    }

    // Окно отметок переключается по кругу: неделя → месяц → квартал (§5.1).
    function cyclePeriod() {
        const order = [7, 30, 90];
        const index = order.indexOf(page.periodDays);

        page.viewModel.setPeriodDays(order[(index + 1) % order.length]);
    }

    function periodText() {
        if (page.periodDays === 30)
            return qsTr("Месяц");

        if (page.periodDays === 90)
            return qsTr("Квартал");

        return qsTr("Неделя");
    }

    function adherenceText() {
        if (page.hasProgram === false || page.viewModel.hasAdherencePercent === false)
            return "—";

        return qsTr("%1 %").arg(Math.round(page.viewModel.adherencePercent));
    }
}
