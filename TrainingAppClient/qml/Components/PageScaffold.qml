import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Каркас экрана: фон, заголовок с подписью и прокручиваемая область контента.
// Дети страницы попадают в колонку контента; anchors внутри детей не использовать.
Rectangle {
    id: scaffold

    property real bottomInset: 0
    property string caption: ""
    default property alias content: bodyColumn.data
    property alias spacing: bodyColumn.spacing
    property string title: ""
    property real topInset: 0

    color: Theme.background

    Column {
        id: header

        anchors.left: parent.left
        anchors.leftMargin: Spacing.screenPadding
        anchors.right: parent.right
        anchors.rightMargin: Spacing.screenPadding
        anchors.top: parent.top
        anchors.topMargin: scaffold.topInset + Spacing.screenPadding
        spacing: 4

        Label {
            color: Theme.textPrimary
            font: Typography.screenTitle
            text: scaffold.title
            width: parent.width
            wrapMode: Text.WordWrap
        }
        Label {
            color: Theme.textSecondary
            font: Typography.caption
            text: scaffold.caption
            visible: text.length > 0
            width: parent.width
            wrapMode: Text.WordWrap
        }
    }
    Flickable {
        id: flick

        readonly property bool touchPlatform: Qt.platform.os === "android" || Qt.platform.os === "ios"

        ScrollIndicator.vertical: flick.touchPlatform ? touchIndicator : null
        anchors.bottom: parent.bottom
        anchors.bottomMargin: scaffold.bottomInset
        anchors.left: parent.left
        // Контент отстоит от краёв экрана, как карточки в референсе (§3.3).
        anchors.leftMargin: Spacing.screenPadding
        anchors.right: parent.right
        anchors.rightMargin: Spacing.screenPadding
        anchors.top: header.bottom
        anchors.topMargin: header.visible ? Spacing.sectionGap : 0
        bottomMargin: Spacing.sectionGap
        boundsBehavior: Flickable.StopAtBounds
        clip: true
        contentHeight: bodyColumn.implicitHeight
        contentWidth: width
        topMargin: Spacing.safeGap

        // Десктоп: привычный скроллбар. Touch: только транзиентный индикатор
        // (у ScrollIndicator нет policy, поэтому на десктопе он не подключается).
        ScrollBar.vertical: ScrollBar {
            policy: flick.touchPlatform ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded
        }

        ScrollIndicator {
            id: touchIndicator
        }
        Column {
            id: bodyColumn

            spacing: Spacing.sectionGap
            width: flick.width
        }
    }
}
