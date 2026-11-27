import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

Item {
    id: control
    property string label: ""
    property string caption: ""
    property real from: 0
    property real to: 1000
    property real step: 1
    property int decimals: 0
    property bool showDivider: true
    property alias text: input.text
    readonly property real numberValue: text.trim().length > 0 ? Number(text.replace(",", ".")) : NaN
    readonly property bool valid: isFinite(numberValue) && numberValue >= from && numberValue <= to && (decimals > 0 || Number.isInteger(numberValue))
    implicitHeight: 76
    implicitWidth: 0

    function setValue(value) {
        input.text = String(Math.round(Number(value) * 100) / 100).replace(".", Qt.locale().decimalPoint);
    }
    function change(direction) {
        const current = valid ? numberValue : from;
        setValue(Math.min(to, Math.max(from, current + direction * step)));
    }
    RowLayout {
        anchors {
            fill: parent
            leftMargin: Spacing.cardPadding
            rightMargin: Spacing.cardPadding
        }
        spacing: 6
        Column {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 3
            Label {
                width: parent.width
                text: control.label
                font: Typography.bodyMedium
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }
            Label {
                width: parent.width
                text: control.caption
                visible: text.length > 0
                font: Typography.caption
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
        }
        StepButton {
            objectName: "valueDecrease"
            text: "−"
            enabled: !control.valid || control.numberValue > control.from
            Accessible.name: qsTr("Уменьшить: %1").arg(control.label)
            onClicked: control.change(-1)
        }
        AppInput {
            id: input
            objectName: "valueInput"
            Layout.preferredWidth: control.decimals > 0 ? 82 : 60
            Layout.minimumWidth: 48
            Layout.preferredHeight: Theme.touchMin
            text: "0"
            font: Typography.inputNumber
            color: Theme.textPrimary
            horizontalAlignment: Text.AlignHCenter
            selectByMouse: true
            leftPadding: 0
            rightPadding: 0
            onActiveFocusChanged: if (activeFocus)
                selectAll()
            inputMethodHints: control.decimals > 0 ? Qt.ImhFormattedNumbersOnly : Qt.ImhDigitsOnly
            Accessible.name: control.label
            validator: RegularExpressionValidator {
                regularExpression: control.decimals > 0 ? /^\d{0,4}([.,]\d{0,2})?$/ : /^\d{0,4}$/
            }
            background: Rectangle {
                radius: Theme.radiusSm
                color: input.activeFocus ? Theme.surfaceMuted : "transparent"
                border.width: input.activeFocus ? 1 : 0
                border.color: control.valid ? Theme.accent : Theme.negative
                Behavior on color {
                    ColorAnimation {
                        duration: Theme.pressFeedbackDuration
                    }
                }
            }
        }
        StepButton {
            objectName: "valueIncrease"
            text: "+"
            enabled: !control.valid || control.numberValue < control.to
            Accessible.name: qsTr("Увеличить: %1").arg(control.label)
            onClicked: control.change(1)
        }
    }
    Rectangle {
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            leftMargin: Spacing.cardPadding
            rightMargin: Spacing.cardPadding
        }
        height: 1
        visible: control.showDivider
        color: Theme.borderSubtle
    }
    component StepButton: AppButton {
        id: stepButton
        Layout.preferredWidth: Theme.touchMin
        Layout.minimumWidth: Theme.touchMin
        Layout.maximumWidth: Theme.touchMin
        Layout.preferredHeight: Theme.touchMin
        implicitWidth: Theme.touchMin
        variant: AppButton.Secondary
        labelFont: Typography.sectionTitle
        autoRepeat: true
        autoRepeatDelay: 400
        autoRepeatInterval: 110
        background: Rectangle {
            radius: width / 2
            border.width: 1
            border.color: Theme.border
            color: stepButton.down ? Theme.accentSurface : Theme.surfaceMuted
            opacity: stepButton.enabled ? 1 : 0.45
            Behavior on color {
                ColorAnimation {
                    duration: Theme.pressFeedbackDuration
                }
            }
        }
    }
}
