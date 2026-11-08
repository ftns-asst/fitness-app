import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Секция атрибутов упражнения; выбранные ключи принадлежат ViewModel.
AppCard {
    id: card
    property string title: ""
    property var options: []
    property var selectedKeys: []
    signal toggled(string key)
    ColumnLayout {
        width: parent.width
        spacing: Spacing.itemGap
        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                color: Theme.textPrimary
                font: Typography.bodyStrong
                text: card.title
            }
            Label {
                color: Theme.accent
                font: Typography.captionStrong
                text: card.selectedKeys.length > 0 ? qsTr("Выбрано: %1").arg(card.selectedKeys.length) : ""
                visible: text.length > 0
            }
        }
        Flow {
            Layout.fillWidth: true
            spacing: Spacing.listGap
            Repeater {
                model: card.options
                delegate: ChoiceChip {
                    required property var modelData
                    chipText: modelData.title
                    chipValue: modelData.key
                    selected: card.selectedKeys.indexOf(chipValue) >= 0
                    onChipTapped: function (key) {
                        card.toggled(key);
                    }
                }
            }
        }
    }
    component ChoiceChip: Rectangle {
        id: chip

        property bool selected: false
        property string chipText: ""
        property string chipValue: ""

        signal chipTapped(string name)

        border.color: chip.selected ? Theme.accent : "transparent"
        border.width: Theme.borderWidth
        color: chip.selected ? Theme.accentTint : Theme.surfaceMuted
        height: Theme.touchMin * 0.75
        implicitWidth: chipLabel.implicitWidth + Spacing.listGap * 2
        radius: height / 2

        TapHandler {
            onTapped: chip.chipTapped(chip.chipValue)
        }
        Label {
            id: chipLabel

            anchors.centerIn: parent
            color: chip.selected ? Theme.accent : Theme.textSecondary
            font: Typography.captionStrong
            text: chip.chipText
        }
    }
}
