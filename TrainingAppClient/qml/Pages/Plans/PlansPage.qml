import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Экран «Планы» (§2.2): каталог программ с рейтингом,
// promo-карточка подбора и сортировка chips-рядом.
PageScaffold {
    id: page

    required property var viewModel
    property int sortIndex: 0
    readonly property var visiblePlans: viewModel.sortedPlans(sortIndex)

    caption: qsTr("Возьмите готовый шаблон как есть или скопируйте и измените под себя")
    title: qsTr("Планы тренировок")

    // — Promo-карточка подбора: короткий опрос — этап D3 —
    AppCard {
        borderColor: "transparent"
        spacing: 0
        surfaceColor: Theme.accentTint
        width: parent.width

        RowLayout {
            spacing: Spacing.listGap
            width: parent.width

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    Layout.fillWidth: true
                    color: Theme.textPrimary
                    font: Typography.bodyStrong
                    text: qsTr("Не знаете, что выбрать?")
                    wrapMode: Text.WordWrap
                }
                Label {
                    Layout.fillWidth: true
                    color: Theme.textSecondary
                    font: Typography.caption
                    text: qsTr("3 вопроса об опыте, днях и инвентаре")
                    wrapMode: Text.WordWrap
                }
            }
            AppButton {
                compact: true
                text: qsTr("Подобрать")
                variant: AppButton.Primary

                onClicked: Demo.notify(qsTr("Опрос подбора плана — этап D3"))
            }
        }
    }

    // — Сортировка: рейтинг, популярность по отзывам, новые сверху —
    FilterChips {
        currentIndex: page.sortIndex
        items: [qsTr("По рейтингу"), qsTr("Популярные"), qsTr("По времени")]
        width: parent.width

        onSelected: function (index) {
            page.sortIndex = index;
        }
    }
    Column {
        spacing: Spacing.listGap
        width: parent.width

        Repeater {
            model: page.visiblePlans

            // Кликабельная зона — брат карточки: anchors внутри positioner недопустимы.
            // Высоту задаёт только implicitHeight карточки: явная привязка
            // height → planCard.height вместе с anchors.fill даёт цикл биндингов
            // и схлопывание карточки.
            delegate: Item {
                implicitHeight: planCard.implicitHeight
                width: parent.width

                AppCard {
                    id: planCard

                    anchors.fill: parent
                    spacing: Spacing.itemGap

                    RowLayout {
                        spacing: Spacing.itemGap
                        width: parent.width

                        Label {
                            Layout.fillWidth: true
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                            font: Typography.bodyStrong
                            text: modelData.title
                        }
                        RatingBadge {
                            Layout.alignment: Qt.AlignTop
                            text: "★ " + modelData.ratingAvg.toFixed(1).replace(".", ",")
                        }
                    }
                    Label {
                        color: Theme.textSecondary
                        font: Typography.caption
                        text: modelData.subtitle
                        width: parent.width
                        wrapMode: Text.WordWrap
                    }

                    // Flow вместо Row: на узких экранах три pill не всегда влезают
                    Flow {
                        spacing: Spacing.itemGap
                        width: parent.width

                        Repeater {
                            model: [modelData.goal, modelData.level, modelData.reviewText]

                            delegate: TagPill {
                                text: modelData
                            }
                        }
                    }
                }
                MouseArea {
                    anchors.fill: parent

                    onClicked: Demo.notify(qsTr("Карточка плана и «взять план» — этап D3"))
                }
            }
        }
    }
}
