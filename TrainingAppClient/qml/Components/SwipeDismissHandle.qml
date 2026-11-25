import QtQuick
import TrainingAppClient

// Область жеста отделена от содержимого: списки и поля сохраняют свои жесты.
Item {
    id: handle
    property real offset: 0
    property real dragStartOffset: 0
    property string accessibleName: qsTr("Закрыть окно")
    property real dismissDistance: 80
    property real upwardLimit: 0
    property bool expandable: false
    property bool expanded: false
    signal dismissed
    implicitWidth: 180
    implicitHeight: Theme.touchMin
    activeFocusOnTab: true
    Accessible.role: Accessible.Button
    Accessible.name: handle.accessibleName
    Accessible.onPressAction: handle.dismissed()
    Keys.onReturnPressed: handle.dismissed()
    Keys.onSpacePressed: handle.dismissed()

    function reset() {
        rebound.stop();
        expanded = false;
        offset = 0;
        dragStartOffset = 0;
    }
    function updateOffset() {
        const displacement = drag.centroid.scenePosition.y - drag.centroid.scenePressPosition.y;
        if (expandable) {
            offset = Math.max(-upwardLimit, dragStartOffset + displacement);
            return;
        }
        // При повторном захвате во время возврата начальная позиция остаётся под пальцем.
        const start = dragStartOffset < 0 && upwardLimit > 0 ? dragStartOffset / Math.max(0.01, 1 + dragStartOffset / upwardLimit) : dragStartOffset;
        const distance = start + displacement;
        offset = distance >= 0 ? distance : upwardLimit * distance / Math.max(1, upwardLimit - distance);
    }
    onUpwardLimitChanged: {
        if (expanded && !drag.active) {
            rebound.stop();
            offset = -upwardLimit;
        }
    }
    Rectangle {
        anchors.horizontalCenter: parent.horizontalCenter
        y: 8
        width: 36
        height: 4
        radius: 2
        color: Theme.textMuted
    }
    NumberAnimation {
        id: rebound
        target: handle
        property: "offset"
        to: handle.expanded ? -handle.upwardLimit : 0
        duration: Theme.popupEnterDuration
        easing.type: Easing.OutCubic
    }
    DragHandler {
        id: drag
        target: null
        xAxis.enabled: false
        onActiveTranslationChanged: {
            if (active)
                handle.updateOffset();
        }
        onActiveChanged: {
            if (active) {
                rebound.stop();
                handle.dragStartOffset = handle.offset;
                handle.updateOffset();
            } else if (handle.offset >= handle.dismissDistance) {
                handle.dismissed();
            } else {
                if (handle.expandable && handle.upwardLimit > 0) {
                    const threshold = handle.expanded ? handle.upwardLimit * 0.65 : Math.min(48, handle.upwardLimit * 0.45);
                    handle.expanded = handle.offset < -threshold;
                }
                rebound.restart();
            }
        }
    }
}
