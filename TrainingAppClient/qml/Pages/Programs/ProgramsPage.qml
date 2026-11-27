import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Экран «Программы» (вкладка 2). Раньше это были «Планы»: вход в свою программу,
// промо-опрос подбора и каталог шаблонов в одном списке, где половина первого
// экрана — заглушки. Теперь два сегмента с разными задачами: «Мои» показывает
// реальные программы из БД, «Шаблоны» создаёт редактируемый личный снимок PSMF.
//
// Разметка — обычные Column с явной шириной: колонка контента PageScaffold не
// layout, а у скрытого элемента ColumnLayout ширина обнуляется, и аудит вёрстки
// справедливо ругается на нулевую карточку.
PageScaffold {
    id: page

    // Свои программы приходят из Avm_Program_Catalog: группы уже разложены по
    // состояниям, поэтому фильтрация в QML не нужна.
    property var catalog: typeof ProgramCatalogVM !== "undefined" ? ProgramCatalogVM : null
    // ViewModel и владелец передаются снаружи; маршрут редактора обрабатывает templateCreated.
    property var templateVM: null
    property string templateUserId: typeof AppContext !== "undefined" && AppContext !== null ? AppContext.currentUserId : ""
    property int segment: 0

    readonly property bool hasCatalog: page.catalog !== undefined && page.catalog !== null
    readonly property bool hasPrograms: page.hasCatalog && page.catalog.hasPrograms
    readonly property bool failed: page.hasCatalog && page.catalog.errorKey !== ""
    readonly property int programCount: page.hasPrograms ? page.catalog.active_list.count + page.catalog.draft_list.count + page.catalog.finished_list.count : 0
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

    caption: page.segment === 0 ? (page.hasPrograms ? qsTr("%1 в каталоге").arg(page.programCount) : qsTr("Свои программы и шаблоны")) : qsTr("PSMF — личный редактируемый снимок")
    title: qsTr("Программы")

    signal createRequested
    signal templateCreated(string programId)
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

    // — «Шаблоны»: новый черновик без смены активной программы —
    Column {
        id: templatesColumn

        property var presetValues: ({
                caloriesMax: "1300",
                proteinMin: "180",
                carbsMax: "25",
                fatMax: "25"
            })
        property var initialValues: ({})

        function filledValues(values) {
            let result = {};
            for (let key in values) {
                let value = String(values[key]).trim();
                if (value.length > 0)
                    result[key] = value.replace(",", ".");
            }
            return result;
        }

        spacing: Spacing.listGap
        visible: page.segment === 1
        width: parent.width

        AppCard {
            spacing: Spacing.itemGap
            width: parent.width

            Label {
                color: Theme.textPrimary
                font: Typography.sectionTitle
                text: qsTr("PSMF 4 недели")
                width: parent.width
                wrapMode: Text.WordWrap
            }
            Label {
                color: Theme.textSecondary
                font: Typography.body
                text: qsTr("28 дней. Верх → отдых → Низ + пресс → отдых. Замеры каждые 3 дня, допуск повторов 2. Питание — редактируемый пользовательский пресет, не медицинская рекомендация. Настройте упражнения и начните программу в редакторе.")
                width: parent.width
                wrapMode: Text.WordWrap
            }
            DateField {
                id: psmfStartDate
                objectName: "psmfStartDate"
                label: qsTr("Дата старта")
                value: Qt.formatDate(new Date(), "yyyy-MM-dd")
                width: parent.width
            }
            Repeater {
                model: [
                    {
                        key: "caloriesMax",
                        label: qsTr("Калорийность ≤, ккал"),
                        value: "1300"
                    },
                    {
                        key: "proteinMin",
                        label: qsTr("Белок ≥, г"),
                        value: "180"
                    },
                    {
                        key: "carbsMax",
                        label: qsTr("Углеводы ≤, г"),
                        value: "25"
                    },
                    {
                        key: "fatMax",
                        label: qsTr("Жиры ≤, г"),
                        value: "25"
                    },
                    {
                        key: "waterMinMl",
                        label: qsTr("Вода ≥, мл (необязательно)"),
                        value: ""
                    },
                    {
                        key: "sleepMinHours",
                        label: qsTr("Сон ≥, часов (необязательно)"),
                        value: ""
                    },
                    {
                        key: "stepsMin",
                        label: qsTr("Шаги ≥ (необязательно)"),
                        value: ""
                    }
                ]

                delegate: AppTextField {
                    required property var modelData

                    objectName: "psmf_" + modelData.key
                    label: modelData.label
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    text: modelData.value
                    width: parent.width

                    onUserEdited: templatesColumn.presetValues[modelData.key] = text
                }
            }
            CheckBox {
                id: showInitialMeasurements
                font: Typography.body
                palette.text: Theme.textPrimary
                indicator: SelectionMark {
                    checked: showInitialMeasurements.checked
                    x: 0
                    anchors.verticalCenter: parent.verticalCenter
                }
                text: qsTr("Добавить стартовые замеры")
                width: parent.width
            }
            Column {
                spacing: Spacing.itemGap
                visible: showInitialMeasurements.checked
                width: parent.width

                Label {
                    color: Theme.textSecondary
                    font: Typography.caption
                    text: qsTr("Заполните известные значения. Пустые поля не изменят уже записанные замеры на дату старта.")
                    width: parent.width
                    wrapMode: Text.WordWrap
                }
                Repeater {
                    model: [
                        {
                            key: "weightKg",
                            label: qsTr("Вес, кг")
                        },
                        {
                            key: "chestCm",
                            label: qsTr("Грудь, см")
                        },
                        {
                            key: "waistCm",
                            label: qsTr("Талия, см")
                        },
                        {
                            key: "hipCm",
                            label: qsTr("Бёдра, см")
                        },
                        {
                            key: "armCm",
                            label: qsTr("Рука, см")
                        },
                        {
                            key: "thighCm",
                            label: qsTr("Бедро, см")
                        },
                        {
                            key: "bellyCm",
                            label: qsTr("Живот, см")
                        },
                        {
                            key: "shouldersCm",
                            label: qsTr("Плечи, см")
                        },
                        {
                            key: "calvesCm",
                            label: qsTr("Икры, см")
                        },
                        {
                            key: "neckCm",
                            label: qsTr("Шея, см")
                        }
                    ]

                    delegate: AppTextField {
                        required property var modelData

                        objectName: "psmf_" + modelData.key
                        visible: modelData.key === "weightKg" || !page.templateVM || page.templateVM.measurementKeys.indexOf(modelData.key.slice(0, -2)) >= 0
                        label: modelData.label
                        inputMethodHints: Qt.ImhFormattedNumbersOnly
                        width: parent.width

                        onUserEdited: templatesColumn.initialValues[modelData.key] = text
                    }
                }
            }
            Label {
                color: Theme.negative
                font: Typography.caption
                text: page.templateVM ? page.templateVM.errorDetail : ""
                visible: text.length > 0
                width: parent.width
                wrapMode: Text.WordWrap
            }
            AppButton {
                objectName: "psmfCreateDraftButton"
                enabled: page.templateVM !== null && page.templateUserId.length > 0 && psmfStartDate.value.trim().length > 0
                text: qsTr("Создать черновик и настроить")
                variant: AppButton.Primary
                width: parent.width

                onClicked: {
                    let preset = templatesColumn.filledValues(templatesColumn.presetValues);
                    let initial = showInitialMeasurements.checked ? templatesColumn.filledValues(templatesColumn.initialValues) : {};
                    if (page.templateVM.createDraft(page.templateUserId, psmfStartDate.value.trim(), preset, initial)) {
                        page.reload();
                        page.templateCreated(page.templateVM.programId);
                    }
                }
            }
        }
    }
}
