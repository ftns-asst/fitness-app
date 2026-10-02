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

    // Идентификатор пользователя: до появления реальной авторизации в UI это
    // единственный источник «кто я», поэтому он вынесен в одно место.
    function currentUserId() {
        return "u1";
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
                // Роли берутся прямо из модели через model.<role>. Объявлять их
                // как required property нельзя: у CatalogProgramRow уже есть
                // свои свойства с теми же именами, и Qt молча игнорирует
                // перекрывающее объявление — карточка осталась бы пустой.
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                programId: model.id
                title: model.title
                goal: model.goal
                state: model.state
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

                // Роли берутся прямо из модели через model.<role>. Объявлять их
                // как required property нельзя: у CatalogProgramRow уже есть
                // свои свойства с теми же именами, и Qt молча игнорирует
                // перекрывающее объявление — карточка осталась бы пустой.
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                programId: model.id
                title: model.title
                goal: model.goal
                state: model.state
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

                // Роли берутся прямо из модели через model.<role>. Объявлять их
                // как required property нельзя: у CatalogProgramRow уже есть
                // свои свойства с теми же именами, и Qt молча игнорирует
                // перекрывающее объявление — карточка осталась бы пустой.
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                programId: model.id
                title: model.title
                goal: model.goal
                state: model.state
                weeksTotal: model.weeksTotal
                canEdit: model.canEdit
                onOpenRequested: programId => page.openRequested(programId)
                onEditRequested: programId => page.editRequested(programId)
                onStatsRequested: programId => page.statsRequested(programId)
            }
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
