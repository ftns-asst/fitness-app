import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Hero-карточка дня (§4.2): бейджи акцентной строкой, сводка
// тренировки, номер недели справа, CTA внутри карточки.
AppCard {
    id: card

    property string badgesText: ""
    property string ctaText: ""
    property string summaryText: ""
    property int weekIndex: 0
    property int weeksTotal: 0

    signal startClicked

    borderColor: "transparent"
    implicitWidth: 320
    paddingHorizontal: Spacing.screenPadding
    paddingVertical: Spacing.screenPadding
    spacing: Spacing.listGap
    surfaceColor: Theme.accentTint

    RowLayout {
        spacing: Spacing.itemGap
        width: parent.width

        ColumnLayout {
            Layout.fillWidth: true
            spacing: 4

            Label {
                Layout.fillWidth: true
                color: Theme.accent
                elide: Text.ElideRight
                font: Typography.captionStrong
                text: card.badgesText
            }
            Label {
                Layout.fillWidth: true
                color: Theme.textPrimary
                font: Typography.bodyStrong
                text: card.summaryText
                wrapMode: Text.WordWrap
            }
        }
        ColumnLayout {
            spacing: 0

            Label {
                Layout.alignment: Qt.AlignHCenter
                color: Theme.accent
                font: Typography.metric
                text: card.weekIndex
            }
            Label {
                Layout.alignment: Qt.AlignHCenter
                color: Theme.textSecondary
                font: Typography.caption
                text: qsTr("нед. из ") + card.weeksTotal
            }
        }
    }
    AppButton {
        text: card.ctaText
        variant: AppButton.Primary
        width: parent.width

        onClicked: card.startClicked()
    }
}
