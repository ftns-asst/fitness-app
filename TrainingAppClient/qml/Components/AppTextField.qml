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
    TextField {
        id: input

        activeFocusOnTab: true
        color: Theme.textPrimary
        font: Typography.body
        height: Theme.buttonPrimary
        leftPadding: Spacing.cardPadding
        placeholderTextColor: Theme.textMuted
        rightPadding: Spacing.cardPadding
        selectedTextColor: Theme.textPrimary
        selectionColor: Theme.accentSoft
        verticalAlignment: Text.AlignVCenter
        width: parent.width

        background: Rectangle {
            border.color: field.errorText.length > 0 ? Theme.negative : input.activeFocus ? Theme.accent : Theme.border
            border.width: field.errorText.length > 0 || input.activeFocus ? 2 : Theme.borderWidth
            color: Theme.surfaceMuted
            radius: Theme.radiusMd

            Behavior on border.color {
                ColorAnimation {
                    duration: 120
                }
            }
        }

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
