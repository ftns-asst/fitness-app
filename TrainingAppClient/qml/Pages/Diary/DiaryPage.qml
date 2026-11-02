import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// История и дозаполнение: одна дата, без повторения отметок экрана «Сегодня».
PageScaffold {
    id: page

    // Шапка по макету 13: подпись — выбранная дата («пн, 2 ноября»). Название
    // программы стоит строкой над карточками: дата меняется от тапа к тапу, а
    // программа — нет, и в подписи шапки она мигала бы вместе с днями.
    caption: page.selectedDayCaption()
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
    readonly property alias measurementsPopup: measurementPopup
    readonly property int numberPopupMaxWidth: 420
    property string activeDate: ""
    property int weekStart: 0
    // Прокрутка полосы недель программная: `positionViewAtIndex` вызывается до
    // того, как ListView посчитает contentWidth (модель-число меняется вместе с
    // загрузкой), и остаётся без эффекта — полоса показывала первую неделю, пока
    // выбранная дата была в последней. Смещение считается из ширины плитки и
    // применяется после раскладки.
    property bool stripAligned: false
    onWeekStartChanged: Qt.callLater(page.alignStrip)
    property int revision: 0
    property bool summariesExpanded: false
    property var noteDrafts: ({})
    property bool jumpInvalid: false
    property bool dateJumpExpanded: false
    readonly property int dayCount: hasModel ? viewModel.days.count : 0
    readonly property int visibleDayCount: Math.max(0, Math.min(7, dayCount - weekStart))
    // Ширина плитки недели — целое число. Дробная ширина (было `(width - 12) / 7`)
    // накапливала доли пикселя между делегатами, и последняя плитка вылезала за
    // contentWidth списка на 2–3 px — аудит вёрстки (`--diag`) считает это
    // дефектом OVERFLOW-X.
    readonly property int stripTileWidth: Math.max(40, Math.floor((page.width - Spacing.screenPadding * 2 - 12) / 7))
    onActiveDateChanged: page.refreshSelectedDay()
    // Выбранный день — обычное свойство, а не привязка с чтением счётчика
    // ревизии: биндинг с побочным чтением ловил «binding loop» (пересчёт совпадал
    // с записью revision в refresh), Qt обрывал его, и карточка дня замирала на
    // старых данных. Обновляет его refresh и смена активной даты.
    property var selectedDay: null
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
    function refreshSelectedDay() {
        // Сброс в null перед присваиванием: `days.get()` отдаёт тот же JS-объект
        // после изменения роли, и повторное присваивание того же объекта QML не
        // считает изменением — карточка дня оставалась на старом весе. Оба
        // присваивания синхронны, поэтому промежуточного пустого кадра нет.
        page.selectedDay = null;
        page.selectedDay = page.dayAt(page.indexForDate(page.activeDate));
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
        page.refreshSelectedDay();
        Qt.callLater(page.alignStrip);
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
    // Подпись шапки: «пн, 2 ноября» — как в макете 13. Точку после сокращённого
    // дня недели убираем: в русской локали Qt пишет «пн.».
    function selectedDayCaption() {
        const value = Date.fromLocaleString(Qt.locale("en_US"), page.activeDate, "yyyy-MM-dd");
        if (!page.activeDate || isNaN(value.getTime()))
            return "";
        return value.toLocaleDateString(Qt.locale("ru_RU"), "ddd, d MMMM").replace(".", "");
    }
    // Короткий день недели для плиток недели: «пн» + число под ним.
    function shortWeekday(date) {
        const value = Date.fromLocaleString(Qt.locale("en_US"), date, "yyyy-MM-dd");
        return isNaN(value.getTime()) ? "" : value.toLocaleDateString(Qt.locale("ru_RU"), "ddd").replace(".", "");
    }
    function dayOfMonth(date) {
        return date && date.length >= 10 ? date.substring(8, 10) : "";
    }
    // Чей это дневник: правила принадлежат программе, вес и заметка — человеку.
    // Без программы строка говорит прямо, что записи личные.
    function scopeText() {
        return page.hasProgram ? qsTr("Программа: %1").arg(page.viewModel.programTitle) : qsTr("Личные записи · без программы");
    }
    // «82,4»: запятая как десятичный разделитель в русской локали.
    function numberText(value) {
        const number = Number(value);
        if (value === null || value === undefined || value === "" || !isFinite(number))
            return "—";
        return String(Math.round(number * 10) / 10).replace(".", ",");
    }
    function weightText(day) {
        return page.hasWeight(day) ? page.numberText(day.weight) + " " + qsTr("кг") : "—";
    }
    // Записан ли вес за день: одно условие для текста и для подсказки, иначе они
    // могли разойтись («82 кг» рядом с «вес не записан»).
    function hasWeight(day) {
        return day !== null && day !== undefined && day.weight !== null && day.weight !== undefined && day.weight !== "";
    }
    function shortDate(isoDate) {
        const value = Date.fromLocaleString(Qt.locale("en_US"), isoDate, "yyyy-MM-dd");
        return isNaN(value.getTime()) ? isoDate : value.toLocaleDateString(Qt.locale("ru_RU"), "d MMMM");
    }
    // Дельта к весу неделю назад (роль delta7 считает C++). Показывается только
    // когда известны оба замера: «за 7 дней» без второго значения обещало бы
    // динамику, которой нет.
    function deltaText(day) {
        if (!day || day.delta7 === null || day.delta7 === undefined || day.delta7 === "")
            return "";
        const delta = Number(day.delta7);
        if (!isFinite(delta))
            return "";
        if (Math.abs(delta) < 0.05)
            return qsTr("без изменений за 7 дней");
        return (delta < 0 ? "↓ " : "↑ ") + page.numberText(Math.abs(delta)) + " " + qsTr("за 7 дней");
    }
    // Тон дельты взят из макета: снижение веса — спокойное «хорошо», набор —
    // предупреждение. Это подсказка, а не оценка цели: цель бывает и набор
    // массы, поэтому цвет не красный.
    function deltaColor(day) {
        if (!day || day.delta7 === null || day.delta7 === undefined || day.delta7 === "")
            return Theme.textMuted;
        const delta = Number(day.delta7);
        if (!isFinite(delta) || Math.abs(delta) < 0.05)
            return Theme.textMuted;
        return delta < 0 ? Theme.positive : Theme.warning;
    }
    function averageText(day) {
        return day && page.displayValue(day.average7) !== "—" ? qsTr("Среднее за 7 дней · %1 кг").arg(page.numberText(day.average7)) : "";
    }
    // Подпись обхватов: последний замер, в котором они есть. Дата относится
    // именно к обхватам — вес пишется чаще, и «измерены сегодня» для обхватов
    // было бы неправдой.
    function measurementCaption() {
        if (!page.hasModel)
            return "";
        const last = page.viewModel.lastMeasurement;
        if (last === undefined || last === null || last.date === undefined)
            return qsTr("Обхваты не заполнены — их можно добавить к любому дню");
        const parts = [];
        const keys = page.viewModel.measurementKeys;
        for (let i = 0; i < keys.length; ++i) {
            const key = keys[i];
            if (key === "weight" || last[key] === undefined || last[key] === null)
                continue;
            parts.push(MeasurementTexts.title(key).toLowerCase() + " " + page.numberText(last[key]));
        }
        if (parts.length === 0)
            return qsTr("Обхваты не заполнены — их можно добавить к любому дню");
        return qsTr("Обхваты: %1 см — измерены %2").arg(parts.join(" · ")).arg(page.shortDate(last.date));
    }
    function measurementActionText() {
        const current = page.hasModel ? page.viewModel.selectedMeasurement : null;
        return current !== null && current !== undefined && Object.keys(current).length > 0 ? qsTr("Изменить замеры") : qsTr("Добавить замеры");
    }
    // Строка истории: «Завершена · 55 мин · 12,4 т». Тоннаж — в тоннах, как в
    // макете: 12400 кг читаются хуже, чем 12,4 т.
    function sessionMeta(session) {
        if (!session || session.title === undefined)
            return "";
        const parts = [page.sessionStateText(session.workoutState)];
        if (Number(session.minutes) > 0)
            parts.push(qsTr("%1 мин").arg(session.minutes));
        if (Number(session.tonnage) > 0)
            parts.push(qsTr("%1 т").arg(page.numberText(Number(session.tonnage) / 1000)));
        return parts.join(" · ");
    }
    function sessionStateText(state) {
        const labels = {
            completed: qsTr("Завершена"),
            cancelled: qsTr("Пропущена"),
            draft: qsTr("Черновик")
        };
        return labels[state] !== undefined ? labels[state] : qsTr("Тренировка");
    }
    // «1 из 2» — сколько правил дня в норме. Счётчик идёт по тем же строкам,
    // что и список под ним.
    function ruleCountText(values) {
        if (!values || values === null)
            return "";
        // Список правил приходит либо массивом (C++ List_Model), либо ListModel
        // (подставная модель тестов): у массива длина — length, у ListModel —
        // count, и обращение к несуществующей даёт undefined в подписи.
        const total = values.count !== undefined ? values.count : values.length;
        if (!total)
            return "";
        let done = 0;
        for (let i = 0; i < total; ++i) {
            const row = typeof values.get === "function" ? values.get(i) : values[i];
            if (row !== undefined && row !== null && row.verdict === "done")
                ++done;
        }
        return qsTr("%1 из %2").arg(done).arg(total);
    }
    function verdictBadge(verdict) {
        const badges = {
            done: {
                label: qsTr("в норме"),
                color: Theme.positive
            },
            failed: {
                label: qsTr("мимо"),
                color: Theme.negative
            },
            unknown: {
                label: qsTr("нет значения"),
                color: Theme.textMuted
            }
        };
        return badges[verdict] !== undefined ? badges[verdict] : badges.unknown;
    }
    // Тренировка выбранной даты: пустая карта — занятия в этот день не было.
    function selectedSessionHasData() {
        const session = page.hasModel ? page.viewModel.selectedSession : null;
        return session !== null && session !== undefined && session.title !== undefined;
    }
    function sessionTitle() {
        const session = page.hasModel ? page.viewModel.selectedSession : null;
        return session !== null && session !== undefined && session.title !== undefined ? session.title : "";
    }
    function sessionMetaText() {
        const session = page.hasModel ? page.viewModel.selectedSession : null;
        return page.sessionMeta(session);
    }
    // «28 окт.» — дата строки истории: год не нужен, окно дневника — месяцы.
    function shortDay(isoDate) {
        const value = Date.fromLocaleString(Qt.locale("en_US"), isoDate, "yyyy-MM-dd");
        return isNaN(value.getTime()) ? isoDate : value.toLocaleDateString(Qt.locale("ru_RU"), "d MMM");
    }
    // Форма замеров: вес и обхваты одной карточкой (док 13, §2 — «где сохранять
    // замеры» имеет один ответ). Значения выбранной даты подставляются в поля,
    // чтобы «Изменить» показывало текущее, а не пустую форму.
    function openMeasurements() {
        if (!page.selectedDay || !page.selectedDay.editable)
            return;
        if (!page.openDay(page.activeDate, true))
            return;
        const current = page.hasModel ? page.viewModel.selectedMeasurement : null;
        const keys = page.hasModel ? page.viewModel.measurementKeys : [];
        const values = {};
        for (let i = 0; i < keys.length; ++i) {
            const value = current !== null && current !== undefined ? current[keys[i]] : undefined;
            if (value !== undefined && value !== null)
                values[keys[i]] = page.numberText(value);
        }
        measurementPopup.values = values;
        measurementPopup.saveFailed = false;
        measurementPopup.validationError = "";
        measurementPopup.open();
    }
    // Сбор формы замеров: пустые поля не отправляются («не менялось»), мусор и
    // отрицательные числа — ошибка, совсем пустая форма — тоже.
    function saveMeasurements() {
        const keys = page.hasModel ? page.viewModel.measurementKeys : [];
        const values = {};
        let invalid = false;
        for (let i = 0; i < keys.length; ++i) {
            const raw = measurementPopup.values[keys[i]];
            const text = raw === undefined || raw === null ? "" : String(raw).trim();
            if (text.length === 0)
                continue;
            if (!/^[0-9]+([.,][0-9]+)?$/.test(text)) {
                invalid = true;
                continue;
            }
            values[keys[i]] = Number(text.replace(",", "."));
        }
        if (invalid) {
            measurementPopup.validationError = qsTr("Введите неотрицательное число");
            return;
        }
        if (Object.keys(values).length === 0) {
            measurementPopup.validationError = qsTr("Заполните хотя бы одно поле");
            return;
        }
        measurementPopup.validationError = "";
        if (!page.viewModel.saveMeasurements(values)) {
            measurementPopup.saveFailed = true;
            return;
        }
        measurementPopup.close();
    }
    function alignStrip() {
        page.stripAligned = true;
        daysStrip.contentX = Math.max(0, page.weekStart * (page.stripTileWidth + daysStrip.spacing));
        Qt.callLater(function () {
            page.stripAligned = false;
        });
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
        // Подзаголовок и область — как `large-sub` в макете 13: экран сразу
        // говорит, что здесь живёт, и чьи это правила.
        Label {
            objectName: "diarySubtitle"
            width: parent.width
            text: qsTr("Вес, отметки и заметки за дату. Будущие дни — только просмотр.")
            font: Typography.body
            color: Theme.textSecondary
            wrapMode: Text.WordWrap
        }
        Label {
            objectName: "diaryScope"
            width: parent.width
            text: page.hasModel ? page.scopeText() : ""
            font: Typography.caption
            color: page.hasProgram ? Theme.textMuted : Theme.warning
            wrapMode: Text.WordWrap
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
                // Пока смещение ставит страница (alignStrip), делим его обратно на
                // недели нельзя: иначе weekStart перезапишется половинным сдвигом и
                // полоса уедет на чужую неделю.
                if (!moving || page.stripAligned)
                    return;
                page.weekStart = Math.max(0, Math.floor(contentX / ((width + spacing) / 7) / 7) * 7);
            }
            delegate: DayTile {
                required property int index
                readonly property var day: {
                    page.revision;
                    return page.dayAt(index);
                }
                width: page.stripTileWidth
                objectName: "diaryDate_" + (day ? day.date : "")
                // «пн» сверху и число под ним — как чипы недели в макете 13.
                // Раньше в плитке стояло только число, и дни недели приходилось
                // считать в уме.
                weekdayName: day ? page.shortWeekday(day.date) : ""
                showCount: true
                countText: day ? page.dayOfMonth(day.date) : ""
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
        // Название дня программы под неделей: чипы несут даты, а что за день внутри —
        // говорит эта строка, чтобы до прокрутки было понятно, куда попал тап.
        Label {
            objectName: "diaryDayTitle"
            width: parent.width
            visible: page.selectedDay !== null && page.selectedDay.dayTitle.length > 0
            text: page.selectedDay !== null ? page.selectedDay.dayTitle : ""
            font: Typography.sectionTitle
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
        }
        Label {
            objectName: "diaryReadOnlyPreview"
            width: parent.width
            visible: page.selectedDay !== null && !page.selectedDay.editable
            text: qsTr("Будущий день · только просмотр")
            font: Typography.caption
            color: Theme.textMuted
            wrapMode: Text.WordWrap
        }
        // День собран из блоков макета 13 (вес, правила, заметка) и держит одну
        // подпись-обёртку: так видно, что содержимое относится к одной дате.
        Column {
            id: dayCard

            readonly property var day: page.selectedDay
            readonly property string dayDate: page.activeDate
            readonly property bool canEdit: day !== null && day.editable
            property bool editingNote: false
            property string noteDate: ""
            property bool noteSaveFailed: false
            property string savedNote: ""
            objectName: "diaryDay_" + page.activeDate
            spacing: Spacing.sectionGap
            visible: page.selectedDay !== null
            width: parent.width

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
            // — Вес и обхваты: единственная точка записи замеров —
            // Раньше вес вводился на «Сегодня», а дневник правил его же: два места
            // одного факта. Теперь форма одна и живёт здесь (док 13, §2).
            AppCard {
                id: weightCard

                objectName: "diaryWeightCard"
                spacing: Spacing.listGap
                width: parent.width

                RowLayout {
                    objectName: "diaryWeightSummary"
                    spacing: Spacing.itemGap
                    width: parent.width

                    Column {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 2

                        Label {
                            width: parent.width
                            text: qsTr("Вес")
                            font: Typography.overline
                            color: Theme.textSecondary
                        }
                        RowLayout {
                            spacing: Spacing.listGap
                            width: parent.width

                            Label {
                                objectName: "diaryWeightMetric"
                                text: dayCard.day !== null ? page.weightText(dayCard.day) : "—"
                                font: Typography.metric
                                fontSizeMode: Text.HorizontalFit
                                minimumPixelSize: 16
                                color: Theme.textPrimary
                            }
                            Label {
                                Layout.alignment: Qt.AlignBottom
                                Layout.fillWidth: true
                                elide: Text.ElideRight
                                objectName: "diaryWeightDelta"
                                text: page.deltaText(dayCard.day)
                                font: Typography.captionStrong
                                color: page.deltaColor(dayCard.day)
                                visible: text.length > 0
                            }
                        }
                        Label {
                            objectName: "diaryWeightAverage"
                            width: parent.width
                            text: page.averageText(dayCard.day)
                            font: Typography.caption
                            color: Theme.textMuted
                            visible: text.length > 0
                            wrapMode: Text.WordWrap
                        }
                        // Прочерк без объяснения читается как поломка: подпись
                        // говорит, что делать, и живёт в той же карточке, что и
                        // кнопка ввода.
                        Label {
                            objectName: "diaryWeightHint"
                            width: parent.width
                            visible: dayCard.day !== null && !page.hasWeight(dayCard.day)
                            text: qsTr("Вес за этот день не записан")
                            font: Typography.caption
                            color: Theme.textMuted
                            wrapMode: Text.WordWrap
                        }
                    }
                    AppButton {
                        objectName: "diaryWeight_" + dayCard.dayDate
                        Layout.alignment: Qt.AlignTop
                        compact: true
                        enabled: dayCard.canEdit
                        text: qsTr("Изменить")
                        variant: AppButton.Ghost
                        onClicked: page.editNumber(dayCard.dayDate, dayCard.canEdit, null, true)
                    }
                }
                Label {
                    objectName: "diaryMeasurementsSummary"
                    width: parent.width
                    text: page.measurementCaption()
                    font: Typography.caption
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
                AppButton {
                    objectName: "diaryMeasurements_" + dayCard.dayDate
                    width: parent.width
                    enabled: dayCard.canEdit
                    text: page.measurementActionText()
                    variant: AppButton.Secondary
                    onClicked: page.openMeasurements()
                }
            }
            // — Правила программы: определение живёт в программе, факт — здесь —
            // Строка на правило: название и цель слева, значение и вердикт справа.
            // Тап открывает ввод числа за эту дату (макет 13, дневник).
            Column {
                objectName: "diaryRules"
                spacing: Spacing.listGap
                width: parent.width

                RowLayout {
                    objectName: "diaryRulesHeader"
                    spacing: Spacing.itemGap
                    width: parent.width

                    Label {
                        Layout.fillWidth: true
                        text: qsTr("Правила программы")
                        font: Typography.sectionTitle
                        color: Theme.textPrimary
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        objectName: "diaryRulesCounter"
                        text: page.ruleCountText(dayCard.day !== null ? dayCard.day.values : null)
                        font: Typography.caption
                        color: Theme.textMuted
                    }
                }
                Repeater {
                    id: dayValues
                    model: dayCard.day ? dayCard.day.values : null
                    delegate: AppButton {
                        id: valueButton
                        readonly property var cell: typeof modelData !== "undefined" ? modelData : model
                        readonly property var badge: page.verdictBadge(valueButton.cell.verdict)
                        width: parent.width
                        implicitHeight: Math.max(Theme.touchMin, contentItem.implicitHeight + topPadding + bottomPadding)
                        objectName: "diaryValue_" + dayCard.dayDate + "_" + cell.ruleId
                        enabled: dayCard.canEdit && cell.editable !== false
                        text: cell.title + ": " + page.displayValue(cell.value) + (cell.unit ? " " + page.unitLabel(cell.unit) : "") + "\n" + qsTr("Норма: %1%2").arg(cell.target).arg(cell.unit ? " " + page.unitLabel(cell.unit) : "") + " · " + page.verdictFor(cell.verdict).label
                        variant: AppButton.Ghost
                    contentItem: Column {
                        // Цвет вердикта наружу: подсветка значения и тест читают
                        // одно свойство, а не разбирают строки подписей.
                        readonly property color color: page.verdictFor(valueButton.cell.verdict).color
                        spacing: 2

                        RowLayout {
                            spacing: Spacing.itemGap
                            width: parent.width

                            Column {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 2

                                Label {
                                    width: parent.width
                                    text: valueButton.cell.title
                                    font: Typography.body
                                    color: Theme.textPrimary
                                    wrapMode: Text.WordWrap
                                }
                                Label {
                                    width: parent.width
                                    text: qsTr("цель %1%2").arg(valueButton.cell.target).arg(valueButton.cell.unit ? " " + page.unitLabel(valueButton.cell.unit) : "")
                                    font: Typography.caption
                                    color: Theme.textMuted
                                    wrapMode: Text.WordWrap
                                }
                            }
                            Label {
                                Layout.maximumWidth: parent.width * 0.4
                                text: page.displayValue(valueButton.cell.value) + (valueButton.cell.unit ? " " + page.unitLabel(valueButton.cell.unit) : "")
                                font: Typography.bodyStrong
                                color: valueButton.badge.color
                                horizontalAlignment: Text.AlignRight
                                wrapMode: Text.WrapAnywhere
                            }
                            // Бейдж вердикта: «в норме / мимо / нет значения» — то же
                            // слово, что в подписи строки, но читается с одного взгляда.
                            Rectangle {
                                Layout.alignment: Qt.AlignVCenter
                                color: Theme.surfaceMuted
                                implicitHeight: 20
                                implicitWidth: badgeLabel.implicitWidth + Spacing.listGap
                                radius: Theme.radiusSm

                                Label {
                                    id: badgeLabel

                                    anchors.centerIn: parent
                                    color: valueButton.badge.color
                                    font: Typography.captionStrong
                                    text: valueButton.badge.label
                                }
                            }
                        }
                        // Разделитель вместо отступа: строки правил идут подряд, и
                        // линия отделяет один факт от другого дешевле карточки.
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
            }
            // — Заметка дня: отдельная карточка, как в макете 13 —
            AppCard {
                objectName: "diaryNoteCard"
                spacing: Spacing.listGap
                width: parent.width

                Label {
                    width: parent.width
                    text: qsTr("Заметка дня")
                    font: Typography.overline
                    color: Theme.textSecondary
                }
                Label {
                    objectName: "diaryNotePreview"
                    width: parent.width
                    visible: !dayCard.editingNote
                    text: dayCard.day && dayCard.day.note.length > 0 ? dayCard.day.note : qsTr("Заметка дня пока не добавлена")
                    font: Typography.body
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
            AppButton {
                    objectName: "diaryNoteEdit_" + dayCard.dayDate
                    width: parent.width
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
                    objectName: "diaryNoteSave_" + dayCard.dayDate
                    width: parent.width
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
                    objectName: "diaryNoteCollapse_" + dayCard.dayDate
                    width: parent.width
                    visible: dayCard.editingNote
                    text: qsTr("Свернуть заметку")
                    variant: AppButton.Secondary
                    onClicked: dayCard.editingNote = false
                }
                Label {
                    objectName: "diaryNoteError_" + dayCard.dayDate
                    width: parent.width
                    visible: dayCard.noteSaveFailed
                    text: qsTr("Не удалось сохранить заметку. Повторите попытку.")
                    font: Typography.caption
                    color: Theme.negative
                    wrapMode: Text.WordWrap
                }
            }
            // — Тренировка дня: результат занятия этой даты —
            // Источник — сессия из БД, а не отметка правила: дневник показывает
            // факт (длительность, тоннаж), если тренировка была записана.
            Column {
                objectName: "diaryWorkout"
                spacing: Spacing.listGap
                width: parent.width

                Label {
                    width: parent.width
                    text: qsTr("Тренировка дня")
                    font: Typography.sectionTitle
                    color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                }
                AppCard {
                    spacing: Spacing.listGap
                    visible: page.selectedSessionHasData()
                    width: parent.width

                    RowLayout {
                        spacing: Spacing.itemGap
                        width: parent.width

                        Column {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: 2

                            Label {
                                objectName: "diarySessionTitle"
                                width: parent.width
                                text: page.sessionTitle()
                                font: Typography.bodyStrong
                                color: Theme.textPrimary
                                wrapMode: Text.WordWrap
                            }
                            Label {
                                objectName: "diarySessionMeta"
                                width: parent.width
                                text: page.sessionMetaText()
                                font: Typography.caption
                                color: Theme.textSecondary
                                wrapMode: Text.WordWrap
                            }
                        }
                        Rectangle {
                            Layout.alignment: Qt.AlignVCenter
                            color: Theme.accentSurface
                            implicitHeight: 20
                            implicitWidth: sessionBadge.implicitWidth + Spacing.listGap
                            radius: Theme.radiusSm

                            Label {
                                id: sessionBadge

                                anchors.centerIn: parent
                                color: Theme.accent
                                font: Typography.captionStrong
                                text: qsTr("записана")
                            }
                        }
                    }
                }
                Label {
                    objectName: "diaryWorkoutEmpty"
                    width: parent.width
                    visible: !page.selectedSessionHasData()
                    text: page.hasProgram ? qsTr("Тренировка за этот день не записана.") : qsTr("Тренировок пока нет: они появятся здесь после первой записанной тренировки.")
                    font: Typography.caption
                    color: Theme.textMuted
                    wrapMode: Text.WordWrap
                }
            }
        }
        // — Прошедшие тренировки: история занятий живёт в дневнике —
        // Раньше она была блоком «История» в профиле, рядом с демонстрационной
        // аналитикой: два источника правды об одном. Теперь история — там же, где
        // даты, из которых она состоит (док 13, §2).
        Column {
            objectName: "diaryHistory"
            spacing: Spacing.listGap
            visible: page.hasModel && page.viewModel.sessions.count > 0
            width: parent.width

            RowLayout {
                spacing: Spacing.itemGap
                width: parent.width

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Прошедшие тренировки")
                    font: Typography.sectionTitle
                    color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                }
                Label {
                    text: page.viewModel.sessions.count > 0 ? String(page.viewModel.sessions.count) : ""
                    font: Typography.caption
                    color: Theme.textMuted
                }
            }
            AppCard {
                paddingHorizontal: 0
                paddingVertical: Spacing.listGap
                spacing: 0
                width: parent.width

                Repeater {
                    model: page.hasModel ? page.viewModel.sessions : null

                    delegate: Item {
                        required property int index
                        required property string date
                        required property string title
                        // Имя не `state`: у Item оно уже есть (механизм состояний),
                        // и объявление одноимённого свойства ломает его.
                        required property string workoutState
                        required property int minutes
                        required property real tonnage

                        height: Theme.touchMin
                        width: parent.width

                        RowLayout {
                            anchors.fill: parent
                            anchors.leftMargin: Spacing.cardPadding
                            anchors.rightMargin: Spacing.cardPadding
                            spacing: Spacing.listGap

                            Column {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 2

                                Label {
                                    objectName: "diaryHistoryTitle_" + index
                                    width: parent.width
                                    elide: Text.ElideRight
                                    text: title
                                    font: Typography.body
                                    color: Theme.textPrimary
                                }
                                Label {
                                    objectName: "diaryHistoryMeta_" + index
                                    width: parent.width
                                    elide: Text.ElideRight
                                    text: page.shortDay(date) + " · " + page.sessionMeta({
                                        title: title,
                                        workoutState: workoutState,
                                        minutes: minutes,
                                        tonnage: tonnage
                                    })
                                    font: Typography.caption
                                    color: Theme.textSecondary
                                }
                            }
                        }
                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.leftMargin: Spacing.cardPadding
                            anchors.right: parent.right
                            anchors.rightMargin: Spacing.cardPadding
                            color: Theme.borderSubtle
                            height: Theme.borderWidth
                            visible: index < page.viewModel.sessions.count - 1
                        }
                    }
                }
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

    // Форма замеров дня: вес и обхваты одной карточкой. Раньше вес правился в
    // дневнике, а обхваты — в форме на «Сегодня»; после переноса ввода на главной
    // обхватам не осталось места, и «где сохранить замер» имел два ответа и ни
    // одного полного. Теперь ответ один — здесь.
    SwipePopup {
        id: measurementPopup

        // Окно обязано иметь ручку перетаскивания: SwipePopup объявляет её
        // required, и без неё падает не окно, а весь экран (StackView не может
        // создать initialItem) — экран оставался пустым.
        dragHandle: measurementGrip

        // Текущие значения полей по ключам замера. Карта пересобирается при
        // открытии: так «Изменить замеры» показывает уже записанное.
        property bool saveFailed: false
        // Текст ошибки проверки: пустые поля — это «не менялось», но совсем
        // пустую форму сохранять нечего.
        property string validationError: ""
        property var values: ({})
        // Поля — ключи замера из модели: вес всегда, обхваты — по программе.
        readonly property var fields: page.hasModel ? page.viewModel.measurementKeys : []
        parent: Overlay.overlay
        padding: Spacing.cardPadding
        modal: true
        dim: true
        width: Math.min(page.width - Spacing.screenPadding * 2, page.numberPopupMaxWidth)

        onAboutToShow: {
            measurementGrip.reset();
            saveFailed = false;
            validationError = "";
        }

        background: Rectangle {
            color: Theme.surface
            border.color: Theme.border
            border.width: Theme.borderWidth
            radius: Theme.radiusLg
        }

        contentItem: Column {
            spacing: Spacing.itemGap
            width: parent.width

            SwipeDismissHandle {
                id: measurementGrip
                width: parent.width
                onDismissed: measurementPopup.close()
            }
            Label {
                width: parent.width
                text: qsTr("Замеры за %1").arg(page.selectedDayCaption())
                font: Typography.sectionTitle
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }
            Label {
                width: parent.width
                text: qsTr("Заполните то, что измерили: пустые поля останутся прежними.")
                font: Typography.caption
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
            // Прокрутка: обхватов может быть до девяти, и на низком экране форма
            // выше самого окна.
            Flickable {
                flickableDirection: Flickable.VerticalFlick
                height: Math.min(measurementFields.height, page.height * 0.45)
                width: parent.width

                contentHeight: measurementFields.height

                Column {
                    id: measurementFields

                    spacing: Spacing.listGap
                    width: parent.width

                    Repeater {
                        model: measurementPopup.fields

                        delegate: Column {
                            id: measurementField

                            required property int index
                            required property string modelData

                            readonly property bool isWeight: modelData === "weight"
                            readonly property string key: modelData

                            property string text: measurementPopup.values[key] !== undefined ? String(measurementPopup.values[key]) : ""

                            spacing: 4
                            width: parent.width

                            Label {
                                width: parent.width
                                text: MeasurementTexts.title(measurementField.key) + ", " + MeasurementTexts.unit(measurementField.key)
                                font: Typography.captionStrong
                                color: Theme.textSecondary
                                wrapMode: Text.WordWrap
                            }
                            TextField {
                                id: measurementInput

                                objectName: "diaryMeasurementInput_" + measurementField.key
                                width: parent.width
                                color: Theme.textPrimary
                                font: Typography.body
                                inputMethodHints: Qt.ImhFormattedNumbersOnly
                                padding: Spacing.cardPadding
                                placeholderText: measurementField.isWeight ? qsTr("Например, 75,5") : qsTr("см")
                                selectByMouse: true
                                text: measurementField.text
                                background: Rectangle {
                                    color: Theme.surfaceMuted
                                    radius: Theme.radiusMd
                                    border.color: measurementInput.activeFocus ? Theme.accent : Theme.border
                                    border.width: Theme.borderWidth
                                }
                                onTextChanged: {
                                    measurementField.text = text;
                                    measurementPopup.values[measurementField.key] = text;
                                }
                            }
                        }
                    }
                }
            }
            Label {
                objectName: "diaryMeasurementError"
                width: parent.width
                visible: measurementPopup.saveFailed || measurementPopup.validationError.length > 0
                text: measurementPopup.validationError.length > 0 ? measurementPopup.validationError : qsTr("Не удалось сохранить замеры. Проверьте значения и повторите.")
                color: Theme.negative
                font: Typography.caption
                wrapMode: Text.WordWrap
            }
            AppButton {
                objectName: "diaryMeasurementSave"
                width: parent.width
                enabled: page.selectedDay !== null && page.selectedDay.editable
                text: qsTr("Сохранить замеры")
                variant: AppButton.Primary
                onClicked: page.saveMeasurements()
            }
            AppButton {
                width: parent.width
                text: qsTr("Отмена")
                variant: AppButton.Secondary
                onClicked: measurementPopup.close()
            }
        }
    }
}
