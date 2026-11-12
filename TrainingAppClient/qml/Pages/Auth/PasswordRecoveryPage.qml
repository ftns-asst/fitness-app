import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

PageScaffold {
    id: page

    property var auth: AuthViewModel
    property alias email: controller.email
    readonly property alias step: controller.step
    readonly property alias busy: controller.busy
    signal authenticated
    signal closeRequested

    showHeader: false
    objectName: "passwordRecoveryPage"

    PasswordRecoveryController {
        id: controller
        auth: page.auth
        onAuthenticated: page.authenticated()
        onCloseRequested: page.closeRequested()
        onStepChanged: {
            page.pageFlickable.contentY = 0;
            stepAnimation.restart();
            Qt.callLater(function () {
                if (controller.step === "code")
                    codeField.focusInput();
                else if (controller.step === "password")
                    passwordField.focusInput();
            });
        }
    }
    BackHandler {
        enabled: page.visible
        priority: 40
        onBackPerformed: controller.back()
    }
    Column {
        width: Math.min(parent.width, 420)
        x: (parent.width - width) / 2
        spacing: Spacing.sectionGap

        BackBar {
            objectName: "recoveryBackButton"
            enabled: !(controller.busy && controller.step === "password")
            onClicked: controller.back()
        }
        Column {
            id: stepContent
            width: parent.width
            spacing: Spacing.listGap

            Label {
                text: qsTr("ВОССТАНОВЛЕНИЕ · %1 ИЗ 3").arg(controller.step === "email" ? 1 : controller.step === "code" ? 2 : 3)
                font: Typography.captionStrong
                color: Theme.textMuted
                width: parent.width
            }
            Label {
                text: controller.step === "email" ? qsTr("Забыли пароль?") : controller.step === "code" ? qsTr("Проверьте почту") : qsTr("Новый пароль")
                font: Typography.screenTitle
                color: Theme.textPrimary
                width: parent.width
                wrapMode: Text.WordWrap
            }
            Label {
                text: controller.step === "email" ? qsTr("Отправим код на email вашего аккаунта.") : controller.step === "code" ? qsTr("Введите 6 цифр из письма для %1. Код действует 10 минут.").arg(controller.email) : qsTr("Придумайте новый пароль. После сохранения вы сразу войдёте в аккаунт.")
                font: Typography.body
                color: Theme.textSecondary
                width: parent.width
                wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            }
            AppCard {
                width: parent.width
                enabled: !controller.busy

                AppTextField {
                    id: emailField
                    objectName: "recoveryEmailField"
                    visible: controller.step === "email"
                    width: parent.width
                    label: qsTr("Email")
                    placeholderText: "name@example.com"
                    maximumLength: 254
                    inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                    text: controller.email
                    onUserEdited: {
                        controller.email = text;
                        controller.errorText = "";
                    }
                    onAccepted: controller.submit()
                }
                AppTextField {
                    id: codeField
                    objectName: "recoveryCodeField"
                    visible: controller.step === "code"
                    width: parent.width
                    label: qsTr("Код из письма")
                    placeholderText: "000000"
                    maximumLength: 6
                    inputMethodHints: Qt.ImhDigitsOnly
                    validator: RegularExpressionValidator {
                        regularExpression: /[0-9]{0,6}/
                    }
                    text: controller.code
                    onUserEdited: {
                        controller.code = text;
                        controller.errorText = "";
                    }
                    onAccepted: controller.submit()
                }
                AppTextField {
                    id: passwordField
                    objectName: "recoveryPasswordField"
                    visible: controller.step === "password"
                    width: parent.width
                    label: qsTr("Новый пароль")
                    helperText: qsTr("8–64 символа: латинские буквы, цифры и специальный символ")
                    maximumLength: 64
                    echoMode: showPassword.checked ? TextInput.Normal : TextInput.Password
                    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    text: controller.password
                    onUserEdited: {
                        controller.password = text;
                        controller.errorText = "";
                    }
                    onAccepted: confirmationField.focusInput()
                }
                AppTextField {
                    id: confirmationField
                    objectName: "recoveryConfirmationField"
                    visible: controller.step === "password"
                    width: parent.width
                    label: qsTr("Повторите пароль")
                    maximumLength: 64
                    echoMode: showPassword.checked ? TextInput.Normal : TextInput.Password
                    inputMethodHints: Qt.ImhSensitiveData | Qt.ImhNoPredictiveText | Qt.ImhNoAutoUppercase
                    text: controller.confirmation
                    onUserEdited: {
                        controller.confirmation = text;
                        controller.errorText = "";
                    }
                    onAccepted: controller.submit()
                }
                CheckBox {
                    id: showPassword
                    visible: controller.step === "password"
                    text: qsTr("Показать пароль")
                    font: Typography.caption
                    height: Theme.touchMin
                    indicator: SelectionMark {
                        x: 0
                        anchors.verticalCenter: parent.verticalCenter
                        checked: showPassword.checked
                        pressed: showPassword.down
                    }
                    contentItem: Label {
                        text: showPassword.text
                        font: Typography.caption
                        color: Theme.textSecondary
                        leftPadding: showPassword.indicator.width + Spacing.itemGap
                        verticalAlignment: Text.AlignVCenter
                    }
                }
            }
            Label {
                objectName: "recoveryError"
                text: controller.errorText
                visible: text.length > 0
                width: parent.width
                font: Typography.captionStrong
                color: Theme.negative
                wrapMode: Text.WordWrap
                Accessible.role: Accessible.AlertMessage
            }
            AppButton {
                objectName: "recoveryResendButton"
                visible: controller.step === "code"
                width: parent.width
                variant: AppButton.Secondary
                enabled: !controller.busy && controller.resendSeconds === 0
                text: controller.resendSeconds > 0 ? qsTr("Отправить повторно через %1 с").arg(controller.resendSeconds) : qsTr("Отправить код повторно")
                onClicked: controller.resend()
            }
        }
    }
    footer: AppButton {
        objectName: "recoverySubmitButton"
        Layout.fillWidth: true
        enabled: !controller.busy
        text: controller.busy ? qsTr("Подождите…") : controller.step === "email" ? qsTr("Получить код") : controller.step === "code" ? qsTr("Подтвердить код") : qsTr("Сохранить пароль и войти")
        onClicked: controller.submit()
    }
    SequentialAnimation {
        id: stepAnimation
        PropertyAction {
            target: stepContent
            property: "opacity"
            value: 0
        }
        NumberAnimation {
            target: stepContent
            property: "opacity"
            to: 1
            duration: Theme.pageTransitionDuration
            easing.type: Easing.OutCubic
        }
    }
}
