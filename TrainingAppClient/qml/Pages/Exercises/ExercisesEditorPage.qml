import QtQuick
import QtQuick.Controls
import QtQuick.Dialogs
import QtQuick.Effects
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

    // ViewModel — контекстное свойство ExercisesEditorVM; свойство перезаписываемое,
    // чтобы qmltestrunner подставлял заглушку (образец — ProgramDayPage.viewModel).
    property var viewModel: typeof ExercisesEditorVM !== "undefined" ? ExercisesEditorVM : null
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
    Label {
        color: Theme.textSecondary
        font: Typography.captionStrong
        text: qsTr("Название")
        width: parent.width
    }
    // — Фото техники (референс 2089): крупный скруглённый блок; тап по нему
    // открывает выбор файла, превью видно до сохранения —
    Item {
        id: photoBlock

        height: 240
        visible: page.hasViewModel
        width: parent.width

        Rectangle {
            id: photoMask

            anchors.fill: parent
            layer.enabled: true
            radius: Theme.radiusLg
            visible: false
        }
        Rectangle {
            anchors.fill: parent
            border.color: page.hasViewModel && page.viewModel.hasPhoto === true ? "transparent" : Theme.border
            border.width: Theme.borderWidth
            color: Theme.surfaceMuted
            radius: Theme.radiusLg
        }
        Image {
            // Превью из VM: новое фото видно до сохранения, правка —
            // сразу открывается с существующим.
            anchors.fill: parent
            asynchronous: true
            fillMode: Image.PreserveAspectCrop
            layer.enabled: true
            source: page.hasViewModel && page.viewModel.photoUrl !== undefined ? page.viewModel.photoUrl : ""
            visible: page.hasViewModel && page.viewModel.hasPhoto === true

            layer.effect: MultiEffect {
                maskEnabled: true
                maskSource: photoMask
            }
        }
        Column {
            anchors.centerIn: parent
            spacing: Spacing.listGap
            visible: page.hasViewModel === false || page.viewModel.hasPhoto !== true
            width: parent.width - Spacing.screenPadding * 2

            Rectangle {
                anchors.horizontalCenter: parent.horizontalCenter
                color: Theme.accentTint
                height: Theme.touchMin
                radius: height / 2
                width: height

                Label {
                    anchors.centerIn: parent
                    color: Theme.accent
                    font: Typography.metric
                    text: "+"
                }
            }
            Label {
                color: Theme.textPrimary
                font: Typography.bodyStrong
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Добавить фото техники")
                width: parent.width
            }
            Label {
                color: Theme.textMuted
                font: Typography.caption
                horizontalAlignment: Text.AlignHCenter
                text: qsTr("Необязательно — снимок остаётся на устройстве")
                width: parent.width
                wrapMode: Text.WordWrap
            }
        }
        TapHandler {
            enabled: page.hasViewModel

            onTapped: photoDialog.open()
        }
    }
    TextField {
        id: nameField

        color: Theme.textPrimary
        height: Theme.touchMin
        font: Typography.body
        leftPadding: Spacing.screenPadding
        placeholderText: qsTr("Название упражнения")
        placeholderTextColor: Theme.textMuted
        rightPadding: Spacing.screenPadding
        verticalAlignment: Text.AlignVCenter
        width: parent.width

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

    RowLayout {
        spacing: Spacing.itemGap
        visible: page.hasViewModel && page.viewModel.hasPhoto === true
        width: parent.width

        AppButton {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            onClicked: photoDialog.open()
            text: qsTr("Заменить фото")
            variant: AppButton.Secondary
        }
        AppButton {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            onClicked: {
                if (page.hasViewModel)
                    page.viewModel.removePhoto();
            }
            text: qsTr("Убрать фото")
            variant: AppButton.Secondary
        }
    }
    // — Группы мышц: карточка-секция со счётчиком выбранного —
    AppCard {
        width: parent.width

        ColumnLayout {
            spacing: Spacing.itemGap
            width: parent.width

            RowLayout {
                Layout.fillWidth: true

                Label {
                    Layout.fillWidth: true
                    color: Theme.textPrimary
                    font: Typography.bodyStrong
                    text: qsTr("Группы мышц")
                }
                Label {
                    color: Theme.accent
                    font: Typography.captionStrong
                    text: page.hasViewModel && page.viewModel.selectedMuscleGroups.length > 0 ? qsTr("Выбрано: %1").arg(page.viewModel.selectedMuscleGroups.length) : ""
                    visible: text.length > 0
                }
            }
            Flow {
                Layout.fillWidth: true
                spacing: Spacing.listGap

                Repeater {
                    model: page.hasViewModel ? page.viewModel.availableMuscleGroups : []

                    delegate: MuscleChip {
                        chipText: ExerciseTexts.muscleGroupTitle(modelData)
                        chipValue: modelData
                        selected: page.hasViewModel && page.viewModel.selectedMuscleGroups.indexOf(modelData) >= 0

                        onChipTapped: name => page.viewModel.toggleMuscleGroup(name)
                    }
                }
            }
        }
    }
    // — Инвентарь —
    AppCard {
        width: parent.width

        ColumnLayout {
            spacing: Spacing.itemGap
            width: parent.width

            RowLayout {
                Layout.fillWidth: true

                Label {
                    Layout.fillWidth: true
                    color: Theme.textPrimary
                    font: Typography.bodyStrong
                    text: qsTr("Инвентарь")
                }
                Label {
                    color: Theme.accent
                    font: Typography.captionStrong
                    text: page.hasViewModel && page.viewModel.selectedEquipments.length > 0 ? qsTr("Выбрано: %1").arg(page.viewModel.selectedEquipments.length) : ""
                    visible: text.length > 0
                }
            }
            Flow {
                Layout.fillWidth: true
                spacing: Spacing.listGap

                Repeater {
                    model: page.hasViewModel ? page.viewModel.availableEquipments : []

                    delegate: MuscleChip {
                        chipText: ExerciseTexts.equipmentTitle(modelData)
                        chipValue: modelData
                        selected: page.hasViewModel && page.viewModel.selectedEquipments.indexOf(modelData) >= 0

                        onChipTapped: name => page.viewModel.toggleEquipment(name)
                    }
                }
            }
        }
    }
    // — Видимость: переключатель и пояснение, что он значит —
    AppCard {
        width: parent.width

        ColumnLayout {
            spacing: Spacing.itemGap
            width: parent.width

            Label {
                color: Theme.textPrimary
                font: Typography.bodyStrong
                text: qsTr("Видимость")
            }
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
            Label {
                Layout.fillWidth: true
                color: Theme.textMuted
                font: Typography.caption
                text: visibilitySelector.currentIndex === 1 ? qsTr("Публичное упражнение увидят другие пользователи в каталоге") : qsTr("Приватное упражнение видно только вам")
                wrapMode: Text.WordWrap
            }
        }
    }

    // — Чип multi-выбора: выбран обведён акцентом, тап переключает —
    // chipValue — ключ справочника (уходит в ViewModel), chipText — перевод
    // для показа. Тап с переводом ломал выбор: toggleMuscleGroup кладёт в
    // selected значение ключа, а selected-проверка сравнивает с ключом.
    component MuscleChip: Rectangle {
        id: chip

        property bool selected: false
        property string chipText: ""
        property string chipValue: ""

        signal chipTapped(string name)

        border.color: chip.selected ? Theme.accent : "transparent"
        border.width: Theme.borderWidth
        color: chip.selected ? Theme.accentTint : Theme.surfaceMuted
        height: Theme.touchMin * 0.75
        implicitWidth: chipLabel.implicitWidth + Spacing.listGap * 2
        radius: height / 2

        TapHandler {
            onTapped: chip.chipTapped(chip.chipValue)
        }
        Label {
            id: chipLabel

            anchors.centerIn: parent
            color: chip.selected ? Theme.accent : Theme.textSecondary
            font: Typography.captionStrong
            text: chip.chipText
        }
    }

    // Системный выбор файла фото: на десктопе — файловый диалог, на Android —
    // системный пикер (content://-URL читается QFile через движок платформы).
    // Нативный диалог не живёт в дереве экрана: ошибку выбора показывает
    // форма (errorKey), а не диалог.
    FileDialog {
        id: photoDialog

        nameFilters: [qsTr("Изображения (*.jpg *.jpeg *.png *.webp *.gif)")]

        onAccepted: {
            if (page.hasViewModel)
                page.viewModel.attachPhoto(selectedFile);
        }
    }
}
