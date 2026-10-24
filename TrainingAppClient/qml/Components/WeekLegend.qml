import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Легенда ряда дней недели (макет 02): без неё семь плиток читаются как
// пёстрая полоса — чем отличаются тонированные дни и какой отмечен «сегодня»,
// из самих плиток не видно.
//
// Цвета маркеров берутся из палитры DayTile, а не дублируются здесь: своя
// таблица цветов разъехалась бы с плитками при первом же изменении вида дня.
Item {
    id: legend

    // Подписи и порядок как одна таблица с палитрой: добавили вид дня в DayTile —
    // добавляем его и здесь, иначе легенда молча его не объяснит.
    readonly property var keys: ["workout", "cardio", "mobility", "rest", "today"]

    readonly property var labelsByKey: {
        "workout": qsTr("Тренировка"),
        "cardio": qsTr("Кардио"),
        "mobility": qsTr("Мобилити"),
        "rest": qsTr("Отдых"),
        "today": qsTr("Сегодня")
    }

    implicitHeight: markers.implicitHeight
    implicitWidth: 320

    // Палитра видов дня читается с экземпляра DayTile, а не с типа: свойство
    // экземпляра, и `DayTile.palettesByKind` означало бы «создать плитку».
    // Сама плитка при этом скрыта: она нужна только как источник таблицы, иначе
    // легенда тащила бы на экран лишний элемент и его место в ряду.
    readonly property var paletteSource: DayTile {
        visible: false
    }

    // Маркер «сегодня» — акцент активной плитки; у видов дня берётся заливка
    // обычной плитки: легенда описывает обычный ряд, а не подсвеченный.
    function markerColor(key) {
        return key === "today" ? Theme.accent : paletteSource.palettesByKind[key].plain.fill;
    }

    Flow {
        id: markers

        width: parent.width
        spacing: Spacing.itemGap * 2

        Repeater {
            model: legend.keys

            delegate: Row {
                id: item

                readonly property string key: modelData

                spacing: Spacing.itemGap / 2

                Rectangle {
                    anchors.verticalCenter: parent.verticalCenter
                    color: legend.markerColor(item.key)
                    height: 10
                    radius: Theme.radiusSm
                    width: 10
                }
                Label {
                    color: Theme.textSecondary
                    font: Typography.caption
                    text: legend.labelsByKey[item.key]
                }
            }
        }
    }
}
