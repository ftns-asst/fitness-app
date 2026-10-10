import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Каталог программ (срез S1, issue 47). Показывает ВСЕ программы
// пользователя, а не только активную: созданная программа обязана быть
// найдена, иначе её создание бессмысленно.
//
// Данные — из настоящей БД через Avm_Program_Catalog, а не из mock. Группы
// приходят готовыми моделями (active/draft/finished): фильтровать одну общую
// модель в QML нельзя, потому что AList_Model отдаёт роли только по номеру.
PageScaffold {
    id: page

    property var viewModel: ProgramCatalogVM
    BackBar {
        Layout.fillWidth: true
        onClicked: page.backRequested()
        text: qsTr("Назад")
        visible: true
    }

    title: qsTr("Мои программы")

    readonly property bool hasPrograms: page.viewModel !== undefined && page.viewModel !== null && page.viewModel.hasPrograms
    readonly property bool failed: page.viewModel !== undefined && page.viewModel !== null && page.viewModel.errorKey !== ""
    readonly property int programCount: page.hasPrograms ? page.viewModel.active_list.count + page.viewModel.draft_list.count + page.viewModel.finished_list.count : 0

    caption: page.hasPrograms ? qsTr("%1 в каталоге").arg(page.programCount) : qsTr("Каталог программ")

    // Загрузка идёт при создании и при каждом показе: у страницы, открытой
    // прямо в StackView (например, по --screen), visible не меняется — она
    // создаётся уже видимой, — поэтому onVisibleChanged не срабатывает и данные
    // остались бы незагруженными. onVisibleChanged оставлен для возврата на
    // экран: он нужен, чтобы список отражал сделанные правки.
    Component.onCompleted: page.reload()
    onVisibleChanged: {
        if (visible)
            page.reload();
    }

    // Идентификатор пользователя берётся из общего источника: раньше он был
    // литералом «u1» здесь и ещё в трёх экранах, и любое другое значение
    // (например, посев демо-данных под другим id) делало программу невидимой.
    function currentUserId() {
        return AppContext !== undefined && AppContext !== null ? AppContext.currentUserId : "u1";
    }

    function reload() {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        page.viewModel.reload(page.currentUserId());
    }
    //MARK_BACK

    // — Пустой каталог: приглашение создать первую программу —
    AppCard {
        width: parent.width
        visible: page.hasPrograms === false
        ColumnLayout {
            // Внутри AppCard ширину задаёт родитель, а не Layout: содержимое
            // карточки лежит в обычной Column, где Layout-свойства не работают.
            width: parent.width
            spacing: Spacing.itemGap

            Label {
                Layout.fillWidth: true
                color: Theme.textPrimary
                font: Typography.sectionTitle
                text: page.failed ? qsTr("Не удалось загрузить") : qsTr("Программ пока нет")
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                color: Theme.textSecondary
                font: Typography.body
                text: page.failed ? qsTr("Повторите попытку позже.") : qsTr("Создайте программу: выберите срок и цель, настройте каждый день недели и добавьте правила.")
                wrapMode: Text.WordWrap
            }
            AppButton {
                Layout.fillWidth: true
                onClicked: page.createRequested()
                text: qsTr("Создать программу")
                variant: AppButton.Primary
            }
        }
    }
    // — Группы каталога: активная, черновики и пауза, завершённые —
    // Группы перечислены явно, а не итерируются по словарю: порядок групп на
    // экране задан вёрсткой, и словарь в QML не гарантирует порядок ключей.
    ColumnLayout {
        // Ширина задаётся родителем: список лежит в обычной Column
        // PageScaffold, где Layout.fillWidth не действует.
        width: parent.width
        spacing: Spacing.sectionGap
        visible: page.hasPrograms

        Label {
            Layout.fillWidth: true
            color: Theme.textMuted
            font: Typography.sectionTitle
            text: qsTr("Активная")
            visible: page.viewModel.active_list.count > 0
            wrapMode: Text.WordWrap
        }
        Repeater {
            model: page.hasPrograms ? page.viewModel.active_list : null
            delegate: CatalogProgramRow {
                // Роли объявляются required property: имена свойств компонента
                // (programState вместо state) больше не совпадают с ролями
                // модели, поэтому конфликта нет. Раньше он был, и поля
                // приходилось заполнять вручную через model.<role>.
                // Роли берутся через model.<role>, а не объявляются required
                // property: имена ролей (title, goal, state) совпадают с
                // одноимёнными свойствами компонента, и объявление required
                // property перекрыло бы их — карточка осталась бы без полей.
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                programId: model.id
                title: model.title
                goal: model.goal
                programState: model.state
                weeksTotal: model.weeksTotal
                canEdit: model.canEdit
                onOpenRequested: programId => page.openRequested(programId)
                onEditRequested: programId => page.editRequested(programId)
                onStatsRequested: programId => page.statsRequested(programId)
            }
        }

        Label {
            Layout.fillWidth: true
            color: Theme.textMuted
            font: Typography.sectionTitle
            text: qsTr("Черновики и пауза")
            visible: page.hasPrograms && page.viewModel.draft_list.count > 0
            wrapMode: Text.WordWrap
        }
        Repeater {
            model: page.hasPrograms ? page.viewModel.draft_list : null
            delegate: CatalogProgramRow {

                // Роли берутся через model.<role>, а не объявляются required
                // property: имена ролей (title, goal, state) совпадают с
                // одноимёнными свойствами компонента, и объявление required
                // property перекрыло бы их — карточка осталась бы без полей.
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                programId: model.id
                title: model.title
                goal: model.goal
                programState: model.state
                weeksTotal: model.weeksTotal
                canEdit: model.canEdit
                onOpenRequested: programId => page.openRequested(programId)
                onEditRequested: programId => page.editRequested(programId)
                onStatsRequested: programId => page.statsRequested(programId)
            }
        }

        Label {
            Layout.fillWidth: true
            color: Theme.textMuted
            font: Typography.sectionTitle
            text: qsTr("Завершённые")
            visible: page.hasPrograms && page.viewModel.finished_list.count > 0
            wrapMode: Text.WordWrap
        }
        Repeater {
            model: page.hasPrograms ? page.viewModel.finished_list : null
            delegate: CatalogProgramRow {

                // Роли берутся через model.<role>, а не объявляются required
                // property: имена ролей (title, goal, state) совпадают с
                // одноимёнными свойствами компонента, и объявление required
                // property перекрыло бы их — карточка осталась бы без полей.
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                programId: model.id
                title: model.title
                goal: model.goal
                programState: model.state
                weeksTotal: model.weeksTotal
                canEdit: model.canEdit
                onOpenRequested: programId => page.openRequested(programId)
                onEditRequested: programId => page.editRequested(programId)
                onStatsRequested: programId => page.statsRequested(programId)
            }
        }

        // Создание доступно и в наполненном каталоге: кнопка жила только в
        // пустом состоянии, и при наличии хотя бы одной программы новую было
        // не создать.
        AppButton {
            Layout.fillWidth: true
            onClicked: page.createRequested()
            text: qsTr("Создать программу")
            variant: AppButton.Primary
        }
    }

    // Запросы возврата и переходов: экраны программы открыты поверх вкладок,
    // где таб-бара нет.
    signal backRequested
    signal createRequested
    signal openRequested(string programId)
    signal editRequested(string programId)
    signal statsRequested(string programId)
}
