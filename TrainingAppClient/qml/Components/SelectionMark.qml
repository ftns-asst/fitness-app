import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Общая круглая отметка: зона касания и доступность принадлежат контролу-владельцу.
Rectangle {
    id: mark
    objectName: "selectionMark"
    property bool checked: false
    property bool failed: false
    property bool pressed: false
    implicitWidth: 28
    implicitHeight: 28
    radius: width / 2
    border.color: checked ? Theme.accent : failed ? Theme.negative : Theme.border
    border.width: checked || failed ? 0 : 2
    color: checked ? Theme.accent : failed ? Theme.negative : "transparent"
    scale: pressed ? 0.88 : 1
    Behavior on color {
        ColorAnimation {
            duration: Theme.pressFeedbackDuration
        }
    }
    Behavior on scale {
        NumberAnimation {
            duration: Theme.pressFeedbackDuration
            easing.type: Easing.OutCubic
        }
    }
    Label {
        anchors.centerIn: parent
        color: Theme.accentForeground
        font: Typography.captionStrong
        opacity: mark.checked || mark.failed ? 1 : 0
        scale: mark.checked || mark.failed ? 1 : 0.4
        text: mark.checked ? "\u2713" : "\u2715"
        Behavior on opacity {
            NumberAnimation {
                duration: Theme.pressFeedbackDuration
            }
        }
        Behavior on scale {
            NumberAnimation {
                duration: Theme.pageTransitionDuration
                easing.type: Easing.OutBack
            }
        }
    }
}
