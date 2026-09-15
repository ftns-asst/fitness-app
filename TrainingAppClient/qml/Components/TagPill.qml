import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Pill-тег: серая метка цели, уровня или числа отзывов.
Rectangle {
    id: pill

    implicitWidth: pillLabel.implicitWidth + Spacing.listGap * 2
    implicitHeight: 22
    radius: Theme.radiusSm
    color: Theme.surfaceMuted

    property string text: ""

    Label {
        id: pillLabel

        anchors.centerIn: parent
        text: pill.text
        font: Typography.caption
        color: Theme.textSecondary
    }
}
