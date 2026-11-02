import QtQuick

// Начало жеста только у левого края, вне горизонтальных контролов страницы.
Item {
    id: edge
    property real distance: 0
    property real verticalDistance: 0
    signal backRequested
    implicitWidth: 24
    DragHandler {
        target: null
        yAxis.enabled: false
        onActiveTranslationChanged: {
            if (active) {
                edge.distance = centroid.scenePosition.x - centroid.scenePressPosition.x;
                edge.verticalDistance = centroid.scenePosition.y - centroid.scenePressPosition.y;
            }
        }
        onActiveChanged: {
            if (active) {
                edge.distance = centroid.scenePosition.x - centroid.scenePressPosition.x;
                edge.verticalDistance = centroid.scenePosition.y - centroid.scenePressPosition.y;
            } else if (edge.distance >= 72 && Math.abs(edge.verticalDistance) < edge.distance * 0.75) {
                edge.backRequested();
            }
        }
    }
}
