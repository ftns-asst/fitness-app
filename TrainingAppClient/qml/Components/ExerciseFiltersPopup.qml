import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Поиск и черновик фильтров; применение данных принадлежит вызывающей странице.
SwipePopup {
    id: sheet
    property var muscleGroups: []
    property var equipments: []
    property var draftGroups: []
    property var draftEquipment: []
    property string query: ""
    property string kind: "group"
    signal applied(var groups, var equipment)

    function openFor(groups, equipment) {
        sheet.draftGroups = groups.slice();
        sheet.draftEquipment = equipment.slice();
        sheet.query = "";
        sheet.open();
    }
    function apply() {
        sheet.applied(sheet.draftGroups.slice(), sheet.draftEquipment.slice());
        sheet.close();
    }
    function toggleDraft(kind, key) {
        const next = (kind === "group" ? sheet.draftGroups : sheet.draftEquipment).slice();
        const index = next.indexOf(key);
        if (index < 0)
            next.push(key);
        else
            next.splice(index, 1);
        if (kind === "group")
            sheet.draftGroups = next;
        else
            sheet.draftEquipment = next;
    }
    function filterKeys(kind) {
        const keys = kind === "group" ? sheet.muscleGroups : sheet.equipments;
        const query = sheet.query.trim().toLocaleLowerCase();
        return keys.filter(function (key) {
            const title = kind === "group" ? ExerciseTexts.muscleGroupTitle(key) : ExerciseTexts.equipmentTitle(key);
            return title.toLocaleLowerCase().indexOf(query) >= 0;
        }).sort(function (a, b) {
            const first = kind === "group" ? ExerciseTexts.muscleGroupTitle(a) : ExerciseTexts.equipmentTitle(a);
            const second = kind === "group" ? ExerciseTexts.muscleGroupTitle(b) : ExerciseTexts.equipmentTitle(b);
            return first.localeCompare(second);
        });
    }

    objectName: "exerciseFiltersSheet"
    parent: Overlay.overlay
    dragHandle: filterGrip
    modal: true
    dim: true
    width: Math.min(440, parent !== null ? parent.width - 2 * Spacing.safeGap : 390)
    height: Math.min(680, parent !== null ? parent.height - 2 * Spacing.safeGap : 680)
    restingY: parent !== null ? parent.height - height - Spacing.safeGap : 0
    padding: Spacing.screenPadding
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    onClosed: Qt.inputMethod.hide()
    contentItem: ColumnLayout {
        spacing: Spacing.itemGap
        SwipeDismissHandle {
            id: filterGrip
            Layout.alignment: Qt.AlignHCenter
            onDismissed: sheet.close()
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                text: qsTr("Фильтры")
                font: Typography.sectionTitle
                color: Theme.textPrimary
                Layout.fillWidth: true
            }
            AppButton {
                text: qsTr("Отмена")
                variant: AppButton.Ghost
                onClicked: sheet.close()
            }
        }
        AppInput {
            objectName: "exerciseFilterSearch"
            Layout.fillWidth: true
            Layout.preferredHeight: Theme.touchMin
            placeholderText: qsTr("Найти мышцу или инвентарь")
            text: sheet.query
            onTextChanged: sheet.query = text
            leftPadding: Spacing.itemGap
            focusBorderColor: Theme.border
        }
        SegmentedControl {
            objectName: "exerciseFilterSections"
            Layout.fillWidth: true
            visible: sheet.query.trim().length === 0
            items: [sheet.draftGroups.length > 0 ? qsTr("Мышцы · %1").arg(sheet.draftGroups.length) : qsTr("Мышцы"), sheet.draftEquipment.length > 0 ? qsTr("Инвентарь · %1").arg(sheet.draftEquipment.length) : qsTr("Инвентарь")]
            currentIndex: sheet.kind === "group" ? 0 : 1
            onSelected: function (index) {
                sheet.kind = index === 0 ? "group" : "equipment";
            }
        }
        Flickable {
            id: filterScroll
            Layout.fillWidth: true
            Layout.fillHeight: true
            clip: true
            contentHeight: filterOptions.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            flickDeceleration: Theme.flickDeceleration
            ScrollIndicator.vertical: ScrollIndicator {}
            Column {
                id: filterOptions
                width: filterScroll.width
                spacing: Spacing.itemGap
                Repeater {
                    model: sheet.query.trim().length > 0 ? ["group", "equipment"] : [sheet.kind]
                    delegate: Column {
                        id: filterSection
                        required property string modelData
                        readonly property string kind: modelData
                        width: filterOptions.width
                        visible: sheet.filterKeys(kind).length > 0
                        Label {
                            text: filterSection.kind === "group" ? qsTr("Мышцы") : qsTr("Инвентарь")
                            font: Typography.captionStrong
                            color: Theme.textMuted
                            topPadding: Spacing.itemGap
                            bottomPadding: Spacing.itemGap
                        }
                        Repeater {
                            model: sheet.filterKeys(filterSection.kind)
                            delegate: CheckDelegate {
                                id: filterOption
                                required property string modelData
                                objectName: "exerciseFilter_" + filterSection.kind + "_" + modelData
                                width: filterOptions.width
                                height: Math.max(Theme.touchMin, implicitHeight)
                                text: filterSection.kind === "group" ? ExerciseTexts.muscleGroupTitle(modelData) : ExerciseTexts.equipmentTitle(modelData)
                                font: Typography.body
                                checked: (filterSection.kind === "group" ? sheet.draftGroups : sheet.draftEquipment).indexOf(modelData) >= 0
                                onClicked: sheet.toggleDraft(filterSection.kind, modelData)
                                contentItem: Label {
                                    text: filterOption.text
                                    font: Typography.body
                                    color: Theme.textPrimary
                                    rightPadding: 40
                                    verticalAlignment: Text.AlignVCenter
                                    wrapMode: Text.WordWrap
                                }
                                indicator: SelectionMark {
                                    x: filterOption.width - width - Spacing.itemGap
                                    y: (filterOption.height - height) / 2
                                    checked: filterOption.checked
                                    pressed: filterOption.down
                                }
                                background: Rectangle {
                                    color: filterOption.down ? Theme.accentTint : "transparent"
                                    radius: Theme.radiusMd
                                }
                            }
                        }
                    }
                }
                Label {
                    visible: sheet.filterKeys("group").length === 0 && sheet.filterKeys("equipment").length === 0
                    text: qsTr("Ничего не найдено")
                    color: Theme.textMuted
                    font: Typography.body
                }
            }
        }
        RowLayout {
            Layout.fillWidth: true
            AppButton {
                text: qsTr("Сбросить")
                variant: AppButton.Ghost
                enabled: sheet.draftGroups.length + sheet.draftEquipment.length > 0
                onClicked: {
                    sheet.draftGroups = [];
                    sheet.draftEquipment = [];
                }
            }
            AppButton {
                objectName: "exerciseFiltersApply"
                Layout.fillWidth: true
                text: qsTr("Применить")
                compact: true
                onClicked: sheet.apply()
            }
        }
    }
}
