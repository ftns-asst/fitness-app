import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Поле формы с общей геометрией, ошибкой и helper-текстом. Сигналы отделяют
// пользовательское редактирование от программного заполнения формы.
Column {
    id: field

    property alias echoMode: input.echoMode
    property string errorText: ""
    property string helperText: ""
    property alias inputMethodHints: input.inputMethodHints
    property string label: ""
    property alias maximumLength: input.maximumLength
    property alias placeholderText: input.placeholderText
    property alias readOnly: input.readOnly
    property alias text: input.text
    property alias validator: input.validator

    signal accepted
    signal editingFinished
    signal userEdited

    function focusInput() {
        input.forceActiveFocus();
    }

    spacing: 6
    width: 280

    Label {
        color: Theme.textSecondary
        elide: Text.ElideRight
        font: Typography.captionStrong
        text: field.label
        visible: text.length > 0
        width: parent.width
    }
    AppInput {
        id: input

        activeFocusOnTab: true
        hoverEnabled: !Theme.touchPlatform
        height: Theme.buttonPrimary
        leftPadding: Spacing.cardPadding
        rightPadding: Spacing.cardPadding
        selectedTextColor: Theme.textPrimary
        selectionColor: Theme.accentSoft
        width: parent.width

        invalid: field.errorText.length > 0
        errorBorderWidth: 2
        focusBorderWidth: 2
        hoverBorderColor: Theme.textMuted
        borderAnimationDuration: Theme.pressFeedbackDuration

        onAccepted: field.accepted()
        onEditingFinished: field.editingFinished()
        onTextEdited: field.userEdited()
    }
    Label {
        color: field.errorText.length > 0 ? Theme.negative : Theme.textMuted
        font: Typography.caption
        text: field.errorText.length > 0 ? field.errorText : field.helperText
        visible: text.length > 0
        width: parent.width
        wrapMode: Text.WordWrap
    }
}
