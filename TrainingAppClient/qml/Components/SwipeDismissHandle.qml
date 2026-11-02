import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Область жеста отделена от содержимого: списки и поля сохраняют свои жесты.
Item {
    id: handle
    property real offset: 0
    property real dragStartOffset: 0
    property string accessibleName: qsTr("Закрыть окно")
    property real dismissDistance: 80
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
        offset = 0;
        dragStartOffset = 0;
    }
    function updateOffset() {
        offset = Math.max(0, dragStartOffset + drag.centroid.scenePosition.y - drag.centroid.scenePressPosition.y);
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
        to: 0
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
                rebound.restart();
            }
        }
    }
}
