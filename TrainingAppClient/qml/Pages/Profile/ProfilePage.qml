import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Экран «Профиль» (§5.4): Прогресс / История / Параметры (сегментированный контрол).
PageScaffold {
    id: page

    property var auth: AuthViewModel
    required property var viewModel
    readonly property string activeInitials: {
        const words = activeName.trim().split(/\s+/);

        if (words.length === 0 || words[0].length === 0)
            return "?";
        if (words.length === 1)
            return words[0].substring(0, Math.min(2, words[0].length)).toUpperCase();
        return String(words[0].charAt(0) + words[1].charAt(0)).toUpperCase();
    }
    readonly property var activeLocalProfile: authenticated ? auth.localProfile : null
    readonly property string activeName: authenticated ? auth.user.name : qsTr("Гость")
    readonly property string activeProfileDetails: {
        if (!authenticated)
            return qsTr("Войдите, чтобы сохранять прогресс, рекорды и историю");
        const profile = viewModel.profile || {};
        const parts = [];
        if (profile.age !== undefined)
            parts.push(qsTr("%1 лет").arg(profile.age));
        if (profile.heightCm !== undefined)
            parts.push(qsTr("%1 см").arg(profile.heightCm));
        if (profile.weightKg !== undefined)
            parts.push(qsTr("%1 кг").arg(profile.weightKg));
        return parts.length > 0 ? parts.join(" · ") : auth.user.email;
    }
    readonly property bool authenticated: auth.authenticated
    readonly property bool avatarVisible: authenticated
    readonly property bool loginButtonVisible: !authenticated
    readonly property bool logoutButtonVisible: authenticated

    signal loginRequested
    signal logoutRequested

    objectName: "profilePage"
    spacing: Spacing.listGap
    title: qsTr("Профиль")

    function reloadProfile() {
        if (viewModel && typeof viewModel.reload === "function")
            viewModel.reload(authenticated ? String(auth.user.id) : "");
    }
    function profileStatusText() {
        if (viewModel.pendingSync) {
            if (viewModel.syncing)
                return qsTr("Сохранено на устройстве · отправляем на сервер…");
            if (viewModel.syncErrorText)
                return qsTr("Сохранено на устройстве. %1").arg(viewModel.syncErrorText);
            return qsTr("Сохранено на устройстве · ожидает отправки");
        }
        if (viewModel.loading)
            return qsTr("Обновляем профиль…");
        return viewModel.errorText || (viewModel.fromCache ? qsTr("Сохранённые данные профиля") : "");
    }
    Component.onCompleted: reloadProfile()
    onAuthenticatedChanged: {
        reloadProfile();
        if (!authenticated && profileEditor)
            profileEditor.close();
    }
    Connections {
        target: page.auth
        function onUserChanged() {
            page.reloadProfile();
            if (!page.authenticated)
                profileEditor.close();
        }
    }
    ProfileEditorPopup {
        id: profileEditor
        viewModel: page.viewModel
    }

    // Профиль открывается поверх вкладок (аватар в шапке), поэтому возврат —
    // кнопкой «Назад» и системным жестом, а не переключением вкладки.
    signal backRequested

    BackBar {
        objectName: "profileBackBar"
        width: parent.width
        text: qsTr("Назад")
        onClicked: page.backRequested()
    }

    // — Шапка: avatar с инициалами либо кнопка «Войти» для гостя (§4.9) —
    RowLayout {
        id: profileHeader

        objectName: "profileHeader"
        spacing: Spacing.listGap
        width: parent.width

        Rectangle {
            id: avatar

            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: 56
            Layout.preferredWidth: 56
            color: Theme.accentTint
            objectName: "profileAvatar"
            radius: width / 2
            visible: page.avatarVisible

            Label {
                anchors.centerIn: parent
                color: Theme.accent
                font: Typography.sectionTitle
                text: page.activeInitials
            }
        }

        // Гость: аватар заменён кнопкой входа — она открывает экран «Вход»
        // и остаётся единственной точкой входа в аккаунт из «Профиля».
        AppButton {
            id: loginButton

            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: 112
            compact: true
            objectName: "profileLoginButton"
            text: qsTr("Войти")
            variant: AppButton.Primary
            visible: page.loginButtonVisible

            onClicked: page.loginRequested()
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: 2

            Label {
                Layout.fillWidth: true
                color: Theme.textPrimary
                elide: Text.ElideRight
                font: Typography.sectionTitle
                text: page.activeName
            }
            Label {
                Layout.fillWidth: true
                color: Theme.textSecondary
                font: Typography.caption
                text: page.activeProfileDetails
                wrapMode: Text.WordWrap
            }
        }
    }

    // — Настройки: параметры, оформление, аккаунт —
    Column {
        spacing: Spacing.listGap
        width: parent.width

        // Оформление — первым блоком параметров: акцент и тема это настройка
        // пользователя, а не инструмент дизайн-ревью (§4.11, issue 32).
        AppearanceCard {
            // objectName задан самим компонентом («appearanceCard»): переопределение
            // стирало его контракт, и блок оформления становился ненаходимым.
            width: parent.width
        }
        AppCard {
            visible: page.authenticated
            paddingHorizontal: Spacing.screenPadding
            paddingVertical: Spacing.itemGap
            spacing: 0
            width: parent.width

            Repeater {
                model: page.viewModel.profileParams

                delegate: Item {
                    height: Theme.touchMin
                    width: parent.width

                    Label {
                        id: valueLabel

                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                        font: Typography.bodyStrong
                        horizontalAlignment: Text.AlignRight
                        text: modelData.value
                        width: Math.min(implicitWidth, parent.width * 0.5)
                    }
                    Label {
                        anchors.left: parent.left
                        anchors.right: valueLabel.left
                        anchors.rightMargin: Spacing.itemGap
                        anchors.verticalCenter: parent.verticalCenter
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                        font: Typography.body
                        text: modelData.label
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        color: Theme.border
                        height: Theme.borderWidth
                        visible: index < page.viewModel.profileParams.length - 1
                    }
                }
            }
        }
        AppButton {
            objectName: "profileEditButton"
            text: qsTr("Редактировать параметры")
            variant: AppButton.Secondary
            width: parent.width

            visible: page.authenticated && page.viewModel.canEdit !== false
            enabled: !page.viewModel.saving
            onClicked: profileEditor.openForProfile()
        }
        Label {
            width: parent.width
            visible: page.authenticated && text.length > 0
            text: page.profileStatusText()
            font: Typography.caption
            color: page.viewModel.errorText || page.viewModel.syncBlocked ? Theme.negative : Theme.textSecondary
            wrapMode: Text.WordWrap
        }
        AppButton {
            visible: page.authenticated && Boolean(page.viewModel.errorText || page.viewModel.syncErrorText)
            text: page.viewModel.syncErrorText ? qsTr("Повторить отправку") : qsTr("Повторить загрузку")
            variant: AppButton.Ghost
            enabled: !page.viewModel.loading && !page.viewModel.saving && !page.viewModel.syncing
            width: parent.width
            onClicked: page.viewModel.syncErrorText ? page.viewModel.retrySync() : page.reloadProfile()
        }
        AppButton {
            id: logoutButton

            objectName: "profileLogoutButton"
            text: qsTr("Выйти из аккаунта")
            variant: AppButton.Ghost
            visible: page.logoutButtonVisible
            width: parent.width

            onClicked: page.logoutRequested()
        }
    }
}
