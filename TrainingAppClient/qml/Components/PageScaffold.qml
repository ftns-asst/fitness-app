import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Каркас экрана: фон, заголовок с подписью и прокручиваемая область контента.
// Дети страницы попадают в колонку контента; anchors внутри детей не использовать.
Rectangle {
    id: scaffold

    color: Theme.background

    property string title: ""
    property string caption: ""
    property real topInset: 0
    property real bottomInset: 0
    property alias spacing: bodyColumn.spacing

    default property alias content: bodyColumn.data

    Column {
        id: header

        anchors.top: parent.top
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.topMargin: scaffold.topInset + Spacing.screenPadding
        anchors.leftMargin: Spacing.screenPadding
        anchors.rightMargin: Spacing.screenPadding
        spacing: 4

        Label {
            width: parent.width
            text: scaffold.title
            font: Typography.screenTitle
            color: Theme.textPrimary
            wrapMode: Text.WordWrap
        }

        Label {
            width: parent.width
            text: scaffold.caption
            font: Typography.caption
            color: Theme.textSecondary
            visible: text.length > 0
            wrapMode: Text.WordWrap
        }
    }

    Flickable {
        id: flick

        anchors.top: header.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.topMargin: header.visible ? Spacing.sectionGap : 0
        anchors.bottomMargin: scaffold.bottomInset
        // Контент отстоит от краёв экрана, как карточки в референсе (§3.3).
        anchors.leftMargin: Spacing.screenPadding
        anchors.rightMargin: Spacing.screenPadding
        contentWidth: width
        contentHeight: bodyColumn.implicitHeight
        topMargin: Spacing.safeGap
        bottomMargin: Spacing.sectionGap
        clip: true
        boundsBehavior: Flickable.StopAtBounds

        readonly property bool touchPlatform: Qt.platform.os === "android" || Qt.platform.os === "ios"

        // Десктоп: привычный скроллбар. Touch: только транзиентный индикатор
        // (у ScrollIndicator нет policy, поэтому на десктопе он не подключается).
        ScrollBar.vertical: ScrollBar {
            policy: flick.touchPlatform ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded
        }

        ScrollIndicator {
            id: touchIndicator
        }

        ScrollIndicator.vertical: flick.touchPlatform ? touchIndicator : null

        Column {
            id: bodyColumn

            width: flick.width
            spacing: Spacing.sectionGap
        }
    }
}
