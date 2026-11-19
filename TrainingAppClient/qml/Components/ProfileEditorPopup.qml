import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

SwipePopup {
    id: popup
    required property var viewModel
    property var original: ({})
    property string formError: ""
    property string gender: ""
    property var focusedField: null
    dragHandle: grip
    modal: true
    focus: true
    parent: Overlay.overlay
    width: parent !== null ? Math.min(parent.width - Spacing.screenPadding * 2, 420) : 360
    height: parent !== null ? Math.min(parent.height - Spacing.screenPadding * 2, editorLayout.implicitHeight + padding * 2) : editorLayout.implicitHeight + padding * 2
    padding: Spacing.screenPadding
    closePolicy: viewModel.saving ? Popup.NoAutoClose : Popup.CloseOnEscape

    function openForProfile() {
        original = Object.assign({}, viewModel.profile);
        ageField.text = original.age === undefined ? "" : String(original.age);
        heightField.text = original.heightCm === undefined ? "" : String(original.heightCm);
        weightField.text = original.weightKg === undefined ? "" : String(original.weightKg);
        gender = original.gender || "";
        formError = "";
        open();
    }
    function save() {
        if (viewModel.saving)
            return;
        const fields = [
            {
                key: "age",
                text: ageField.text,
                max: 150,
                integer: true
            },
            {
                key: "heightCm",
                text: heightField.text,
                max: 300,
                integer: true
            },
            {
                key: "weightKg",
                text: weightField.text,
                max: 500,
                integer: false
            }
        ];
        const changes = {};
        formError = "";
        for (let i = 0; i < fields.length; ++i) {
            const field = fields[i];
            const text = field.text.trim().replace(",", ".");
            if (text.length === 0 && original[field.key] === undefined)
                continue;
            const value = Number(text);
            if (text.length === 0 || !Number.isFinite(value) || value < 1 || value > field.max || (field.integer && Math.floor(value) !== value)) {
                formError = qsTr("Проверьте возраст (1–150), рост (1–300 см) и вес (1–500 кг)");
                return;
            }
            if (value !== original[field.key])
                changes[field.key] = value;
        }
        if (gender.length > 0 && gender !== original.gender)
            changes.gender = gender;
        if (Object.keys(changes).length === 0) {
            close();
            return;
        }
        viewModel.saveProfile(changes);
    }
    function revealField(field) {
        if (!field || !field.inputActiveFocus || !visible)
            return;
        const top = field.mapToItem(formFlick.contentItem, 0, 0).y;
        const bottom = top + field.height;
        let target = formFlick.contentY;
        if (bottom > target + formFlick.height)
            target = bottom - formFlick.height;
        if (top < target)
            target = top;
        formFlick.contentY = Math.max(0, Math.min(Math.max(0, formFlick.contentHeight - formFlick.height), target));
    }
    Connections {
        target: popup.viewModel
        function onProfileSaved() {
            popup.close();
        }
    }
    onClosed: Qt.inputMethod.hide()
    contentItem: ColumnLayout {
        id: editorLayout
        spacing: Spacing.listGap
        SwipeDismissHandle {
            id: grip
            Layout.fillWidth: true
            enabled: !popup.viewModel.saving
            onDismissed: popup.close()
        }
        Label {
            Layout.fillWidth: true
            text: qsTr("Параметры профиля")
            font: Typography.sectionTitle
            color: Theme.textPrimary
        }
        Flickable {
            id: formFlick
            objectName: "profileEditorFlick"
            Layout.fillWidth: true
            Layout.fillHeight: true
            Layout.minimumHeight: 0
            Layout.preferredHeight: fields.implicitHeight
            onHeightChanged: Qt.callLater(function () {
                if (popup)
                    popup.revealField(popup.focusedField);
            })
            clip: true
            contentHeight: fields.implicitHeight
            contentWidth: width
            flickableDirection: Flickable.VerticalFlick
            boundsBehavior: Flickable.StopAtBounds
            flickDeceleration: Theme.flickDeceleration
            maximumFlickVelocity: Theme.flickMaxVelocity
            Column {
                id: fields
                width: parent.width
                spacing: Spacing.listGap
                enabled: !popup.viewModel.saving
                AppTextField {
                    id: ageField
                    objectName: "profileAgeField"
                    width: parent.width
                    label: qsTr("Возраст")
                    maximumLength: 3
                    inputMethodHints: Qt.ImhDigitsOnly
                    onAccepted: heightField.focusInput()
                    onInputActiveFocusChanged: {
                        if (inputActiveFocus) {
                            popup.focusedField = ageField;
                            popup.revealField(ageField);
                        }
                    }
                }
                FormChoice {
                    width: parent.width
                    label: qsTr("Пол")
                    selectedValue: popup.gender
                    options: [
                        {
                            value: "male",
                            label: qsTr("Мужской")
                        },
                        {
                            value: "female",
                            label: qsTr("Женский")
                        }
                    ]
                    onSelected: function (value) {
                        popup.gender = value;
                    }
                }
                AppTextField {
                    id: heightField
                    objectName: "profileHeightField"
                    width: parent.width
                    label: qsTr("Рост, см")
                    maximumLength: 6
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    onAccepted: weightField.focusInput()
                    onInputActiveFocusChanged: {
                        if (inputActiveFocus) {
                            popup.focusedField = heightField;
                            popup.revealField(heightField);
                        }
                    }
                }
                AppTextField {
                    id: weightField
                    objectName: "profileWeightField"
                    width: parent.width
                    label: qsTr("Вес, кг")
                    maximumLength: 6
                    inputMethodHints: Qt.ImhFormattedNumbersOnly
                    onAccepted: popup.save()
                    onInputActiveFocusChanged: {
                        if (inputActiveFocus) {
                            popup.focusedField = weightField;
                            popup.revealField(weightField);
                        }
                    }
                }
            }
        }
        Label {
            Layout.fillWidth: true
            text: popup.formError.length > 0 ? popup.formError : popup.viewModel.errorText
            visible: text.length > 0
            color: Theme.negative
            font: Typography.caption
            wrapMode: Text.WordWrap
        }
        Label {
            Layout.fillWidth: true
            visible: popup.viewModel.offline
            text: qsTr("Сохраним на устройстве. Отправим изменения, когда появится сеть.")
            color: Theme.textSecondary
            font: Typography.caption
            wrapMode: Text.WordWrap
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Spacing.itemGap
            AppButton {
                Layout.fillWidth: true
                variant: AppButton.Secondary
                text: qsTr("Отмена")
                enabled: !popup.viewModel.saving
                onClicked: popup.close()
            }
            AppButton {
                Layout.fillWidth: true
                objectName: "profileSaveButton"
                text: popup.viewModel.saving ? qsTr("Сохраняем…") : qsTr("Сохранить")
                enabled: !popup.viewModel.saving
                onClicked: popup.save()
            }
        }
    }
}
