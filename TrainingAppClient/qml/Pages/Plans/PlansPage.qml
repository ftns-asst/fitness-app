import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Экран «Планы» (§2.2): каталог программ с рейтингом,
// promo-карточка подбора и сортировка chips-рядом.
PageScaffold {
    id: page

    title: qsTr("Планы тренировок")
    caption: qsTr("Возьмите готовый шаблон как есть или скопируйте и измените под себя")

    property int sortIndex: 0

    readonly property var visiblePlans: {
        const items = MockCatalog.plans.slice();

        if (page.sortIndex === 0) {
            items.sort(function (a, b) {
                return b.ratingAvg - a.ratingAvg;
            });
        } else if (page.sortIndex === 1) {
            items.sort(function (a, b) {
                return b.reviewCount - a.reviewCount;
            });
        } else {
            items.sort(function (a, b) {
                return a.createdAt < b.createdAt ? 1 : -1;
            });
        }

        return items;
    }

    // — Promo-карточка подбора: короткий опрос — этап D3 —
    AppCard {
        width: parent.width
        surfaceColor: Theme.accentTint
        borderColor: "transparent"
        spacing: 0

        RowLayout {
            width: parent.width
            spacing: Spacing.listGap

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 4

                Label {
                    Layout.fillWidth: true
                    text: qsTr("Не знаете, что выбрать?")
                    font: Typography.bodyStrong
                    color: Theme.textPrimary
                    wrapMode: Text.WordWrap
                }

                Label {
                    Layout.fillWidth: true
                    text: qsTr("3 вопроса об опыте, днях и инвентаре")
                    font: Typography.caption
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
            }

            AppButton {
                variant: "primary"
                compact: true
                text: qsTr("Подобрать")

                onClicked: Demo.notify(qsTr("Опрос подбора плана — этап D3"))
            }
        }
    }

    // — Сортировка: рейтинг, популярность по отзывам, новые сверху —
    FilterChips {
        width: parent.width
        items: [qsTr("По рейтингу"), qsTr("Популярные"), qsTr("По времени")]
        currentIndex: page.sortIndex

        onSelected: function (index) {
            page.sortIndex = index;
        }
    }

    Column {
        width: parent.width
        spacing: Spacing.listGap

        Repeater {
            model: page.visiblePlans

            // Кликабельная зона — брат карточки: anchors внутри positioner недопустимы.
            // Высоту задаёт только implicitHeight карточки: явная привязка
            // height → planCard.height вместе с anchors.fill даёт цикл биндингов
            // и схлопывание карточки.
            delegate: Item {
                width: parent.width
                implicitHeight: planCard.implicitHeight

                AppCard {
                    id: planCard

                    anchors.fill: parent
                    spacing: Spacing.itemGap

                    RowLayout {
                        width: parent.width
                        spacing: Spacing.itemGap

                        Label {
                            Layout.fillWidth: true
                            text: modelData.title
                            font: Typography.bodyStrong
                            color: Theme.textPrimary
                            elide: Text.ElideRight
                        }

                        RatingBadge {
                            Layout.alignment: Qt.AlignTop
                            text: "★ " + modelData.ratingAvg.toFixed(1).replace(".", ",")
                        }
                    }

                    Label {
                        width: parent.width
                        text: modelData.subtitle
                        font: Typography.caption
                        color: Theme.textSecondary
                        wrapMode: Text.WordWrap
                    }

                    // Flow вместо Row: на узких экранах три pill не всегда влезают
                    Flow {
                        width: parent.width
                        spacing: Spacing.itemGap

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
