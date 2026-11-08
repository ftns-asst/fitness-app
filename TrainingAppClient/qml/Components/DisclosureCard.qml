import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Одна раскрывающаяся карточка: содержимое сохраняется при сворачивании.
AppCard {
    id: card

    property string title: ""
    property string summary: ""
    property string iconName: "settings"
    property string badgeText: ""
    property string badgeObjectName: ""
    property string errorText: ""
    property bool expanded: false
    property alias body: bodyColumn.data
    property alias preview: previewColumn.data

    signal toggleRequested
    signal doneRequested

    paddingHorizontal: Spacing.listGap
    paddingVertical: Spacing.itemGap
    spacing: 0
    elevated: false
    borderColor: errorText.length > 0 ? Theme.negative : expanded ? Theme.accentSurfaceLine : Theme.border

    AbstractButton {
        id: heading
        objectName: "disclosureHeading"

        width: parent.width
        height: Math.max(Theme.buttonPrimary, contentItem.implicitHeight)
        focusPolicy: Qt.StrongFocus
        hoverEnabled: !Theme.touchPlatform
        Accessible.name: card.title + ". " + card.summary
        Accessible.description: card.expanded ? qsTr("Раздел раскрыт") : qsTr("Раздел свёрнут")
        onClicked: card.toggleRequested()

        background: Rectangle {
            color: heading.hovered ? Theme.surfaceMuted : "transparent"
            radius: Theme.radiusSm
            border.color: heading.visualFocus ? Theme.accent : "transparent"
            border.width: Theme.borderWidth
        }
        contentItem: RowLayout {
            spacing: Spacing.itemGap

            Rectangle {
                Layout.preferredWidth: 32
                Layout.preferredHeight: 32
                radius: Theme.radiusSm
                color: card.expanded ? Theme.accentSurface : Theme.surfaceMuted
                AppIcon {
                    anchors.centerIn: parent
                    name: card.iconName
                    iconColor: card.expanded ? Theme.accent : Theme.textSecondary
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                Layout.preferredWidth: 0
                spacing: 4
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Spacing.itemGap
                    Label {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        text: card.title
                        font: Typography.bodyStrong
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                    }
                    Label {
                        objectName: card.badgeObjectName
                        text: card.badgeText
                        visible: text.length > 0 && text !== "0"
                        font: Typography.captionStrong
                        color: Theme.textSecondary
                    }
                }
                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    text: card.summary
                    visible: text.length > 0
                    font: Typography.caption
                    color: Theme.textSecondary
                    elide: Text.ElideRight
                }
            }
            AppIcon {
                Layout.preferredWidth: 24
                Layout.preferredHeight: 24
                name: "chevronDown"
                iconColor: card.expanded ? Theme.accent : Theme.textMuted
                rotation: card.expanded ? 180 : 0
                Behavior on rotation {
                    NumberAnimation {
                        duration: Theme.pageTransitionDuration
                        easing.type: Easing.OutCubic
                    }
                }
            }
        }
    }
    Item {
        width: parent.width
        height: card.expanded ? 0 : previewColumn.implicitHeight
        visible: height > 0
        enabled: !card.expanded
        clip: true
        Behavior on height {
            NumberAnimation {
                duration: Theme.pageTransitionDuration
                easing.type: Easing.OutCubic
            }
        }
        Column {
            id: previewColumn
            width: parent.width
            spacing: Spacing.itemGap
        }
    }
    Item {
        width: parent.width
        height: card.expanded ? detailsColumn.implicitHeight : 0
        visible: height > 0
        enabled: card.expanded
        clip: true
        Behavior on height {
            NumberAnimation {
                duration: Theme.pageTransitionDuration
                easing.type: Easing.OutCubic
            }
        }
        // Высота тела не зависит от высоты клипа: поля не сжимаются при анимации.
        Column {
            id: detailsColumn
            width: parent.width
            spacing: Spacing.itemGap
            Rectangle {
                width: parent.width
                height: Theme.borderWidth
                color: Theme.borderSubtle
            }
            Column {
                id: bodyColumn
                width: parent.width
                spacing: Spacing.itemGap
            }
            Label {
                width: parent.width
                text: card.errorText
                visible: text.length > 0
                color: Theme.negative
                font: Typography.caption
                wrapMode: Text.WordWrap
            }
            AppButton {
                width: parent.width
                text: qsTr("Готово")
                variant: AppButton.Secondary
                onClicked: card.doneRequested()
            }
        }
    }
}
