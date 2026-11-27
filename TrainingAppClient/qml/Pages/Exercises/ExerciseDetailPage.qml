import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

PageScaffold {
    id: page

    property var viewModel: typeof ExercisesOneVM !== "undefined" ? ExercisesOneVM : null
    readonly property bool hasViewModel: page.viewModel !== undefined && page.viewModel !== null
    readonly property bool failed: page.hasViewModel && page.viewModel.errorKey !== ""
    readonly property bool hasId: page.exerciseId.length > 0
    // Показ карточки: данные есть (кэш или сервер) и ошибки нет.
    readonly property bool showCard: page.hasViewModel && page.viewModel.hasExercise && page.failed === false
    readonly property bool hasHeroMedia: showCard && (!hasId || viewModel.exerciseId === exerciseId) && (viewModel.hasVideo === true || !!viewModel.photoUrl)
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
    heroExtendsToTop: hasHeroMedia
    heroOverlap: hasHeroMedia ? 2 * Spacing.itemGap : 0

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
    scrollHero: Item {
        id: hero
        objectName: "exerciseDetailHero"

        readonly property real scrollDistance: Math.max(0, page.pageFlickable.contentY - page.pageFlickable.originY + page.pageFlickable.topMargin)
        readonly property real heightWithControls: back.y + back.height + (heading.visible ? heading.implicitHeight + Spacing.itemGap : 0)
        clip: true
        implicitHeight: page.hasHeroMedia ? Math.max(Math.round(page.height * 0.5), heightWithControls) : heightWithControls

        Behavior on implicitHeight {
            NumberAnimation {
                duration: Theme.pageTransitionDuration
                easing.type: Easing.OutCubic
            }
        }

        ExerciseMediaPanel {
            id: media
            objectName: "exerciseDetailMedia"

            width: hero.width
            height: hero.height
            edgeToEdge: true
            fadeHeight: heading.height + 110
            inlineControlsInset: heading.height + heading.anchors.bottomMargin
            backdropOffset: Math.min(height * 0.25, hero.scrollDistance * 0.3)
            backdropOpacity: 1 - Math.min(1, hero.scrollDistance / Math.max(1, height)) * 0.65
            exerciseId: page.exerciseId
            pageActive: page.pageActive && hero.scrollDistance < hero.height
            topInset: page.topInset
            bottomInset: page.bottomInset
            viewModel: page.showCard ? page.viewModel : null
        }
        BackBar {
            id: back

            anchors.left: parent.left
            anchors.leftMargin: Spacing.screenPadding
            anchors.top: parent.top
            anchors.topMargin: page.hasHeroMedia ? page.topInset + Spacing.safeGap : Spacing.itemGap
            width: parent.width - 2 * Spacing.screenPadding
            text: qsTr("Каталог")
            onClicked: page.backRequested()
        }
        Column {
            id: heading
            anchors.bottom: parent.bottom
            anchors.bottomMargin: page.hasHeroMedia ? 4 * Spacing.itemGap : 0
            x: Spacing.screenPadding
            spacing: Spacing.listGap
            visible: page.showCard
            width: parent.width - 2 * x
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
    }
    collapsedHeroContent: Item {
        implicitHeight: compactHeading.implicitHeight

        RowLayout {
            id: compactHeading

            anchors.bottom: parent.bottom
            width: parent.width
            AppButton {
                text: qsTr("← Каталог")
                variant: AppButton.Secondary
                labelFont: Typography.detail
                onClicked: page.backRequested()
            }
            Label {
                Layout.fillWidth: true
                text: page.showCard ? page.viewModel.name : ""
                font: Typography.bodyStrong
                color: Theme.textPrimary
                elide: Text.ElideRight
            }
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

        // CTA референса 2089 — «График прогресса». Данных для графика нет, пока
        // не записаны тренировки (этап D2/D3): кнопка выключена и подпись
        // объясняет почему, вместо нажатия, которое ничего не делает.
        AppButton {
            Layout.fillWidth: true
            enabled: false
            text: qsTr("График прогресса")
            variant: AppButton.Primary
        }
        Label {
            Layout.fillWidth: true
            color: Theme.textMuted
            font: Typography.caption
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Появится после первой записанной тренировки")
            visible: page.showCard
            wrapMode: Text.WordWrap
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
