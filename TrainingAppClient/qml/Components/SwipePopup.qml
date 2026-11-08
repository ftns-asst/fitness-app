import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Одна траектория для открытия, жеста и закрытия — без дополнительного zoom.
Popup {
    id: sheet
    required property SwipeDismissHandle dragHandle
    property real restingY: parent !== null ? (parent.height - height) / 2 : 0
    readonly property real travel: Math.max(height, Overlay.overlay !== null ? Overlay.overlay.height : 0)
    x: parent !== null ? (parent.width - width) / 2 : 0
    y: restingY + (dragHandle !== null ? dragHandle.offset : 0)
    onAboutToShow: {
        if (dragHandle !== null)
            dragHandle.reset();
    }
    background: Rectangle {
        color: Theme.surface
        border.color: Theme.border
        border.width: Theme.borderWidth
        radius: Theme.radiusLg
    }
    enter: Transition {
        NumberAnimation {
            target: sheet.dragHandle
            property: "offset"
            from: sheet.travel
            to: 0
            duration: Theme.popupEnterDuration
            easing.type: Easing.OutCubic
        }
    }
    exit: Transition {
        NumberAnimation {
            target: sheet.dragHandle
            property: "offset"
            to: sheet.travel
            duration: Theme.pageTransitionDuration
            easing.type: Easing.OutCubic
        }
    }
    Overlay.modal: Rectangle {
        color: Qt.rgba(0, 0, 0, 0.4 * Math.max(0, 1 - (sheet.dragHandle !== null ? sheet.dragHandle.offset : 0) / Math.max(1, sheet.travel)))
    }
}
