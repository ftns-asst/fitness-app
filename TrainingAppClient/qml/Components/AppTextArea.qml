import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Многострочное поле формы. Отдельный компонент, а не флаг у AppTextField:
// однострочное поле для рекомендации обрезало бы текст по горизонтали, и
// пользователь не видел бы, что написал. Геометрия, ошибка и helper-текст —
// те же, что у AppTextField, чтобы форма выглядела одинаково.
Column {
    id: area

    property string errorText: ""
    property string helperText: ""
    property string label: ""
    // Число видимых строк вместо высоты в пикселях: высота строки зависит от
    // шрифта и масштаба, а набор строк переносится одинаково при любом размере.
    property int lineCount: minLines

    property alias placeholderText: input.placeholderText
    property alias readOnly: input.readOnly
    property alias text: input.text

    signal edited

    function focusInput() {
        input.forceActiveFocus();
    }

    // Строки без прокрутки. Совет в одно предложение не должен превращать
    // форму в три пустые строки, поэтому берётся минимум, а не «сколько влезет».
    readonly property int minLines: 3
    // Потолок высоты: без него длинная рекомендация растянула бы форму на
    // весь экран и вытолкнула кнопку добавления за пределы видимого.
    readonly property int maxLines: 8
    readonly property real lineHeight: metrics.height + lineSpacing

    spacing: Spacing.itemGap
    width: parent ? parent.width : 280

    FontMetrics {
        id: metrics

        font: Typography.body
    }
    Label {
        color: Theme.textSecondary
        font: Typography.captionStrong
        text: area.label
        visible: text.length > 0
        width: parent.width
        wrapMode: Text.WordWrap
    }
    ScrollView {
        id: scroll

        // Высота задаётся явно: внутри Column Layout-свойства не работают, а
        // ScrollView без высоты схлопывается в ничего.
        height: Math.min(area.lineCount, area.maxLines) * area.lineHeight + 2 * Theme.borderWidth
        clip: true
        width: parent.width

        background: Rectangle {
            border.color: area.errorText.length > 0 ? Theme.negative : input.activeFocus ? Theme.accent : Theme.border
            border.width: area.errorText.length > 0 || input.activeFocus ? 2 : Theme.borderWidth
            color: Theme.surfaceMuted
            radius: Theme.radiusMd
        }

        // Ширина прокручиваемого содержимого задаётся явно. По умолчанию
        // ScrollView берёт её по самой длинной строке, и TextArea вылезал за
        // край карточки на широком экране (аудит ловил это на 480 px).
        contentWidth: width

        TextArea {
            id: input

            color: Theme.textPrimary
            font: Typography.body
            leftPadding: Spacing.cardPadding
            placeholderTextColor: Theme.textMuted
            readOnly: area.readOnly
            rightPadding: Spacing.cardPadding
            selectedTextColor: Theme.textPrimary
            selectionColor: Theme.accentSoft
            // Ширина равна ширине прокручиваемой области (contentWidth у ScrollView), а не
            // содержимому: иначе TextArea растягивается по самой длинной строке.
            width: Math.max(1, scroll.width - leftPadding - rightPadding - 2 * Theme.borderWidth)
            // Перенос по словам, а не по символам: текст рекомендации должен
            // читаться как абзац, а не распадаться посреди слова.
            wrapMode: TextEdit.WordWrap
            onTextChanged: area.edited()
        }
    }
    Label {
        color: area.errorText.length > 0 ? Theme.negative : Theme.textMuted
        font: Typography.caption
        text: area.errorText.length > 0 ? area.errorText : area.helperText
        visible: text.length > 0
        width: parent.width
        wrapMode: Text.WordWrap
    }

    // Межстрочный отступ: без него строки в TextArea слипаются визуально, и
    // абзац из трёх строк читается как одна длинная.
    readonly property int lineSpacing: 4
}
