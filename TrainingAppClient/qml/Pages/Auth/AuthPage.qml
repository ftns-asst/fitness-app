import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Вход и регистрация на mock API (issue 2). Параметры тела сохраняются локально
// после signup и не входят в серверный auth DTO.
Rectangle {
    id: page

    // Внедряемый presentation-контракт. Тесты могут передать fake, production
    // использует singleton-адаптер; после issue 8 здесь будет Avm_Auth.
    property var auth: AuthViewModel
    property alias authRequestId: controller.authRequestId
    property alias authState: controller.state
    property real bottomInset: 0
    readonly property alias busy: controller.busy
    property alias checkedEmail: controller.checkedEmail

    // closable — экран открыт поверх каркаса из «Профиля» (гость): его можно
    // закрыть кнопкой «Назад» или системным жестом. При истечении сессии экран
    // обязателен и закрытию не подлежит.
    property bool closable: false
    property alias emailError: controller.emailError
    property alias emailFree: controller.emailFree
    property alias emailRequestId: controller.emailRequestId
    property alias emailTaken: controller.emailTaken
    property alias globalError: controller.globalError
    property string initialMode: "login"
    property alias loginEmail: loginEmailField.text
    readonly property bool loginMode: controller.mode === "login"
    property alias loginPassword: loginPasswordField.text
    property alias mode: controller.mode
    property alias passwordError: controller.passwordError
    property alias pendingLocalProfile: controller.pendingLocalProfile
    property alias pendingSignup: controller.pendingSignup
    property bool recoveryEnabled: false
    property alias signupAge: ageField.text
    property alias signupEmail: signupEmailField.text
    property alias signupGender: genderChoice.selectedValue
    property alias signupGoal: goalChoice.selectedValue
    property alias signupHeight: heightField.text
    property alias signupName: signupNameField.text
    property alias signupPassword: signupPasswordField.text
    property alias signupWeight: weightField.text
    property real topInset: 0

    signal authenticated

    // Закрытие экрана без входа: доступно при открытии из «Профиля».
    signal cancelRequested

    function checkEmailNow(forSubmit) {
        controller.checkEmailNow(signupEmail, forSubmit, registration());
        signupEmail = controller.signupEmail;
    }
    function isValidEmail(email) {
        return auth.isValidEmail(email);
    }
    function onSignupEmailEdited() {
        controller.signupEmailEdited(signupEmail);
    }
    function passwordErrorKey(password) {
        return auth.passwordErrorKey(password);
    }
    function resetMessages() {
        controller.resetMessages();
    }
    function registration() {
        return {
            age: signupAge,
            email: signupEmail,
            gender: signupGender,
            height: signupHeight,
            name: signupName,
            goal: signupGoal,
            password: signupPassword,
            weight: signupWeight
        };
    }
    function requestClose() {
        if (!closable || authState === "sessionExpired")
            return;

        cancelRequested();
    }
    function submitLogin() {
        controller.submitLogin(loginEmail, loginPassword);
        loginEmail = controller.normalizeEmail(loginEmail);
    }
    function submitRegistration() {
        controller.submitRegistration(registration());
        signupEmail = controller.signupEmail;
    }
    function submitSignupRequest() {
        controller.pendingRegistration = registration();
        controller.submitSignupRequest();
    }
    function switchMode(nextMode) {
        if (mode === nextMode)
            return;

        controller.switchMode(nextMode);

        if (mode === "login" && loginEmail.length === 0 && signupEmail.length > 0)
            loginEmail = signupEmail;
        if (mode === "signup" && signupEmail.length === 0 && loginEmail.length > 0)
            signupEmail = loginEmail;
    }

    color: Theme.background

    AuthFormController {
        id: controller

        auth: page.auth
        initialMode: page.initialMode

        onAuthenticated: page.authenticated()
    }

    // Экран входа опционален: «Назад» возвращает гостя к вкладкам. Кнопка
    // находится над Flickable (z: 2), чтобы область прокрутки не перехватывала
    // реальные pointer-события.
    Item {
        id: backButtonContainer

        anchors.left: parent.left
        anchors.leftMargin: Spacing.screenPadding
        anchors.top: parent.top
        anchors.topMargin: page.topInset + Spacing.itemGap
        height: Theme.touchMin
        objectName: "backAuthButtonContainer"
        visible: page.closable && page.authState !== "sessionExpired"
        width: 112
        z: 2

        AppButton {
            anchors.fill: parent
            compact: true
            objectName: "backAuthButton"
            text: qsTr("← Назад")
            variant: AppButton.Secondary

            onClicked: page.requestClose()
        }
    }

    // Системная «назад»: у гостя закрывает вход и возвращает вкладки.
    // Приоритет выше, чем у каркаса приложения (20 — панель, 10 — стек).
    BackHandler {
        enabled: page.closable && page.authState !== "sessionExpired"
        priority: 30

        onBackPerformed: page.requestClose()
    }
    Flickable {
        id: flick

        anchors.bottom: parent.bottom
        anchors.bottomMargin: page.bottomInset
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: page.topInset
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        contentHeight: contentColumn.implicitHeight + Spacing.sectionGap * 2
        contentWidth: width

        ScrollBar.vertical: ScrollBar {
            policy: Qt.platform.os === "android" || Qt.platform.os === "ios" ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded
        }

        Column {
            id: contentColumn

            // Верхний отступ учитывает кнопку закрытия: она висит в углу
            // Flickable и не должна перекрывать заголовок.
            readonly property real reservedTop: page.closable ? Theme.touchMin : 0

            anchors.horizontalCenter: parent.horizontalCenter
            bottomPadding: Spacing.sectionGap
            spacing: Spacing.listGap
            topPadding: Spacing.sectionGap + reservedTop
            width: Math.min(flick.width - Spacing.screenPadding * 2, 420)

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.accentTint
                height: 60
                radius: 20
                width: 60

                Label {
                    anchors.centerIn: parent
                    color: Theme.accent
                    font: Typography.metric
                    text: "Ф"
                }
            }
            Label {
                color: Theme.textPrimary
                font: Typography.screenTitle
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Фитнес-помощник")
                width: parent.width
            }
            Label {
                color: Theme.textSecondary
                font: Typography.caption
                horizontalAlignment: Text.AlignHCenter
                text: page.loginMode ? qsTr("Продолжайте тренироваться с сохранённым прогрессом") : qsTr("Создайте профиль и начните первую программу")
                width: parent.width
                wrapMode: Text.WordWrap
            }
            Item {
                height: Spacing.safeGap
                width: 1
            }
            SegmentedControl {
                currentIndex: page.loginMode ? 0 : 1
                items: [qsTr("Вход"), qsTr("Регистрация")]
                width: parent.width

                onSelected: function (index) {
                    page.switchMode(index === 0 ? "login" : "signup");
                }
            }
            AppCard {
                spacing: Spacing.listGap
                width: parent.width

                Column {
                    spacing: Spacing.listGap
                    visible: page.loginMode
                    width: parent.width

                    AppTextField {
                        id: loginEmailField

                        errorText: page.loginMode ? page.emailError : ""
                        inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                        label: qsTr("Email")
                        maximumLength: 254
                        objectName: "loginEmailField"
                        placeholderText: "name@example.com"
                        width: parent.width

                        onAccepted: loginPasswordField.focusInput()
                        onUserEdited: page.resetMessages()
                    }
                    AppTextField {
                        id: loginPasswordField

                        echoMode: TextInput.Password
                        errorText: page.loginMode ? page.passwordError : ""
                        label: qsTr("Пароль")
                        maximumLength: 64
                        objectName: "loginPasswordField"
                        placeholderText: qsTr("Введите пароль")
                        width: parent.width

                        onAccepted: page.submitLogin()
                        onUserEdited: page.resetMessages()
                    }
                    AppButton {
                        enabled: !page.busy
                        text: page.authState === "loggingIn" ? qsTr("Входим…") : qsTr("Войти")
                        width: parent.width

                        onClicked: page.submitLogin()
                    }
                    AppButton {
                        text: qsTr("Забыли пароль?")
                        variant: AppButton.Ghost
                        visible: page.recoveryEnabled
                        width: parent.width

                        onClicked: Demo.notify(qsTr("Восстановление пароля пока недоступно"))
                    }
                }
                Column {
                    spacing: Spacing.listGap
                    visible: !page.loginMode
                    width: parent.width

                    AppTextField {
                        id: signupEmailField

                        errorText: !page.loginMode ? page.emailError : ""
                        helperText: page.emailFree ? qsTr("Email свободен") : ""
                        inputMethodHints: Qt.ImhEmailCharactersOnly | Qt.ImhNoAutoUppercase
                        label: qsTr("Email")
                        maximumLength: 254
                        objectName: "signupEmailField"
                        placeholderText: "name@example.com"
                        width: parent.width

                        onAccepted: signupNameField.focusInput()
                        onEditingFinished: page.checkEmailNow(false)
                        onTextChanged: controller.signupEmail = text
                        onUserEdited: page.onSignupEmailEdited()
                    }
                    AppButton {
                        text: qsTr("Email уже занят — войти")
                        variant: AppButton.Ghost
                        visible: page.emailTaken
                        width: parent.width

                        onClicked: {
                            page.loginEmail = page.signupEmail;
                            page.switchMode("login");
                            loginPasswordField.focusInput();
                        }
                    }
                    AppTextField {
                        id: signupNameField

                        helperText: qsTr("Имя может совпадать с именами других пользователей")
                        label: qsTr("Имя")
                        maximumLength: 80
                        objectName: "signupNameField"
                        placeholderText: qsTr("Как к вам обращаться")
                        width: parent.width

                        onAccepted: signupPasswordField.focusInput()
                        onUserEdited: page.resetMessages()
                    }
                    AppTextField {
                        id: signupPasswordField

                        echoMode: showPassword.checked ? TextInput.Normal : TextInput.Password
                        errorText: !page.loginMode ? page.passwordError : ""
                        helperText: qsTr("Латинские буквы, цифры и специальный символ")
                        label: qsTr("Пароль")
                        maximumLength: 64
                        objectName: "signupPasswordField"
                        placeholderText: qsTr("Не менее 8 символов")
                        width: parent.width

                        onUserEdited: page.resetMessages()
                    }
                    CheckBox {
                        id: showPassword

                        font: Typography.caption
                        height: Theme.touchMin
                        spacing: Spacing.itemGap
                        text: qsTr("Показать пароль")

                        contentItem: Label {
                            color: Theme.textSecondary
                            font: Typography.caption
                            leftPadding: showPassword.indicator.width + showPassword.spacing
                            text: showPassword.text
                            verticalAlignment: Text.AlignVCenter
                        }
                        indicator: Rectangle {
                            anchors.verticalCenter: parent.verticalCenter
                            border.color: Theme.border
                            border.width: showPassword.checked ? 0 : Theme.borderWidth
                            color: showPassword.checked ? Theme.accent : Theme.surfaceMuted
                            height: 24
                            radius: 7
                            width: 24
                            x: 0

                            Label {
                                anchors.centerIn: parent
                                color: Theme.accentForeground
                                font: Typography.captionStrong
                                text: "✓"
                                visible: showPassword.checked
                            }
                        }
                    }
                    Rectangle {
                        color: Theme.border
                        height: Theme.borderWidth
                        width: parent.width
                    }
                    Label {
                        color: Theme.textPrimary
                        font: Typography.bodyStrong
                        text: qsTr("Параметры для старта")
                        width: parent.width
                    }
                    Label {
                        color: Theme.textMuted
                        font: Typography.caption
                        text: qsTr("Сохраняются локально и не отправляются при регистрации")
                        width: parent.width
                        wrapMode: Text.WordWrap
                    }
                    RowLayout {
                        spacing: Spacing.itemGap
                        width: parent.width

                        AppTextField {
                            id: ageField

                            Layout.fillWidth: true
                            Layout.minimumWidth: 72
                            Layout.preferredWidth: 100
                            inputMethodHints: Qt.ImhDigitsOnly
                            label: qsTr("Возраст")
                            maximumLength: 3
                            text: "27"

                            validator: IntValidator {
                                bottom: 14
                                top: 100
                            }
                        }
                        AppTextField {
                            id: heightField

                            Layout.fillWidth: true
                            Layout.minimumWidth: 72
                            Layout.preferredWidth: 100
                            inputMethodHints: Qt.ImhDigitsOnly
                            label: qsTr("Рост, см")
                            maximumLength: 3
                            text: "170"

                            validator: IntValidator {
                                bottom: 100
                                top: 250
                            }
                        }
                        AppTextField {
                            id: weightField

                            Layout.fillWidth: true
                            Layout.minimumWidth: 72
                            Layout.preferredWidth: 100
                            inputMethodHints: Qt.ImhDigitsOnly
                            label: qsTr("Вес, кг")
                            maximumLength: 3
                            text: "65"

                            validator: IntValidator {
                                bottom: 30
                                top: 300
                            }
                        }
                    }
                    FormChoice {
                        id: genderChoice

                        label: qsTr("Пол")
                        options: [
                            {
                                value: "male",
                                label: qsTr("Мужской")
                            },
                            {
                                value: "female",
                                label: qsTr("Женский")
                            }
                        ]
                        selectedValue: "male"
                        width: parent.width
                    }
                    FormChoice {
                        id: goalChoice

                        label: qsTr("Цель")
                        options: [
                            {
                                value: "strength",
                                label: qsTr("Сила")
                            },
                            {
                                value: "muscle",
                                label: qsTr("Масса")
                            },
                            {
                                value: "fitness",
                                label: qsTr("Тонус")
                            }
                        ]
                        selectedValue: "strength"
                        width: parent.width
                    }
                    AppButton {
                        enabled: !page.busy
                        text: page.authState === "signingUp" || page.authState === "checkingEmail" ? qsTr("Создаём профиль…") : qsTr("Создать аккаунт")
                        width: parent.width

                        onClicked: page.submitRegistration()
                    }
                }
                Rectangle {
                    color: Qt.rgba(Theme.negative.r, Theme.negative.g, Theme.negative.b, 0.10)
                    height: errorLabel.implicitHeight + Spacing.itemGap * 2
                    radius: Theme.radiusMd
                    visible: page.globalError.length > 0 || page.authState === "sessionExpired"
                    width: parent.width

                    Label {
                        id: errorLabel

                        anchors.fill: parent
                        anchors.margins: Spacing.itemGap
                        color: Theme.negative
                        font: Typography.captionStrong
                        text: page.globalError.length > 0 ? page.globalError : qsTr("Сессия истекла. Войдите снова")
                        verticalAlignment: Text.AlignVCenter
                        wrapMode: Text.WordWrap
                    }
                }
            }
            Label {
                color: Theme.textMuted
                font: Typography.caption
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Продолжая, вы соглашаетесь с обработкой данных профиля")
                width: parent.width
                wrapMode: Text.WordWrap
            }
        }
    }
}
