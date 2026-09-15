import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Бейдж рейтинга
Rectangle {
    id: badge

    implicitWidth: badgeLabel.implicitWidth + Spacing.listGap * 2
    implicitHeight: 24
    radius: Theme.radiusSm
    color: Theme.accentTint

    property string text: ""

    Label {
        id: badgeLabel

        anchors.centerIn: parent
        text: badge.text
        font: Typography.captionStrong
        color: Theme.accent
    }
}
