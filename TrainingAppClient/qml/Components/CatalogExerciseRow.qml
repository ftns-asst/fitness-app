import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Строка каталога упражнений: отдельная карточка с плейсхолдером
// фото техники, названием, подписью группы и шевроном входа.
Item {
    id: row

    property string subtitle: ""
    property string title: ""

    signal clicked

    implicitHeight: card.implicitHeight
    implicitWidth: 320

    AppCard {
        id: card

        anchors.fill: parent
        paddingHorizontal: Spacing.cardPadding
        paddingVertical: Spacing.listGap + 2
        spacing: 0

        RowLayout {
            spacing: Spacing.listGap
            width: parent.width

            // Плейсхолдер фото техники: изображения добавит этап M3 (каталог с сервера).
            // 64 px — минимум, при котором подпись «фото техники» влезает в две
            // строки без обрезки (слово «техники» — 52 px при caption 12 px).
            Rectangle {
                Layout.preferredHeight: 64
                Layout.preferredWidth: 64
                color: Theme.surfaceMuted
                radius: Theme.radiusMd

                Label {
                    anchors.centerIn: parent
                    color: Theme.textMuted
                    font: Typography.caption
                    horizontalAlignment: Text.AlignHCenter
                    text: qsTr("фото техники")
                    width: parent.width - Spacing.itemGap
                    wrapMode: Text.WordWrap
                }
            }
            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                    font: Typography.bodyStrong
                    text: row.title
                }
                Label {
                    Layout.fillWidth: true
                    color: Theme.textMuted
                    elide: Text.ElideRight
                    font: Typography.caption
                    text: row.subtitle
                    visible: text.length > 0
                }
            }
            Label {
                color: Theme.textMuted
                font: Typography.body
                text: "\u203A"
            }
        }
    }
    MouseArea {
        anchors.fill: parent

        onClicked: row.clicked()
    }
}
