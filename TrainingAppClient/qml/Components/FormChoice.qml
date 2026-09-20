import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Небольшой набор взаимоисключающих вариантов для мобильных форм.
Column {
    id: choice

    property string label: ""
    property var options: []
    property string selectedValue: ""

    signal selected(string value)

    spacing: 6
    width: 280

    Label {
        color: Theme.textSecondary
        font: Typography.captionStrong
        text: choice.label
        width: parent.width
        wrapMode: Text.WordWrap
    }
    Flow {
        spacing: Spacing.itemGap
        width: parent.width

        Repeater {
            model: choice.options

            delegate: Button {
                id: optionButton

                readonly property bool active: choice.selectedValue === modelData.value

                height: Theme.touchMin
                width: Math.max(92, optionLabel.implicitWidth + Spacing.screenPadding * 2)

                background: Rectangle {
                    border.color: optionButton.active ? Theme.accent : Theme.border
                    border.width: optionButton.active ? 2 : Theme.borderWidth
                    color: optionButton.active ? Theme.accentTint : Theme.surfaceMuted
                    radius: Theme.radiusMd
                }
                contentItem: Label {
                    id: optionLabel

                    color: optionButton.active ? Theme.accent : Theme.textSecondary
                    font: Typography.captionStrong
                    horizontalAlignment: Text.AlignHCenter
                    text: modelData.label
                    verticalAlignment: Text.AlignVCenter
                }

                onClicked: {
                    choice.selectedValue = modelData.value;
                    choice.selected(modelData.value);
                }
            }
        }
    }
}
