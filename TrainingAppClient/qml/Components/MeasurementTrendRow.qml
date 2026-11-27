import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Динамика одного параметра тела (FR-T8, FR-T9): «Вес — 89,0 → 87,1 кг», ниже
// изменение со знаком, процентом и стрелкой, затем темп и прогноз на финиш.
// Незаданные числа приходят как null и не показываются: «нет данных» — не ноль.
ColumnLayout {
    id: row

    // Изменение в сторону цели: true / false; null — цели нет или динамика неизвестна.
    property var delta: null
    property var deltaPercent: null
    property var favorable: null
    property var forecastAtEnd: null
    property var latest: null
    property string param: ""
    property int points: 0
    property var ratePerWeek: null
    property var start: null
    property string trend: "unknown"
    readonly property var arrows: ({
            down: "↓",
            flat: "→",
            unknown: "",
            up: "↑"
        })
    // Цвет дополняет знак и стрелку, а не заменяет их: к цели — зелёный, от цели —
    // предупреждающий, цели нет — нейтральный.
    readonly property color deltaColor: row.has(row.favorable) ? (row.favorable ? Theme.positive : Theme.warning) : Theme.textSecondary
    readonly property string unitText: MeasurementTexts.unit(row.param)

    // Число задано? Пустое значение из C++ приходит в QML как undefined, а не null.
    function has(value) {
        return value !== null && value !== undefined;
    }

    function format(value, digits) {
        return Qt.locale().toString(Number(value), "f", digits);
    }

    // «+1,5» / «−2,2» (настоящий минус, не дефис); ноль без знака.
    function signed(value, digits) {
        const number = Number(value);
        const sign = number > 0 ? "+" : (number < 0 ? "−" : "");

        return sign + row.format(Math.abs(number), digits);
    }

    function changeText() {
        if (!row.has(row.delta))
            return "";

        let text = row.signed(row.delta, 1) + " " + row.unitText;

        if (row.has(row.deltaPercent))
            text += " (" + row.signed(row.deltaPercent, 1) + " %)";

        return text + " " + row.arrows[row.trend];
    }

    // Темп и прогноз одной строкой; метка «прогноз» отделяет оценку от факта.
    function outlookText() {
        const parts = [];

        if (row.has(row.ratePerWeek))
            parts.push(qsTr("%1 в неделю").arg(row.signed(row.ratePerWeek, 1) + " " + row.unitText));

        if (row.has(row.forecastAtEnd))
            parts.push(qsTr("прогноз к финишу: %1").arg(row.format(row.forecastAtEnd, 1) + " " + row.unitText));

        return parts.join(" · ");
    }

    // Один замер — это старт, а не динамика: показываем его как «первый замер».
    function valuesText() {
        if (!row.has(row.start))
            return "";

        if (row.points < 2 || !row.has(row.latest))
            return qsTr("%1 · первый замер").arg(row.format(row.start, 1) + " " + row.unitText);

        return row.format(row.start, 1) + " → " + row.format(row.latest, 1) + " " + row.unitText;
    }

    spacing: 2

    RowLayout {
        Layout.fillWidth: true
        spacing: Spacing.itemGap

        Label {
            Layout.fillWidth: true
            color: Theme.textPrimary
            elide: Text.ElideRight
            font: Typography.bodyStrong
            text: MeasurementTexts.title(row.param)
        }
        Label {
            color: Theme.textPrimary
            font: Typography.body
            text: row.valuesText()
        }
    }
    Label {
        Layout.fillWidth: true
        color: row.deltaColor
        font: Typography.captionStrong
        text: row.changeText()
        visible: text.length > 0
    }
    Label {
        Layout.fillWidth: true
        color: Theme.textMuted
        font: Typography.caption
        text: row.outlookText()
        visible: text.length > 0
        wrapMode: Text.WordWrap
    }
}
