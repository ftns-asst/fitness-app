import QtQuick
import QtQuick.Controls
import TrainingAppClient

Rectangle {
    id: banner
    property var monitor: typeof NetworkMonitor !== "undefined" ? NetworkMonitor : null
    readonly property string status: monitor !== null ? monitor.status : "unknown"
    property bool dismissed: false
    readonly property bool hasWarning: status === "offline" || status === "limited" || status === "captive"
    function dismiss() {
        dismissed = true;
    }
    onStatusChanged: dismissed = false
    visible: hasWarning && !dismissed
    height: hasWarning && !dismissed ? Math.max(44, message.implicitHeight + 16) : 0
    color: Theme.accentSurface
    clip: true
    Label {
        id: message
        anchors.left: parent.left
        anchors.leftMargin: Spacing.screenPadding
        anchors.right: closeButton.left
        anchors.rightMargin: Spacing.itemGap
        anchors.verticalCenter: parent.verticalCenter
        font: Typography.caption
        color: Theme.textPrimary
        text: banner.status === "captive" ? qsTr("Для доступа к интернету войдите в сеть Wi-Fi") : banner.status === "limited" ? qsTr("Сеть доступна без выхода в интернет") : qsTr("Нет подключения к сети")
        wrapMode: Text.WordWrap
    }
    AppButton {
        id: closeButton
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        width: Theme.touchMin
        text: "×"
        variant: AppButton.Ghost
        Accessible.name: qsTr("Скрыть уведомление о сети")
        onClicked: banner.dismiss()
    }
}
