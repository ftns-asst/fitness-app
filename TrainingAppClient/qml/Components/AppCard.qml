import QtQuick
import QtQuick.Effects
import TrainingAppClient

// Карточка: surface + radius-lg + border 1 px + мягкая тень (§3.5, §4.2).
// Дети попадают во внутреннюю колонку; anchors внутри детей не использовать.
Item {
    id: card

    property color borderColor: Theme.border
    default property alias content: contentColumn.data
    property real cornerRadius: Theme.radiusLg
    property bool elevated: !Theme.darkMode
    property int paddingHorizontal: Spacing.cardPadding
    property int paddingVertical: Spacing.cardPadding
    property int spacing: Spacing.itemGap
    property color surfaceColor: Theme.surface

    implicitHeight: contentColumn.implicitHeight + paddingVertical * 2
    implicitWidth: 240

    // Источник тени. Его геометрия точно равна layout-боксу карточки, поэтому
    // тень не «раздувает» карточку и не перехлёстывается с соседними (§3.5).
    Rectangle {
        id: shadowSource

        anchors.fill: parent
        color: card.surfaceColor
        radius: card.cornerRadius

        // Источник рендерится через MultiEffect, иначе карточка рисуется дважды.
        visible: false
    }

    // Тень — отдельный элемент поверх источника; padding под blur резервируется
    // автоматически (autoPaddingEnabled по умолчанию), границы карточки не едут.
    MultiEffect {
        anchors.fill: shadowSource
        shadowBlur: Theme.shadowBlur
        shadowColor: Theme.shadowColor
        shadowEnabled: true
        shadowOpacity: Theme.shadowOpacity
        shadowVerticalOffset: Theme.shadowOffset
        source: shadowSource
        visible: card.elevated
    }

    // Реальная поверхность рисуется после тени: граница crisp и точно по боксу.
    Rectangle {
        anchors.fill: parent
        border.color: card.borderColor
        border.width: Theme.borderWidth
        color: card.surfaceColor
        radius: card.cornerRadius
    }
    Column {
        id: contentColumn

        anchors.bottomMargin: card.paddingVertical
        anchors.fill: parent
        anchors.leftMargin: card.paddingHorizontal
        anchors.margins: 0
        anchors.rightMargin: card.paddingHorizontal
        anchors.topMargin: card.paddingVertical
        spacing: card.spacing
    }
}
