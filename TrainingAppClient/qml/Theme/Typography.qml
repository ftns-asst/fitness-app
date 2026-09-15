pragma Singleton
import QtQuick

// Типографика (§3.2). Шрифт Inter встроен в ресурсы (:/fonts, см. CMakeLists.txt)
// и регистрируется в main.cpp до создания QML-движка; если загрузка не удалась,
// Qt молча подменит семейство системным sans-serif — вёрстка не ломается.
//
// Статические начертания Inter регистрируют Medium и Semi Bold отдельными
// семействами («Inter Medium», «Inter Semi Bold»), а Regular и Bold — в
// семействе «Inter». Поэтому у каждого веса своё семейство: иначе Qt не найдёт
// начертание внутри «Inter» и подменит его ближайшим (Medium → Regular).
// Маппинг проверяется зондом шрифтов в qml/Main.qml (строка fontWeights в --diag).
QtObject {
    readonly property string fontFamily: "Inter"
    readonly property string fontFamilyMedium: "Inter Medium"
    readonly property string fontFamilyDemiBold: "Inter Semi Bold"

    readonly property font screenTitle: Qt.font({
        family: fontFamily,
        pixelSize: 22,
        weight: Font.Bold
    })
    readonly property font sectionTitle: Qt.font({
        family: fontFamilyDemiBold,
        pixelSize: 17,
        weight: Font.DemiBold
    })
    readonly property font body: Qt.font({
        family: fontFamily,
        pixelSize: 15,
        weight: Font.Normal
    })
    readonly property font bodyStrong: Qt.font({
        family: fontFamilyDemiBold,
        pixelSize: 15,
        weight: Font.DemiBold
    })
    readonly property font caption: Qt.font({
        family: fontFamily,
        pixelSize: 12,
        weight: Font.Normal
    })
    readonly property font captionStrong: Qt.font({
        family: fontFamilyMedium,
        pixelSize: 12,
        weight: Font.Medium
    })
    readonly property font metric: Qt.font({
        family: fontFamily,
        pixelSize: 28,
        weight: Font.Bold
    })
    readonly property font tabLabel: Qt.font({
        family: fontFamilyMedium,
        pixelSize: 11,
        weight: Font.Medium
    })
    readonly property font overline: Qt.font({
        family: fontFamilyMedium,
        pixelSize: 11,
        weight: Font.Medium,
        capitalization: Font.AllUppercase,
        letterSpacing: 1
    })
}
