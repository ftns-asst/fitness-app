import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Экран «Сегодня» на данных программы (§5.1, референс 2080/2081).
//
// Данные — из настоящей БД через Avm_Today, а не из MockCatalog: главная
// показывает то, что пользователь настроил (срок, день недели, упражнения).
//
// Состояния экрана (§5.5): «программы нет», «день отдыха», «упражнений не
// назначено», «тренировка уже записана». Каждое — отдельное состояние, а не
// пустой список упражнений.
PageScaffold {
    id: page

    required property var viewModel

    // Навигация с главного: «Открыть программу» (день отдыха) ведёт в обзор
    // активной программы, «Перейти к планам» — на вкладку планов, где программу
    // и создают. Экран только сообщает намерение, путь выбирает каркас.
    signal plansRequested
    signal programRequested
    signal diaryRequested

    // Строка упражнения на главном — самостоятельный объект: тап открывает
    // его обзор (спринт U6), а не молчит заглушкой. Экран сообщает намерение,
    // путь — стек поверх вкладок — выбирает каркас.
    signal exerciseOpenRequested(string exerciseId)

    // Модель задана? Проверка одной строкой, а не в каждой привязке.
    //
    // Без неё страница падала TypeError'ами на каждом чтении свойства, когда
    // модель ещё не подставлена (или не подставится вовсе — например, в
    // тестах дерева QML). Такие ошибки забивают вывод и маскируют настоящие:
    // в прогоне тестов они давали сотни QWARN на каждый прогон.
    readonly property bool hasModel: page.viewModel !== undefined && page.viewModel !== null

    caption: page.weekdayLabel()
    // Заголовок «Сегодня» крупной шапкой: верх экрана заполнен, а название
    // программы и день стоят в hero — так же, как в макете 13.
    heroBackground: true
    largeTitle: true
    title: qsTr("Сегодня")

    // Загрузка идёт при создании и при каждом показе: у страницы вкладки
    // onVisibleChanged не срабатывает при первом открытии, а данные должны быть
    // актуальны и после смены дня.
    Component.onCompleted: page.reload()
    onPageActiveChanged: {
        if (pageActive)
            page.reload();
    }

    function currentUserId() {
        return typeof AppContext !== "undefined" && AppContext !== null ? AppContext.currentUserId : "u1";
    }

    function reload() {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        page.viewModel.reload(page.currentUserId());
    }

    // Сутки сменились, пока приложение было открыто или лежало в фоне: «сегодня»
    // модели иначе осталось бы вчерашним, и замеры с отметками писались бы не в
    // тот день. Незаписанный ввод вчерашнего дня в новый день не переносится.
    function reloadIfDayChanged() {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        // Подставная модель тестов дерева QML этого метода не имеет.
        if (typeof page.viewModel.reloadIfDayChanged !== "function")
            return;

        if (page.viewModel.reloadIfDayChanged(page.currentUserId()))
            page.refreshCheckableRules();
    }

    Connections {
        function onStateChanged() {
            if (page.pageActive && Application.state === Qt.ApplicationActive)
                page.reloadIfDayChanged();
        }

        target: Application
    }

    // Приложение, открытое на этом экране в полночь, на возврат из фона не
    // реагирует: минутной проверки хватает, чтобы день не отставал надолго.
    Timer {
        interval: 60000
        repeat: true
        running: page.pageActive
        onTriggered: page.reloadIfDayChanged()
    }

    // — Состояние «программы нет»: приглашение создать, а не пустой план —
    AppCard {
        width: parent.width
        visible: !page.hasModel || page.viewModel.hasProgram === false

        ColumnLayout {
            width: parent.width
            spacing: Spacing.itemGap

            Label {
                Layout.fillWidth: true
                color: Theme.textPrimary
                font: Typography.sectionTitle
                text: qsTr("Пока нет программы")
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                color: Theme.textSecondary
                font: Typography.body
                text: qsTr("Создайте программу в разделе «Планы» — здесь появится план на сегодня.")
                wrapMode: Text.WordWrap
            }
            AppButton {
                Layout.fillWidth: true
                onClicked: page.plansRequested()
                text: qsTr("Перейти к планам")
                variant: AppButton.Secondary
            }
        }
    }
    // — Hero: программа, день, срок и главное действие —
    // Карточки-обёртки нет: фон под шапкой и первым блоком — переход акцента в
    // поверхность (PageScaffold.heroBackground), поэтому содержимое стоит прямо
    // на нём. Так же сделано в макете 13 и в 12-app.
    Column {
        id: hero

        objectName: "todayHero"
        spacing: Spacing.itemGap
        visible: page.hasModel && page.viewModel.hasProgram
        width: parent.width

        Row {
            spacing: Spacing.itemGap
            width: parent.width

            Label {
                color: Theme.accent
                elide: Text.ElideRight
                font: Typography.overline
                text: page.heroOverline()
                width: parent.width - pageLink.width - Spacing.itemGap
            }
            AppButton {
                id: pageLink

                compact: true
                objectName: "todayProgramLink"
                text: qsTr("Программа ›")
                variant: AppButton.Ghost

                onClicked: page.programRequested()
            }
        }
        Label {
            color: Theme.textPrimary
            font: Typography.largeTitle
            text: page.heroTitle()
            width: parent.width
            wrapMode: Text.WordWrap
        }
        Label {
            color: Theme.textSecondary
            font: Typography.body
            text: page.heroSummary()
            width: parent.width
            wrapMode: Text.WordWrap
        }

        Label {
            objectName: "todaySessionBadge"
            color: Theme.accent
            font: Typography.captionStrong
            text: qsTr("✓ Тренировка записана")
            visible: page.hasModel && page.viewModel.sessionCompleted === true
            width: parent.width
        }

        // Срок программы: «День N из M» и полоса прогресса. Это протяжённость
        // программы, а не выполнение тренировок — подпись говорит об этом прямо.
        Column {
            id: termBlock

            objectName: "todayTerm"
            spacing: 4
            visible: page.weeksTotal() > 0
            width: parent.width

            RowLayout {
                spacing: 0
                width: parent.width

                Label {
                    Layout.fillWidth: true
                    color: Theme.textSecondary
                    font: Typography.caption
                    text: qsTr("День %1 из %2").arg(page.passedDays()).arg(page.totalDays())
                }
                Label {
                    Layout.alignment: Qt.AlignRight
                    color: Theme.textSecondary
                    font: Typography.caption
                    text: qsTr("Осталось %1").arg(page.leftDays())
                }
            }
            Rectangle {
                color: Theme.surfaceMuted
                height: 6
                radius: 3
                width: parent.width

                Rectangle {
                    color: Theme.accent
                    height: parent.height
                    radius: parent.radius
                    width: parent.width * page.termProgress()
                }
            }
        }

        AppButton {
            objectName: "todayStartButton"
            text: page.hasModel && page.viewModel.isRestDay ? qsTr("Открыть программу") : qsTr("Начать тренировку")
            variant: AppButton.Primary
            width: parent.width

            // День отдыха ведёт в обзор программы; тренировочный день — этап D2:
            // запись подходов ещё не реализована, и кнопка честно сообщает об этом.
            onClicked: {
                if (page.hasModel && page.viewModel.isRestDay) {
                    page.programRequested();

                    return;
                }

                Demo.notify(qsTr("Запись тренировки — этап D2"));
            }
        }
    }
    Label {
        objectName: "todayPlanTitle"
        color: Theme.textSecondary
        font: Typography.captionStrong
        text: qsTr("План на сегодня")
        visible: page.hasModel && page.viewModel.hasProgram && page.viewModel.isRestDay === false
        width: parent.width
    }
    AppCard {
        objectName: "todayPlan"
        paddingHorizontal: 0
        paddingVertical: Spacing.itemGap
        spacing: 0
        visible: page.hasModel && page.viewModel.hasProgram && page.viewModel.isRestDay === false
        width: parent.width

        Repeater {
            // Модель ролей, а не массив: AList_Model отдаёт роли именами, и
            // делегат читает model.имя (§7 QML-контракт).
            model: page.hasModel && page.viewModel.hasProgram && page.viewModel.isRestDay === false ? page.viewModel.exercises : []

            delegate: ExerciseRow {
                // Роли читаются через model, а не через required property:
                // `title` у ExerciseRow уже есть, и объявление одноимённого
                // required property перекрывало бы свойство базового типа
                // (qmllint: property-override). Остальные роли объявлены явно —
                // так опечатку в имени ловит компилятор.
                required property string exerciseId
                required property var model
                required property int sets
                required property string repsText
                required property double targetWeightKg
                required property string weightMode
                // index тоже объявляется явно: у делегата с required property
                // неявный контекст отключается, и нумерация упражнений падала с
                // ReferenceError — строки шли без номеров.
                required property int index

                number: index + 1
                setsText: page.setsText(sets, repsText)
                showDivider: index < page.viewModel.exercises.count - 1
                title: model.title
                weightText: page.weightText(targetWeightKg, weightMode)
                width: parent.width

                // Обзор упражнения по тапу — то же место, что и в каталоге
                // (входы из Сегодня/Упражнения, §7 контракта экрана D3).
                onClicked: page.exerciseOpenRequested(exerciseId)
            }
        }
    }
    // День без упражнений — отдельное состояние: пустой список означал бы, что
    // день потерян, а на деле его упражнения ещё не назначены.
    Label {
        color: Theme.textSecondary
        font: Typography.body
        text: qsTr("На этот день упражнения не назначены. Добавьте их в настройке программы.")
        visible: page.hasModel && page.viewModel.hasProgram && page.viewModel.isRestDay === false && page.viewModel.exercises.count === 0
        width: parent.width
        wrapMode: Text.WordWrap
    }
    // — День отдыха: состояние дня, а не пустой список упражнений —
    AppCard {
        spacing: Spacing.itemGap
        visible: page.hasModel && page.viewModel.hasProgram && page.viewModel.isRestDay
        width: parent.width

        Label {
            color: Theme.textPrimary
            font: Typography.sectionTitle
            text: qsTr("Сегодня отдых")
            width: parent.width
            wrapMode: Text.WordWrap
        }
        Label {
            color: Theme.textSecondary
            font: Typography.body
            text: page.restHintText()
            visible: text.length > 0
            width: parent.width
            wrapMode: Text.WordWrap
        }
    }

    AppButton {
        width: parent.width
        text: qsTr("Другой план")
        variant: AppButton.Secondary
        onClicked: page.plansRequested()
    }

    // — «В фокусе»: вес и расписание замеров без ввода —
    // Ввод веса и обхватов живёт в дневнике: два места записи одного факта
    // расходились, и непонятно было, какое из них главное. Здесь только
    // чтение и переход туда, где значение действительно меняется.
    //
    // Тап-слой стоит ПОВЕРХ карточки, а не внутри неё: `MetricCard` — это
    // `AppCard`, а содержимое карточки живёт в собственной `Column`. Якорный
    // ребёнок внутри позиционера обнуляет её `implicitHeight` (проверено:
    // 84 → 28 px), и карточка рисуется ниже своего содержимого — значение и
    // тренд выезжали на заголовок «Отметки дня». Обёртка `Item` держит клик и
    // не вмешивается в раскладку карточки.
    RowLayout {
        id: focusRow

        objectName: "todayFocus"
        spacing: Spacing.itemGap
        visible: page.hasModel && page.viewModel.hasProgram
        width: parent.width

        Item {
            id: weightCell

            Layout.fillWidth: true
            implicitHeight: weightCard.implicitHeight

            MetricCard {
                id: weightCard

                width: parent.width
                objectName: "todayFocusWeight"
                deltaText: page.focusWeightHint()
                deltaKind: "neutral"
                highlight: page.viewModel.hasMeasurement
                label: qsTr("Вес сегодня")
                valueText: page.focusWeightText()
            }
            MouseArea {
                objectName: "todayFocusWeightTap"
                anchors.fill: parent
                onClicked: page.diaryRequested()
            }
        }
        Item {
            id: measurementCell

            Layout.fillWidth: true
            implicitHeight: measurementCard.implicitHeight

            MetricCard {
                id: measurementCard

                width: parent.width
                objectName: "todayFocusMeasurements"
                deltaText: page.measurementHint()
                highlight: false
                label: qsTr("Замеры по плану")
                valueText: page.measurementIsDue() ? qsTr("Пора") : qsTr("По графику")
            }
            MouseArea {
                objectName: "todayFocusMeasurementsTap"
                anchors.fill: parent
                onClicked: page.diaryRequested()
            }
        }
    }
    // — Правила дня плитками: по две в строке, отметка одним касанием —
    // Раньше были два вертикальных списка («Отметки дня» и «Правила на сегодня»), строки которых
    // дублировали друг друга. Теперь одна сетка: тап по плитке ставит/снимает «выполнено»;
    // у рекомендации текстом отметки нет (§3.6), плитка информационная.
    // Сегодня — быстрые отметки; дневник рядом с ними открывает историю по датам.
    RowLayout {
        objectName: "todayCheckinsHeader"
        spacing: Spacing.listGap
        visible: page.hasModel && page.viewModel.hasProgram
        width: parent.width

        ColumnLayout {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 2

            Label {
                Layout.fillWidth: true
                color: Theme.textPrimary
                font: Typography.sectionTitle
                text: qsTr("Отметки дня")
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                color: Theme.textSecondary
                font: Typography.caption
                text: page.doneRuleCount + " / " + page.checkableRuleCount + " " + qsTr("выполнено")
                visible: page.checkableRuleCount > 0
                wrapMode: Text.WordWrap
            }
        }
        AppButton {
            objectName: "todayDiaryButton"
            text: qsTr("Дневник")
            variant: AppButton.Ghost
            onClicked: page.diaryRequested()
            Accessible.description: qsTr("Дневник: отметки, вес и заметки за прошлые дни")
        }
    }
    Flow {
        id: ruleFlow

        readonly property real tileWidth: Math.floor((width - spacing) / 2)

        objectName: "ruleTiles"
        spacing: Spacing.listGap
        visible: page.hasModel && page.viewModel.hasProgram && page.viewModel.rules.count > 0
        width: parent.width

        Repeater {
            model: page.hasModel && page.viewModel.hasProgram ? page.viewModel.rules : []

            // Делегат — обёртка: роли модели объявляются на ней, а не на самой
            // плитке. Прежде `required property string title` на RuleTile
            // затеняло её собственное свойство title, и плитки рисовались белыми
            // и без текста.
            delegate: Item {
                id: ruleCell

                required property string displayText
                required property bool isAdvice
                required property var goalMax
                required property var goalMin
                required property string ruleId
                required property string targetUnit
                required property string title
                required property string todayState
                required property var todayValue

                readonly property bool measurable: ruleCell.isAdvice === false && (ruleCell.goalMin !== null || ruleCell.goalMax !== null)

                height: 112
                width: ruleFlow.tileWidth

                RuleTile {
                    anchors.fill: parent
                    caption: ruleCell.isAdvice ? page.ruleCaption(ruleCell.title, ruleCell.displayText) : (ruleCell.todayState === "done" ? qsTr("Выполнено") : page.ruleCaption(ruleCell.title, ruleCell.displayText))
                    checkable: ruleCell.isAdvice === false
                    checkinState: ruleCell.todayState
                    goalMax: ruleCell.goalMax !== null ? ruleCell.goalMax : -1
                    goalMin: ruleCell.goalMin !== null ? ruleCell.goalMin : -1
                    numeric: ruleCell.measurable
                    title: ruleCell.title
                    unitText: page.unitLabel(ruleCell.targetUnit)
                    value: ruleCell.todayValue !== null ? ruleCell.todayValue : -1

                    onEdit: valuePopup.openFor(ruleCell.ruleId, ruleCell.title, page.unitLabel(ruleCell.targetUnit), ruleCell.goalMin !== null ? ruleCell.goalMin : (ruleCell.goalMax !== null ? ruleCell.goalMax : 0), ruleCell.todayValue !== null ? ruleCell.todayValue : -1, page.unitStep(ruleCell.targetUnit))
                    onOpened: page.programRequested()
                    onToggled: page.toggleCheckin(ruleCell.ruleId, ruleCell.todayState)
                }
            }
        }
    }

    // Окно ввода числа для правила (калории, белок, вода…). Родитель — страница,
    // а не колонка контента: окно центрируется по экрану.
    RuleValuePopup {
        id: valuePopup

        parent: page

        onDoneRequested: ruleId => page.viewModel.markCheckinDone(ruleId)
        onValueSaved: (ruleId, value) => page.saveRuleValue(ruleId, value)
    }

    // Имена дней недели в ISO-порядке 1…7 (§3.4): номер приходит из C++, а
    // название переводит разметка.
    readonly property var weekdayNames: [qsTr("Понедельник"), qsTr("Вторник"), qsTr("Среда"), qsTr("Четверг"), qsTr("Пятница"), qsTr("Суббота"), qsTr("Воскресенье")]

    // Сколько правил можно отметить вручную. Пересчитывается при изменении
    // модели: обращения к ней внутри функции QML не отслеживает, поэтому
    // число хранится свойством, а не вычисляется в привязке `visible`.
    property int checkableRuleCount: 0
    // Сколько правил отмечено выполненными: счётчик над сеткой плиток.
    property int doneRuleCount: 0

    // Правила, которые можно отметить здесь: рекомендация текстом отметки не
    // имеет (§3.6), и кнопка у неё обещала бы действие, которого нет.
    //
    // Список — свойство, а не результат вызова в привязке `model`. Обращения к
    // модели внутри функции QML не отслеживает, поэтому список строится явно, но
    // делать это прямо в привязке нельзя: функция писала бы в
    // `checkableRuleCount` во время вычисления привязки — QML считает это
    // binding loop и обрывает её.
    property var checkableRuleRows: []

    Connections {
        function onChanged() {
            page.refreshCheckableRules();
        }

        target: page.viewModel
    }

    function refreshCheckableRules() {
        const result = [];

        if (page.viewModel === undefined || page.viewModel === null) {
            page.checkableRuleRows = result;
            page.checkableRuleCount = 0;
            page.doneRuleCount = 0;

            return;
        }

        const model = page.viewModel.rules;

        for (let i = 0; i < model.rowCount(); ++i) {
            const row = model.get(i);

            if (row.isAdvice === true)
                continue;

            result.push(row);
        }

        page.checkableRuleRows = result;
        page.checkableRuleCount = result.length;

        let done = 0;
        for (let j = 0; j < result.length; ++j) {
            if (result[j].todayState === "done")
                ++done;
        }
        page.doneRuleCount = done;
    }

    function toggleCheckin(ruleId, currentState) {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        // Повторное нажатие снимает отметку (и введённое число), иначе «выполнено»
        // нельзя было бы исправить, не уходя на экран правил. Любое другое
        // состояние (частично, не выполнено) по тапу становится «выполнено», а у
        // числового правила число заполняется целью.
        const ok = currentState === "done" ? page.viewModel.clearCheckin(ruleId) : page.viewModel.markCheckinDone(ruleId);

        if (ok === false) {
            Demo.notify(qsTr("Не удалось сохранить отметку"));
        }
    }

    // Ввод числа в окне: состояние выводит модель из цели правила.
    function saveRuleValue(ruleId, value) {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        if (page.viewModel.saveCheckinValue(ruleId, value) === false)
            Demo.notify(qsTr("Не удалось сохранить значение"));
    }

    // Подпись единицы правила; значения приходят строкой из домена.
    function unitLabel(unit) {
        return RuleTexts.unitLabel(unit, "");
    }

    // Шаг кнопок «−/+» в окне ввода: калории по 50, вода по 100 мл — по
    // единице, в которой человек считает.
    function unitStep(unit) {
        const steps = {
            g: 5,
            kcal: 50,
            ml: 100,
            hours: 0.5,
            kg: 0.5,
            count: 500
        };

        return steps[unit] !== undefined ? steps[unit] : 1;
    }

    function measurementIsDue() {
        return page.hasModel && page.viewModel.measurementDue === true;
    }

    // — «В фокусе»: только чтение. Значения берутся из модели, ввод — в дневнике.
    function focusWeightText() {
        if (!page.hasModel || page.viewModel.hasMeasurement !== true)
            return "—";

        return page.formatDecimal(page.viewModel.weightKg) + " " + qsTr("кг");
    }

    function focusWeightHint() {
        if (!page.hasModel)
            return "";

        if (page.viewModel.hasMeasurement !== true)
            return qsTr("Нет замера — добавьте в дневнике");

        return qsTr("Изменить — в дневнике");
    }

    // «82.4» → «82,4»: запятая как десятичный разделитель в русской локали.
    function formatDecimal(value) {
        const number = Number(value);

        if (!isFinite(number))
            return "—";

        return String(Math.round(number * 10) / 10).replace(".", ",");
    }

    // «2026-10-07» → «7 октября»: дата приходит из C++ строкой ISO.
    function formatShortDate(isoDate) {
        const parts = String(isoDate).split("-");

        return Qt.formatDate(new Date(Number(parts[0]), Number(parts[1]) - 1, Number(parts[2])), "d MMMM");
    }

    // Подсказка расписания замеров: пора мерить сейчас либо дата ближайшего.
    // Пустая строка — программы нет, и показывать нечего.
    function measurementHint() {
        if (!page.hasModel)
            return "";

        if (page.viewModel.measurementDue === true)
            return qsTr("Пора мерить: замер по расписанию");

        const next = page.viewModel.nextMeasurementDate;

        if (next === undefined || next === null || String(next).length === 0)
            return "";

        return qsTr("Следующий замер — %1").arg(page.formatShortDate(next));
    }

    // Подпись hero: программа и её срок — то, что человек выбрал сам. Срок не
    // повторяется, если он уже в названии («Сила 8 недель · 8 недель» — то же
    // число дважды подряд читается как ошибка данных).
    function heroOverline() {
        if (!page.hasModel || page.viewModel.hasProgram === false)
            return "";

        const title = String(page.viewModel.programTitle);
        const weeks = page.weeksTotal();

        if (weeks <= 0)
            return title;
        if (title.indexOf(String(weeks)) >= 0)
            return title;

        return qsTr("%1 · %2 недель").arg(title).arg(weeks);
    }

    // Крупный заголовок hero: что это за день.
    function heroTitle() {
        if (!page.hasModel || page.viewModel.hasProgram === false)
            return qsTr("Сегодня");

        const title = page.viewModel.dayTitle;
        const kind = page.dayKindText();

        return title !== undefined && title !== null && String(title).length > 0 ? title : kind;
    }

    function weeksTotal() {
        return page.hasModel ? page.viewModel.weeksTotal : 0;
    }

    // Срок считается в неделях: дни внутри недели модель не отдаёт отдельно,
    // поэтому прогресс — «сколько недель программы прошло».
    function totalDays() {
        return page.weeksTotal() * 7;
    }

    function passedDays() {
        const week = page.hasModel ? page.viewModel.currentWeek : 0;

        return Math.max(0, Math.min(page.totalDays(), week * 7));
    }

    function leftDays() {
        return Math.max(0, page.totalDays() - page.passedDays());
    }

    function termProgress() {
        const total = page.totalDays();

        return total > 0 ? Math.max(0, Math.min(1, page.passedDays() / total)) : 0;
    }

    // Подпись заголовка — дата и день недели: без неё «Сегодня» не даёт понять,
    // какой это день программы.
    function weekdayLabel() {
        if (page.viewModel === undefined || page.viewModel === null)
            return "";

        const index = page.viewModel.weekday - 1;
        const name = index >= 0 && index < page.weekdayNames.length ? page.weekdayNames[index] : "";

        return name.length > 0 ? name + " · " + page.viewModel.today : page.viewModel.today;
    }

    // Сводка дня: сколько упражнений и сколько минут. В дне без упражнений
    // показывается вид дня, а не «0 упражнений» (§5.5).
    function heroSummary() {
        if (page.viewModel === undefined || page.viewModel === null)
            return "";

        if (page.viewModel.isRestDay)
            return qsTr("День отдыха по программе");

        const count = page.viewModel.exercises.count;
        const minutes = page.viewModel.estimatedMinutes;

        if (count === 0)
            return qsTr("Упражнения не назначены");

        if (minutes > 0)
            return count + " " + qsTr("упражнений") + " · " + qsTr("около") + " " + minutes + " " + qsTr("мин");

        return count + " " + qsTr("упражнений");
    }

    // Подпись вида дня. Значение приходит строкой из домена (`workout` / `rest`
    // / `cardio` / `mobility`), а подпись на экране — человеческая: «rest» на
    // главной читалось бы как ошибка локализации.
    function dayKindText() {
        if (page.viewModel === undefined || page.viewModel === null)
            return "";

        const names = {
            workout: qsTr("Тренировка"),
            rest: qsTr("Отдых"),
            cardio: qsTr("Кардио"),
            mobility: qsTr("Мобилити")
        };
        const key = page.viewModel.dayKindName;

        return names[key] !== undefined ? names[key] : "";
    }

    // Подходы и повторы одной строкой. Пустое предписание не показывается
    // заглушкой: у упражнения без заданных подходов нет и строки «0 × ».
    function setsText(sets, repsText) {
        if (sets <= 0)
            return "";

        return repsText !== undefined && repsText.length > 0 ? sets + " × " + repsText : String(sets);
    }

    // Вес упражнения подписью. Пустое значение — это «вес не задан», и подпись
    // «0 кг» враньё, поэтому строка пустая и колонка скрывается сама.
    function weightText(kg, mode) {
        if (kg === undefined || kg === null)
            return "";

        // Процент от 1RM на экране дня показывать нечем: абсолютного веса у
        // предписания нет (§3.4, weightMode = percentOf1rm).
        if (mode === "percentOf1rm")
            return "";

        return kg + " " + qsTr("кг");
    }

    // Подсказка дня отдыха. Показывает ближайший рабочий день недели: он
    // отвечает на вопрос «что делать сегодня», который иначе остаётся без ответа.
    function restHintText() {
        if (page.viewModel === undefined || page.viewModel === null)
            return "";

        return qsTr("Восстановление — часть программы. Следующий тренировочный день — %1.").arg(page.weekdayNameAt(page.viewModel.nextTrainingWeekday));
    }

    // Подпись правила — цель или совет. Строка показывается не всегда: цель
    // «7–9 hours» под заголовком «Сон 7–9 часов» лишь повторяет её числами в
    // другой записи, а у советов («Без алкоголя») чисел нет — там подпись
    // единственный носитель смысла.
    function ruleCaption(title, displayText) {
        if (displayText === undefined || displayText.length === 0 || displayText === title)
            return "";

        const numbers = (displayText.match(/\d+(?:[.,]\d+)?/g) || []);

        for (let i = 0; i < numbers.length; ++i) {
            if (title.indexOf(numbers[i]) === -1)
                return displayText;
        }

        return "";
    }

    // Название дня недели по ISO-номеру. Ноль — дня нет (неделя кончилась или
    // программы нет), и подпись остаётся пустой, а не выдуманной.
    function weekdayNameAt(index) {
        return index >= 1 && index <= page.weekdayNames.length ? page.weekdayNames[index - 1] : "";
    }
}
