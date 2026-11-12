import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Создание программы (экран 42, UC-1/UC-2). Настраивается срок, цель и
// КАЖДЫЙ день недели отдельно — это ядро продукта (§3.4).
//
// Экран работает через Avm_Program_Edit: правила создания (рабочий день
// требует упражнения, измеримая цель — цели) проверяет ViewModel, а экран
// лишь показывает errorKey и не даёт отправить заведомо плохое.
PageScaffold {
    id: page

    property var viewModel: ProgramEditVM
    // Программа для правки. Пусто — создание новой; каталог и обзор передают
    // идентификатор существующей (срез S2).
    property string programId: ""
    property int sectionIndex: -1
    readonly property var recommendationPopupItem: advicePopup
    property int summaryRevision: 0
    readonly property int programGoal: page.viewModel !== undefined && page.viewModel !== null && page.viewModel.goal !== undefined ? page.viewModel.goal : 2
    readonly property bool requiresGoal: programGoal === 0 || programGoal === 1 || programGoal === 3
    readonly property int ruleCount: page.hasProgram ? page.viewModel.rules.count : 0
    readonly property int errorSection: page.sectionForError(page.errorKey)
    readonly property var directionOptions: [
        {
            id: 0,
            title: qsTr("Сила")
        },
        {
            id: 1,
            title: qsTr("Набор массы")
        },
        {
            id: 2,
            title: qsTr("Общая форма")
        },
        {
            id: 3,
            title: qsTr("Снижение веса")
        },
        {
            id: 4,
            title: qsTr("Выносливость")
        }
    ]

    spacing: Spacing.itemGap
    showHeader: false

    function showSection(index) {
        sectionIndex = Math.max(-1, Math.min(3, index));
        if (sectionIndex >= 0)
            sectionRevealTimer.restart();
    }
    function toggleSection(index) {
        showSection(sectionIndex === index ? -1 : index);
    }
    function collapseSection() {
        showSection(-1);
        boundsTimer.restart();
    }
    function sectionForError(key) {
        if (key === "goal_required")
            return 2;
        if (key === "empty_training_day")
            return 1;
        if (["invalid_start_date", "invalid_term", "invalid_draft", "title"].indexOf(key) >= 0)
            return 0;
        if (["rule_title_required", "rule_advice_text_required", "rule_target_required", "text"].indexOf(key) >= 0)
            return 3;
        return -1;
    }
    function goalSummaryText(revision) {
        if (page.goalCount === 0)
            return page.requiresGoal ? qsTr("Добавьте измеримый результат") : qsTr("Можно добавить позже");
        const goal = page.viewModel.goal_targets.get(0);
        return page.goalCardText(goal.metric, goal.targetValue);
    }
    function ruleSummaryText(revision) {
        if (page.ruleCount === 0)
            return qsTr("Показатели для дневника и рекомендации");
        return page.viewModel.rules.get(0).displayText + (page.ruleCount > 1 ? qsTr(" · ещё %1").arg(page.ruleCount - 1) : "");
    }

    // get() не создаёт привязку к данным строки; сводки обновляет сигнал VM.
    Connections {
        target: page.viewModel !== undefined ? page.viewModel : null
        ignoreUnknownSignals: true
        function onChanged() {
            ++page.summaryRevision;
        }
    }
    Timer {
        id: boundsTimer
        interval: Theme.pageTransitionDuration
        onTriggered: page.pageFlickable.returnToBounds()
    }
    Timer {
        id: sectionRevealTimer
        interval: Theme.pageTransitionDuration
        onTriggered: {
            const cards = [basicsCard, weekCard, goalsCard, rulesCard];
            const card = cards[page.sectionIndex];
            if (card === undefined)
                return;
            const flick = page.pageFlickable;
            const top = card.mapToItem(flick.contentItem, 0, 0).y - Spacing.itemGap;
            const maximum = Math.max(flick.originY, flick.originY + flick.contentHeight - flick.height);
            revealScroll.to = Math.max(flick.originY, Math.min(maximum, top));
            revealScroll.restart();
        }
    }
    NumberAnimation {
        id: revealScroll
        target: page.pageFlickable
        property: "contentY"
        duration: Theme.pageTransitionDuration
        easing.type: Easing.OutCubic
    }
    Connections {
        target: page.pageFlickable
        function onDraggingChanged() {
            if (page.pageFlickable.dragging) {
                revealScroll.stop();
                sectionRevealTimer.stop();
            }
        }
    }
    // Панель возврата обязательна: экраны программы открыты поверх вкладок, где
    // таб-бара нет.
    BackBar {
        Layout.fillWidth: true
        onClicked: page.backRequested()
        text: qsTr("Назад")
        visible: true
    }
    Label {
        width: parent.width
        text: page.title
        font: Typography.screenTitle
        color: Theme.textPrimary
        wrapMode: Text.WordWrap
    }

    caption: ""
    // Признак режима — идентификатор, с которым открыт экран. Заголовок и кнопка
    // зависят от него: на экране правки «Создать программу» читалось как «будет
    // создана вторая», хотя программа уже есть.
    readonly property bool editing: page.programId.length > 0
    // Заголовок правки — название программы (§5.3): по нему видно, какую именно
    // программу открыли. Пустое название (программа не загрузилась) заменяется
    // подписью режима, иначе шапка осталась бы без заголовка.
    readonly property string editingTitle: page.viewModel !== undefined && page.viewModel !== null ? page.viewModel.title : ""
    title: page.editing === false ? qsTr("Новая программа") : (page.editingTitle.length > 0 ? page.editingTitle : qsTr("Редактирование программы"))

    readonly property bool canCreate: page.viewModel !== undefined && page.viewModel !== null && page.viewModel.canCreate
    // Черновик, а не созданная программа: reset() уже наполняет семь дней недели,
    readonly property bool hasProgram: page.viewModel !== undefined && page.viewModel !== null && page.viewModel.hasDraft
    // Сохранённый черновик (продолженный resumeDraft) — у него есть
    // идентификатор в БД, и его можно удалить. После чистого reset() у
    // черновика идентификатора нет: удалять нечего, кнопка не нужна.
    readonly property bool hasSavedDraft: page.editing === false && page.viewModel !== undefined && page.viewModel !== null && page.viewModel.programId.length > 0
    readonly property int trainingDays: page.viewModel !== undefined && page.viewModel !== null ? page.viewModel.trainingDays : 0

    // Черновик рекомендации живёт на странице, а не в ViewModel: он не должен
    // попасть в программу, пока пользователь не нажал «Добавить». Хранить его
    // в модели значило бы держать в БД наполовину заполненную форму.
    property string adviceTitle: ""
    property string adviceText: ""
    // Подсветка достаётся конкретному полю: одна строка ошибки на оба поля
    // заставила бы подсветить не то, которое пустое.
    readonly property string adviceErrorField: page.viewModel !== undefined && page.viewModel !== null ? page.viewModel.errorKey : ""
    readonly property string adviceErrorText: page.adviceErrorField.length > 0 ? page.errorText() : ""

    function addAdvice() {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        // Поля очищаются только после успеха: иначе при отказе пользователь
        // потерял бы написанный текст, а отказ возможен — например, пустой
        // текст совета. Сами поля остаются владельцами своего текста (иначе
        // цикл привязок), поэтому очищаются напрямую.
        if (page.viewModel.addAdvice(page.adviceTitle, page.adviceText) === false)
            return;

        page.adviceTitle = "";
        page.adviceText = "";
        page.clearAdvice();
        advicePopup.close();
    }

    function clearAdvice() {
        adviceTitleField.text = "";
        adviceTextField.text = "";
    }

    Component.onCompleted: page.reset()

    // Идентификатор берётся из AppContext: литерал «u1» повторялся в четырёх
    // экранах, и любое другое значение делало программу невидимой.
    function currentUserId() {
        return typeof AppContext !== "undefined" && AppContext !== null ? AppContext.currentUserId : "u1";
    }

    function initialiseUnsetFields() {
        if (page.editing || !page.hasProgram)
            return;
        if (page.viewModel.weeks < 1)
            page.viewModel.setWeeks(8);
        if (page.viewModel.startDate.length === 0)
            page.viewModel.setStartDate(Qt.formatDate(new Date(), Qt.ISODate));
    }

    function reset() {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        // Автосохранение — контракт настоящей ViewModel (черновик пишется в БД
        // на каждом действии, отзыв: «добавил упражнение, нажал назад — ничего
        // не сохранилось»). Заглушки тестов его не объявляют, и включение по
        // гаранту их не задевает.
        if (page.viewModel.autoSaveDraft !== undefined)
            page.viewModel.autoSaveDraft = true;

        // С идентификатором открывается существующая программа. Без него —
        // редактор создания, который продолжит прерванный черновик из БД:
        // повторное открытие «Новой программы» больше не стирает набранные
        // дни и упражнения.
        if (page.programId.length > 0)
            page.viewModel.load(page.currentUserId(), page.programId);
        else if (page.viewModel.resumeDraft !== undefined)
            page.viewModel.resumeDraft(page.currentUserId());
        else
            page.viewModel.reset(page.currentUserId());
        page.initialiseUnsetFields();
    }

    Label {
        width: parent.width
        text: qsTr("План тренировок")
        font: Typography.caption
        color: Theme.textSecondary
        topPadding: Spacing.itemGap
    }
    DisclosureCard {
        id: basicsCard
        objectName: "programEditBasics"
        width: parent.width
        title: qsTr("Название и срок")
        summary: (page.editingTitle.length > 0 ? page.editingTitle : qsTr("Без названия")) + " · " + page.weeks + " " + qsTr("нед.")
        iconName: "settings"
        expanded: page.sectionIndex === 0
        errorText: page.errorSection === 0 ? page.errorText() : ""
        onToggleRequested: page.toggleSection(0)
        onDoneRequested: page.collapseSection()
        body: ColumnLayout {
            // Внутри AppCard ширину задаёт родитель, а не Layout: содержимое
            // карточки лежит в обычной Column, где Layout-свойства не работают.
            width: parent.width
            spacing: Spacing.itemGap

            AppTextField {
                Layout.fillWidth: true
                label: qsTr("Название программы")
                onUserEdited: page.viewModel.setTitle(text)
                text: page.hasProgram ? page.viewModel.title : ""
            }
            DateField {
                Layout.fillWidth: true
                label: qsTr("Дата старта")
                onEdited: function (isoDate) {
                    page.viewModel.setStartDate(isoDate);
                }
                value: page.hasProgram ? page.viewModel.startDate : ""
            }
            Label {
                Layout.fillWidth: true
                text: qsTr("Направление")
                font: Typography.caption
                color: Theme.textSecondary
            }
            AppComboBox {
                id: programDirection
                objectName: "programDirectionSelector"
                Layout.fillWidth: true
                model: page.directionOptions
                onActivated: {
                    if (page.viewModel !== undefined && page.viewModel !== null)
                        page.viewModel.setGoal(Number(currentValue));
                }
            }
            Binding {
                target: programDirection
                property: "currentIndex"
                value: page.programGoal
                restoreMode: Binding.RestoreBindingOrValue
            }
            // Срок задаётся ползунком (решение из `docs/mockups-programs/README.md`,
            // «Срок программы — ползунок, а не кнопки 4/8/12/16»): кнопки оставляли
            // 5, 6, 7, 9, 10, 11 недель недостижимыми, и пользователь выбирал
            // соседнее значение и правил его вручную. Дальше ярлыки — только
            // ускорение ввода, диапазон задаёт шкала 1…52 недели.
            //
            // Текст по-прежнему не вводится: пустое значение давало NaN в модели.
            WeightRuler {
                id: termRuler

                objectName: "termRuler"
                Layout.fillWidth: true
                from: 1
                hint: page.termHint()
                step: 1
                title: qsTr("Срок программы")
                to: 52
                unit: qsTr("нед.")
                valueDecimals: 0

                onEdited: function (value) {
                    page.viewModel.setWeeks(value);
                }
            }
            // Шкала ← модель. Binding, а не `value: page.weeks`: ползунок пишет
            // value сам, и прямая привязка умирала бы на первом же жесте — после
            // этого ярлык 8 недель не сдвинул бы шкалу с 12.
            Binding {
                target: termRuler
                property: "value"
                value: page.weeks
                restoreMode: Binding.RestoreBindingOrValue
            }
            // Частые длительности — быстрые ярлыки: они ускоряют ввод, но не
            // ограничивают диапазон.
            RowLayout {
                Layout.fillWidth: true
                spacing: Spacing.itemGap

                Repeater {
                    model: page.weekOptions

                    delegate: AppButton {
                        required property int modelData

                        Layout.fillWidth: true
                        labelFont: Typography.captionStrong
                        height: Theme.buttonSecondary
                        objectName: "termPreset" + modelData
                        onClicked: page.viewModel.setWeeks(modelData)
                        text: qsTr("%1").arg(modelData)
                        variant: page.weeks === modelData ? AppButton.Primary : AppButton.Secondary
                    }
                }
            }
        }
    }
    DisclosureCard {
        id: weekCard
        objectName: "programEditWeek"
        width: parent.width
        title: page.viewModel && page.viewModel.rotation ? qsTr("Цикл и упражнения") : qsTr("Неделя и упражнения")
        summary: page.trainingDays > 0 ? qsTr("Тренировочных дней: %1 · отдыха: %2").arg(page.trainingDays).arg((page.viewModel ? page.viewModel.program_days.count : 7) - page.trainingDays) : qsTr("Все дни — отдых")
        iconName: "today"
        expanded: page.sectionIndex === 1
        errorText: page.errorSection === 1 ? page.errorText() : ""
        onToggleRequested: page.toggleSection(1)
        onDoneRequested: page.collapseSection()
        preview: ProgramWeekStrip {
            width: parent.width
            showCount: true
            cycleSchedule: page.viewModel && page.viewModel.rotation === true
            daysModel: page.hasProgram ? page.viewModel.program_days : null
            onDayRequested: (weekday, kind) => page.dayRequested(weekday, kind)
        }
        body: Column {
            width: parent.width
            spacing: Spacing.itemGap
            ProgramWeekStrip {
                width: parent.width
                showCount: true
                cycleSchedule: page.viewModel && page.viewModel.rotation === true
                daysModel: page.hasProgram ? page.viewModel.program_days : null
                onDayRequested: (weekday, kind) => page.dayRequested(weekday, kind)
            }
            WeekLegend {
                width: parent.width
            }
        }
    }
    RowLayout {
        width: parent.width
        Label {
            Layout.fillWidth: true
            text: qsTr("Результат и привычки")
            font: Typography.caption
            color: Theme.textSecondary
        }
        Label {
            text: page.requiresGoal ? (page.goalCount > 0 ? qsTr("Цель задана") : qsTr("Нужна цель")) : qsTr("По желанию")
            font: Typography.caption
            color: page.requiresGoal && page.goalCount === 0 ? Theme.accent : Theme.textMuted
        }
    }
    DisclosureCard {
        id: goalsCard
        objectName: "programEditGoals"
        width: parent.width
        title: qsTr("Цели")
        summary: page.goalSummaryText(page.summaryRevision)
        badgeText: String(page.goalCount)
        badgeObjectName: "goalSectionCount"
        iconName: "target"
        expanded: page.sectionIndex === 2
        errorText: page.errorSection === 2 ? page.errorText() : ""
        onToggleRequested: page.toggleSection(2)
        onDoneRequested: page.collapseSection()
        body: Column {
            width: parent.width
            spacing: Spacing.itemGap
            Label {
                objectName: "goalEmptyHint"
                color: Theme.textMuted
                font: Typography.caption
                text: qsTr("Цели пока не заданы")
                visible: page.goalCount === 0
                width: parent.width
                wrapMode: Text.WordWrap
            }
            ColumnLayout {
                id: goalList

                width: parent.width
                spacing: Spacing.itemGap

                Repeater {
                    model: page.hasProgram ? page.viewModel.goal_targets : []

                    delegate: AppCard {
                        id: goalRow

                        // Роли строки цели (`programGoalTargets`, `02-program-model.md`
                        // §7). `displayText` у цели нет: подпись собирает экран, потому
                        // что имя метрики переводится вместе с приложением, — а делегат
                        // с обязательной ролью, которой модель не отдаёт, не рисовал
                        // ничего, и добавленная цель пропадала из виду.
                        required property string direction
                        required property int index
                        required property string metric
                        required property real targetValue

                        Layout.fillWidth: true
                        borderColor: Theme.borderSubtle

                        RowLayout {
                            width: parent.width
                            spacing: Spacing.listGap

                            // Значок направления — из макета (`.dir`): «↓» на акцентной
                            // подложке, «↑» и «=» на нейтральной. Слова «снизить» и
                            // «повысить» в строке списка не помещались рядом с целью.
                            Rectangle {
                                Layout.preferredHeight: 24
                                Layout.preferredWidth: 24
                                color: page.goalDirectionHighlighted(goalRow.direction) ? Theme.accentTint : Theme.surfaceMuted
                                radius: 8

                                Label {
                                    anchors.centerIn: parent
                                    color: page.goalDirectionHighlighted(goalRow.direction) ? Theme.accent : Theme.textSecondary
                                    font: Typography.captionStrong
                                    text: page.goalDirectionArrow(goalRow.direction)
                                }
                            }
                            Label {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                objectName: "goalCardText"
                                color: Theme.textPrimary
                                font: Typography.bodyStrong
                                text: page.goalCardText(goalRow.metric, goalRow.targetValue)
                                wrapMode: Text.WordWrap
                            }
                            AppButton {
                                // Цель обязана удаляться: ошибочно выбранную метрику
                                // иначе не исправить, и активация была бы заблокирована.
                                Layout.minimumWidth: 0
                                onClicked: page.viewModel.removeTarget(goalRow.index)
                                padding: Spacing.itemGap
                                text: qsTr("Убрать")
                                variant: AppButton.Secondary
                            }
                        }
                    }
                }
            }
            ColumnLayout {
                width: parent.width
                spacing: Spacing.itemGap

                Label {
                    Layout.fillWidth: true
                    color: Theme.textMuted
                    font: Typography.caption
                    text: qsTr("Добавить цель")
                }
                AppComboBox {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    // Метрика доезжает до модели только обработчиком: без него в форме
                    // всегда оставалась первая метрика списка («Вес тела»), и цель
                    // создавалась не та, что показана на экране.
                    model: page.goalMetricOptions
                    // Список метрик — пары «название/код» (`value`), а не роли `id`,
                    // которые AppComboBox берёт по умолчанию.
                    valueRole: "value"

                    onActivated: page.setGoalMetric(Number(currentValue))
                }
                // Направление цели — сегментированный выбор, как в макете
                // (`03-edit.html`, `.seg`): «Снизить» и «Повысить» взаимоисключающие,
                // и активное направление обязано читаться без нажатия. Две
                // равноправные вторичные кнопки не показывали, что выбрано, а цель
                // создавали сразу по нажатию — до того, как выбрано значение.
                SegmentedControl {
                    Layout.fillWidth: true
                    currentIndex: page.goalDirections.indexOf(page.goalDirection)
                    items: page.goalDirectionItems
                    // Тест-хук: id компонента снаружи не виден, и элемент находится
                    // по objectName — так же, как страницы в тестах.
                    objectName: "goalDirectionSelector"

                    onSelected: function (index) {
                        page.goalDirection = page.goalDirections[index];
                    }
                }
                // Целевое значение — подпись и число в одной строке над шкалой
                // (`.slider-head` в макете), а не отдельной подписью слева от ползунка:
                // в строке на двоих шкала получала половину карточки и читалась сжатой
                // полоской у правого края.
                //
                // Набирать значение кнопками по 0,5 кг невозможно: от 80 до 70 кг это
                // двадцать нажатий «−».
                WeightRuler {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    // Границы, шаг, единица и разрядность — из метрики
                    // (`goalMetricOptions`): у веса тела 50–130 кг и шаг 0,5, как в
                    // макете 03-edit.html; у тоннажа недели — килограммы тысячами.
                    // Прежняя нейтральная шкала 50–300 оставляла недостижимой половину
                    // метрик, а её шаг 0,5 кг отстоял на 0,6 px, и метки сливались в
                    // серую полосу. Узкий осмысленный диапазон и есть то, что в макете
                    // называется «ощущением масштаба»: ползунок показывает, где цель
                    // стоит относительно текущего значения метрики.
                    from: page.goalOption.from
                    // Тест-хук: элемент находится по objectName (см. выше).
                    objectName: "goalRuler"
                    step: page.goalOption.step
                    // Подпись поля: в макете она стоит слева от числа, над шкалой.
                    title: qsTr("Целевое значение")
                    to: page.goalOption.to
                    unit: page.goalOption.unit
                    value: page.goalTargetValue
                    valueDecimals: page.goalOption.decimals

                    onEdited: function (newValue) {
                        page.goalTargetValue = newValue;
                    }
                }
                // Добавление — отдельное действие (в макете `button.btn.primary`
                // «Добавить»): направление и значение сначала выбирают, потом
                // добавляют, и случайное касание цели не создаёт.
                AppButton {
                    Layout.fillWidth: true
                    text: qsTr("Добавить")
                    variant: AppButton.Primary

                    onClicked: page.addGoal(page.goalDirection)
                }
            }
        }
    }
    DisclosureCard {
        id: rulesCard
        objectName: "programEditRules"
        width: parent.width
        title: qsTr("Правила")
        summary: page.ruleSummaryText(page.summaryRevision)
        badgeText: page.ruleCount > 0 ? String(page.ruleCount) : ""
        iconName: "plans"
        expanded: page.sectionIndex === 3
        errorText: page.errorSection === 3 ? page.errorText() : ""
        onToggleRequested: page.toggleSection(3)
        onDoneRequested: page.collapseSection()
        body: Column {
            width: parent.width
            spacing: Spacing.itemGap
            ColumnLayout {
                id: ruleList

                width: parent.width
                spacing: Spacing.itemGap

                Repeater {
                    model: page.hasProgram ? page.viewModel.rules : []

                    delegate: AppCard {
                        id: ruleRow

                        required property string displayText
                        required property int index

                        Layout.fillWidth: true
                        borderColor: Theme.borderSubtle

                        RowLayout {
                            width: parent.width
                            spacing: Spacing.itemGap

                            Label {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                color: Theme.textPrimary
                                font: Typography.body
                                text: ruleRow.displayText
                                wrapMode: Text.WordWrap
                            }
                            AppButton {
                                // Правило обязано удаляться: опечатку в названии иначе не
                                // исправить, и программа осталась бы с мусором.
                                Layout.minimumWidth: 0
                                onClicked: page.viewModel.removeRule(ruleRow.index)
                                padding: Spacing.itemGap
                                text: qsTr("Убрать")
                                variant: AppButton.Secondary
                            }
                        }
                    }
                }
            }
            ColumnLayout {
                width: parent.width
                spacing: Spacing.itemGap

                Label {
                    Layout.fillWidth: true
                    color: Theme.textMuted
                    font: Typography.caption
                    text: qsTr("Каталог правил")
                }
                Label {
                    Layout.fillWidth: true
                    color: Theme.textSecondary
                    font: Typography.body
                    text: qsTr("Белок, сон, шаги и другие типовые проверки — одним нажатием.")
                    wrapMode: Text.WordWrap
                }
                // Нормы из расчёта КБЖУ: кнопка всегда открывает экран расчёта,
                // где виден сохранённый результат прошлого расчёта и есть кнопка
                // «Создать правила в программе». Так расчёт не потеряется между
                // экранами: пользователь попадает на него одним нажатием.
                AppButton {
                    Layout.fillWidth: true
                    onClicked: page.macroRequested()
                    text: qsTr("Из расчёта питания")
                    variant: AppButton.Primary
                }
                // Вход в отдельный экран, а не ряд пресетов здесь: с фильтром по
                // категориям и формой своего правила каталог не помещается в блок
                // рядом с неделей и всегда показывал бы часть пресетов.
                AppButton {
                    Layout.fillWidth: true
                    onClicked: page.rulesPickerRequested()
                    text: qsTr("Выбрать правила")
                    variant: AppButton.Secondary
                }
            }
            AppButton {
                width: parent.width
                objectName: "programAdviceButton"
                text: qsTr("Своя рекомендация")
                variant: AppButton.Secondary
                onClicked: advicePopup.open()
            }
        }
    }
    AppButton {
        width: parent.width
        visible: page.hasSavedDraft
        text: qsTr("Удалить черновик")
        variant: AppButton.Ghost
        onClicked: page.deleteDraft()
    }
    SwipePopup {
        id: advicePopup
        parent: Overlay.overlay
        dragHandle: adviceGrip
        width: parent !== null ? Math.min(480, parent.width - Spacing.screenPadding * 2) : 360
        height: Math.min(implicitHeight, parent !== null ? parent.height - Spacing.screenPadding * 2 : implicitHeight)
        modal: true
        dim: true
        padding: Spacing.listGap
        closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
        contentItem: ColumnLayout {
            width: parent.width
            spacing: Spacing.itemGap

            SwipeDismissHandle {
                id: adviceGrip
                Layout.fillWidth: true
                onDismissed: advicePopup.close()
            }
            RowLayout {
                Layout.fillWidth: true
                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: qsTr("Своя рекомендация")
                    font: Typography.sectionTitle
                    color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                }
                AppButton {
                    text: qsTr("Закрыть")
                    variant: AppButton.Secondary
                    onClicked: advicePopup.close()
                }
            }

            // Рекомендация отвечает на вопрос «что делать», а не «сколько»:
            // цели у неё нет, вместо неё текст. Поэтому здесь два текстовых
            // поля, а не строка чисел с единицами (§3.6).
            // Текст остаётся в самих полях, а не в свойствах страницы: привязка
            // text к свойству плюс запись обратно в это же свойство давали цикл
            // привязок — поле перезаписывало само себя. Очистка после успеха
            // делается через clearAdvice().
            AppTextField {
                id: adviceTitleField
                objectName: "programAdviceTitle"

                Layout.fillWidth: true
                errorText: (page.adviceErrorField === "title" || page.adviceErrorField === "rule_title_required") ? page.adviceErrorText : ""
                label: qsTr("Название")
                onUserEdited: page.adviceTitle = text
                placeholderText: qsTr("Например: не пить кофе после 16:00")
            }
            AppTextArea {
                id: adviceTextField
                objectName: "programAdviceText"

                Layout.fillWidth: true
                errorText: (page.adviceErrorField === "text" || page.adviceErrorField === "rule_advice_text_required") ? page.adviceErrorText : ""
                helperText: qsTr("Совет, который нечем измерить")
                label: qsTr("Текст совета")
                onEdited: page.adviceText = text
                placeholderText: qsTr("Опишите, что делать вместо числовой цели")
            }
            AppButton {
                Layout.fillWidth: true
                onClicked: page.addAdvice()
                text: qsTr("Добавить рекомендацию")
                variant: AppButton.Secondary
            }
        }
    }
    function setupHint() {
        if (page.editingTitle.trim().length === 0)
            return qsTr("Добавьте название программы");
        if (page.trainingDays === 0)
            return qsTr("Настройте хотя бы один тренировочный день");
        if (page.requiresGoal && page.goalCount === 0)
            return qsTr("Задайте измеримую цель программы");
        return page.canCreate ? (page.editing ? qsTr("Изменения готовы к сохранению") : qsTr("Программа готова к созданию")) : qsTr("Добавьте упражнения в тренировочные дни");
    }
    footer: ColumnLayout {
        spacing: Spacing.itemGap
        Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            objectName: "programSetupHint"
            text: page.errorKey.length > 0 && page.errorSection < 0 ? page.errorText() : page.setupHint()
            color: page.errorKey.length > 0 && page.errorSection < 0 ? Theme.negative : Theme.textSecondary
            font: Typography.caption
            wrapMode: Text.WordWrap
        }
        AppButton {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            objectName: "programEditPrimaryButton"
            text: page.viewModel && page.viewModel.isDraft ? qsTr("Начать программу") : (page.editing ? qsTr("Сохранить изменения") : qsTr("Создать программу"))
            variant: AppButton.Primary
            onClicked: page.create()
        }
    }

    CompactStepper {
        width: parent.width
        label: qsTr("Допуск снижения повторов")
        value: page.viewModel && page.viewModel.repTolerance !== undefined ? page.viewModel.repTolerance : 2
        from: 0
        to: 20
        onEdited: value => page.viewModel.setRepTolerance(value)
    }
    readonly property var weekOptions: [4, 8, 12, 16]
    readonly property int weeks: viewModel !== undefined && viewModel !== null ? viewModel.weeks : 0
    // Дата окончания — производная (`старт + срок − 1 день`, §3.3), считается в
    // домене: своя формула в QML разошлась бы с ним при первом изменении правила.
    readonly property string plannedEndDate: viewModel !== undefined && viewModel !== null ? viewModel.plannedEndDate : ""

    // Подсказка под шкалой — та самая производная дата. Без неё срок остаётся
    // абстракцией, которую пересчитывают в уме. Пока не хватает даты старта,
    // подсказка называет причину, а не молчит: пустая строка под шкалой читается
    // как «здесь ничего и не появится».
    function termHint() {
        if (page.viewModel === undefined || page.viewModel === null)
            return "";

        if (page.viewModel.startDate === "")
            return page.weeks < 1 ? qsTr("Шкала 1–52 недели") : qsTr("Укажите дату старта — окончание посчитается");

        // Проверка на строку, а не на пустоту: у заглушек модели (QML-тесты,
        // превью) свойства может не быть вовсе, и «Окончание undefined» было бы
        // хуже, чем честная подсказка про шкалу.
        const end = page.plannedEndDate;

        if (typeof end !== "string" || end === "")
            return qsTr("Шкала 1–52 недели");

        return qsTr("Окончание %1").arg(page.formatDate(end));
    }

    // ISO-дата → «дд.мм.гггг». Разбором строки, а не `new Date(iso)`: JS читает
    // дату без времени как UTC и в отрицательной зоне показал бы день назад.
    function formatDate(iso) {
        const parts = String(iso).split("-");

        if (parts.length !== 3)
            return iso;

        return parts[2] + "." + parts[1] + "." + parts[0];
    }

    // Метрики целей. Значения — EGoal_Metric (§7: в QML числа, не enum).
    //
    // Диапазон, шаг и единица ползунка цели задаются метрикой, а не одной парой на
    // весь список: шкала 50–130 кг из макета `03-edit.html` верна для веса тела, а
    // цель «12 000 кг тоннажа» на ней недостижима — как и на прежней нейтральной
    // шкале 50–300. Границы повторяют уже принятые в приложении (рабочий вес
    // 20–200 × 2,5 — как в записи подхода, обхваты 50–150 × 0,5 — как в
    // «Замерах»), а не выдуманы заново.
    readonly property var goalMetricOptions: [
        {
            decimals: 1,
            from: 50,
            name: qsTr("Вес тела"),
            step: 0.5,
            title: qsTr("Вес тела, кг"),
            to: 130,
            unit: qsTr("кг"),
            value: 0
        },
        {
            decimals: 1,
            from: 20,
            name: qsTr("Рабочий вес"),
            step: 2.5,
            title: qsTr("Рабочий вес, кг"),
            to: 200,
            unit: qsTr("кг"),
            value: 1
        },
        {
            decimals: 1,
            from: 3,
            name: qsTr("Процент жира"),
            step: 0.5,
            title: qsTr("Процент жира"),
            to: 60,
            unit: qsTr("%"),
            value: 2
        },
        {
            decimals: 1,
            from: 50,
            name: qsTr("Обхваты"),
            step: 0.5,
            title: qsTr("Обхваты, см"),
            to: 150,
            unit: qsTr("см"),
            value: 3
        },
        {
            decimals: 0,
            from: 1,
            name: qsTr("Тренировок в неделю"),
            step: 1,
            title: qsTr("Тренировок в неделю"),
            to: 14,
            unit: qsTr("раз/нед"),
            value: 4
        },
        {
            decimals: 0,
            from: 1000,
            name: qsTr("Тоннаж в неделю"),
            step: 100,
            title: qsTr("Тоннаж в неделю, кг"),
            to: 50000,
            unit: qsTr("кг"),
            value: 5
        }
    ]
    property double goalTargetValue: 75
    property int selectedGoalMetric: 0
    // Строка метрики выбранной цели: из неё берутся границы, шаг, единица и
    // разрядность ползунка. Поиск по значению метрики, а не по индексу списка:
    // индекс начнёт врать, как только список переставят или дополнят.
    readonly property var goalOption: {
        for (let i = 0; i < page.goalMetricOptions.length; ++i) {
            if (page.goalMetricOptions[i].value === page.selectedGoalMetric)
                return page.goalMetricOptions[i];
        }

        return page.goalMetricOptions[0];
    }
    // — Подпись цели в списке (`03-edit.html`, `.list-row`) —
    // Число целей берётся у модели свойством `count`: вызов `rowCount()` не
    // создаёт привязки, и счётчик в заголовке не обновлялся бы.
    readonly property int goalCount: page.hasProgram && page.viewModel !== undefined && page.viewModel !== null ? page.viewModel.goal_targets.count : 0
    // Роль `metric` приходит строкой перечисления `EGoal_Metric`
    // (`02-program-model.md` §3.2), а подписи лежат выше по числовому значению:
    // список имён повторяет порядок перечисления.
    readonly property var goalMetricNames: ["bodyWeightKg", "workingWeightKg", "bodyFatPercent", "measurementCm", "workoutsPerWeek", "tonnagePerWeekKg", "repsAtWeight"]

    function goalOptionFor(metricName) {
        const index = page.goalMetricNames.indexOf(metricName);

        for (let i = 0; i < page.goalMetricOptions.length; ++i) {
            if (page.goalMetricOptions[i].value === index)
                return page.goalMetricOptions[i];
        }

        return null;
    }

    // «Вес тела — 82 кг»: имя метрики вместе со значением цели. Разрядность
    // берётся у метрики, а нулевая дробь отбрасывается: «82,0 кг» в карточке
    // читается шумно, а в макете (`03-edit.html`) стоит «82 кг».
    function goalCardText(metricName, targetValue) {
        const option = page.goalOptionFor(metricName);
        const decimals = option === null ? 1 : option.decimals;
        const value = page.goalValueText(targetValue, decimals);

        if (option === null)
            return qsTr("%1 — %2").arg(metricName).arg(value);

        return qsTr("%1 — %2 %3").arg(option.name).arg(value).arg(option.unit);
    }

    function goalValueText(targetValue, decimals) {
        const text = Number(targetValue).toFixed(decimals);

        // Нулевая дробь отбрасывается: «82,0 кг» в карточке читается шумно, а в
        // макете (`03-edit.html`) стоит «82 кг». Проверка идёт через endsWith, а
        // не через indexOf: у целых значений (`toFixed(0)` → «4») indexOf(".0")
        // возвращает -1, а `text.length - 2` для строки из одного знака тоже -1 —
        // совпадение срезало единственный знак, и в карточке оставалось
        // «Тренировок в неделю —  раз/нед».
        if (text.endsWith(".0"))
            return text.substring(0, text.length - 2);

        return text.replace(".", ",");
    }

    function goalDirectionArrow(direction) {
        if (direction === "increase")
            return "↑";
        if (direction === "decrease")
            return "↓";

        return "=";
    }

    // Акцентная подложка у снижения — как в макете (`.dir.down`); повышение и
    // удержание нейтральны, иначе две цели в списке конкурировали бы за внимание.
    function goalDirectionHighlighted(direction) {
        return direction === "decrease";
    }
    // Направление цели (§7: EDirection — 0 «повысить», 1 «снизить», 2 «удержать»).
    // Порядок повторяет макет `03-edit.html`: «Снизить» слева и выбрано по
    // умолчанию. Направление — состояние формы, а не аргумент нажатия: цель
    // создаётся отдельной кнопкой «Добавить», и до неё видно, что выбрано.
    readonly property var goalDirections: [1, 0]
    readonly property var goalDirectionItems: [qsTr("↓ Снизить"), qsTr("↑ Повысить")]
    property int goalDirection: 1
    // Открытый день недели: карта не меняет состояние сама, она только
    // сообщает, какой день нажали.
    signal dayRequested(int weekday, int kind)

    // Смена метрики зажимает значение в её диапазон: 75 кг осмысленны для веса
    // тела и невозможны для процента жира, а ползунок не должен показывать
    // значение, до которого нельзя дотянуть перетаскиванием.
    function setGoalMetric(metric) {
        page.selectedGoalMetric = metric;

        const option = page.goalOption;

        page.goalTargetValue = Math.max(option.from, Math.min(option.to, page.goalTargetValue));
    }

    function addGoal(direction) {
        if (viewModel === undefined || viewModel === null)
            return;

        // Стартовое значение не спрашивается: его уточняют замеры позже, а пустое
        // поле в форме цели отпугивает сильнее, чем лишний вопрос.
        viewModel.setTarget(selectedGoalMetric, -1, false, goalTargetValue, direction);
    }

    // Запрос возврата: экран открыт поверх вкладок, где таб-бара нет.
    signal backRequested
    // Запрос открытия экрана выбора правил: сам переход делает оболочка, экран
    // знает только что его попросили (§5.7).
    signal rulesPickerRequested
    // Кнопка «Создать правила из расчёта» всегда открывает экран расчёта КБЖУ:
    // там показывается сохранённый результат прошлого расчёта и есть кнопка
    // «Создать правила в программе», применяющая нормы к этому же черновику.
    // Путь выбирает каркас — странице нужен только сигнал.
    signal macroRequested
    // Программа сохранена. Наружу уходят и идентификатор, и режим: оболочке
    // нужно решить, открыть обзор созданной программы или вернуться к той,
    // которую правили, — без этого нажатие не оставляло видимого следа. Третий
    // аргумент — «прежняя активная ушла в паузу» (§2.3): о смене активной
    // программы пользователь узнаёт из сообщения, а не догадывается по списку.
    signal created(string programId, bool wasEditing, bool previousPaused)

    // Удаление брошенного черновика: работа стирается из БД, форма очищается, и
    // экран продолжает жить как «Новая программа» — идти назад не обязательно.
    // Отказ ViewModel показывается баннером errorKey/errorDetail, тост — только
    // после фактического удаления.
    function deleteDraft() {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        if (page.viewModel.deleteDraft === undefined)
            return;

        if (page.viewModel.deleteDraft() === false)
            return;

        Demo.notify(qsTr("Черновик удалён"));
    }

    function create() {
        // Создание и запуск черновика включают программу. Правка существующей
        // программы сохраняет её состояние; активация доступна отдельно в обзоре.
        if (page.viewModel === undefined || page.viewModel === null)
            return;
        if (page.viewModel.create(!page.editing || page.viewModel.isDraft === true) === false) {
            if (page.errorSection >= 0) {
                page.showSection(page.errorSection);
                sectionRevealTimer.restart();
            }
            return;
        }

        // Идентификатор берётся у ViewModel: у новой программы он появляется
        // только сейчас, и без него оболочка не знала бы, что открывать.
        const program_id = page.viewModel !== undefined && page.viewModel !== null ? page.viewModel.programId : "";
        // §2.3: активная программа у пользователя одна, поэтому включение новой
        // ставит прежнюю на паузу — экран обязан сказать об этом словами.
        const previous_paused = page.viewModel !== undefined && page.viewModel !== null ? page.viewModel.previousProgramPaused : false;

        page.created(program_id, page.editing, previous_paused);
    }

    function errorText() {
        const texts = {
            empty_training_day: qsTr("В рабочем дне нужно хотя бы одно упражнение"),
            goal_required: qsTr("Добавьте цель программы"),
            invalid_start_date: qsTr("Проверьте дату старта"),
            invalid_draft: qsTr("Заполните название, дату старта и срок"),
            invalid_term: qsTr("Срок должен быть от 1 до 104 недель"),
            rule_advice_text_required: qsTr("Введите текст совета"),
            rule_title_required: qsTr("Введите название рекомендации"),
            rule_target_required: qsTr("Укажите цель правила"),
            save_failed: qsTr("Не удалось сохранить программу")
        };

        // Техническая причина (FOREIGN KEY, занятый id и т.п.) показывается
        // второй строкой: без неё отказ записи выглядел как «кнопка ничего не
        // делает».
        const detail = page.viewModel !== undefined && page.viewModel !== null ? page.viewModel.errorDetail : "";

        if (page.errorKey === "save_failed" && detail !== "")
            return texts["save_failed"] + ": " + detail;

        return texts[page.errorKey] !== undefined ? texts[page.errorKey] : qsTr("Не удалось создать программу");
    }

    // Что ещё нужно заполнить для создания программы. Условия те же, что у
    // canCreate в C++, но перечисляются здесь: кнопка сама говорит, чего не
    // хватает, вместо молчаливого отказа.
    function missingHint() {
        const missing = [];

        if (page.viewModel === undefined || page.viewModel === null)
            return "";

        if (page.viewModel.startDate === "")
            missing.push(qsTr("укажите дату старта"));
        if (page.viewModel.weeks < 1)
            missing.push(qsTr("укажите срок"));
        if (page.trainingDays === 0)
            missing.push(qsTr("добавьте хотя бы один тренировочный день"));

        if (missing.length === 0)
            return "";

        // «Создать» и «сохранить» — разные действия, и подсказка называет то,
        // которое ждёт пользователя под кнопкой.
        const prefix = page.editing ? qsTr("Чтобы сохранить программу") : qsTr("Чтобы создать программу");

        return qsTr("%1: %2.").arg(prefix).arg(missing.join(qsTr(", ")));
    }

    readonly property string errorKey: page.viewModel !== undefined && page.viewModel !== null ? page.viewModel.errorKey : ""
}
