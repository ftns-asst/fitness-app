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
    title: page.dayTitleText()

    // Загрузка идёт при создании и при каждом показе: у страницы вкладки
    // onVisibleChanged не срабатывает при первом открытии, а данные должны быть
    // актуальны и после смены дня.
    Component.onCompleted: page.reload()
    onVisibleChanged: {
        if (visible)
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
            page.syncMeasurementFromModel(true);
    }

    Connections {
        function onStateChanged() {
            if (Application.state === Qt.ApplicationActive)
                page.reloadIfDayChanged();
        }

        target: Application
    }

    // Приложение, открытое на этом экране в полночь, на возврат из фона не
    // реагирует: минутной проверки хватает, чтобы день не отставал надолго.
    Timer {
        interval: 60000
        repeat: true
        running: page.visible
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
    // — Hero card (§4.2): CTA внутри карточки, а не в прокручиваемом списке —
    HeroCard {
        badgesText: page.heroBadges()
        // Кнопка «Начать тренировку» у дня отдыха обещала бы работу, которой
        // нет, поэтому у неё другое действие.
        ctaText: page.hasModel && page.viewModel.isRestDay ? qsTr("Открыть программу") : qsTr("Начать тренировку")
        summaryText: page.heroSummary()
        weekIndex: page.hasModel ? page.viewModel.currentWeek : 0
        weeksTotal: page.hasModel ? page.viewModel.weeksTotal : 0
        width: parent.width
        visible: page.hasModel && page.viewModel.hasProgram

        // День отдыха: «Открыть программу» ведёт в обзор активной программы.
        // Тренировочный день — этап D2: запись подходов ещё не реализована, и
        // кнопка честно сообщает об этом вместо молчаливого нажатия.
        onStartClicked: {
            if (page.hasModel && page.viewModel.isRestDay) {
                page.programRequested();

                return;
            }

            Demo.notify(qsTr("Запись тренировки — этап D2"));
        }
    }
    Label {
        color: Theme.textSecondary
        font: Typography.captionStrong
        text: qsTr("План на сегодня")
        visible: page.hasModel && page.viewModel.hasProgram && page.viewModel.isRestDay === false
        width: parent.width
    }
    AppCard {
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

    // — Два равноширинных secondary-действия (референс 2080). RowLayout вместо
    // ручного расчёта ширины: кнопки честно делят строку на любой ширине —
    RowLayout {
        spacing: Spacing.itemGap
        width: parent.width

        AppButton {
            Layout.fillWidth: true
            text: qsTr("Другой план")
            variant: AppButton.Secondary

            onClicked: Demo.notify(qsTr("Каталог планов — вкладка «Планы»"))
        }
        AppButton {
            Layout.fillWidth: true
            text: qsTr("История")
            variant: AppButton.Secondary

            onClicked: Demo.notify(qsTr("История — вкладка «Профиль», этап D4"))
        }
    }

    // — Правила дня плитками: по две в строке, отметка одним касанием —
    // Раньше были два вертикальных списка («Отметки дня» и «Правила на сегодня»), строки которых
    // дублировали друг друга. Теперь одна сетка: тап по плитке ставит/снимает «выполнено»;
    // у рекомендации текстом отметки нет (§3.6), плитка информационная.
    Label {
        color: Theme.textSecondary
        font: Typography.captionStrong
        text: page.doneRuleCount + " / " + page.checkableRuleCount + " " + qsTr("правил выполнено")
        visible: page.checkableRuleCount > 0
        width: parent.width
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

    // — Замеры: вес и обхваты за сегодня (спринт S6) —
    Label {
        color: Theme.textSecondary
        font: Typography.captionStrong
        text: qsTr("Замеры")
        width: parent.width
    }
    AppCard {
        spacing: Spacing.itemGap
        width: parent.width

        Label {
            color: Theme.textMuted
            font: Typography.caption
            text: qsTr("Вес и обхваты за сегодня. Повторный ввод обновляет запись, а не создаёт новую.")
            width: parent.width
            wrapMode: Text.WordWrap
        }
        // Замеры тела — ползунком: шаг 0,1 кг на диапазоне 0–300 кнопками набирался
        // сотнями нажатий, а смена веса за день (плюс-минус килограмм от воды и
        // еды) — это ровно тот случай, где нажатия и не нужны. Шаг 0,1 и диапазон
        // 0–300 сохранены: измерение отличается от инвентаря на плитах, и здесь
        // нужна десятая доля.
        WeightRuler {
            // Ширина — явно по родителю: карточка держит детей в обычной Column,
            // где `Layout.fillWidth` не работает (см. PageScaffold.qml), и без
            // привязки линейка брала свой implicitWidth — сжатая полоса слева.
            width: parent.width
            // Диапазон 35–200 кг вместо 0–300. На 300 делениях шаг 0,1
            // приходился на 0,1 px: метки сливались в серую полосу, и вес нельзя
            // было поставить пальцем. Для замеров шаг 0,1 сохранён — измерение
            // отличается от инвентаря на плитах, и десятая доля тут нужна.
            from: 35
            step: 0.1
            title: qsTr("Вес")
            to: 200
            unit: qsTr("кг")
            value: page.measurementValue("weight")
            valueDecimals: 1

            onEdited: function (newValue) {
                page.setMeasurementValue("weight", newValue);
            }
        }
        // Обхваты спрятаны: их пять, а измеряют их раз в месяц, поэтому
        // постоянно занимаемое ими место на главной было бы неоправданно.
        AppButton {
            text: page.girthsOpen ? qsTr("Скрыть обхваты") : qsTr("Обхваты")
            variant: AppButton.Secondary
            width: parent.width

            onClicked: page.girthsOpen = !page.girthsOpen
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Spacing.itemGap
            visible: page.girthsOpen

            Repeater {
                model: [
                    {
                        key: "chest",
                        label: qsTr("Грудь, см")
                    },
                    {
                        key: "waist",
                        label: qsTr("Талия, см")
                    },
                    {
                        key: "hip",
                        label: qsTr("Бёдра, см")
                    },
                    {
                        key: "arm",
                        label: qsTr("Бедро, см")
                    },
                    {
                        key: "thigh",
                        label: qsTr("Голень, см")
                    }
                ]

                delegate: WeightRuler {
                    id: girthField

                    required property var model

                    Layout.fillWidth: true
                    // Диапазон 50–150 см покрывает любой обхват взрослого человека
                    // и делает шкалу читаемой: на прежних 0–300 шаг 0,5 приходился на
                    // 0,5 px, и метки сливались в сплошную полосу.
                    from: 50
                    step: 0.5
                    title: girthField.model.label
                    to: 150
                    unit: qsTr("см")
                    value: page.measurementValue(girthField.model.key)
                    valueDecimals: 1

                    onEdited: function (newValue) {
                        page.setMeasurementValue(girthField.model.key, newValue);
                    }
                }
            }
        }
        AppButton {
            Layout.fillWidth: true
            text: qsTr("Сохранить замеры")
            variant: AppButton.Primary

            onClicked: page.saveMeasurements()
        }
    }

    // Имена дней недели в ISO-порядке 1…7 (§3.4): номер приходит из C++, а
    // название переводит разметка.
    readonly property var weekdayNames: [qsTr("Понедельник"), qsTr("Вторник"), qsTr("Среда"), qsTr("Четверг"), qsTr("Пятница"), qsTr("Суббота"), qsTr("Воскресенье")]

    // — Состояние формы замеров —
    // Обхваты раскрываются по требованию: измеряют их раз в месяц, а пять полей
    // на главной занимали бы место каждый день.
    property bool girthsOpen: false
    // Значения полей формы и «было ли это поле задано» — двумя словами, а не
    // одним числом: пустой обхват — это «не измерял», а 0 см — измерение,
    // и запись без этого различия затирала бы прежнее значение (upsert).
    property var measurementValues: ({})

    // Есть ли несохранённый ввод. Без этого флага любое обновление модели
    // перезаписывало бы форму данными из базы и стирало бы набранное.
    property bool measurementDirty: false
    property var measurementFilled: ({})

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
            page.syncMeasurementFromModel();
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
        const labels = {
            g: qsTr("г"),
            gPerKg: qsTr("г/кг"),
            kg: qsTr("кг"),
            kcal: qsTr("ккал"),
            ml: qsTr("мл"),
            hours: qsTr("ч"),
            rpe: "RPE"
        };

        return labels[unit] !== undefined ? labels[unit] : "";
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

    function measurementValue(key) {
        const values = page.measurementValues;

        return values[key] !== undefined ? values[key] : 0;
    }

    function setMeasurementValue(key, newValue) {
        const values = Object.assign({}, page.measurementValues);

        values[key] = newValue;
        page.measurementValues = values;

        const filled = Object.assign({}, page.measurementFilled);

        // Поле считается заполненным с первого нажатия «+»: значение 0 —
        // допустимое измерение, и без этого флага запись молча ушла бы в
        // «не измерял».
        filled[key] = true;
        page.measurementFilled = filled;
        page.measurementDirty = true;
    }

    // Переносит сохранённый замер в форму.
    //
    // Только при первой загрузке и после собственного сохранения. Раньше
    // функция вызывалась на каждое `changed` модели — а его эмитит и отметка
    // правила, — и набор полей у человека исчезал на глазах: отметил «сон» и
    // потерял только что введённый вес.
    function syncMeasurementFromModel(force) {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        // Несохранённый ввод — источник истины о форме: перезаписывать его
        // данными из базы нельзя, пока пользователь его не подтвердил.
        if (page.measurementDirty === true && force !== true)
            return;

        const map = {
            arm: "armCm",
            chest: "chestCm",
            hip: "hipCm",
            thigh: "thighCm",
            waist: "waistCm",
            weight: "weightKg"
        };
        const values = {};
        const filled = {};

        for (const key in map) {
            // Только заполненные поля: у незаданного обхвата значение -1, и
            // показывать его в поле значило бы выдумать измерение.
            if (page.viewModel.hasMeasurement) {
                const raw = page.viewModel[map[key]];

                if (raw !== undefined && raw >= 0) {
                    values[key] = raw;
                    filled[key] = true;
                }
            }
        }

        page.measurementValues = values;
        page.measurementFilled = filled;
        // Форма приведена к базе — ввод считается несохранённым только после
        // того, как человек сам что-то поменяет.
        page.measurementDirty = false;
    }

    function saveMeasurements() {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        const filled = page.measurementFilled;
        const values = page.measurementValues;
        // Флаг и значение идут отдельными аргументами: незаполненное поле
        // передаётся как «не задано», а не как ноль, иначе оно затирало бы
        // значение, введённое ранее.
        const saved = page.viewModel.saveMeasurement(values.weight !== undefined ? values.weight : -1, filled.weight === true, values.chest !== undefined ? values.chest : -1, filled.chest === true, values.waist !== undefined ? values.waist : -1, filled.waist === true, values.hip !== undefined ? values.hip : -1, filled.hip === true, values.arm !== undefined ? values.arm : -1, filled.arm === true, values.thigh !== undefined ? values.thigh : -1, filled.thigh === true);

        if (saved === false) {
            Demo.notify(qsTr("Заполните хотя бы одно поле"));

            return;
        }

        // Модель эмитит `changed` и внутри saveMeasurement, но форма в этот
        // момент ещё «грязная» и от синхронизации отказалась бы: сбросить её
        // явно нужно, иначе значения остались бы только в памяти до перезахода.
        page.measurementDirty = false;
        page.syncMeasurementFromModel(true);
        Demo.notify(qsTr("Замеры сохранены"));
    }

    // Заголовок главной — название программы, а не вид дня: вид дня относится
    // к плану, а не к тому, чем является приложение.
    function dayTitleText() {
        if (page.viewModel === undefined || page.viewModel === null || page.viewModel.hasProgram === false)
            return qsTr("Сегодня");

        return page.viewModel.programTitle;
    }

    // Подпись заголовка — дата и день недели: без неё «Сила 8 недель» не даёт
    // понять, какой это день.
    function weekdayLabel() {
        if (page.viewModel === undefined || page.viewModel === null)
            return "";

        const index = page.viewModel.weekday - 1;
        const name = index >= 0 && index < page.weekdayNames.length ? page.weekdayNames[index] : "";

        return name.length > 0 ? name + " · " + page.viewModel.today : page.viewModel.today;
    }

    // Бейджи hero: цель программы, вид дня и отметка о завершённой тренировке.
    // Отметка важна: повторно начать записанный день — ошибка пользователя, а
    // экран обязан её предотвращать подписью (§5.5).
    function heroBadges() {
        if (page.viewModel === undefined || page.viewModel === null || page.viewModel.hasProgram === false)
            return "";

        const badges = [page.goalText(), page.dayKindText()];

        if (page.viewModel.sessionCompleted)
            badges.push(qsTr("Тренировка записана"));

        return badges.join(" · ");
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

    // Цель программы: значение приходит строкой из enum, разметка переводит его
    // в подпись.
    function goalText() {
        const goals = {
            strength: qsTr("Сила"),
            fatLoss: qsTr("Похудение"),
            muscle: qsTr("Масса"),
            maintain: qsTr("Поддержание")
        };
        const key = page.viewModel.goal;

        return goals[key] !== undefined ? goals[key] : qsTr("Программа");
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
