import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Pill-тег: серая метка цели, уровня или числа отзывов.
Rectangle {
    id: pill

    property string text: ""

    color: Theme.surfaceMuted
    implicitHeight: 22
    implicitWidth: pillLabel.implicitWidth + Spacing.listGap * 2
    radius: Theme.radiusSm

    Label {
        id: pillLabel

        anchors.centerIn: parent
        color: Theme.textSecondary
        font: Typography.caption
        text: pill.text
    }
}
