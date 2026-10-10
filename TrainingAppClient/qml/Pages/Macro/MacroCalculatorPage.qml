import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Экран «Рассчитать» КБЖУ (спринт S7, issue 50).
//
// Считает модель MacroVM по формулам домена; экран только спрашивает параметры и
// показывает результат. Формул здесь нет намеренно: их копия в разметке рано или
// поздно разошлась бы с доменной, и человек считал бы по устаревшей версии.
//
// Порядок режимов (поддержание / дефицит / профицит) приходит из модели: ключи
// совпадают со значениями цели программы, их принимает домен.
PageScaffold {
    id: page

    required property var viewModel

    caption: qsTr("Калории и макросы по вашим данным")
    title: qsTr("Рассчитать КБЖУ")

    // Панель возврата обязательна: экран открыт поверх вкладок, где таб-бара нет.
    signal backRequested

    BackBar {
        onClicked: page.backRequested()
        text: qsTr("Назад")
        visible: true
        width: parent.width
    }

    readonly property bool hasModel: page.viewModel !== undefined && page.viewModel !== null
    readonly property bool hasResult: page.hasModel && page.viewModel.hasResult

    // Значения полей формы. Отдельные свойства, а не чтение из модели: модель
    // обновляется при каждом расчёте, и привязка к ней обнуляла бы ввод посреди
    // редактирования.
    property double weightValue: 0
    property double heightValue: 0
    property string birthDate: ""
    property string activityLevel: "moderate"
    property int modeIndex: 0
    property bool isFemaleSelected: false

    Component.onCompleted: {
        if (page.hasModel) {
            page.viewModel.load(page.currentUserId());
            page.initFromModel();
        }
    }

    function currentUserId() {
        return AppContext !== undefined && AppContext !== null ? AppContext.currentUserId : "u1";
    }

    // Высота и вес подставляются из модели один раз. Дальше форма принадлежит
    // человеку: иначе пересчёт затирал бы правильный введённый вес замером.
    function initFromModel() {
        if (!page.hasModel || page.viewModel.hasWeight)
            return;

        if (page.viewModel.weightKg > 0)
            page.weightValue = page.viewModel.weightKg;

        if (page.viewModel.hasHeight)
            page.heightValue = page.viewModel.heightCm;

        if (page.viewModel.hasBirthDate)
            page.birthDate = page.viewModel.birthDate;

        const level = page.viewModel.activityLevel;

        if (level.length > 0)
            page.activityLevel = level;
    }

    readonly property var modes: ["maintain", "fatLoss", "muscle"]

    function modeCaption(index) {
        const captions = [qsTr("Поддержание"), qsTr("Дефицит 15 %"), qsTr("Профицит 10 %")];

        return index >= 0 && index < captions.length ? captions[index] : "";
    }

    function modeDescription(index) {
        const descriptions = [qsTr("Ешьте столько же, сколько тратите. Вес держится."), qsTr("Уменьшаем калорийность — темп похудения около 0,5 кг в неделю."), qsTr("Добавляем 10 % — набор массы с контролем жиров.")];

        return index >= 0 && index < descriptions.length ? descriptions[index] : "";
    }

    function activityCaptions() {
        const levels = page.hasModel ? page.viewModel.activityLevels() : [];
        const captions = [];

        for (let i = 0; i < levels.length; ++i)
            captions.push(page.hasModel ? page.viewModel.activityCaption(levels[i]) : levels[i]);

        return captions;
    }

    function activityIndexFor(level) {
        const levels = page.hasModel ? page.viewModel.activityLevels() : [];

        return levels.indexOf(level);
    }

    // Подпись ошибки. Ключи приходят из домена, и экран их переводит: модель не
    // знает про язык интерфейса.
    function errorText() {
        const texts = {
            macro_age_out_of_range: qsTr("Укажите дату рождения: расчёт верен с 14 лет."),
            macro_activity_required: qsTr("Выберите уровень активности."),
            macro_below_bmr: qsTr("Получилось меньше, чем основной обмен. Уточните параметры."),
            macro_height_out_of_range: qsTr("Проверьте рост: нужен от 100 до 250 см."),
            macro_no_space_for_carbs: qsTr("При таких параметрах белок и жиры не оставляют места углеводам."),
            macro_params_save_failed: qsTr("Не удалось сохранить параметры"),
            macro_weight_out_of_range: qsTr("Проверьте вес: нужен от 20 до 300 кг.")
        };
        const key = page.hasModel ? page.viewModel.errorKey : "";

        return key.length > 0 && texts[key] !== undefined ? texts[key] : "";
    }

    function caloriesText() {
        if (!page.hasResult)
            return "";

        return page.viewModel.caloriesKcal + " " + qsTr("ккал");
    }

    function macroText(key) {
        if (!page.hasResult)
            return "";

        const values = {
            carbs: page.viewModel.carbsG,
            fat: page.viewModel.fatG,
            protein: page.viewModel.proteinG
        };
        const value = values[key];

        return (value !== undefined ? value : 0) + " " + qsTr("г");
    }

    // Основной обмен и расход объясняют, откуда взялась калорийность, но не
    // отвечают на вопрос «сколько есть».
    function detailsText() {
        if (!page.hasResult)
            return "";

        return qsTr("Основной обмен %1 ккал · расход с учётом активности %2 ккал").arg(page.viewModel.bmrKcal).arg(page.viewModel.tdeeKcal);
    }

    // Оговорка обязательна: расчёт опирается на формулы для среднего человека, и
    // без неё число выглядит так же точно, как медицинский анализ.
    function disclaimerText() {
        if (!page.hasResult)
            return "";

        return qsTr("Расчёт ориентировочный: при болезнях, беременности или диете спросите врача.");
    }

    function calculate() {
        if (!page.hasModel)
            return;

        page.initFromModel();

        const mode = page.modeIndex >= 0 && page.modeIndex < page.modes.length ? page.modes[page.modeIndex] : "maintain";

        page.viewModel.calculate(page.currentUserId(), page.weightValue, page.heightValue, page.isFemaleSelected, page.birthDate, page.activityLevel, mode);
    }
    // — Форма параметров —
    AppCard {
        spacing: Spacing.itemGap
        width: parent.width

        Label {
            color: Theme.textMuted
            font: Typography.caption
            text: qsTr("Вес подставляется из последнего замера тела; укажите дату рождения, рост и активность.")
            width: parent.width
            wrapMode: Text.WordWrap
        }
        // Вес и рост вводятся ползунками, а не полями с «−/+»: шаг 0,1 кг на
        // диапазоне 0–300 кнопками набирается сотнями нажатий, и ввод становится
        // главным препятствием на пути к расчёту. Диапазоны и шаги совпадают с
        // прежними полями, чтобы значение, введённое в одном экране, совпадало с
        // тем же числом на другом.
        WeightRuler {
            // Ширина — явно по родителю: карточка держит детей в обычной Column,
            // где `Layout.fillWidth` не работает (см. PageScaffold.qml), и без
            // привязки линейка брала свой implicitWidth — сжатая полоса слева.
            width: parent.width
            // Диапазон 35–200 кг, а не 0–300: на 300 делениях шаг 0,1 отстоял
            // на 0,1 px, и человек физически не мог поставить вес — метки
            // сливались в полосу. Нижняя граница 35 — самый лёгкий человек,
            // для которого форма вообще имеет смысл.
            from: 35
            step: 0.1
            title: qsTr("Вес")
            to: 200
            unit: qsTr("кг")
            value: page.weightValue
            valueDecimals: 1

            onEdited: function (newValue) {
                page.weightValue = newValue;
            }
        }
        WeightRuler {
            width: parent.width
            // Рост 120–220 см: те же соображения, что и для веса. На 0–250
            // подписи стояли вчетверо реже и не давали ощущения масштаба.
            from: 120
            step: 1
            title: qsTr("Рост")
            to: 220
            unit: qsTr("см")
            value: page.heightValue
            valueDecimals: 0

            onEdited: function (newValue) {
                page.heightValue = newValue;
            }
        }
        DateField {
            label: qsTr("Дата рождения")
            value: page.birthDate
            width: parent.width

            onEdited: function (isoDate) {
                page.birthDate = isoDate;
            }
        }
        RowLayout {
            spacing: Spacing.itemGap
            width: parent.width

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                color: Theme.textSecondary
                elide: Text.ElideRight
                font: Typography.captionStrong
                text: qsTr("Пол")
            }
            AppButton {
                text: qsTr("Мужской")
                variant: page.isFemaleSelected === false ? AppButton.Primary : AppButton.Secondary

                onClicked: page.isFemaleSelected = false
            }
            AppButton {
                text: qsTr("Женский")
                variant: page.isFemaleSelected === true ? AppButton.Primary : AppButton.Secondary

                onClicked: page.isFemaleSelected = true
            }
        }
    }

    // — Уровень активности —
    Label {
        color: Theme.textSecondary
        font: Typography.captionStrong
        text: qsTr("Активность")
        width: parent.width
    }
    FilterChips {
        currentIndex: Math.max(0, page.activityIndexFor(page.activityLevel))
        items: page.activityCaptions()
        width: parent.width

        onSelected: function (index) {
            const levels = page.hasModel ? page.viewModel.activityLevels() : [];

            if (index >= 0 && index < levels.length)
                page.activityLevel = levels[index];
        }
    }
    Label {
        color: Theme.textMuted
        font: Typography.caption
        text: qsTr("Сколько вы двигаетесь в неделю, включая тренировки и бытовую активность.")
        width: parent.width
        wrapMode: Text.WordWrap
    }

    // — Режим —
    Label {
        color: Theme.textSecondary
        font: Typography.captionStrong
        text: qsTr("Режим")
        width: parent.width
    }
    FilterChips {
        currentIndex: page.modeIndex
        items: [page.modeCaption(0), page.modeCaption(1), page.modeCaption(2)]
        width: parent.width

        onSelected: function (index) {
            page.modeIndex = index;
        }
    }
    Label {
        color: Theme.textMuted
        font: Typography.caption
        text: page.modeDescription(page.modeIndex)
        width: parent.width
        wrapMode: Text.WordWrap
    }
    // — Результат —
    Label {
        color: Theme.textSecondary
        font: Typography.captionStrong
        text: qsTr("Результат")
        visible: page.hasResult
        width: parent.width
    }
    AppCard {
        visible: page.hasResult
        width: parent.width

        Label {
            color: Theme.textSecondary
            font: Typography.caption
            text: qsTr("Калорийность")
            width: parent.width
        }
        Label {
            color: Theme.accent
            font: Typography.metric
            text: page.caloriesText()
            width: parent.width
        }
        // Макросы тремя равными колонками: они сопоставимы между собой, и ряд
        // читается сразу, без сравнения значений по строкам.
        RowLayout {
            Layout.fillWidth: true
            spacing: Spacing.itemGap

            Repeater {
                model: [
                    {
                        caption: qsTr("Белки"),
                        key: "protein"
                    },
                    {
                        caption: qsTr("Жиры"),
                        key: "fat"
                    },
                    {
                        caption: qsTr("Углеводы"),
                        key: "carbs"
                    }
                ]

                delegate: ColumnLayout {
                    required property var model

                    Layout.fillWidth: true
                    spacing: 0

                    Label {
                        Layout.fillWidth: true
                        color: Theme.textMuted
                        font: Typography.caption
                        horizontalAlignment: Text.AlignHCenter
                        text: model.caption
                    }
                    Label {
                        Layout.fillWidth: true
                        color: Theme.textPrimary
                        font: Typography.bodyStrong
                        horizontalAlignment: Text.AlignHCenter
                        text: page.macroText(model.key)
                    }
                }
            }
        }
        Label {
            color: Theme.textMuted
            font: Typography.caption
            text: page.detailsText()
            visible: text.length > 0
            width: parent.width
            wrapMode: Text.WordWrap
        }
        Label {
            color: Theme.warning
            font: Typography.caption
            text: page.disclaimerText()
            visible: text.length > 0
            width: parent.width
            wrapMode: Text.WordWrap
        }
    }
    Label {
        color: Theme.negative
        font: Typography.caption
        text: page.errorText()
        visible: text.length > 0
        width: parent.width
        wrapMode: Text.WordWrap
    }

    // Расчёт в подвале: он длиннее прокрутки, и искать его внизу не нужно.
    footer: RowLayout {
        AppButton {
            Layout.fillWidth: true
            text: qsTr("Рассчитать")
            variant: AppButton.Primary

            onClicked: page.calculate()
        }
    }
}
