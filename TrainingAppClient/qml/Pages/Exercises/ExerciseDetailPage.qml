import QtQuick
import QtQuick.Controls
import QtQuick.Effects
import QtQuick.Layouts
import TrainingAppClient

// Детальная карточка упражнения (спринт U6, референс 2089).
//
// Приватное чтение GET /exercises/{id}: свои — с CTA «Редактировать»,
// чужое публичное детально не открывается (403 -> понятный текст отказа).
// Офлайн карточка читается из кэша с плашкой «сохранённое упражнение».
// Техническое фото и tiles рекордов референса приходят позже (этап M3
// и история тренировок D3) — на месте остаётся структура: теги, описание.
//
// ViewModel — контекстное свойство ExercisesOneVM; в qmltestrunner его нет,
// все обращения под guard'ом (образец — ExercisesPage).
PageScaffold {
    id: page

    readonly property var viewModel: typeof ExercisesOneVM !== "undefined" ? ExercisesOneVM : null
    readonly property bool hasViewModel: page.viewModel !== undefined && page.viewModel !== null
    readonly property bool failed: page.hasViewModel && page.viewModel.errorKey !== ""
    readonly property bool hasId: page.exerciseId.length > 0
    // Показ карточки: данные есть (кэш или сервер) и ошибки нет.
    readonly property bool showCard: page.hasViewModel && page.viewModel.hasExercise && page.failed === false

    // Акцентная строка над названием: первая группа мышц · первый инвентарь.
    readonly property string overlineText: {
        if (page.showCard === false)
            return "";
        const parts = [];
        const groups = page.viewModel.muscleGroups;
        const gear = page.viewModel.equipments;
        if (groups.length > 0)
            parts.push(ExerciseTexts.muscleGroupTitle(groups[0]));
        if (gear.length > 0)
            parts.push(ExerciseTexts.equipmentTitle(gear[0]).toLowerCase());
        return parts.join(" · ");
    }
    // Подписи тегов: все группы мышц, затем весь инвентарь.
    readonly property var tagTexts: {
        if (page.showCard === false)
            return [];
        const out = [];
        page.viewModel.muscleGroups.forEach(g => out.push(ExerciseTexts.muscleGroupTitle(g)));
        page.viewModel.equipments.forEach(e => out.push(ExerciseTexts.equipmentTitle(e)));
        return out;
    }

    // Идентификатор задаёт оболочка до пуша (AppShell.exerciseDetailId).
    property string exerciseId: ""

    signal backRequested
    signal editRequested(string exerciseId)

    // Название показывается под фото (референс 2089), шапка каркаса пуста.
    title: ""

    Component.onCompleted: {
        if (page.hasViewModel && page.hasId)
            page.viewModel.load(page.exerciseId);
    }
    onVisibleChanged: {
        // Возврат из редактора: имя и описание могли измениться.
        if (visible && page.hasViewModel && page.hasId)
            page.viewModel.load(page.exerciseId);
    }
    onExerciseIdChanged: {
        if (page.hasViewModel && page.hasId)
            page.viewModel.load(page.exerciseId);
    }

    BackBar {
        Layout.fillWidth: true
        text: qsTr("Каталог")

        onClicked: page.backRequested()
    }
    // — Фото техники (референс 2089): крупный скруглённый блок без карточки-
    // обёртки; локальное фото своего упражнения или плейсхолдер —
    Item {
        id: photoBlock

        height: 220
        visible: page.showCard
        width: parent.width

        Rectangle {
            id: photoMask

            anchors.fill: parent
            layer.enabled: true
            radius: Theme.radiusLg
            visible: false
        }
        Rectangle {
            anchors.fill: parent
            color: Theme.surfaceMuted
            radius: Theme.radiusLg
        }
        Image {
            anchors.fill: parent
            asynchronous: true
            fillMode: Image.PreserveAspectCrop
            layer.enabled: true
            source: page.showCard ? page.viewModel.photoUrl : ""
            visible: page.showCard && page.viewModel.photoUrl.length > 0

            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: photoMask
            }
        }
        Label {
            anchors.centerIn: parent
            color: Theme.textMuted
            font: Typography.caption
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Изображение техники")
            visible: page.showCard === false || page.viewModel.photoUrl.length === 0
            width: parent.width - Spacing.screenPadding
            wrapMode: Text.WordWrap
        }
    }
    // — Заголовок по референсу: акцентная строка «группа · инвентарь» и
    // крупное название под фото —
    Column {
        spacing: Spacing.listGap
        visible: page.showCard
        width: parent.width

        Label {
            color: Theme.accent
            font: Typography.overline
            text: page.overlineText
            visible: text.length > 0
            width: parent.width
            wrapMode: Text.WordWrap
        }
        Label {
            color: Theme.textPrimary
            font: Typography.screenTitle
            text: page.showCard ? page.viewModel.name : ""
            width: parent.width
            wrapMode: Text.WordWrap
        }
    }
    // — Офлайн: карточка из кэша — тихая подпись под фото —
    Label {
        color: Theme.textMuted
        font: Typography.caption
        text: qsTr("Показано сохранённое упражнение")
        visible: page.showCard && page.viewModel.fromCache
        width: parent.width
    }
    // — Теги (референс 2089): серые «таблетки», группы мышц и инвентарь —
    Flow {
        spacing: Spacing.listGap
        visible: page.showCard && repeaterTags.count > 0
        width: parent.width

        Repeater {
            id: repeaterTags

            model: page.tagTexts

            delegate: Rectangle {
                color: Theme.surfaceMuted
                height: Theme.touchMin * 0.6
                radius: height / 2
                width: tagLabel.implicitWidth + Spacing.itemGap * 2

                Label {
                    id: tagLabel

                    anchors.centerIn: parent
                    color: Theme.textSecondary
                    font: Typography.captionStrong
                    text: modelData
                }
            }
        }
    }
    // — Описание: обычным текстом, без карточки и подписи —
    Label {
        color: page.showCard && page.viewModel.description.length > 0 ? Theme.textPrimary : Theme.textMuted
        font: Typography.body
        lineHeight: 1.15
        text: page.showCard && page.viewModel.description.length > 0 ? page.viewModel.description : qsTr("Описание не заполнено")
        visible: page.showCard
        width: parent.width
        wrapMode: Text.WordWrap
    }
    // — Tiles рекорда (референс 2089): первая — tinted-accent, остальные —
    // серые; та же вёрстка, что в «Профиль · Прогресс» (MetricCard). Записи
    // подходов ещё не ведутся (этапы D2/D3): вместо выдуманных чисел — «—» —
    Row {
        spacing: Spacing.itemGap
        visible: page.showCard
        width: parent.width

        Repeater {
            model: [qsTr("Рекорд"), qsTr("Рабочий вес"), qsTr("Подходов")]

            delegate: MetricCard {
                highlight: index === 0
                label: modelData
                valueText: "—"
                width: (parent.width - Spacing.itemGap * 2) / 3
            }
        }
    }
    Label {
        color: Theme.textMuted
        font: Typography.caption
        text: qsTr("Рекорды появятся после первых тренировок")
        visible: page.showCard
        width: parent.width
        wrapMode: Text.WordWrap
    }
    // — Загрузка —
    AppCard {
        borderColor: "transparent"
        elevated: false
        surfaceColor: Theme.surfaceMuted
        visible: page.hasViewModel && page.hasId && page.viewModel.loading && page.showCard === false && page.failed === false
        width: parent.width

        Label {
            color: Theme.textMuted
            font: Typography.caption
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Загрузка…")
            width: parent.width
        }
    }
    // — Отказ: чужое упражнение (403), не найдено (404), прочие ошибки —
    AppCard {
        width: parent.width
        visible: page.failed && page.hasId

        Label {
            color: Theme.negative
            font: Typography.body
            text: page.failed && page.hasViewModel ? ApiErrorText.textFor(page.viewModel.errorKey) : ""
            width: parent.width
            wrapMode: Text.WordWrap
        }
    }
    // — Без данных: прямой запуск без идентификатора при пустом кэше —
    AppCard {
        borderColor: "transparent"
        elevated: false
        surfaceColor: Theme.surfaceMuted
        visible: page.hasViewModel && page.failed === false && page.showCard === false && page.viewModel.loading === false
        width: parent.width

        Label {
            color: Theme.textMuted
            font: Typography.caption
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Упражнение не выбрано")
            width: parent.width
        }
    }

    footer: ColumnLayout {
        spacing: Spacing.itemGap

        // CTA референса 2089 — «График прогресса». Экран графика — этап D3
        // (история тренировок): кнопка честно сообщает об этом вместо
        // молчаливого нажатия.
        AppButton {
            Layout.fillWidth: true
            enabled: page.showCard
            text: qsTr("График прогресса")
            variant: AppButton.Primary

            onClicked: Demo.notify(qsTr("График прогресса — этап D3"))
        }
        // Правка — только свои: чужое публичное 403-ится ещё и сервером, но
        // кнопку не показываем вовсе (docs/08, §4 U6).
        AppButton {
            Layout.fillWidth: true
            enabled: page.showCard && page.viewModel.isOwn
            text: qsTr("Редактировать")
            variant: AppButton.Secondary
            visible: page.showCard && page.viewModel.isOwn

            onClicked: page.editRequested(page.viewModel.exerciseId)
        }
    }
}
