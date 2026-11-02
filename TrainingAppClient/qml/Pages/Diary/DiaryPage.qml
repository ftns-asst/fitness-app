import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// История и дозаполнение: одна дата, без повторения отметок экрана «Сегодня».
PageScaffold {
    id: page

    // Заголовок живёт в шапке каркаса — рядом с кнопкой профиля, как на
    // остальных корневых экранах. Подпись — программа, к которой относятся
    // отметки; без программы подпись честно говорит, что это личные записи.
    caption: page.hasProgram ? page.viewModel.programTitle : qsTr("Личные записи · без программы")
    title: qsTr("Дневник")

    property var viewModel: typeof DiaryVM !== "undefined" ? DiaryVM : null
    property string programId: ""
    // Дневник — корень вкладки: панель возврата нужна только когда экран
    // открыт поверх (диагностика `--screen diary`).
    property bool showBackBar: true
    readonly property bool hasModel: viewModel !== null && viewModel !== undefined
    // Программы может не быть: тогда дневник показывает личные записи, а блок
    // правил заменяется подсказкой с переходом к созданию программы. Свойство
    // читается «не false», чтобы подставные модели без него оставались
    // эквивалентны рабочей программе.
    readonly property bool hasProgram: page.hasModel && page.viewModel.hasProgram !== false
    readonly property alias valuePopup: numberPopup
    readonly property int numberPopupMaxWidth: 420
    property string activeDate: ""
    property int weekStart: 0
    onWeekStartChanged: {
        if (!daysStrip.moving)
            daysStrip.positionViewAtIndex(weekStart, ListView.Beginning);
    }
    property int revision: 0
    property bool summariesExpanded: false
    property var noteDrafts: ({})
    property bool jumpInvalid: false
    property bool dateJumpExpanded: false
    readonly property int dayCount: hasModel ? viewModel.days.count : 0
    readonly property int visibleDayCount: Math.max(0, Math.min(7, dayCount - weekStart))
    readonly property var selectedDay: {
        page.revision;
        return page.dayAt(page.indexForDate(page.activeDate));
    }
    readonly property var verdicts: ({
            done: {
                label: qsTr("✓ В норме"),
                color: Theme.positive
            },
            failed: {
                label: qsTr("✕ Мимо нормы"),
                color: Theme.negative
            },
            unknown: {
                label: qsTr("— Нет значения"),
                color: Theme.textMuted
            }
        })
    readonly property var diaryErrors: ({
            "diary.future_day": qsTr("Будущий день пока недоступен для ввода"),
            "diary.select_day": qsTr("Выберите день для ввода"),
            "diary.invalid_value": qsTr("Введите неотрицательное число"),
            "diary.invalid_weight": qsTr("Введите вес больше нуля"),
            "diary.rule_not_applicable": qsTr("Это правило не применяется к выбранному дню"),
            "diary.forbidden": qsTr("Эта программа недоступна"),
            "diary.invalid_date": qsTr("Выберите день программы"),
            "diary.invalid_term": qsTr("Проверьте даты программы"),
            "diary.user_required": qsTr("Не удалось определить пользователя")
        })
    signal backRequested

    function errorText(key) {
        if (!key)
            return "";
        if (key.indexOf("diary.") === 0)
            return page.diaryErrors[key] || qsTr("Не удалось обновить дневник. Повторите попытку.");
        return ApiErrorText.textFor(key);
    }
    function currentUserId() {
        return typeof AppContext !== "undefined" && AppContext !== null ? AppContext.currentUserId : "u1";
    }
    function dayAt(index) {
        return page.hasModel && index >= 0 && index < page.dayCount ? page.viewModel.days.get(index) : null;
    }
    function indexForDate(date) {
        if (!page.hasModel)
            return -1;
        for (let i = 0; i < page.dayCount; ++i)
            if (page.viewModel.days.get(i).date === date)
                return i;
        return -1;
    }
    function reload() {
        if (!page.hasModel)
            return;
        page.viewModel.reload(page.currentUserId(), page.programId);
        page.refresh();
    }
    // C++ emits changed both while reloading a save and after restoring selection.
    // Defer signal refresh until that synchronous transaction has finished.
    function refresh() {
        ++page.revision;
        let index = page.indexForDate(page.activeDate);
        if (index < 0) {
            index = -1;
            for (let i = 0; i < page.dayCount; ++i) {
                const day = page.dayAt(i);
                if (day.editable && (index < 0 || day.date > page.dayAt(index).date))
                    index = i;
            }
            if (index < 0 && page.dayCount > 0)
                index = 0;
            if (index >= 0)
                page.selectDate(page.dayAt(index).date);
            else
                page.activeDate = "";
        } else if (page.dayAt(index).editable && page.viewModel.selectedDate !== page.activeDate) {
            page.viewModel.openDay(page.activeDate);
        }
        page.weekStart = Math.min(page.weekStart, Math.max(0, Math.floor((page.dayCount - 1) / 7) * 7));
        if (!page.selectedDay || !page.selectedDay.editable)
            numberPopup.close();
        dayCard.syncNote();
    }
    function selectDate(date, reveal = true) {
        const index = page.indexForDate(date);
        const day = page.dayAt(index);
        if (!day) {
            page.jumpInvalid = true;
            return false;
        }
        // Future previews use day rows without opening the model editor.
        if (day.editable && !page.openDay(date, true))
            return false;
        numberPopup.close();
        page.activeDate = date;
        if (reveal)
            page.weekStart = Math.floor(index / 7) * 7;
        page.jumpInvalid = false;
        dayCard.syncNote();
        return true;
    }
    function displayValue(value) {
        return value === null || value === undefined || value === "" ? "—" : String(value);
    }
    function verdictFor(verdict) {
        return page.verdicts[verdict] || page.verdicts.unknown;
    }
    function unitLabel(unit) {
        const labels = {
            g: qsTr("г"),
            gPerKg: qsTr("г/кг"),
            kg: qsTr("кг"),
            kcal: qsTr("ккал"),
            ml: qsTr("мл"),
            hours: qsTr("ч"),
            rpe: "RPE"
        };
        return labels[unit] !== undefined ? labels[unit] : unit;
    }
    function dateTitle(date) {
        const value = Date.fromLocaleString(Qt.locale("en_US"), date, "yyyy-MM-dd");
        return isNaN(value.getTime()) ? date : value.toLocaleDateString(Qt.locale("ru_RU"), "dd MMM · ddd");
    }
    function openDay(date, editable) {
        const day = page.dayAt(page.indexForDate(date));
        if (!page.hasModel || !editable || !day || !day.editable)
            return false;
        if (page.viewModel.selectedDate === date)
            return true;
        return page.viewModel.openDay(date);
    }
    function editNumber(date, editable, cell, isWeight) {
        if (!isWeight && (!cell || cell.editable === false))
            return;
        if (!page.openDay(date, editable))
            return;
        numberPopup.editDate = date;
        numberPopup.ruleId = isWeight ? "" : cell.ruleId;
        numberPopup.isWeight = isWeight;
        numberPopup.titleText = isWeight ? qsTr("Вес, кг") : cell.title + " · " + page.unitLabel(cell.unit);
        numberPopup.saveFailed = false;
        numberPopup.hintText = isWeight ? qsTr("Вес тела в килограммах. Остальные замеры дня сохранятся.") : qsTr("Норма: %1%2").arg(cell.target).arg(cell.unit ? " " + page.unitLabel(cell.unit) : "");
        const current = isWeight ? page.viewModel.selectedWeight : cell.value;
        numberInput.text = page.displayValue(current) === "—" ? "" : String(current);
        numberPopup.open();
        numberInput.forceActiveFocus();
        numberInput.selectAll();
    }
    Component.onCompleted: page.reload()
    onVisibleChanged: {
        if (visible)
            page.reload();
    }
    // Смена программы приходит из обзора программы: дневник уже создан как
    // вкладка, и onVisibleChanged не сработает, если он и так открыт.
    onProgramIdChanged: {
        if (page.visible)
            page.reload();
    }
    onDayCountChanged: Qt.callLater(page.refresh)
    Connections {
        target: page.viewModel
        ignoreUnknownSignals: true
        function onChanged() {
            Qt.callLater(page.refresh);
        }
    }
    Connections {
        target: page.hasModel ? page.viewModel.days : null
        ignoreUnknownSignals: true
        function onModelReset() {
            Qt.callLater(page.refresh);
        }
        function onDataChanged() {
            Qt.callLater(page.refresh);
        }
    }
    Connections {
        target: Application
        function onStateChanged() {
            if (Application.state === Qt.ApplicationActive && page.visible)
                page.reload();
        }
    }

    Column {
        id: historyContent
        width: parent.width
        spacing: Spacing.itemGap
        BackBar {
            width: parent.width
            text: qsTr("Назад")
            visible: page.showBackBar
            onClicked: page.backRequested()
        }
        DateField {
            id: datePicker
            width: parent.width
            objectName: "diaryDateJump"
            visible: page.dayCount > 0 && page.dateJumpExpanded
            pickerOnly: true
            label: qsTr("Перейти к дате")
            value: page.activeDate
            onEdited: function (isoDate) {
                if (page.selectDate(isoDate)) {
                    closePicker();
                    page.dateJumpExpanded = false;
                }
            }
            onExpandedChanged: {
                if (!expanded)
                    page.dateJumpExpanded = false;
            }
        }
        Label {
            width: parent.width
            visible: page.jumpInvalid
            text: qsTr("Выберите дату в пределах программы.")
            font: Typography.caption
            color: Theme.negative
            wrapMode: Text.WordWrap
        }
        RowLayout {
            width: parent.width
            visible: page.dayCount > 0
            spacing: Spacing.itemGap
            AppButton {
                objectName: "diaryPreviousWeek"
                Layout.preferredWidth: Theme.touchMin
                implicitWidth: Theme.touchMin
                text: "‹"
                Accessible.name: qsTr("Предыдущая неделя")
                variant: AppButton.Secondary
                enabled: page.weekStart > 0
                onClicked: page.weekStart = Math.max(0, page.weekStart - 7)
            }
            AppButton {
                id: periodButton
                objectName: "diaryDateJumpToggle"
                readonly property string periodStart: page.dayAt(page.weekStart) ? page.dayAt(page.weekStart).date : ""
                readonly property string monthName: periodStart.length === 10 ? Qt.locale("ru_RU").standaloneMonthName(Number(periodStart.slice(5, 7)) - 1) : ""
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                labelFont: Typography.captionStrong
                text: monthName.length > 0 ? monthName.charAt(0).toUpperCase() + monthName.slice(1) + " " + periodStart.slice(0, 4) : qsTr("Выбрать дату")
                variant: AppButton.Secondary
                Accessible.name: qsTr("Открыть календарь")
                contentItem: Item {
                    implicitWidth: periodLabel.implicitWidth
                    implicitHeight: periodLabel.implicitHeight
                    Row {
                        id: periodLabel
                        anchors.centerIn: parent
                        spacing: Spacing.itemGap
                        AppIcon {
                            name: "today"
                            iconColor: Theme.textSecondary
                            width: 18
                            height: 18
                        }
                        Label {
                            text: periodButton.text
                            font: periodButton.labelFont
                            color: Theme.textPrimary
                        }
                    }
                }
                onClicked: {
                    page.dateJumpExpanded = true;
                    datePicker.openPicker();
                }
            }
            AppButton {
                objectName: "diaryNextWeek"
                Layout.preferredWidth: Theme.touchMin
                implicitWidth: Theme.touchMin
                text: "›"
                Accessible.name: qsTr("Следующая неделя")
                variant: AppButton.Secondary
                enabled: page.weekStart + 7 < page.dayCount
                onClicked: page.weekStart += 7
            }
        }
        ListView {
            id: daysStrip
            objectName: "diaryDaysStrip"
            width: parent.width
            height: 46
            clip: true
            orientation: ListView.Horizontal
            boundsBehavior: Flickable.StopAtBounds
            cacheBuffer: 0
            spacing: 2
            model: page.dayCount
            onContentXChanged: {
                if (moving)
                    page.weekStart = Math.max(0, Math.floor(contentX / ((width + spacing) / 7) / 7) * 7);
            }
            delegate: DayTile {
                required property int index
                readonly property var day: {
                    page.revision;
                    return page.dayAt(index);
                }
                width: (historyContent.width - 12) / 7
                objectName: "diaryDate_" + (day ? day.date : "")
                weekdayName: day ? day.date.substring(8, 10) : ""
                tileKind: "rest"
                isActive: day !== null && day.date === page.activeDate
                Accessible.name: day ? page.dateTitle(day.date) : ""
                Accessible.role: Accessible.Button
                Accessible.onPressAction: clicked()
                activeFocusOnTab: true
                Keys.onSpacePressed: clicked()
                Keys.onReturnPressed: clicked()
                enabled: day !== null
                onClicked: page.selectDate(day.date, false)
            }
        }
        Label {
            width: parent.width
            objectName: "diaryError"
            visible: page.hasModel && page.viewModel.errorKey.length > 0
            text: page.hasModel ? page.errorText(page.viewModel.errorKey) : ""
            font: Typography.body
            color: Theme.negative
            wrapMode: Text.WordWrap
        }
        Label {
            width: parent.width
            visible: !page.selectedDay
            text: page.dayCount === 0 ? qsTr("Пока нет дней программы. Создайте или выберите программу.") : qsTr("Программа ещё не началась. Записи станут доступны в день старта.")
            font: Typography.body
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
        }
        AppCard {
            id: dayCard
            width: parent.width
            visible: page.selectedDay !== null
            objectName: "diaryDay_" + page.activeDate
            elevated: false
            readonly property var day: page.selectedDay
            readonly property string dayDate: page.activeDate
            readonly property bool canEdit: day !== null && day.editable
            property bool noteSaveFailed: false
            property string noteDate: ""
            property string savedNote: ""
            property bool editingNote: false
            function syncNote() {
                if (!day)
                    return;
                const changedDate = noteDate !== dayDate;
                if (changedDate && noteDate.length > 0 && noteInput.text !== savedNote)
                    page.noteDrafts[noteDate] = noteInput.text;
                if (changedDate)
                    noteInput.text = page.noteDrafts[dayDate] !== undefined ? page.noteDrafts[dayDate] : day.note;
                else if (noteInput.text === savedNote)
                    noteInput.text = day.note;
                savedNote = day.note;
                noteDate = dayDate;
                if (changedDate) {
                    noteSaveFailed = false;
                    editingNote = false;
                }
            }
            Label {
                width: parent.width
                text: dayCard.day ? page.dateTitle(dayCard.dayDate) + " · " + dayCard.day.dayTitle : ""
                font: Typography.sectionTitle
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }
            Label {
                width: parent.width
                objectName: "diaryReadOnlyPreview"
                visible: dayCard.day !== null && !dayCard.canEdit
                text: qsTr("Будущий день · только просмотр")
                font: Typography.caption
                color: Theme.textMuted
                wrapMode: Text.WordWrap
            }
            RowLayout {
                width: parent.width
                objectName: "diaryWeightSummary"
                spacing: Spacing.listGap

                MetricCard {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    objectName: "diaryWeightMetric"
                    label: qsTr("Вес, кг")
                    deltaText: qsTr("За день")
                    valueText: dayCard.day ? page.displayValue(dayCard.day.weight) : "—"
                }
                MetricCard {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    label: qsTr("Среднее, кг")
                    deltaText: qsTr("За 7 дней")
                    valueText: dayCard.day ? page.displayValue(dayCard.day.average7) : "—"
                }
            }
            Label {
                width: parent.width
                text: dayCard.day && dayCard.day.workoutDone ? qsTr("✓ Тренировка сделана") : qsTr("— Тренировка не отмечена")
                font: Typography.caption
                color: dayCard.day && dayCard.day.workoutDone ? Theme.positive : Theme.textMuted
                wrapMode: Text.WordWrap
            }
            Label {
                width: parent.width
                text: qsTr("Ежедневные правила программы")
                font: Typography.captionStrong
                color: Theme.textSecondary
                visible: dayValues.count > 0
                wrapMode: Text.WordWrap
            }
            Repeater {
                id: dayValues
                model: dayCard.day ? dayCard.day.values : null
                delegate: AppButton {
                    id: valueButton
                    readonly property var cell: typeof modelData !== "undefined" ? modelData : model
                    width: parent.width
                    implicitHeight: Math.max(Theme.buttonSecondary, contentItem.implicitHeight + topPadding + bottomPadding)
                    objectName: "diaryValue_" + dayCard.dayDate + "_" + cell.ruleId
                    enabled: dayCard.canEdit && cell.editable !== false
                    text: cell.title + ": " + page.displayValue(cell.value) + (cell.unit ? " " + page.unitLabel(cell.unit) : "") + "\n" + qsTr("Норма: %1%2").arg(cell.target).arg(cell.unit ? " " + page.unitLabel(cell.unit) : "") + " · " + page.verdictFor(cell.verdict).label
                    variant: AppButton.Ghost
                    contentItem: Column {
                        spacing: 4
                        readonly property color color: page.verdictFor(valueButton.cell.verdict).color
                        RowLayout {
                            width: parent.width
                            spacing: Spacing.itemGap
                            Label {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                text: valueButton.cell.title
                                font: Typography.body
                                color: Theme.textPrimary
                                wrapMode: Text.WordWrap
                            }
                            Label {
                                Layout.maximumWidth: parent.width * 0.5
                                text: page.displayValue(valueButton.cell.value) + (valueButton.cell.unit ? " " + page.unitLabel(valueButton.cell.unit) : "")
                                font: Typography.bodyStrong
                                color: page.verdictFor(valueButton.cell.verdict).color
                                horizontalAlignment: Text.AlignRight
                                wrapMode: Text.WrapAnywhere
                            }
                        }
                        Label {
                            width: parent.width
                            text: qsTr("Норма: %1%2").arg(valueButton.cell.target).arg(valueButton.cell.unit ? " " + page.unitLabel(valueButton.cell.unit) : "") + " · " + page.verdictFor(valueButton.cell.verdict).label
                            font: Typography.caption
                            color: page.verdictFor(valueButton.cell.verdict).color
                            wrapMode: Text.WordWrap
                        }
                        Rectangle {
                            width: parent.width
                            height: Theme.borderWidth
                            color: Theme.borderSubtle
                        }
                    }
                    onClicked: page.editNumber(dayCard.dayDate, valueButton.enabled, cell, false)
                }
            }
            Label {
                width: parent.width
                objectName: "diaryRulesEmpty"
                visible: dayValues.count === 0
                text: page.hasProgram ? qsTr("В программе пока нет ежедневных числовых правил. Добавьте нужные показатели в разделе «Правила и отметки» программы.") : qsTr("Программы пока нет: вес и заметка сохраняются как личные записи. Отмечать правила можно после выбора программы — во вкладке «Программы».")
                font: Typography.caption
                color: Theme.textMuted
                wrapMode: Text.WordWrap
            }
            AppButton {
                width: parent.width
                objectName: "diaryWeight_" + dayCard.dayDate
                text: qsTr("Изменить вес")
                variant: AppButton.Secondary
                enabled: dayCard.canEdit
                onClicked: page.editNumber(dayCard.dayDate, dayCard.canEdit, null, true)
            }
            Label {
                width: parent.width
                visible: !dayCard.editingNote
                text: dayCard.day && dayCard.day.note.length > 0 ? dayCard.day.note : qsTr("Заметка дня пока не добавлена")
                font: Typography.body
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
            AppButton {
                width: parent.width
                objectName: "diaryNoteEdit_" + dayCard.dayDate
                visible: !dayCard.editingNote && dayCard.canEdit
                text: noteInput.text !== dayCard.savedNote ? qsTr("Продолжить заметку") : dayCard.day && dayCard.day.note.length > 0 ? qsTr("Изменить заметку") : qsTr("Добавить заметку")
                variant: AppButton.Secondary
                onClicked: {
                    dayCard.editingNote = true;
                    noteInput.focusInput();
                }
            }
            AppTextArea {
                id: noteInput
                width: parent.width
                visible: dayCard.editingNote
                objectName: "diaryNote_" + dayCard.dayDate
                enabled: dayCard.canEdit
                placeholderText: qsTr("Заметка дня (необязательно)")
            }
            AppButton {
                width: parent.width
                objectName: "diaryNoteSave_" + dayCard.dayDate
                visible: dayCard.editingNote
                text: qsTr("Сохранить заметку")
                variant: AppButton.Primary
                enabled: dayCard.canEdit
                onClicked: {
                    if (!page.openDay(dayCard.dayDate, dayCard.canEdit))
                        return;
                    dayCard.noteSaveFailed = !page.viewModel.saveNote(noteInput.text);
                    if (!dayCard.noteSaveFailed) {
                        dayCard.savedNote = noteInput.text;
                        delete page.noteDrafts[dayCard.dayDate];
                        dayCard.editingNote = false;
                    }
                }
            }
            AppButton {
                width: parent.width
                objectName: "diaryNoteCollapse_" + dayCard.dayDate
                visible: dayCard.editingNote
                text: qsTr("Свернуть заметку")
                variant: AppButton.Secondary
                onClicked: dayCard.editingNote = false
            }
            Label {
                width: parent.width
                objectName: "diaryNoteError_" + dayCard.dayDate
                visible: dayCard.noteSaveFailed
                text: qsTr("Не удалось сохранить заметку. Повторите попытку.")
                font: Typography.caption
                color: Theme.negative
                wrapMode: Text.WordWrap
            }
        }
        AppButton {
            width: parent.width
            objectName: "diarySummaryToggle"
            visible: page.dayCount > 0
            text: page.summariesExpanded ? qsTr("Скрыть средние за период") : qsTr("Средние за период")
            variant: AppButton.Secondary
            onClicked: page.summariesExpanded = !page.summariesExpanded
        }
        Label {
            width: parent.width
            visible: page.dayCount > 0
            text: qsTr("Отметки относятся к этой программе. Вес и заметка — общие для выбранной даты.")
            font: Typography.caption
            color: Theme.textMuted
            wrapMode: Text.WordWrap
        }
        Loader {
            width: parent.width
            active: page.summariesExpanded
            sourceComponent: Component {
                AppCard {
                    width: historyContent.width
                    elevated: false
                    Label {
                        width: parent.width
                        text: qsTr("По заполненным дням программы; пропуски не учитываются.")
                        font: Typography.caption
                        color: Theme.textMuted
                        wrapMode: Text.WordWrap
                    }
                    Repeater {
                        model: page.hasModel ? page.viewModel.summaries : null
                        delegate: Label {
                            width: parent.width
                            objectName: "diarySummary_" + model.ruleId
                            text: qsTr("%1 · среднее %2\nВ норме: %3 дн. · Мимо: %4 дн.").arg(model.title).arg(page.displayValue(model.average)).arg(model.daysInNorm).arg(model.daysOff)
                            font: Typography.body
                            color: Theme.textSecondary
                            wrapMode: Text.WordWrap
                        }
                    }
                }
            }
        }
    }

    SwipePopup {
        id: numberPopup
        dragHandle: numberGrip

        property bool isWeight: false
        property string ruleId: ""
        property string editDate: ""
        property string titleText: ""
        property string hintText: ""
        property bool saveFailed: false
        readonly property real numberValue: Number(numberInput.text.replace(",", "."))
        readonly property bool validNumber: numberInput.text.trim().length > 0 && isFinite(numberValue) && (isWeight ? numberValue > 0 : numberValue >= 0) && /^[0-9]+([.,][0-9]+)?$/.test(numberInput.text.trim())
        parent: Overlay.overlay
        onAboutToShow: numberGrip.reset()
        width: Math.min(page.width - Spacing.screenPadding * 2, page.numberPopupMaxWidth)
        padding: Spacing.cardPadding
        modal: true
        dim: true
        background: Rectangle {
            color: Theme.surface
            border.color: Theme.border
            border.width: Theme.borderWidth
            radius: Theme.radiusLg
        }
        contentItem: ColumnLayout {
            spacing: Spacing.itemGap
            SwipeDismissHandle {
                id: numberGrip
                Layout.fillWidth: true
                objectName: "diaryNumberGrip"
                onDismissed: numberPopup.close()
            }

            Label {
                Layout.fillWidth: true
                text: numberPopup.titleText
                font: Typography.sectionTitle
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                text: page.dateTitle(numberPopup.editDate)
                font: Typography.caption
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                text: numberPopup.hintText
                font: Typography.caption
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
            TextField {
                id: numberInput

                Layout.fillWidth: true
                Layout.minimumWidth: 0
                objectName: "diaryNumberInput"
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                font: Typography.body
                color: Theme.textPrimary
                selectByMouse: true
                padding: Spacing.cardPadding
                placeholderText: numberPopup.isWeight ? qsTr("Например, 75,5") : qsTr("Введите значение")
                background: Rectangle {
                    color: Theme.surfaceMuted
                    radius: Theme.radiusMd
                    border.width: Theme.borderWidth
                    border.color: numberInput.activeFocus ? Theme.accent : Theme.border
                }
                onAccepted: {
                    if (numberPopup.validNumber)
                        numberSave.clicked();
                }
            }
            Label {
                Layout.fillWidth: true
                objectName: "diarySaveError"
                visible: numberPopup.saveFailed
                text: qsTr("Не удалось сохранить. Проверьте значение и повторите.")
                color: Theme.negative
                font: Typography.caption
                wrapMode: Text.WordWrap
            }
            AppButton {
                id: numberSave

                Layout.fillWidth: true
                objectName: "diaryNumberSave"
                text: qsTr("Сохранить")
                enabled: numberPopup.validNumber && page.selectedDay !== null && page.selectedDay.editable && page.activeDate === numberPopup.editDate
                variant: AppButton.Primary
                onClicked: {
                    if (!numberPopup.validNumber || page.activeDate !== numberPopup.editDate || !page.openDay(numberPopup.editDate, true))
                        return;

                    const saved = numberPopup.isWeight ? page.viewModel.saveWeight(numberPopup.numberValue) : page.viewModel.saveRuleValue(numberPopup.ruleId, numberPopup.numberValue);

                    numberPopup.saveFailed = !saved;
                    if (saved)
                        numberPopup.close();
                }
            }
            AppButton {
                Layout.fillWidth: true
                text: qsTr("Отмена")
                variant: AppButton.Secondary
                onClicked: numberPopup.close()
            }
        }
    }
}
