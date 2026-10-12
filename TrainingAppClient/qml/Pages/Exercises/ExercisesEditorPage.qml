import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Форма своего упражнения (спринт U5): имя, описание, multi-выбор групп мышц
// и инвентаря, переключатель «Публичное». Одна форма на создание и правку
// (Is_Selected редактора различает режимы). Удаление — двушаговое подтверждение.
//
// ViewModel — контекстное свойство ExercisesEditorVM; в qmltestrunner его нет,
// поэтому все обращения под guard'ом. Справочники чипов приходят из кэша
// репозитория (экран каталога их подгружает раньше формы).
PageScaffold {
    id: page

    readonly property var viewModel: typeof ExercisesEditorVM !== "undefined" ? ExercisesEditorVM : null
    readonly property bool hasViewModel: page.viewModel !== undefined && page.viewModel !== null
    // Двушаговое подтверждение удаления: первый тап готовит, второй удаляет.
    property bool deleteArmed: false

    signal backRequested
    signal finished

    function submitLabel() {
        return page.hasViewModel && page.viewModel.selected ? qsTr("Сохранить изменения") : qsTr("Создать упражнение");
    }

    title: page.hasViewModel && page.viewModel.selected ? qsTr("Правка упражнения") : qsTr("Своё упражнение")

    Component.onCompleted: {
        if (page.hasViewModel) {
            page.viewModel.reset();
            page.viewModel.loadCatalogs();
        }
    }
    Component.onDestruction: {
        if (page.hasViewModel)
            page.viewModel.reset();
    }

    footer: RowLayout {
        spacing: Spacing.itemGap

        // Двушаговое удаление: первый тап спрашивает («Точно удалить?»),
        // повторный удаляет.
        AppButton {
            Layout.fillWidth: true
            enabled: page.hasViewModel
            text: page.deleteArmed ? qsTr("Точно удалить?") : qsTr("Удалить")
            variant: page.deleteArmed ? AppButton.Primary : AppButton.Secondary
            visible: page.hasViewModel && page.viewModel.selected

            onClicked: {
                if (page.deleteArmed === false) {
                    page.deleteArmed = true;
                    return;
                }

                page.viewModel.remove();
            }
        }
        AppButton {
            Layout.fillWidth: true
            enabled: page.hasViewModel
            text: page.submitLabel()
            variant: AppButton.Primary

            onClicked: {
                if (page.hasViewModel) {
                    page.deleteArmed = false;
                    page.viewModel.submit();
                }
            }
        }
    }
    BackBar {
        Layout.fillWidth: true
        text: qsTr("Назад")

        onClicked: page.backRequested()
    }
    // — Ошибка сохранения: форма сохраняет поля, пользователь правит и шлёт снова —
    Label {
        color: Theme.negative
        font: Typography.caption
        text: page.hasViewModel && page.viewModel.errorKey.length > 0 ? ApiErrorText.textFor(page.viewModel.errorKey) : ""
        visible: text.length > 0
        width: parent.width
        wrapMode: Text.WordWrap
    }
    TextField {
        id: nameField

        Layout.fillWidth: true
        Layout.preferredHeight: Theme.touchMin
        color: Theme.textPrimary
        font: Typography.body
        leftPadding: Spacing.screenPadding
        placeholderText: qsTr("Название упражнения")
        placeholderTextColor: Theme.textMuted
        rightPadding: Spacing.screenPadding
        verticalAlignment: Text.AlignVCenter

        background: Rectangle {
            border.color: nameField.activeFocus ? Theme.accent : Theme.border
            border.width: Theme.borderWidth
            color: Theme.surface
            radius: Theme.radiusMd
        }

        onTextChanged: {
            if (page.hasViewModel)
                page.viewModel.name = text;
        }
        // Связь в обратную сторону: поля задаёт loadExisting (правка из кэша).
        Connections {
            function onFieldsChanged() {
                if (page.hasViewModel === false || nameField.activeFocus)
                    return;

                nameField.text = page.viewModel.name;
            }

            target: page.viewModel
        }
    }
    AppTextArea {
        id: descriptionArea

        function syncFromViewModel() {
            if (page.hasViewModel === false || descriptionArea.areaFocused())
                return;

            descriptionArea.text = page.viewModel.description;
        }

        label: qsTr("Описание")
        lineCount: 4
        placeholderText: qsTr("Техника, замечания — необязательно")
        width: parent.width

        onEdited: {
            if (page.hasViewModel)
                page.viewModel.description = descriptionArea.text;
        }
        Connections {
            function onFieldsChanged() {
                descriptionArea.syncFromViewModel();
            }

            target: page.viewModel
        }
        function areaFocused() {
            return false;
        }
    }
    Label {
        color: Theme.textSecondary
        font: Typography.captionStrong
        text: qsTr("Группы мышц")
        width: parent.width
    }
    Flow {
        spacing: Spacing.listGap
        width: parent.width

        Repeater {
            model: page.hasViewModel ? page.viewModel.availableMuscleGroups : []

            delegate: MuscleChip {
                chipText: ExerciseTexts.muscleGroupTitle(modelData)
                selected: page.hasViewModel && page.viewModel.selectedMuscleGroups.indexOf(modelData) >= 0

                onChipTapped: name => page.viewModel.toggleMuscleGroup(name)
            }
        }
    }
    Label {
        color: Theme.textSecondary
        font: Typography.captionStrong
        text: qsTr("Инвентарь")
        width: parent.width
    }
    Flow {
        spacing: Spacing.listGap
        width: parent.width

        Repeater {
            model: page.hasViewModel ? page.viewModel.availableEquipments : []

            delegate: MuscleChip {
                chipText: ExerciseTexts.equipmentTitle(modelData)
                selected: page.hasViewModel && page.viewModel.selectedEquipments.indexOf(modelData) >= 0

                onChipTapped: name => page.viewModel.toggleEquipment(name)
            }
        }
    }
    Label {
        color: Theme.textSecondary
        font: Typography.caption
        text: qsTr("Видимость")
        width: parent.width
    }
    RowLayout {
        width: parent.width

        SegmentedControl {
            id: visibilitySelector

            Layout.fillWidth: true

            currentIndex: page.hasViewModel && page.viewModel.isPublic ? 1 : 0
            items: [qsTr("Приватное"), qsTr("Публичное")]

            onSelected: function (index) {
                if (page.hasViewModel)
                    page.viewModel.isPublic = index === 1;
            }
        }
    }
    // — Чип multi-выбора: выбран обведён акцентом, тап переключает —
    component MuscleChip: Rectangle {
        id: chip

        property bool selected: false
        property string chipText: ""

        signal chipTapped(string name)

        color: chip.selected ? Theme.accentTint : Theme.surfaceMuted
        height: Theme.touchMin * 0.75
        implicitWidth: chipLabel.implicitWidth + Spacing.listGap * 2
        radius: Theme.radiusSm

        TapHandler {
            onTapped: chip.chipTapped(chip.chipText)
        }
        Label {
            id: chipLabel

            anchors.centerIn: parent
            color: chip.selected ? Theme.accent : Theme.textSecondary
            font: Typography.captionStrong
            text: chip.chipText
        }
    }
}
