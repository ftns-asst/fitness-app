import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

AbstractButton {
    id: row
    property string title: ""
    property string subtitle: ""
    property string valueText: ""
    property string detailText: ""
    property font titleFont: Typography.bodyMedium
    property font valueFont: Typography.bodyMedium
    property color valueColor: Theme.textPrimary
    property string symbol: ""
    property color symbolColor: Theme.textSecondary
    property bool showDivider: true
    property bool navigation: false
    property bool expandable: false
    property bool expanded: false
    property bool interactive: true
    implicitHeight: Math.max(68, contentItem.implicitHeight + topPadding + bottomPadding)
    implicitWidth: 0
    leftPadding: Spacing.cardPadding
    rightPadding: Spacing.cardPadding
    topPadding: 12
    bottomPadding: 12
    enabled: interactive
    hoverEnabled: !Theme.touchPlatform
    Accessible.name: [title, subtitle, valueText, detailText].filter(value => value.length > 0).join(", ")
    background: Rectangle {
        color: row.down || row.hovered ? Theme.surfaceMuted : "transparent"
        radius: Theme.radiusSm
        Behavior on color {
            ColorAnimation {
                duration: Theme.pressFeedbackDuration
            }
        }
    }
    contentItem: RowLayout {
        spacing: Spacing.itemGap
        Column {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            spacing: 4
            Label {
                width: parent.width
                text: row.title
                font: row.titleFont
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }
            Label {
                width: parent.width
                text: row.subtitle
                visible: text.length > 0
                font: Typography.detail
                color: Theme.textSecondary
                elide: Text.ElideRight
            }
        }
        Column {
            Layout.preferredWidth: Math.min(132, Math.max(valueLabel.implicitWidth, detailLabel.implicitWidth))
            spacing: 4
            Label {
                id: valueLabel
                width: parent.width
                text: row.valueText
                font: row.valueFont
                color: row.valueColor
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }
            Label {
                id: detailLabel
                width: parent.width
                text: row.detailText
                visible: text.length > 0
                font: Typography.detail
                color: Theme.textSecondary
                horizontalAlignment: Text.AlignRight
                elide: Text.ElideRight
            }
        }
        Label {
            text: row.symbol
            visible: text.length > 0
            font: Typography.bodyMedium
            color: row.symbolColor
        }
        AppIcon {
            visible: row.expandable || row.navigation
            Layout.preferredWidth: 24
            Layout.preferredHeight: 24
            name: "chevronDown"
            rotation: row.navigation ? -90 : row.expanded ? 180 : 0
            Behavior on rotation {
                NumberAnimation {
                    duration: Theme.selectionDuration
                    easing.type: Easing.OutCubic
                }
            }
        }
    }
    Rectangle {
        anchors {
            left: parent.left
            right: parent.right
            bottom: parent.bottom
            leftMargin: row.leftPadding
            rightMargin: row.rightPadding
        }
        height: 1
        color: Theme.borderSubtle
        visible: row.showDivider
    }
}
