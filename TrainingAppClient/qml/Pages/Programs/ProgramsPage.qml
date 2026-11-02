import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Экран «Программы» (вкладка 2). Раньше это были «Планы»: вход в свою программу,
// промо-опрос подбора и каталог шаблонов в одном списке, где половина первого
// экрана — заглушки. Теперь два сегмента с разными задачами: «Мои» показывает
// реальные программы из БД, «Шаблоны» — готовые планы, честно помеченные как
// демонстрационные.
//
// Разметка — обычные Column с явной шириной: колонка контента PageScaffold не
// layout, а у скрытого элемента ColumnLayout ширина обнуляется, и аудит вёрстки
// справедливо ругается на нулевую карточку.
PageScaffold {
    id: page

    // Свои программы приходят из Avm_Program_Catalog: группы уже разложены по
    // состояниям, поэтому фильтрация в QML не нужна.
    property var catalog: typeof ProgramCatalogVM !== "undefined" ? ProgramCatalogVM : null
    // Шаблоны пока демонстрационные: у продукта ещё нет их источника.
    property var templates: PlansViewModel
    property int segment: 0

    readonly property bool hasCatalog: page.catalog !== undefined && page.catalog !== null
    readonly property bool hasPrograms: page.hasCatalog && page.catalog.hasPrograms
    readonly property bool failed: page.hasCatalog && page.catalog.errorKey !== ""
    readonly property int programCount: page.hasPrograms ? page.catalog.active_list.count + page.catalog.draft_list.count + page.catalog.finished_list.count : 0
    readonly property var templateList: page.templates !== undefined && page.templates !== null ? page.templates.plans : []
    // Три группы каталога одним списком: порядок «активная → черновики →
    // завершённые» задаётся здесь, а не тремя повторами разметки.
    readonly property var programGroups: [
        {
            label: qsTr("Активная"),
            model: page.hasPrograms ? page.catalog.active_list : null
        },
        {
            label: qsTr("Черновики и пауза"),
            model: page.hasPrograms ? page.catalog.draft_list : null
        },
        {
            label: qsTr("Завершённые"),
            model: page.hasPrograms ? page.catalog.finished_list : null
        }
    ]

    caption: page.segment === 0 ? (page.hasPrograms ? qsTr("%1 в каталоге").arg(page.programCount) : qsTr("Свои программы и шаблоны")) : qsTr("Готовые планы — демонстрационные")
    title: qsTr("Программы")

    signal createRequested
    signal editRequested(string programId)
    signal openRequested(string programId)
    signal statsRequested(string programId)

    Component.onCompleted: page.reload()
    onVisibleChanged: {
        if (visible)
            page.reload();
    }

    function currentUserId() {
        return typeof AppContext !== "undefined" && AppContext !== null ? AppContext.currentUserId : "u1";
    }

    function reload() {
        if (!page.hasCatalog)
            return;

        page.catalog.reload(page.currentUserId());
    }

    SegmentedControl {
        objectName: "programsSegments"
        currentIndex: page.segment
        items: [qsTr("Мои"), qsTr("Шаблоны")]
        width: parent.width

        onSelected: function (index) {
            page.segment = index;
        }
    }

    // — «Мои»: активная, черновики и пауза, завершённые —
    Column {
        id: myPrograms

        spacing: Spacing.sectionGap
        visible: page.segment === 0
        width: parent.width

        // Пустой каталог: одна карточка с приглашением, а не пустой экран.
        AppCard {
            spacing: Spacing.itemGap
            visible: page.hasPrograms === false
            width: parent.width

            Label {
                color: Theme.textPrimary
                font: Typography.sectionTitle
                text: page.failed ? qsTr("Не удалось загрузить") : qsTr("Программ пока нет")
                width: parent.width
                wrapMode: Text.WordWrap
            }
            Label {
                color: Theme.textSecondary
                font: Typography.body
                text: page.failed ? qsTr("Повторите попытку позже.") : qsTr("Программа задаёт дни, упражнения и правила. Начните с готового шаблона или соберите свою.")
                width: parent.width
                wrapMode: Text.WordWrap
            }
            AppButton {
                text: qsTr("Создать программу")
                variant: AppButton.Primary
                width: parent.width

                onClicked: page.createRequested()
            }
        }

        Repeater {
            model: page.programGroups

            delegate: Column {
                required property var modelData

                spacing: Spacing.listGap
                visible: modelData.model !== null && modelData.model.count > 0
                width: parent.width

                Label {
                    color: Theme.textSecondary
                    font: Typography.captionStrong
                    text: modelData.label
                    width: parent.width
                    wrapMode: Text.WordWrap
                }
                Repeater {
                    model: modelData.model

                    delegate: CatalogProgramRow {
                        canEdit: model.canEdit
                        goal: model.goal
                        programId: model.id
                        programState: model.state
                        title: model.title
                        weeksTotal: model.weeksTotal
                        width: parent.width

                        onEditRequested: programId => page.editRequested(programId)
                        onOpenRequested: programId => page.openRequested(programId)
                        onStatsRequested: programId => page.statsRequested(programId)
                    }
                }
            }
        }

        AppButton {
            objectName: "programsCreateButton"
            text: qsTr("Создать программу")
            variant: AppButton.Secondary
            visible: page.hasPrograms
            width: parent.width

            onClicked: page.createRequested()
        }

        Label {
            color: Theme.textMuted
            font: Typography.caption
            text: qsTr("Пауза и завершение — на экране программы. Завершённую программу можно только смотреть: её состав не меняется.")
            width: parent.width
            wrapMode: Text.WordWrap
        }
    }

    // — «Шаблоны»: готовые планы, пока демонстрационные —
    Column {
        id: templatesColumn

        spacing: Spacing.listGap
        visible: page.segment === 1
        width: parent.width

        Label {
            color: Theme.textSecondary
            font: Typography.caption
            text: qsTr("Готовый план можно взять как есть или скопировать и изменить под себя. В приложении шаблоны пока не подключены — карточки демонстрационные.")
            width: parent.width
            wrapMode: Text.WordWrap
        }
        Repeater {
            model: page.templateList

            delegate: AppCard {
                spacing: Spacing.itemGap
                width: parent.width

                RowLayout {
                    spacing: Spacing.itemGap
                    width: parent.width

                    Label {
                        Layout.fillWidth: true
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                        font: Typography.bodyStrong
                        text: modelData.title
                    }
                    RatingBadge {
                        Layout.alignment: Qt.AlignTop
                        text: "★ " + modelData.ratingAvg.toFixed(1).replace(".", ",")
                    }
                }
                Label {
                    color: Theme.textSecondary
                    font: Typography.caption
                    text: modelData.subtitle
                    width: parent.width
                    wrapMode: Text.WordWrap
                }
                Flow {
                    spacing: Spacing.itemGap
                    width: parent.width

                    Repeater {
                        model: [modelData.goal, modelData.level, modelData.reviewText]

                        delegate: TagPill {
                            text: modelData
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent

                    onClicked: Demo.notify(qsTr("Копирование шаблона — следующий этап"))
                }
            }
        }
    }
}
