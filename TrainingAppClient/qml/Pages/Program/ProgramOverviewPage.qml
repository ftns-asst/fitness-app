import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Обзор активной программы (экран 41, §5.1). Данные — из настоящей БД через
// Avm_Program, а не из mock: экран показывает то, что пользователь реально
// настроил (срок, цель, дни недели).
//
// Дни недели выводятся из week_slots: слот отдыха виден отдельно от
// тренировки, поэтому «день без тренировки» не прячется.
PageScaffold {
    id: page

    // Дни недели в порядке Пн…Вс — ISO 1…7 (§3.4).
    readonly property var weekdayNames: [qsTr("Пн"), qsTr("Вт"), qsTr("Ср"), qsTr("Чт"), qsTr("Пт"), qsTr("Сб"), qsTr("Вс")]
    property var viewModel: ProgramVM
    // Программа для показа. Пусто — активная (вход из вкладок); каталог
    // передаёт идентификатор конкретной программы (срез S2).
    property string programId: ""
    // Панель возврата обязательна: экраны программы открыты поверх вкладок, где
    // таб-бара нет.
    BackBar {
        Layout.fillWidth: true
        onClicked: page.backRequested()
        text: qsTr("Назад")
        visible: true
    }

    caption: page.hasProgram ? qsTr("Активная программа") : qsTr("Активной программы пока нет")
    title: qsTr("Моя программа")

    readonly property bool hasProgram: page.viewModel !== undefined && page.viewModel !== null && page.viewModel.hasProgram
    readonly property int weeksTotal: page.hasProgram ? page.viewModel.weeksTotal : 0

    // Загрузка идёт при создании и при каждом показе: у страницы, открытой
    // прямо в StackView (например, по --screen), visible не меняется — она
    // создаётся уже видимой, — поэтому onVisibleChanged не срабатывает и данные
    // остались бы незагруженными. onVisibleChanged оставлен для возврата на
    // экран: он нужен, чтобы обзор отражал сделанные правки.
    Component.onCompleted: page.reload()
    onVisibleChanged: {
        if (visible)
            page.reload();
    }

    // Идентификатор пользователя: до появления реальной авторизации в UI это
    // единственный источник «кто я», поэтому он вынесен в одно место.
    function currentUserId() {
        return "u1";
    }

    function reload() {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        page.viewModel.reload(page.currentUserId(), page.programId);
    }
    //MARK_BACK

    readonly property bool readOnly: page.hasProgram && page.viewModel.isReadOnly

    // — Состояние без программы: приглашение создать первую —
    AppCard {
        width: parent.width
        visible: page.hasProgram === false
        ColumnLayout {
            // Внутри AppCard ширину задаёт родитель, а не Layout: содержимое
            // карточки лежит в обычной Column, где Layout-свойства не работают.
            width: parent.width
            spacing: Spacing.itemGap

            Label {
                Layout.fillWidth: true
                color: Theme.textPrimary
                font: Typography.sectionTitle
                text: qsTr("Начните с программы")
                wrapMode: Text.WordWrap
            }
            Label {
                Layout.fillWidth: true
                color: Theme.textSecondary
                font: Typography.body
                text: qsTr("Создайте программу: выберите срок и цель, настройте каждый день недели и добавьте правила.")
                wrapMode: Text.WordWrap
            }
            AppButton {
                Layout.fillWidth: true
                onClicked: page.createRequested()
                text: qsTr("Создать программу")
                variant: AppButton.Primary
            }
        }
    }
    // — Карточка программы: название, цель, срок —
    AppCard {
        width: parent.width
        visible: page.hasProgram
        ColumnLayout {
            // Внутри AppCard ширину задаёт родитель, а не Layout: содержимое
            // карточки лежит в обычной Column, где Layout-свойства не работают.
            width: parent.width
            spacing: Spacing.itemGap

            Label {
                Layout.fillWidth: true
                color: Theme.textPrimary
                font: Typography.screenTitle
                text: page.viewModel.title
                wrapMode: Text.WordWrap
            }
            Flow {
                Layout.fillWidth: true
                spacing: Spacing.itemGap

                TagPill {
                    text: page.goalText()
                }
                TagPill {
                    text: qsTr("%1 нед.").arg(page.weeksTotal)
                }
                TagPill {
                    text: page.viewModel.startDate
                }
            }
        }
    }
    // — Расписание недели: семь дней, у каждого свой план —
    Label {
        color: Theme.textMuted
        font: Typography.sectionTitle
        text: qsTr("Расписание недели")
        visible: page.hasProgram
    }
    ColumnLayout {
        // Ширина задаётся родителем: список лежит в обычной Column
        // PageScaffold, где Layout.fillWidth не действует.
        id: slotList

        width: parent.width
        spacing: Spacing.listGap
        visible: page.hasProgram

        Repeater {
            model: page.hasProgram ? page.viewModel.week_slots : []

            delegate: AppCard {
                id: slotCard

                required property string dayTitle
                required property int dayOfWeek
                required property string kind
                required property string programDayId
                required property int rotationIndex

                Layout.fillWidth: true
                readonly property bool isRest: kind === "rest"
                // Слот ротации не привязан к конкретному дню недели.
                readonly property int weekday: dayOfWeek > 0 ? dayOfWeek : rotationIndex

                RowLayout {
                    width: parent.width
                    spacing: Spacing.itemGap

                    Rectangle {
                        Layout.preferredHeight: 34
                        Layout.preferredWidth: 34
                        color: slotCard.isRest ? Theme.surfaceMuted : Theme.accentSoft
                        radius: Theme.radiusSm

                        Label {
                            anchors.centerIn: parent
                            color: slotCard.isRest ? Theme.textSecondary : Theme.accent
                            font: Typography.captionStrong
                            text: slotCard.weekday > 0 ? page.weekdayNames[slotCard.weekday - 1] : ""
                        }
                    }
                    Label {
                        // Заголовок дня длиннее карточки, поэтому минимальную
                        // ширину снимаем — иначе он держал неявную ширину текста
                        // и выталкивал ряд за край карточки.
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        color: Theme.textPrimary
                        font: Typography.bodyStrong
                        text: slotCard.dayTitle.length > 0 ? slotCard.dayTitle : (slotCard.isRest ? qsTr("Отдых") : qsTr("Тренировка"))
                        wrapMode: Text.WordWrap
                    }
                }
            }
        }
    }
    // — Переходы к остальным экранам программы —
    ColumnLayout {
        // Ширина задаётся родителем: список лежит в обычной Column
        // PageScaffold, где Layout.fillWidth не действует.
        width: parent.width
        spacing: Spacing.listGap
        visible: page.hasProgram

        AppButton {
            Layout.fillWidth: true
            onClicked: page.rulesRequested()
            text: qsTr("Правила и отметки")
            variant: AppButton.Secondary
        }
        AppButton {
            Layout.fillWidth: true
            onClicked: page.statsRequested()
            text: qsTr("Статистика программы")
            variant: AppButton.Secondary
        }
        // Кнопка настройки скрыта у завершённой и архивной программы: её
        // структуру менять нельзя (§3.1), и предложение привело бы к отказу.
        AppButton {
            Layout.fillWidth: true
            onClicked: page.editRequested(page.programId)
            text: qsTr("Настроить программу")
            variant: AppButton.Secondary
            visible: page.readOnly === false
        }
        Label {
            Layout.fillWidth: true
            // Без снятия минимальной ширины длинный текст держал неявную
            // ширину и выталкивал ряд за край карточки.
            Layout.minimumWidth: 0
            color: Theme.textSecondary
            font: Typography.caption
            text: qsTr("Программа завершена — изменить её нельзя")
            visible: page.readOnly
            wrapMode: Text.WordWrap
        }
    }

    // Запрос возврата: экраны программы открыты поверх вкладок, где таб-бара нет.
    signal backRequested
    signal createRequested
    signal editRequested(string programId)
    signal rulesRequested
    signal statsRequested

    // Перевод цели enum в подпись: значения приходят строкой (§7).
    function goalText() {
        const goals = {
            strength: qsTr("Сила"),
            fatLoss: qsTr("Похудение"),
            muscle: qsTr("Масса"),
            maintain: qsTr("Поддержание")
        };
        const key = page.hasProgram ? page.viewModel.goal : "strength";

        return goals[key] !== undefined ? goals[key] : qsTr("Сила");
    }
}
