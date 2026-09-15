import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Строка каталога упражнений: отдельная карточка с плейсхолдером
// фото техники, названием, подписью группы и шевроном входа.
Item {
    id: row

    implicitWidth: 320
    implicitHeight: card.implicitHeight

    property string title: ""
    property string subtitle: ""

    signal clicked

    AppCard {
        id: card

        anchors.fill: parent
        paddingVertical: Spacing.listGap + 2
        paddingHorizontal: Spacing.cardPadding
        spacing: 0

        RowLayout {
            width: parent.width
            spacing: Spacing.listGap

            // Плейсхолдер фото техники: изображения добавит этап M3 (каталог с сервера).
            // 64 px — минимум, при котором подпись «фото техники» влезает в две
            // строки без обрезки (слово «техники» — 52 px при caption 12 px).
            Rectangle {
                Layout.preferredWidth: 64
                Layout.preferredHeight: 64
                radius: Theme.radiusMd
                color: Theme.surfaceMuted

                Label {
                    anchors.centerIn: parent
                    width: parent.width - Spacing.itemGap
                    text: qsTr("фото техники")
                    font: Typography.caption
                    color: Theme.textMuted
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.WordWrap
                }
            }

            ColumnLayout {
                Layout.fillWidth: true
                spacing: 2

                Label {
                    Layout.fillWidth: true
                    text: row.title
                    font: Typography.bodyStrong
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                }

                Label {
                    Layout.fillWidth: true
                    text: row.subtitle
                    font: Typography.caption
                    color: Theme.textMuted
                    visible: text.length > 0
                    elide: Text.ElideRight
                }
            }

            Label {
                text: "\u203A"
                font: Typography.body
                color: Theme.textMuted
            }
        }
    }

    MouseArea {
        anchors.fill: parent
        onClicked: row.clicked()
    }
}
