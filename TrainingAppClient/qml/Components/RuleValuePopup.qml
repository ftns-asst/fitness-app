import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Ввод числа для правила дня: «сколько съедено», «сколько выпито», «сколько
// спал». Поле принимает число с клавиатуры, «−/+» шагают по единице правила
// (50 ккал, 5 г, 100 мл…). «Сохранить» отправляет значение (состояние выводит
// модель из цели); «Выполнено — по цели» заполняет число целью одним касанием.
Popup {
    id: popup

    property real current: 0
    // Цель и шаг — с плитки, которая открыла окно.
    property real goal: 0
    property string ruleId: ""
    property real step: 1
    property string title: ""
    property string unitText: ""

    signal doneRequested(string ruleId)
    signal valueSaved(string ruleId, real value)

    // Открытие под конкретное правило: поле заполняется текущим значением.
    function openFor(id, name, unit, goalValue, currentValue, stepValue) {
        popup.ruleId = id;
        popup.title = name;
        popup.unitText = unit;
        popup.goal = goalValue;
        popup.step = stepValue;
        popup.current = currentValue >= 0 ? currentValue : 0;
        input.text = popup.current > 0 ? String(popup.current) : "";
        popup.open();
        input.forceActiveFocus();
        input.selectAll();
    }
    function parsed() {
        const number = Number(input.text.replace(",", "."));

        return isNaN(number) ? 0 : Math.max(0, number);
    }
    function shift(delta) {
        const next = Math.max(0, Math.round((popup.parsed() + delta) * 100) / 100);

        input.text = next > 0 ? String(next) : "0";
    }

    anchors.centerIn: parent
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    dim: true
    modal: true
    objectName: "ruleValuePopup"
    padding: Spacing.cardPadding
    width: parent !== null ? Math.min(parent.width - Spacing.screenPadding * 2, 420) : 320

    enter: Transition {
        NumberAnimation {
            duration: 200
            easing.type: Easing.OutCubic
            from: 0
            property: "opacity"
            to: 1
        }
        NumberAnimation {
            duration: 240
            easing.type: Easing.OutBack
            from: 0.92
            property: "scale"
            to: 1
        }
    }
    exit: Transition {
        NumberAnimation {
            duration: 120
            property: "opacity"
            to: 0
        }
    }
    background: Rectangle {
        border.color: Theme.border
        border.width: Theme.borderWidth
        color: Theme.surface
        radius: Theme.radiusLg
    }
    contentItem: ColumnLayout {
        spacing: Spacing.itemGap

        Label {
            Layout.fillWidth: true
            color: Theme.textPrimary
            font: Typography.sectionTitle
            text: popup.title
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            color: Theme.textMuted
            font: Typography.caption
            text: popup.goal > 0 ? qsTr("Цель: ") + popup.goal + (popup.unitText.length > 0 ? " " + popup.unitText : "") : ""
            visible: text.length > 0
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Spacing.itemGap

            ShiftButton {
                glyph: "\u2212"

                onActivated: popup.shift(-popup.step)
            }
            TextField {
                id: input

                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredHeight: Theme.buttonPrimary
                color: Theme.textPrimary
                font: Typography.sectionTitle
                horizontalAlignment: Text.AlignHCenter
                inputMethodHints: Qt.ImhFormattedNumbersOnly
                objectName: "ruleValueInput"
                placeholderText: "0"
                placeholderTextColor: Theme.textMuted

                background: Rectangle {
                    border.color: input.activeFocus ? Theme.accent : Theme.border
                    border.width: input.activeFocus ? 2 : Theme.borderWidth
                    color: Theme.surfaceMuted
                    radius: Theme.radiusMd
                }
                validator: DoubleValidator {
                    bottom: 0
                    decimals: 2
                    notation: DoubleValidator.StandardNotation
                    top: 100000
                }

                Keys.onReturnPressed: saveButton.clicked()
            }
            ShiftButton {
                glyph: "+"

                onActivated: popup.shift(popup.step)
            }
        }
        Label {
            Layout.alignment: Qt.AlignHCenter
            color: Theme.textSecondary
            font: Typography.body
            text: popup.unitText
            visible: text.length > 0
        }
        AppButton {
            id: saveButton

            Layout.fillWidth: true
            objectName: "ruleValueSave"
            text: qsTr("Сохранить")
            variant: AppButton.Primary

            onClicked: {
                popup.valueSaved(popup.ruleId, popup.parsed());
                popup.close();
            }
        }
        AppButton {
            Layout.fillWidth: true
            objectName: "ruleValueDone"
            text: qsTr("Выполнено — по цели")
            variant: AppButton.Secondary
            visible: popup.goal > 0

            onClicked: {
                popup.doneRequested(popup.ruleId);
                popup.close();
            }
        }
    }
}
