import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Выбор правил для программы (экран 44, §5.7). Отдельный экран, а не раздел
// редактора: правил много, а с фильтром и формой своего правила они не
// помещаются в один экран вместе с неделей.
//
// Каталог приходит из Avm_Program_Edit::catalog: пресеты объявлены в C++
// (Program_Rows::Build_Rule_Catalog), потому что их нужно фильтровать и
// помечать уже добавленными.
PageScaffold {
    id: page

    property var viewModel: ProgramEditVM

    // Экран двухшаговый, как в макете: подбор из каталога и форма своего правила —
    // разные жанры задачи, и на одной ленте форма съедала пол-экрана после
    // витрины. Форма занимает экран целиком (в макете это отдельный кадр),
    // поэтому «Назад» в ней закрывает форму, а не выходит с экрана.
    property bool customFormOpen: false

    BackBar {
        Layout.fillWidth: true
        onClicked: page.customFormOpen ? page.closeCustomForm() : page.backRequested()
        text: qsTr("Назад")
        visible: true
    }

    // Подпись — из макета: что за программа и что делают её правила. Число
    // выбранных правил стоит в заголовке блока «В программе» внизу: пока идёт
    // выбор, пользователь смотрит на каталог, а не на результат.
    caption: page.customFormOpen ? qsTr("%1 · добавится в программу").arg(page.programTitle()) : qsTr("%1 · действуют весь срок").arg(page.programTitle())
    title: page.customFormOpen ? qsTr("Своё правило") : qsTr("Правила программы")

    // Название программы в подписи: у безымянного черновика его нет, и пустая
    // подпись с ведущей точкой читалась бы как обрезанная.
    function programTitle() {
        const title = page.hasViewModel ? page.viewModel.title : "";

        return title.length > 0 ? title : qsTr("Программа");
    }

    readonly property bool hasProgram: page.viewModel !== undefined && page.viewModel !== null && page.viewModel.hasDraft
    // Активный чип фильтра. Изменяемое свойство, а не readonly: readonly означал
    // бы, что фильтр нельзя переключить и он навсегда остался бы на «Все».
    property int categoryIndex: 0
    // Отдельно от hasProgram: сам каталог доступен и без черновика программы,
    // и подпись «N в программе» — это про правила, а не про наличие модели.
    readonly property bool hasViewModel: page.viewModel !== undefined && page.viewModel !== null

    Component.onCompleted: page.setFilter("")
    onVisibleChanged: {
        if (visible)
            page.setFilter(page.currentCategory());
    }

    function currentCategory() {
        const option = page.categoryOptions[page.categoryIndex];

        return option === undefined ? "" : option.value;
    }

    function setFilter(category) {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        page.viewModel.setCatalogFilter(category);
    }

    function addPreset(code) {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        page.viewModel.addCatalogPreset(code);
    }

    // — Форма своего правила: состояние —
    // Метрика выбирается индексом каталога, а не значением: значение пришло бы
    // строкой и потребовало бы разбора обратно в enum на стороне QML.
    property int metricIndex: 0
    property string customTitle: ""
    // Вид своего правила: измеримое (метрика + порог) или «Другое» — текстовая
    // рекомендация. Раньше «Другое» существовало только фильтром категорий:
    // создать такое правило здесь было нечем, и пользователь либо уходил в
    // редактор, либо получал измеримое правило с нулевым порогом.
    property bool adviceMode: false
    // Текст рекомендации — то, что уходит в `note` правила (§3.6). Числовая цель
    // в этом виде не «необязательна», а невозможна, и об этом сказано рядом с
    // полем, а не оставлено молчанием формы.
    property string adviceText: ""
    // Цель — одно условие и одно значение, как в макете (`.cond`): «выше», «ниже»
    // или «ровно». Раньше в форме стояли два независимых ползунка («Не ниже» и
    // «Не выше»), и правило с двумя порогами читалось как два правила, а выбор
    // одного из двух не был виден вовсе.
    property string customCondition: "min"
    property double customValue: 0

    // Плитки метрик для формы: название и единица измерения. Единица — свойство
    // метрики (Program_Rows::Build_Metric_Catalog), а не выбор пользователя:
    // «сон в граммах» недостижим, поэтому и спрашивать нечего.
    readonly property var metricTiles: {
        const tiles = [];
        const model = page.hasViewModel ? page.viewModel.metrics : null;

        if (model === null)
            return tiles;

        for (let i = 0; i < model.count; ++i) {
            const row = model.get(i);

            tiles.push({
                index: i,
                title: row.title,
                unit: page.unitLabel(row.unit)
            });
        }

        return tiles;
    }

    // Строка метрики целиком: она отвечает и за единицу, и за форму цели.
    // Значение по умолчанию соответствует «ничего не выбрано» — форма показывает
    // минимальный набор полей, а кнопка без цели будет отвергнута моделью.
    readonly property var currentMetric: {
        const model = page.hasViewModel ? page.viewModel.metrics : null;
        const row = model === null ? null : model.get(page.metricIndex);

        if (row === null || row.metric === undefined)
            return {
                allowsMax: true,
                allowsMin: true,
                allowsExact: false,
                unit: ""
            };

        return row;
    }

    // Условия цели, допустимые метрикой: «ровно» осмысленно для сна и
    // бессмысленно для нагрузки, где важна только граница сверху. Таблица отдаёт
    // и подписи, и порядок сегментов — в макете это «выше · ниже · ровно».
    readonly property var conditionOptions: {
        const options = [];
        const metric = page.currentMetric;

        if (metric.allowsMin === true)
            options.push({
                key: "min",
                title: qsTr("выше")
            });

        if (metric.allowsMax === true)
            options.push({
                key: "max",
                title: qsTr("ниже")
            });

        if (metric.allowsExact === true)
            options.push({
                key: "exact",
                title: qsTr("ровно")
            });

        return options;
    }
    readonly property var conditionTitles: {
        const titles = [];

        for (let i = 0; i < page.conditionOptions.length; ++i)
            titles.push(page.conditionOptions[i].title);

        return titles;
    }
    readonly property int conditionIndex: {
        for (let i = 0; i < page.conditionOptions.length; ++i) {
            if (page.conditionOptions[i].key === page.customCondition)
                return i;
        }

        return 0;
    }
    // Подпись выбранного условия: она же заголовок ползунка значения.
    readonly property string conditionLabel: {
        const index = page.conditionIndex;

        return index >= 0 && index < page.conditionOptions.length ? page.conditionOptions[index].title : qsTr("не ниже");
    }

    // Смена метрики приводит условие и значение к её правилам: у новой метрики
    // другой диапазон и другой набор условий, и «ровно 8» в часах сна оставило бы
    // форму в состоянии, которое модель отвергнет.
    function setMetric(index) {
        page.metricIndex = index;

        const options = page.conditionOptions;
        let allowed = false;

        for (let i = 0; i < options.length; ++i) {
            if (options[i].key === page.customCondition)
                allowed = true;
        }

        if (allowed === false)
            page.customCondition = options.length > 0 ? options[0].key : "min";

        const range = page.metricRange(page.currentMetric);

        page.customValue = Math.max(range.from, Math.min(range.to, page.customValue));
    }

    // Форма открывается пустой: «Отмена» — это отказ от ввода, а не пауза в нём,
    // и остатки прошлого правила в полях читались бы как уже введённые данные.
    function openCustomForm() {
        page.customTitle = "";
        page.customValue = 0;
        // Вид правила сбрасывается к измеримому: «Отмена» — это отказ от ввода,
        // и открытая заново форма не должна выглядеть продолжением «Другого».
        page.adviceMode = false;
        page.adviceText = "";
        page.customFormOpen = true;
        customTitleField.text = "";
        adviceTextArea.text = "";
    }

    // Переключение вида правила. Порог при этом не теряется: вернувшись к
    // измеримому виду, пользователь видит прежнюю шкалу, а не ноль.
    function setAdviceMode(isAdvice) {
        page.adviceMode = isAdvice;
    }

    function closeCustomForm() {
        page.customFormOpen = false;
    }

    function addCustomRule() {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        // «Другое» уходит другим входом: у рекомендации нет ни метрики, ни цели —
        // только текст, и домен проверяет его непустоту (`Has_Advice_Text`), а не
        // наличие порога.
        if (page.adviceMode) {
            if (page.viewModel.addAdvice(page.customTitle, page.adviceText) === false)
                return;

            page.customFormOpen = false;
            page.customTitle = "";
            page.adviceText = "";
            page.adviceMode = false;
            customTitleField.text = "";
            adviceTextArea.text = "";

            return;
        }

        const metric = page.currentMetric;
        const metricId = metric.metric === undefined ? -1 : metric.metric;
        const unit = page.unitValue(metric.unit);
        const value = page.customValue;
        let added = false;

        // В модель уходит ровно один порог: правило и читается одним условием,
        // «не выше 8» — это max, а не пара границ.
        if (page.customCondition === "max")
            added = page.viewModel.addCustomRule(page.customTitle, metricId, unit, 0, false, value, true, 0, false);
        else if (page.customCondition === "exact")
            added = page.viewModel.addCustomRule(page.customTitle, metricId, unit, 0, false, 0, false, value, true);
        else
            added = page.viewModel.addCustomRule(page.customTitle, metricId, unit, value, true, 0, false, 0, false);

        // Форма закрывается только после успеха: при отказе (метрика вне
        // каталога, пустое название) пользователь не должен потерять введённое —
        // текст ошибки он прочитает рядом с полем.
        if (added === false)
            return;

        page.customFormOpen = false;
        page.customTitle = "";
        customTitleField.text = "";
    }

    // Превью — то, что человек увидит в списке правил и на «Сегодня»: строка
    // собирается из тех же данных, что уйдут в модель, чтобы опечатку в пороге
    // было видно до сохранения, а не через неделю работы.
    function previewText() {
        // «Другое» показывается тем, чем станет: названием и текстом совета.
        // Пример тот же, что в макете: пустое превью не объясняет, что появится.
        if (page.adviceMode) {
            const advice = page.adviceText.trim();

            if (page.customTitle === "" && advice === "")
                return qsTr("Например: не пить алкоголь за 2 дня до соревнования");

            if (page.customTitle === "")
                return advice;

            return advice === "" ? page.customTitle : page.customTitle + ": " + advice;
        }

        const metric = page.currentMetric;

        // Метрика не выбрана — показываем пример из макета: пустое превью не
        // объясняет, что здесь появится.
        if (metric.metric === undefined)
            return qsTr("Например: 8 000 шагов не ниже 6 000 каждый день");

        const unit = page.unitLabel(metric.unit);

        return qsTr("%1 %2 %3 %4").arg(metric.title).arg(page.conditionLabel).arg(page.formattedValue()).arg(unit).trim();
    }

    // Разрядность и разделитель берутся у ползунка: превью обязано показывать то
    // же число, что стоит в поле, а не округлять его по-своему.
    function formattedValue() {
        return valueRuler.formatWeight(page.customValue, valueRuler.valueDecimals);
    }

    // Периодичность приходит из метрики (ERule_Periodicity: 0 — в день,
    // 1 — в неделю, 2 — на тренировке), поэтому её нет отдельным полем.
    function periodicityText() {
        const periodicity = page.currentMetric.periodicity;

        if (periodicity === 0)
            return qsTr("каждый день");

        if (periodicity === 1)
            return qsTr("каждую неделю");

        return qsTr("на каждой тренировке");
    }

    // Ошибка принадлежит полю формы — баннер общий и подсказывает не то место.
    readonly property var formErrorKeys: ["rule_metric_unknown", "rule_target_kind_invalid", "rule_target_range_invalid", "rule_target_required", "rule_title_required", "rule_unit_mismatch", "rule_advice_text_required"]

    function formError(key) {
        return page.errorKey === key;
    }

    // Единица приходит из каталога строкой, а модель ждёт число: разбор здесь, в
    // единственном месте. Порядок соответствует ERule_Display_Unit
    // (Program_Model) — сдвиг элемента здесь означал бы, что «часы» сохранятся
    // как граммы, и ошибка всплыла бы только в отметках.
    function unitValue(unit) {
        return ["g", "gPerKg", "kg", "kcal", "ml", "hours", "count", "rpe", "units"].indexOf(unit);
    }

    // Диапазон и шаг ползунка задаются по метрике, а не одной парой на весь
    // список. Прежнее поле с «−/+» шага 1 не справлялось с большинством
    // метрик: воду на 2000 мл и шаги на 10000 в нём набирались тысячами
    // нажатий, а обратно — вес тела на 80 кг и RPE на 8 были недостижимы
    // (потолок поля был 100, шаг 1).
    //
    // Границы — осмысленные пределы самой метрики, а не круглые числа: у воды
    // потолок 5000 мл, у сна 14 ч, у RPE 10. Нижняя граница у счётных метрик
    // (шаги, алкоголь) — ноль, у остальных единиц ноль тоже допустим.
    function metricRange(metric) {
        const ranges = {
            calories: {
                step: 10,
                to: 5000
            },
            count: {
                step: 500,
                to: 50000
            },
            g: {
                step: 5,
                to: 500
            },
            gPerKg: {
                step: 0.5,
                to: 20
            },
            hours: {
                step: 0.5,
                to: 14
            },
            kcal: {
                step: 50,
                to: 5000
            },
            kg: {
                step: 0.5,
                to: 300
            },
            ml: {
                step: 50,
                to: 5000
            },
            rpe: {
                step: 0.5,
                to: 10
            },
            units: {
                step: 1,
                to: 50
            }
        };
        const range = ranges[metric.unit] !== undefined ? ranges[metric.unit] : null;

        // Метрика без единицы в каталоге (а это бывает, см. currentMetric) получает
        // нейтральный диапазон: ползунок всё равно должен быть, иначе поле
        // пропадало бы у половины метрик.
        if (range === null)
            return {
                from: 0,
                step: 1,
                to: 100
            };

        return {
            from: 0,
            step: range.step,
            to: range.to
        };
    }

    // Значение зажимается в диапазон метрики перед показом. Приходит оно из
    // черновика правила или из уже добавленного правила, и без зажима ползунок
    // показывал бы «1500 мл» на шкале до 5000 — значение вне решётки, которого
    // нельзя повторить перетаскиванием.
    function metricValue(rawValue, metric) {
        const range = page.metricRange(metric);

        return Math.max(range.from, Math.min(range.to, rawValue));
    }

    // Категории фильтра: строки, а не числа, — так же приходят в строки
    // правил и каталога, поэтому словарь переводов один на оба списка.
    readonly property var categoryOptions: [
        {
            title: qsTr("Все"),
            value: ""
        },
        {
            title: qsTr("Питание"),
            value: "nutrition"
        },
        {
            title: qsTr("Активность"),
            value: "lifestyle"
        },
        {
            title: qsTr("Восстановление"),
            value: "recovery"
        },
        {
            title: qsTr("Тренировки"),
            value: "training"
        }
    ]
    readonly property var categoryTitles: {
        const titles = [];

        for (let i = 0; i < page.categoryOptions.length; ++i)
            titles.push(page.categoryOptions[i].title);

        return titles;
    }

    // Подпись цели пресета: «не ниже 140 г», «7–9 ч». Формат тот же, что у
    // строк правил, иначе каталог и список показывали бы одно и то же разными
    // способами.
    //
    // Единицы переводятся здесь, а не берутся из строки модели: модель отдаёт
    // машинные значения («g», «ml», «count»), и в подписи они выглядели бы
    // как технический сбой (§7: перевод единиц — в UI).
    function unitLabel(unit) {
        const labels = {
            g: qsTr("г"),
            gPerKg: qsTr("г/кг"),
            kg: qsTr("кг"),
            kcal: qsTr("ккал"),
            ml: qsTr("мл"),
            hours: qsTr("ч"),
            count: "",
            rpe: "RPE",
            units: ""
        };

        return labels[unit] !== undefined ? labels[unit] : unit;
    }

    // Число выбранных правил: привязка читает `count` модели, а не зовёт метод
    // `rowCount()`. Вызов метода внутри привязки не создаёт зависимости, и число
    // осталось бы нулём навсегда — ровно то же правило, что у групп правил на
    // экране отметок (см. ProgramRulesPage.ruleGroups).
    readonly property int chosenCount: page.hasViewModel ? page.viewModel.rules.count : 0

    // Категория по ключу модели ("nutrition", "lifestyle", …). Словарь один на
    // фильтр, витрину и список выбранного: категория, названная в трёх местах
    // по-разному, читается как три разные категории.
    function categoryTitle(key) {
        for (let i = 0; i < page.categoryOptions.length; ++i) {
            if (page.categoryOptions[i].value === key)
                return page.categoryOptions[i].title;
        }

        return key;
    }

    // Цвет точки категории — как в макете (`.cat`): питание — акцент, активность —
    // positive, восстановление — warning, тренировки — negative. «Другое» и
    // «техника» своего смысла в палитре не имеют и получают нейтральный тон.
    function categoryColor(key) {
        const colors = {
            lifestyle: Theme.positive,
            nutrition: Theme.accent,
            recovery: Theme.warning,
            training: Theme.negative
        };

        return colors[key] !== undefined ? colors[key] : Theme.textMuted;
    }

    signal backRequested

    // — Подбор: фильтр, витрина каталога, вход в своё правило —
    // Зона отдельной колонкой, а не россыпью по каркасу: шаг формы прячет её
    // целиком — в макете подбор и форма это разные кадры, а не две части ленты.
    Column {
        id: pickerZone

        spacing: Spacing.sectionGap
        visible: page.customFormOpen === false
        width: parent.width

        FilterChips {
            // Ширина от страницы: колонка каркаса — обычная `Column`, где
            // `Layout.fillWidth` не действует и ряд брал бы свои 320 px.
            width: parent.width
            currentIndex: page.categoryIndex
            items: page.categoryTitles
            onSelected: function (index) {
                page.categoryIndex = index;
                page.setFilter(page.currentCategory());
            }
        }
        Column {
            spacing: Spacing.itemGap
            width: parent.width

            Label {
                color: Theme.textMuted
                font: Typography.sectionTitle
                text: qsTr("Каталог правил")
                width: parent.width
            }
            // Витрина, а не список: шесть пресетов в две колонки — это тот же
            // выбор, но на четверти высоты, и «Своё правило» остаётся на экране,
            // а не за прокруткой.
            GridLayout {
                columnSpacing: Spacing.itemGap
                columns: 2
                objectName: "rulePresetGrid"
                rowSpacing: Spacing.itemGap
                width: parent.width

                Repeater {
                    // Каталог не зависит от черновика программы: он общий, и
                    // прятать его до создания программы значило бы показать
                    // пустоту вместо того, из чего выбирают.
                    model: page.hasViewModel ? page.viewModel.catalog : []

                    // Роли читаются через `model.<роль>`, а не обязательными
                    // свойствами: `title` и `isAdded` совпадают с именами свойств
                    // плитки, и `required` перекрыло бы её собственные (§4).
                    delegate: RulePresetTile {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        categoryColor: page.categoryColor(model.category)
                        categoryTitle: page.categoryTitle(model.category)
                        isAdded: model.isAdded
                        title: model.title

                        onClicked: page.addPreset(model.code)
                    }
                }
            }
            Label {
                color: Theme.textSecondary
                font: Typography.caption
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Подходящих правил нет. Добавьте своё — форма ниже.")
                visible: page.hasViewModel && page.viewModel.catalog.count === 0
                width: parent.width
                wrapMode: Text.WordWrap
            }
        }
        // Ошибки каталога остаются баннером: они не про поле формы, а про выбор
        // из витрины (пресет уже добавлен, категория не распознана). Ошибки
        // формы показываются у своих полей — см. formError().
        AppCard {
            width: parent.width
            visible: page.errorKey.length > 0 && page.formErrorKeys.indexOf(page.errorKey) < 0

            Label {
                color: Theme.negative
                font: Typography.body
                text: page.errorText()
                width: parent.width
                wrapMode: Text.WordWrap
            }
        }
        // Вход в форму — карточка-ссылка (`.navrow`): форма длинная, а развёрнутая
        // она съедала пол-экрана после витрины. Тап по всей карточке открывает шаг
        // формы, где она живёт целиком и не спорит с каталогом за место.
        AppCard {
            id: customFormEntry

            objectName: "customRuleEntry"
            width: parent.width

            RowLayout {
                width: parent.width
                spacing: Spacing.itemGap

                ColumnLayout {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    spacing: 2

                    Label {
                        Layout.fillWidth: true
                        color: Theme.textPrimary
                        font: Typography.bodyStrong
                        text: qsTr("Своё правило")
                        wrapMode: Text.WordWrap
                    }
                    Label {
                        Layout.fillWidth: true
                        color: Theme.textSecondary
                        font: Typography.caption
                        // Примеры из макета: они про случаи, которых нет ни в одной
                        // метрике каталога, — иначе «своё» читается как «то же, но
                        // вручную».
                        text: qsTr("Например: присед раз в неделю, алкоголь — стоп за 2 дня до соревнования")
                        wrapMode: Text.WordWrap
                    }
                }
                Label {
                    color: Theme.textMuted
                    font: Typography.sectionTitle
                    text: "›"
                }
            }
            TapHandler {
                onTapped: page.openCustomForm()
            }
        }
    }

    // — Форма своего правила: отдельный шаг экрана —
    // Метрика задаёт единицу и периодичность сама, поэтому «сон — в часах?» и
    // «как часто?» спрашивать нечего: и то и другое приходит из каталога метрик
    // (Program_Rows::Build_Metric_Catalog). Форма спрашивает ровно то, чего домен
    // не знает: что измеряем, с каким порогом и как это назвать.
    Column {
        id: customFormZone

        objectName: "customRuleForm"
        spacing: Spacing.sectionGap
        visible: page.customFormOpen
        width: parent.width

        // Вид своего правила выбирается первым: он меняет весь состав формы —
        // у измеримого есть метрика и порог, у «Другого» только текст.
        Column {
            spacing: Spacing.itemGap
            width: parent.width

            SegmentedControl {
                id: ruleKindSelector

                objectName: "ruleKindSelector"
                currentIndex: page.adviceMode ? 1 : 0
                items: [qsTr("Измеримое"), qsTr("Другое")]
                width: parent.width

                onSelected: function (index) {
                    page.setAdviceMode(index === 1);
                }
            }
            Label {
                color: Theme.textMuted
                font: Typography.caption
                text: page.adviceMode ? qsTr("Совет текстом: числовой цели у него нет, и отметку по нему не поставить") : qsTr("Правило с порогом: по нему ставится отметка каждый день или на тренировке")
                width: parent.width
                wrapMode: Text.WordWrap
            }
        }
        Column {
            spacing: Spacing.itemGap
            // Измеримая часть формы скрыта в «Другом»: метрика и порог там не
            // «необязательны», а невозможны, и пустые поля читались бы как
            // незаполненные — то есть как незаконченный ввод.
            visible: page.adviceMode === false
            width: parent.width

            Label {
                color: Theme.textMuted
                font: Typography.overline
                text: qsTr("Что измеряем")
                width: parent.width
            }
            GridLayout {
                columnSpacing: Spacing.itemGap
                columns: 3
                objectName: "ruleMetricGrid"
                rowSpacing: Spacing.itemGap
                width: parent.width

                Repeater {
                    model: page.metricTiles

                    delegate: MetricTile {
                        required property var modelData

                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        selected: modelData.index === page.metricIndex
                        title: modelData.title
                        unit: modelData.unit

                        onClicked: page.setMetric(modelData.index)
                    }
                }
            }
            // Ошибка метрики — у плиток, а не баннером в конце формы: без метрики
            // проверять нечего, и подсказка обязана стоять там, где выбирают.
            Label {
                color: Theme.negative
                font: Typography.caption
                text: qsTr("Выберите, что измеряем")
                visible: page.formError("rule_metric_unknown") || page.formError("rule_unit_mismatch")
                width: parent.width
                wrapMode: Text.WordWrap
            }
        }
        Column {
            spacing: Spacing.itemGap
            width: parent.width

            Label {
                color: Theme.textMuted
                font: Typography.overline
                text: qsTr("Цель")
                width: parent.width
            }
            // Условие — сегментированный выбор: варианты взаимоисключающие, и
            // активный обязан читаться без нажатия — как направление цели в
            // редакторе программы.
            SegmentedControl {
                id: conditionSelector

                currentIndex: page.conditionIndex
                items: page.conditionTitles
                objectName: "ruleConditionSelector"
                width: parent.width

                onSelected: function (index) {
                    page.customCondition = page.conditionOptions[index].key;
                }
            }
            // Значение — ползунком: у большинства метрик диапазон не помещается в
            // пару кнопок (вода 2000 мл, шаги 10 000), а шаг 1 у старого поля делал
            // ввод недостижимым. Заголовок ползунка — выбранное условие: подпись и
            // число читаются одной строкой, «не выше 8».
            WeightRuler {
                id: valueRuler

                from: page.metricRange(page.currentMetric).from
                objectName: "ruleValueRuler"
                step: page.metricRange(page.currentMetric).step
                title: page.conditionLabel
                to: page.metricRange(page.currentMetric).to
                unit: page.unitLabel(page.currentMetric.unit)
                value: page.metricValue(page.customValue, page.currentMetric)
                valueDecimals: page.metricRange(page.currentMetric).step < 1 ? 1 : 0
                width: parent.width

                onEdited: function (value) {
                    page.customValue = value;
                }
            }
            Label {
                color: Theme.negative
                font: Typography.caption
                text: qsTr("Укажите порог — без него отметку негде поставить")
                visible: page.formError("rule_target_required") || page.formError("rule_target_kind_invalid") || page.formError("rule_target_range_invalid")
                width: parent.width
                wrapMode: Text.WordWrap
            }
        }
        // Текст — главное поле «Другого»: у рекомендации нет ни метрики, ни цели,
        // и содержание совета и есть правило (§3.6).
        AppTextArea {
            id: adviceTextArea

            objectName: "ruleAdviceText"
            errorText: page.formError("rule_advice_text_required") ? page.errorText() : ""
            helperText: qsTr("Совет, который нечем измерить")
            label: qsTr("Текст рекомендации")
            placeholderText: qsTr("Опишите, что делать вместо числовой цели")
            visible: page.adviceMode
            width: parent.width

            onEdited: page.adviceText = text
        }
        AppTextField {
            id: customTitleField

            errorText: page.formError("rule_title_required") ? page.errorText() : ""
            label: page.adviceMode ? qsTr("Название рекомендации") : qsTr("Своё название")
            onUserEdited: page.customTitle = text
            placeholderText: page.adviceMode ? qsTr("Например: алкоголь — стоп за 2 дня") : qsTr("Например: тяжёлые подходы не тяжелее 8")
            width: parent.width
        }
        // Превью (`.rule-preview`): строка, которую человек увидит в списке правил
        // и на «Сегодня». Показываем до сохранения, чтобы опечатку в пороге
        // заметить сразу, а не через неделю работы.
        //
        // Заливка — непрозрачный Theme.accentSurface, а не транслюцентная
        // accentTint: карточный тонированный блок не должен зависеть от альфы,
        // которая на некоторых Android-сборках терялась при композитинге
        // (см. HeroCard).
        Rectangle {
            color: Theme.accentSurface
            height: previewColumn.implicitHeight + Spacing.itemGap * 2
            radius: Theme.radiusMd
            width: parent.width

            Column {
                id: previewColumn

                anchors.left: parent.left
                anchors.leftMargin: Spacing.itemGap
                anchors.right: parent.right
                anchors.rightMargin: Spacing.itemGap
                anchors.verticalCenter: parent.verticalCenter
                spacing: 4

                Label {
                    color: Theme.textMuted
                    font: Typography.overline
                    text: qsTr("Так это будет выглядеть")
                    width: parent.width
                }
                Label {
                    color: Theme.textPrimary
                    font: Typography.bodyStrong
                    text: page.previewText()
                    width: parent.width
                    wrapMode: Text.WordWrap
                }
                Label {
                    color: Theme.textSecondary
                    font: Typography.caption
                    text: page.periodicityText()
                    // У рекомендации периодичности нет: она не измеряется, и
                    // «каждую неделю» рядом с текстом совета обещало бы отметки,
                    // которых не будет.
                    visible: page.adviceMode === false
                    width: parent.width
                    wrapMode: Text.WordWrap
                }
            }
        }
        // «Отмена» рядом с добавлением: отказ — такое же решение, как добавление,
        // и уходить за ним в «Назад» в шапке пришлось бы через всю форму.
        RowLayout {
            spacing: Spacing.itemGap
            width: parent.width

            AppButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                onClicked: page.closeCustomForm()
                text: qsTr("Отмена")
                variant: AppButton.Secondary
            }
            AppButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                onClicked: page.addCustomRule()
                text: page.adviceMode ? qsTr("Добавить рекомендацию") : qsTr("Добавить правило")
                variant: AppButton.Primary
            }
        }
    }

    // — Выбранные правила —
    // Блок «В программе» стоит в самом низу, а не сразу после фильтра: пока
    // пользователь выбирает, он смотрит на каталог, а не на результат. Внизу же
    // результат оказывается рядом с «Готово» — и заодно с кнопкой возврата в
    // редактор, где список правил тоже виден.
    Column {
        id: chosenZone

        spacing: Spacing.itemGap
        visible: page.customFormOpen === false && page.hasViewModel
        width: parent.width

        Row {
            spacing: Spacing.itemGap

            Label {
                color: Theme.textMuted
                font: Typography.sectionTitle
                text: qsTr("В программе")
            }
            // Число правил рядом с заголовком: пока идёт выбор, это единственный
            // ответ на «сколько я уже взял» — таблица перед глазами занята
            // каталогом.
            Label {
                anchors.verticalCenter: parent.verticalCenter
                color: Theme.textMuted
                font: Typography.caption
                text: String(page.chosenCount)
            }
        }
        Repeater {
            // Список выбранного повторяет правила редактора: правило, добавленное
            // здесь, обязано быть видно и там же, иначе его нечем было бы исправить.
            model: page.hasViewModel ? page.viewModel.rules : []

            // Название и категория вместо `displayText`: подпись цели собирается в
            // машинных единицах («≥ 140 g»), а правило узнаётся по названию —
            // «Белок 140 г», и категория рядом говорит, о чём оно.
            delegate: AppCard {
                id: chosenRule

                required property string category
                required property int index
                required property string title

                borderColor: Theme.borderSubtle
                width: parent.width

                RowLayout {
                    width: parent.width
                    spacing: Spacing.itemGap

                    ColumnLayout {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        spacing: 2

                        Label {
                            Layout.fillWidth: true
                            color: Theme.textPrimary
                            font: Typography.bodyStrong
                            text: chosenRule.title
                            wrapMode: Text.WordWrap
                        }
                        Row {
                            spacing: 5

                            Rectangle {
                                anchors.verticalCenter: parent.verticalCenter
                                color: page.categoryColor(chosenRule.category)
                                height: 8
                                radius: 4
                                width: 8
                            }
                            Label {
                                color: Theme.textMuted
                                font: Typography.caption
                                text: page.categoryTitle(chosenRule.category)
                            }
                        }
                    }
                    AppButton {
                        Layout.minimumWidth: 0
                        onClicked: page.viewModel.removeRule(chosenRule.index)
                        padding: Spacing.itemGap
                        text: qsTr("Убрать")
                        variant: AppButton.Secondary
                    }
                }
            }
        }
        // Правила необязательны (canCreate их не требует), поэтому пустота здесь —
        // норма, а не ошибка: подсказка объясняет, что дают правила.
        AppCard {
            width: parent.width
            visible: page.chosenCount === 0

            ColumnLayout {
                width: parent.width
                spacing: Spacing.itemGap

                Label {
                    Layout.fillWidth: true
                    color: Theme.textSecondary
                    font: Typography.body
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Программа будет работать и без правил")
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    color: Theme.textMuted
                    font: Typography.caption
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("Правила добавляют ежедневные проверки: питание, сон, шаги — и отмечаются на экране «Сегодня»")
                    wrapMode: Text.WordWrap
                }
            }
        }
    }
    // Возврат в редактор закреплён внизу: это главное действие экрана, а не
    // «создать программу», и искать его после длинного каталога незачем.
    footer: AppButton {
        Layout.fillWidth: true
        Layout.minimumWidth: 0
        onClicked: page.backRequested()
        text: page.footerText()
        variant: AppButton.Primary
    }

    function footerText() {
        const count = page.chosenCount;

        return count > 0 ? qsTr("Готово · %1 правил").arg(count) : qsTr("Готово");
    }

    function errorText() {
        const texts = {
            rule_preset_already_added: qsTr("Это правило уже добавлено"),
            rule_category_invalid: qsTr("Не удалось применить фильтр"),
            rule_preset_not_found: qsTr("Правило не найдено в каталоге"),
            rule_preset_invalid: qsTr("Правило из каталога повреждено"),
            rule_metric_unknown: qsTr("Выберите, что измеряем"),
            rule_unit_mismatch: qsTr("Единица не подходит этой метрике"),
            rule_target_kind_invalid: qsTr("Такая цель здесь не подходит"),
            rule_target_range_invalid: qsTr("Минимум больше максимума"),
            rule_target_required: qsTr("Укажите цель правила"),
            rule_title_required: qsTr("Введите название правила")
        };

        return texts[page.errorKey] !== undefined ? texts[page.errorKey] : qsTr("Не удалось добавить правило");
    }

    readonly property string errorKey: page.viewModel !== undefined && page.viewModel !== null ? page.viewModel.errorKey : ""
}
