import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Hero-карточка дня (§4.2): бейджи акцентной строкой, сводка
// тренировки, номер недели справа, CTA внутри карточки.
AppCard {
    id: card

    implicitWidth: 320
    surfaceColor: Theme.accentTint
    borderColor: "transparent"
    paddingVertical: Spacing.screenPadding
    paddingHorizontal: Spacing.screenPadding
    spacing: Spacing.listGap

    property string badgesText: ""
    property string summaryText: ""
    property int weekIndex: 0
    property int weeksTotal: 0
    property string ctaText: ""

    signal startClicked

    RowLayout {
        width: parent.width
        spacing: Spacing.itemGap

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                Layout.fillWidth: true
                text: card.badgesText
                font: Typography.captionStrong
                color: Theme.accent
                elide: Text.ElideRight
            }

            Label {
                Layout.fillWidth: true
                text: card.summaryText
                font: Typography.bodyStrong
                color: Theme.textPrimary
                wrapMode: Text.WordWrap
            }
        }

        ColumnLayout {
            spacing: 0

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: card.weekIndex
                font: Typography.metric
                color: Theme.accent
            }

            Label {
                Layout.alignment: Qt.AlignHCenter
                text: qsTr("нед. из ") + card.weeksTotal
                font: Typography.caption
                color: Theme.textSecondary
            }
        }
    }

    AppButton {
        width: parent.width
        variant: "primary"
        text: card.ctaText

        onClicked: card.startClicked()
    }
}
