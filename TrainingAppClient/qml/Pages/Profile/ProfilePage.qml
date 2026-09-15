import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Экран «Профиль» (§5.4): Прогресс / История / Параметры (сегментированный контрол).
PageScaffold {
    id: page

    title: qsTr("Профиль")
    spacing: Spacing.listGap

    property int sectionIndex: 0

    // — Шапка: avatar с инициалами и параметры (§4.9) —
    Row {
        width: parent.width
        spacing: Spacing.listGap

        Rectangle {
            width: 56
            height: 56
            radius: width / 2
            color: Theme.accentTint

            Label {
                anchors.centerIn: parent
                text: MockCatalog.profile.initials
                font: Typography.sectionTitle
                color: Theme.accent
            }
        }

        Column {
            // Ширина ограничена остатком строки: длинные параметры иначе
            // вылезали за правый край экрана.
            width: parent.width - 56 - Spacing.listGap
            spacing: 2

            Label {
                width: parent.width
                text: MockCatalog.profile.name
                font: Typography.sectionTitle
                color: Theme.textPrimary
                elide: Text.ElideRight
            }

            Label {
                width: parent.width
                text: MockCatalog.profile.details
                font: Typography.caption
                color: Theme.textSecondary
                wrapMode: Text.WordWrap
            }
        }
    }

    SegmentedControl {
        width: parent.width
        items: [qsTr("Прогресс"), qsTr("История"), qsTr("Параметры")]

        onSelected: function (index) {
            page.sectionIndex = index;
        }
    }

    // — Прогресс: KPI (§4.6), график, heatmap регулярности, рекорды —
    Column {
        width: parent.width
        spacing: Spacing.listGap
        visible: page.sectionIndex === 0

        Row {
            width: parent.width
            spacing: Spacing.itemGap

            Repeater {
                model: MockCatalog.kpi

                delegate: MetricCard {
                    width: (parent.width - Spacing.itemGap * 2) / 3
                    label: modelData.label
                    valueText: modelData.valueText
                    deltaText: modelData.deltaText
                    deltaKind: modelData.deltaKind
                    highlight: index === 0
                }
            }
        }

        LineChartCard {
            width: parent.width
            title: qsTr("Динамика рабочего веса, кг")
            captionText: MockCatalog.chartCaption
            values: MockCatalog.chartPoints
        }

        RegularityHeatmap {
            width: parent.width
            title: qsTr("Регулярность по неделям")
            weeks: MockCatalog.heatmapWeeks
        }

        Label {
            text: qsTr("Личные рекорды")
            font: Typography.captionStrong
            color: Theme.textSecondary
        }

        AppCard {
            width: parent.width
            paddingVertical: Spacing.itemGap
            paddingHorizontal: 0
            spacing: 0

            Repeater {
                model: MockCatalog.records

                delegate: RecordRow {
                    width: parent.width
                    title: modelData.title
                    valueText: modelData.valueText
                    dateText: modelData.dateText
                    showDivider: index < MockCatalog.records.length - 1
                }
            }
        }
    }

    // — История: фильтры по дате и упражнению появятся на этапе D4 —
    Column {
        width: parent.width
        spacing: Spacing.listGap
        visible: page.sectionIndex === 1

        AppCard {
            width: parent.width
            paddingVertical: Spacing.itemGap
            paddingHorizontal: 0
            spacing: 0

            Repeater {
                model: 3

                delegate: ExerciseRow {
                    width: parent.width
                    title: [qsTr("Тренировка A · Full body"), qsTr("Тренировка B · Спина"), qsTr("Тренировка A · Full body")][index]
                    setsText: [qsTr("12 сентября · 52 мин · 12,4 т"), qsTr("10 сентября · 48 мин · 9,8 т"), qsTr("8 сентября · 55 мин · 11,9 т")][index]
                    showDivider: index < 2

                    onClicked: Demo.notify(qsTr("Детали тренировки — этап D4"))
                }
            }
        }

        Label {
            width: parent.width
            text: qsTr("Фильтры по периоду и упражнению — этап D4")
            font: Typography.caption
            color: Theme.textMuted
            wrapMode: Text.WordWrap
        }
    }

    // — Параметры —
    Column {
        width: parent.width
        spacing: Spacing.listGap
        visible: page.sectionIndex === 2

        AppCard {
            width: parent.width
            paddingVertical: Spacing.itemGap
            paddingHorizontal: Spacing.screenPadding
            spacing: 0

            Repeater {
                model: MockCatalog.profileParams

                delegate: Item {
                    width: parent.width
                    height: Theme.touchMin

                    Label {
                        id: valueLabel

                        anchors.right: parent.right
                        anchors.verticalCenter: parent.verticalCenter
                        width: Math.min(implicitWidth, parent.width * 0.5)
                        text: modelData.value
                        font: Typography.bodyStrong
                        color: Theme.textPrimary
                        horizontalAlignment: Text.AlignRight
                        elide: Text.ElideRight
                    }

                    Label {
                        anchors.left: parent.left
                        anchors.right: valueLabel.left
                        anchors.rightMargin: Spacing.itemGap
                        anchors.verticalCenter: parent.verticalCenter
                        text: modelData.label
                        font: Typography.body
                        color: Theme.textSecondary
                        elide: Text.ElideRight
                    }

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.right: parent.right
                        height: Theme.borderWidth
                        color: Theme.border
                        visible: index < MockCatalog.profileParams.length - 1
                    }
                }
            }
        }

        AppButton {
            width: parent.width
            variant: "secondary"
            text: qsTr("Редактировать параметры")

            onClicked: Demo.notify(qsTr("Редактирование профиля — этап D1"))
        }
    }
}
