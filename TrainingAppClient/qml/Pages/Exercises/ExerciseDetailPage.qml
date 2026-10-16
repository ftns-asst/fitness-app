import QtQuick
import QtQuick.Controls
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

    // Идентификатор задаёт оболочка до пуша (AppShell.exerciseDetailId).
    property string exerciseId: ""

    signal backRequested
    signal editRequested(string exerciseId)

    title: page.showCard && page.viewModel.name.length > 0 ? page.viewModel.name : qsTr("Упражнение")
    caption: page.showCard && page.viewModel.fromCache ? qsTr("Показано сохранённое упражнение") : ""

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
    // — Фото техники (референс 2089): локальное фото своего упражнения
    // (загружается в редакторе) или плейсхолдер до первого фото —
    AppCard {
        visible: page.showCard
        width: parent.width

        Rectangle {
            color: Theme.surfaceMuted
            height: 200
            radius: Theme.radiusMd
            width: parent.width

            Image {
                // Локальный файл (схема v9): загруженное в редакторе фото
                // показывается сразу, без сетевой загрузки.
                anchors.fill: parent
                asynchronous: true
                fillMode: Image.PreserveAspectCrop
                source: page.showCard ? page.viewModel.photoUrl : ""
                visible: page.showCard && page.viewModel.photoUrl.length > 0
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
    }
    // — Теги (референс 2089): группы мышц — акцентные чипы, инвентарь —
    // светлые: различие видно с одного взгляда, без чтения подписи —
    AppCard {
        visible: page.showCard
        width: parent.width

        ColumnLayout {
            spacing: Spacing.itemGap
            width: parent.width

            Flow {
                spacing: Spacing.listGap
                width: parent.width

                Repeater {
                    model: page.showCard ? page.viewModel.muscleGroups : []

                    delegate: Rectangle {
                        // Группа мышц — акцентный тег референса: мягкая
                        // заливка акцентом (accentSoft), текст акцентом.
                        color: Theme.accentSoft
                        height: Theme.touchMin * 0.6
                        radius: Theme.radiusSm
                        width: groupTagLabel.implicitWidth + Spacing.listGap * 2

                        Label {
                            id: groupTagLabel

                            anchors.centerIn: parent
                            color: Theme.accent
                            font: Typography.caption
                            text: ExerciseTexts.muscleGroupTitle(modelData)
                        }
                    }
                }
                Repeater {
                    model: page.showCard ? page.viewModel.equipments : []

                    delegate: Rectangle {
                        color: Theme.surfaceMuted
                        height: Theme.touchMin * 0.6
                        radius: Theme.radiusSm
                        width: equipmentTagLabel.implicitWidth + Spacing.listGap * 2

                        Label {
                            id: equipmentTagLabel

                            anchors.centerIn: parent
                            color: Theme.textSecondary
                            font: Typography.caption
                            text: ExerciseTexts.equipmentTitle(modelData)
                        }
                    }
                }
            }
            // — Описание: необязательное поле, без него — приглушённый текст —
            Label {
                color: Theme.textSecondary
                font: Typography.captionStrong
                text: qsTr("Описание")
            }
            Label {
                Layout.fillWidth: true
                color: page.showCard && page.viewModel.description.length > 0 ? Theme.textPrimary : Theme.textMuted
                font: Typography.body
                text: page.showCard && page.viewModel.description.length > 0 ? page.viewModel.description : qsTr("Описание не заполнено")
                wrapMode: Text.WordWrap
            }
        }
    }
    // — Tiles рекорда (референс 2089): первая — tinted-accent, остальные —
    // серые; та же вёрстка, что в «Профиль · Прогресс» (MetricCard) —
    Row {
        spacing: Spacing.itemGap
        visible: page.showCard
        width: parent.width

        Repeater {
            // Рекорды читаются из истории тренировок (этапы D2/D3): записи
            // подходов ещё не ведутся, и вместо выдуманных чисел — честное
            // «нет данных» в вёрстке референса.
            model: [qsTr("Личный рекорд"), qsTr("Лучший сет"), qsTr("Тренировок")]

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
