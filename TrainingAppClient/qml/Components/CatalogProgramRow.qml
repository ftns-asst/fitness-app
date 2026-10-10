import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Строка каталога программ (срез S1): название, цель, срок и действия.
// Вся ширина карточки — зона нажатия «Открыть»: отдельная кнопка на всю
// высоту не нужна и съедала бы место на узком экране.
AppCard {
    id: row

    property string programId: ""
    property string title: ""
    property string goal: ""
    // Состояние программы названо programState, а не state: `state` уже есть у
    // QQuickItem (состояния и переходы), и перекрытие ломало бы их молча.
    // Имя отличается и от роли модели `state`, поэтому роль в делегате можно
    // объявить required property — раньше это было невозможно и требовало
    // ручного заполнения всех полей в ProgramCatalogPage.
    property string programState: ""
    property int weeksTotal: 0
    property string startDate: ""
    property bool canEdit: true

    signal openRequested(string programId)
    signal editRequested(string programId)
    signal statsRequested(string programId)

    ColumnLayout {
        // Внутри AppCard ширину задаёт родитель, а не Layout: содержимое
        // карточки лежит в обычной Column, где Layout-свойства не работают.
        width: parent.width
        spacing: Spacing.itemGap

        Label {
            Layout.fillWidth: true
            color: Theme.textPrimary
            font: Typography.sectionTitle
            text: row.title
            wrapMode: Text.WordWrap
        }
        Flow {
            Layout.fillWidth: true
            spacing: Spacing.itemGap

            TagPill {
                text: row.goalText()
            }
            TagPill {
                text: row.weeksTotal > 0 ? qsTr("%1 нед.").arg(row.weeksTotal) : qsTr("Без срока")
            }
            TagPill {
                text: row.stateText()
            }
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Spacing.itemGap

            AppButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                onClicked: row.statsRequested(row.programId)
                text: qsTr("Статистика")
                variant: AppButton.Secondary
            }
            // Пункт правки показывается только у редактируемой программы:
            // завершённую структуру менять нельзя (§3.1), и кнопка, которая
            // упрётся в отказ базы, вводит в заблуждение.
            AppButton {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                onClicked: row.editRequested(row.programId)
                text: qsTr("Настроить")
                variant: AppButton.Secondary
                visible: row.canEdit
            }
        }
    }
    TapHandler {
        onTapped: row.openRequested(row.programId)
    }
    // Подпись вне TapHandler: иначе жест перехватывал бы нажатия кнопок.
    function goalText() {
        const goals = {
            strength: qsTr("Сила"),
            fatLoss: qsTr("Похудение"),
            muscle: qsTr("Масса"),
            fitness: qsTr("Фитнес"),
            endurance: qsTr("Выносливость")
        };

        return goals[row.goal] !== undefined ? goals[row.goal] : qsTr("Сила");
    }
    function stateText() {
        const states = {
            active: qsTr("Активная"),
            draft: qsTr("Черновик"),
            paused: qsTr("На паузе"),
            completed: qsTr("Завершена"),
            cancelled: qsTr("Отменена"),
            archived: qsTr("В архиве")
        };

        return states[row.programState] !== undefined ? states[row.programState] : "";
    }
}
