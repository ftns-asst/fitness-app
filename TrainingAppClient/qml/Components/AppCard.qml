import QtQuick
import QtQuick.Effects
import TrainingAppClient

// Карточка: surface + radius-lg + border 1 px + мягкая тень (§3.5, §4.2).
// Дети попадают во внутреннюю колонку; anchors внутри детей не использовать.
Item {
    id: card

    implicitWidth: 240
    implicitHeight: contentColumn.implicitHeight + paddingVertical * 2

    property color surfaceColor: Theme.surface
    property color borderColor: Theme.border
    property real cornerRadius: Theme.radiusLg
    property int paddingVertical: Spacing.cardPadding
    property int paddingHorizontal: Spacing.cardPadding
    property bool elevated: !Theme.darkMode
    property int spacing: Spacing.itemGap

    default property alias content: contentColumn.data

    // Источник тени. Его геометрия точно равна layout-боксу карточки, поэтому
    // тень не «раздувает» карточку и не перехлёстывается с соседними (§3.5).
    Rectangle {
        id: shadowSource

        anchors.fill: parent
        radius: card.cornerRadius
        color: card.surfaceColor

        // Источник рендерится через MultiEffect, иначе карточка рисуется дважды.
        visible: false
    }

    // Тень — отдельный элемент поверх источника; padding под blur резервируется
    // автоматически (autoPaddingEnabled по умолчанию), границы карточки не едут.
    MultiEffect {
        source: shadowSource
        anchors.fill: shadowSource
        visible: card.elevated
        shadowEnabled: true
        shadowColor: Theme.shadowColor
        shadowOpacity: Theme.shadowOpacity
        shadowBlur: Theme.shadowBlur
        shadowVerticalOffset: Theme.shadowOffset
    }

    // Реальная поверхность рисуется после тени: граница crisp и точно по боксу.
    Rectangle {
        anchors.fill: parent
        radius: card.cornerRadius
        color: card.surfaceColor
        border.width: Theme.borderWidth
        border.color: card.borderColor
    }

    Column {
        id: contentColumn

        anchors.fill: parent
        anchors.margins: 0
        anchors.leftMargin: card.paddingHorizontal
        anchors.rightMargin: card.paddingHorizontal
        anchors.topMargin: card.paddingVertical
        anchors.bottomMargin: card.paddingVertical
        spacing: card.spacing
    }
}
