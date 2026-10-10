import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Настройки оформления клиента (экран 32, «Профиль → Параметры»): акцент и тема.
//
// Раньше переключатели жили только в дев-панели дизайн-ревью (§4.11), то есть в
// инструменте разработчика: пользователь не мог выбрать акцент, а панель рядом
// с экраном выглядела как настройка продукта. Здесь это обычный блок параметров:
// значения пишутся в те же токены Theme, что читают все компоненты, поэтому
// смена применяется сразу и без перезапуска.
AppCard {
    id: card

    objectName: "appearanceCard"

    // Плитка нажатия — полный touchMin, а заливка меньше: кружок читается
    // 34 px-заливкой, но попасть в него пальцем нужно не целясь.
    readonly property int swatchFill: 34
    readonly property int swatchSize: Theme.touchMin

    Label {
        color: Theme.textMuted
        font: Typography.sectionTitle
        text: qsTr("Оформление")
        width: parent.width
    }
    // Имя акцента рядом с кружками: «синий» и «фиолетовый» различаются
    // оттенком, и подпись отвечает на вопрос «какой выбран».
    Label {
        objectName: "appearanceAccentName"
        color: Theme.textSecondary
        font: Typography.caption
        text: qsTr("Акцент: %1").arg(Theme.accentNames[Theme.accentIndex])
        width: parent.width
        wrapMode: Text.WordWrap
    }
    Row {
        id: accentRow

        spacing: Spacing.listGap
        width: parent.width

        Repeater {
            model: Theme.accentPalette

            delegate: Item {
                id: swatch

                objectName: "appearanceAccentSwatch"
                height: card.swatchSize
                width: card.swatchSize

                Rectangle {
                    anchors.centerIn: parent
                    // Выбранный обведён рамкой, а не увеличен: рост кружка
                    // сдвигал бы весь ряд при каждом нажатии.
                    border.color: Theme.textPrimary
                    border.width: Theme.accentIndex === index ? 3 : 0
                    color: modelData
                    height: card.swatchFill
                    radius: height / 2
                    width: card.swatchFill
                }
                TapHandler {
                    onTapped: Theme.accentIndex = index
                }
            }
        }
    }
    Label {
        color: Theme.textSecondary
        font: Typography.caption
        text: qsTr("Тема")
        width: parent.width
    }
    // Контрол растягивается по карточке, поэтому стоит в layout — как в
    // редакторе программы. У SegmentedControl есть дефолтная implicitWidth
    // (300), и в обычной колонке карточки на 360 px контрол читался аудитом
    // вёрстки (--diag) как «сжатый»: ширины ему не хватало до собственного
    // implicitWidth, хотя по замыслу её задаёт родитель.
    RowLayout {
        width: parent.width

        SegmentedControl {
            id: themeSelector

            Layout.fillWidth: true
            objectName: "appearanceThemeSelector"

            // Тема — та же, что применяют системные полосы и все компоненты:
            // отдельного состояния у контрола нет.
            currentIndex: Theme.darkMode ? 1 : 0
            items: [qsTr("Светлая"), qsTr("Тёмная")]

            onSelected: function (index) {
                Theme.darkMode = (index === 1);
            }
        }
    }
}
