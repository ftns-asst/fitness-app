import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Бейдж рейтинга
Rectangle {
    id: badge

    property string text: ""

    color: Theme.accentTint
    implicitHeight: 24
    implicitWidth: badgeLabel.implicitWidth + Spacing.listGap * 2
    radius: Theme.radiusSm

    Label {
        id: badgeLabel

        anchors.centerIn: parent
        color: Theme.accent
        font: Typography.captionStrong
        text: badge.text
    }
}
