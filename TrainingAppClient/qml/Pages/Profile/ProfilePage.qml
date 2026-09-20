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
        if (activeLocalProfile === null)
            return auth.user.email;

        const goal_names = {
            strength: qsTr("Сила"),
            muscle: qsTr("Масса"),
            fitness: qsTr("Тонус")
        };
        return activeLocalProfile.age + " " + qsTr("лет") + " · " + activeLocalProfile.heightCm + " " + qsTr("см") + " · " + activeLocalProfile.weightKg + " " + qsTr("кг") + " · " + qsTr("цель: ") + goal_names[activeLocalProfile.goal];
    }
    readonly property bool authenticated: auth.authenticated
    readonly property bool avatarVisible: authenticated
    readonly property bool loginButtonVisible: !authenticated
    readonly property bool logoutButtonVisible: authenticated
    property int sectionIndex: 0

    signal loginRequested
    signal logoutRequested

    objectName: "profilePage"
    spacing: Spacing.listGap
    title: qsTr("Профиль")

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
    SegmentedControl {
        items: [qsTr("Прогресс"), qsTr("История"), qsTr("Параметры")]
        width: parent.width

        onSelected: function (index) {
            page.sectionIndex = index;
        }
    }

    // — Прогресс: KPI (§4.6), график, heatmap регулярности, рекорды —
    Column {
        spacing: Spacing.listGap
        visible: page.sectionIndex === 0
        width: parent.width

        Row {
            spacing: Spacing.itemGap
            width: parent.width

            Repeater {
                model: page.viewModel.kpi

                delegate: MetricCard {
                    deltaKind: modelData.deltaKind
                    deltaText: modelData.deltaText
                    highlight: index === 0
                    label: modelData.label
                    valueText: modelData.valueText
                    width: (parent.width - Spacing.itemGap * 2) / 3
                }
            }
        }
        LineChartCard {
            captionText: page.viewModel.chartCaption
            title: qsTr("Динамика рабочего веса, кг")
            values: page.viewModel.chartPoints
            width: parent.width
        }
        RegularityHeatmap {
            title: qsTr("Регулярность по неделям")
            weeks: page.viewModel.heatmapWeeks
            width: parent.width
        }
        Label {
            color: Theme.textSecondary
            font: Typography.captionStrong
            text: qsTr("Личные рекорды")
        }
        AppCard {
            paddingHorizontal: 0
            paddingVertical: Spacing.itemGap
            spacing: 0
            width: parent.width

            Repeater {
                model: page.viewModel.records

                delegate: RecordRow {
                    dateText: modelData.dateText
                    showDivider: index < page.viewModel.records.length - 1
                    title: modelData.title
                    valueText: modelData.valueText
                    width: parent.width
                }
            }
        }
    }

    // — История: фильтры по дате и упражнению появятся на этапе D4 —
    Column {
        spacing: Spacing.listGap
        visible: page.sectionIndex === 1
        width: parent.width

        AppCard {
            paddingHorizontal: 0
            paddingVertical: Spacing.itemGap
            spacing: 0
            width: parent.width

            Repeater {
                model: page.viewModel.workoutHistory

                delegate: ExerciseRow {
                    setsText: modelData.dateText + " · " + modelData.durationText + " · " + modelData.tonnageText
                    showDivider: index < page.viewModel.workoutHistory.length - 1
                    title: modelData.title
                    width: parent.width

                    onClicked: Demo.notify(qsTr("Детали тренировки — этап D4"))
                }
            }
        }
        Label {
            color: Theme.textMuted
            font: Typography.caption
            text: qsTr("Фильтры по периоду и упражнению — этап D4")
            width: parent.width
            wrapMode: Text.WordWrap
        }
    }

    // — Параметры —
    Column {
        spacing: Spacing.listGap
        visible: page.sectionIndex === 2
        width: parent.width

        AppCard {
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
            text: qsTr("Редактировать параметры")
            variant: AppButton.Secondary
            width: parent.width

            onClicked: Demo.notify(qsTr("Редактирование профиля — этап D1"))
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
