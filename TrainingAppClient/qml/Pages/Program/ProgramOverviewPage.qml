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
// Обзор программы (экран 41, §5.1) — один экран с сегментами вместо четырёх
// равнозначных кнопок. Данные — из настоящей БД через Avm_Program.
//
// Шапка закреплена сверху: возврат, название и единственное действие
// «Изменить». Ниже сегменты, а содержимое сегмента грузится отдельно —
// «Состав» (расписание недели), «Прогресс» и «Правила». Встроенные страницы
// статистики и правил не рисуют собственную шапку: её держит обзор.
Item {
    id: page

    readonly property var weekdayNames: [qsTr("Пн"), qsTr("Вт"), qsTr("Ср"), qsTr("Чт"), qsTr("Пт"), qsTr("Сб"), qsTr("Вс")]
    property var viewModel: ProgramVM
    // Программа для показа. Пусто — активная (вход из вкладок); каталог
    // передаёт идентификатор конкретной программы (срез S2).
    property string programId: ""
    // 0 — состав, 1 — прогресс, 2 — правила.
    property int segment: 0
    property real bottomInset: 0
    property real topInset: 0

    readonly property bool hasProgram: page.viewModel !== undefined && page.viewModel !== null && page.viewModel.hasProgram
    readonly property bool readOnly: page.hasProgram && page.viewModel.isReadOnly
    readonly property int weeksTotal: page.hasProgram ? page.viewModel.weeksTotal : 0

    signal activated
    signal backRequested
    signal createRequested
    signal diaryRequested
    signal editRequested(string programId)
    signal rulesRequested
    signal statsRequested

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

    // Идентификатор пользователя берётся из общего источника Session: раньше
    // «u1» повторялось в пяти местах QML, и другое значение делало программу
    // невидимой.
    function currentUserId() {
        return typeof AppContext !== "undefined" && AppContext !== null ? AppContext.currentUserId : "u1";
    }

    function reload() {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        page.viewModel.reload(page.currentUserId(), page.programId);
    }

    // Подпись шапки: состояние программы и её срок.
    function captionText() {
        if (!page.hasProgram)
            return qsTr("Активной программы пока нет");

        return (page.viewModel.state === "active" ? qsTr("Активная программа") : qsTr("Моя программа")) + (page.weeksTotal > 0 ? qsTr(" · %1 нед.").arg(page.weeksTotal) : "");
    }

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

    Rectangle {
        anchors.fill: parent
        color: Theme.background
    }

    // — Закреплённая шапка: возврат, название, «Изменить», сегменты —
    Column {
        id: chrome

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: page.topInset
        spacing: Spacing.itemGap

        BackBar {
            objectName: "programBackBar"
            width: parent.width
            text: qsTr("Назад")
            onClicked: page.backRequested()
        }
        RowLayout {
            anchors.left: parent.left
            anchors.leftMargin: Spacing.screenPadding
            anchors.right: parent.right
            anchors.rightMargin: Spacing.screenPadding
            spacing: Spacing.itemGap

            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                    font: Typography.largeTitle
                    objectName: "programOverviewTitle"
                    text: page.hasProgram ? page.viewModel.title : qsTr("Моя программа")
                }
                Label {
                    Layout.fillWidth: true
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                    font: Typography.caption
                    text: page.captionText()
                }
            }
            // Кнопка скрыта у завершённой программы: её структуру менять нельзя.
            AppButton {
                Layout.alignment: Qt.AlignVCenter
                compact: true
                objectName: "programEditAction"
                text: qsTr("Изменить")
                variant: AppButton.Secondary
                visible: page.hasProgram && page.readOnly === false

                onClicked: page.editRequested(page.programId)
            }
        }
        SegmentedControl {
            anchors.left: parent.left
            anchors.leftMargin: Spacing.screenPadding
            anchors.right: parent.right
            anchors.rightMargin: Spacing.screenPadding
            currentIndex: page.segment
            items: [qsTr("Состав"), qsTr("Прогресс"), qsTr("Правила")]
            objectName: "programSegments"

            onSelected: function (index) {
                page.segment = index;
            }
        }
    }

    // Содержимое сегмента грузится отдельно: в кадре всегда ровно одна
    // прокручиваемая страница, поэтому вложенных Flickable нет.
    Loader {
        id: body

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: chrome.bottom
        anchors.topMargin: Spacing.itemGap
        sourceComponent: page.segment === 1 ? statsComponent : page.segment === 2 ? rulesComponent : compositionComponent
    }

    // — Состав: расписание недели и переходы —
    Component {
        id: compositionComponent

        PageScaffold {
            bottomInset: page.bottomInset
            showHeader: false
            topInset: 0

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
    Row {
        // Ширина задаётся родителем: ряд лежит в обычной Column PageScaffold,
        // где Layout.fillWidth не действует.
        id: slotList

        width: parent.width
        spacing: Spacing.itemGap
        visible: page.hasProgram

        Repeater {
            model: page.hasProgram ? page.viewModel.week_slots : []

            delegate: DayTile {
                // Роли берутся через model.имя: обязательные свойства для
                // AList_Model не срабатывают, делегат не создаётся.
                // В обзоре расписание информационное; настройка дня — в редакторе.
                enabled: false
                height: heightCompact
                isActive: model.isToday
                isToday: model.isToday
                tileKind: model.kind
                weekdayName: model.dayOfWeek > 0 ? page.weekdayNames[model.dayOfWeek - 1] : ""
                // Ширина — от ширины страницы: ширина самого ряда на момент
                // создания делегата ещё не посчитана.
                width: widthFor(page.width)
            }
        }
    }
    // Легенда под рядом: без неё плитки читаются как пёстрая полоса —
    // чем отличаются тонированные дни и какой отмечен «сегодня» (макет 02).
    WeekLegend {
        width: parent.width
        visible: page.hasProgram
    }
    // — Переходы, оставшиеся после сегментов: дневник и активация —
    ColumnLayout {
        width: parent.width
        spacing: Spacing.listGap
        visible: page.hasProgram

        AppButton {
            Layout.fillWidth: true
            objectName: "programDiaryButton"
            onClicked: page.diaryRequested()
            text: qsTr("Дневник")
            variant: AppButton.Secondary
        }
        Label {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            color: Theme.textSecondary
            font: Typography.caption
            text: qsTr("Программа завершена — изменить её нельзя")
            visible: page.readOnly
            wrapMode: Text.WordWrap
        }
    }

    AppButton {
        width: parent.width
        visible: page.hasProgram && page.viewModel.state === "paused"
        text: qsTr("Сделать активной")
        variant: AppButton.Primary

        onClicked: {
            if (page.viewModel.activate(page.currentUserId())) {
                Demo.notify(qsTr("Программа активирована. Предыдущая программа поставлена на паузу."));
                page.activated();
            } else {
                Demo.notify(qsTr("Не удалось активировать программу"));
            }
        }
    }
        }
    }

    // — Прогресс и правила: те же страницы, что открываются из диагностики,
    //    но без собственной шапки —
    Component {
        id: statsComponent

        ProgramStatsPage {
            bottomInset: page.bottomInset
            embedded: true
            programId: page.programId
            topInset: 0
        }
    }
    Component {
        id: rulesComponent

        ProgramRulesPage {
            bottomInset: page.bottomInset
            embedded: true
            programId: page.programId
            topInset: 0
        }
    }
}
