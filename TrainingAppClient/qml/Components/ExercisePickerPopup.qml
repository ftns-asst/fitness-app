import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Мини-окно выбора упражнения из каталога (экран дня программы).
//
// Выпадающий список не тянул настоящий каталог: сотни упражнений в закрытом
// списке не видно, искать перебором неудобно. Здесь — поиск (тот же дебаунс
// Avm_Exercises, что и на вкладке «Упражнения»), разделы каталога и фильтры
// и настройки нагрузки для выбранного упражнения. Возврат к списку сохраняет
// введённые значения; кнопка добавления закреплена под прокручиваемой формой.
//
// Каталог приходит контекстным свойством ExercisesVM (Avm_Exercises); в
// qmltestrunner его нет — typeof безопасен, окно откатывается в пустое
// состояние.
SwipePopup {
    id: picker
    dragHandle: popupGrip

    // Каталог: контекстное свойство приложения или подменённая заглушка теста.
    property var catalog: typeof ExercisesVM !== "undefined" ? ExercisesVM : null
    readonly property bool hasCatalog: picker.catalog !== undefined && picker.catalog !== null
    readonly property bool failed: picker.hasCatalog && picker.catalog.errorKey !== ""
    readonly property bool showList: picker.hasCatalog && picker.catalog.exercise_list.count > 0
    // Выбор плитки открывает нагрузку; −1 возвращает каталог.
    property int expandedIndex: -1
    property string selectedIdText: ""
    property string selectedTitleText: ""
    property string catalogType: "all"
    property var groupFilters: []
    property var equipmentFilters: []
    readonly property var filtersPopup: filterSheet
    readonly property int filterCount: groupFilters.length + equipmentFilters.length
    onExpandedIndexChanged: {
        if (expandedIndex < 0 || !hasCatalog || expandedIndex >= catalog.exercise_list.count)
            return;
        const item = catalog.exercise_list.get(expandedIndex);
        selectedIdText = String(item.id);
        selectedTitleText = String(item.title);
        if (configuredExerciseId !== selectedIdText) {
            configuredExerciseId = selectedIdText;
            pickedSets = 4;
            pickedWeight = 40;
            pickedRepsMin = 5;
            pickedRepsMax = 8;
        }
        Qt.inputMethod.hide();
    }
    function selectSection(index) {
        catalogType = ["all", "own", "public"][index];
        expandedIndex = -1;
        if (hasCatalog)
            catalog.setCatalogType(catalogType);
        list.positionViewAtBeginning();
    }
    function openFilters() {
        filterSheet.openFor(groupFilters, equipmentFilters);
    }
    function clearFilters() {
        groupFilters = [];
        equipmentFilters = [];
        if (hasCatalog)
            catalog.setFilters([], []);
        list.positionViewAtBeginning();
    }
    property string configuredExerciseId: ""
    // Значения раскрытой строки: подходы и вес по умолчанию — как на экране дня.
    property int pickedSets: 4
    property double pickedWeight: 40
    property int pickedRepsMin: 5
    property int pickedRepsMax: 8

    signal picked(string exerciseId, string title, real weightKg, int sets, int repsMin, int repsMax)

    // Popup размеряется относительно parentItem. Родителя
    // задаёт хостящий экран (ProgramDayPage — корень страницы): Popup,
    // объявленный внутри карточки, без явного родителя сжимался до ширины
    // этой карточки (~290×99) и список упражнений в него не помещался —
    // окно выбора выглядело пустым. Родитель-страница стабилен с момента
    // создания, в отличие от Overlay.overlay, который при создании окна мог
    // быть null и ронял Popup обратно в карточку-объявление.
    // Тест-хук: Popup ищется по objectName (id снаружи не виден).
    objectName: "exercisePickerPopup"
    width: picker.parent !== null ? Math.min(picker.parent.width - Spacing.safeGap * 2, 560) : 360
    height: picker.parent !== null ? Math.min(picker.parent.height - Spacing.safeGap * 2, picker.expandedIndex >= 0 ? 500 : 740) : 480
    restingY: picker.parent !== null ? picker.parent.height - height - Spacing.safeGap : 0
    Behavior on height {
        NumberAnimation {
            duration: Theme.pageTransitionDuration
            easing.type: Easing.OutCubic
        }
    }
    x: picker.parent !== null ? Math.round((picker.parent.width - picker.width) / 2) : 0
    modal: true
    padding: Spacing.listGap
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    dim: true

    // Закрытие продолжает траекторию перетаскивания вниз.

    onAboutToShow: {
        popupGrip.reset();
        picker.expandedIndex = -1;

        if (picker.hasCatalog) {
            // Каталог общий с вкладкой «Упражнения»: её поиск и chips-фильтр
            // просачивались в окно, и оно открывалось пустым («Ничего не
            // найдено») или с чужой выборкой. Окно всегда начинает с полного
            // каталога; вкладка при показе восстановит свои фильтры сама.
            const filtersReset = picker.catalog.resetFilters();

            // Локальные контролы синхронно: их onTextChanged/свойства уйдут
            // в каталог no-op'ами, потому что состояние уже сброшено.
            searchField.text = "";
            picker.catalogType = "all";
            picker.groupFilters = [];
            picker.equipmentFilters = [];

            // Сброс фильтров или первое открытие без прогрева: перечитываем.
            // Повторные открытия без фильтров показывают уже загруженные строки.
            if ((filtersReset || picker.catalog.exercise_list.count === 0) && picker.catalog.loading === false)
                picker.catalog.reload();

            if (picker.catalog.muscleGroups.length === 0 || (picker.catalog.equipments !== undefined && picker.catalog.equipments.length === 0))
                picker.catalog.loadCatalogs();
        }
    }

    contentItem: ColumnLayout {
        spacing: Spacing.itemGap
        SwipeDismissHandle {
            id: popupGrip
            Layout.fillWidth: true
            objectName: "exercisePickerGrip"
            onDismissed: picker.close()
        }

        // — Заголовок окна —
        RowLayout {
            Layout.fillWidth: true
            spacing: Spacing.itemGap

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                color: Theme.textPrimary
                font: Typography.sectionTitle
                text: picker.expandedIndex < 0 ? qsTr("Выбор упражнения") : qsTr("Нагрузка")
                wrapMode: Text.WordWrap
            }
            AppButton {
                Layout.minimumWidth: 0
                onClicked: picker.close()
                padding: Spacing.itemGap
                text: qsTr("Закрыть")
                variant: AppButton.Ghost
            }
        }
        SegmentedControl {
            objectName: "exercisePickerSections"
            Layout.fillWidth: true
            visible: picker.expandedIndex < 0
            items: [qsTr("Все"), qsTr("Мои"), qsTr("Сообщество")]
            currentIndex: ["all", "own", "public"].indexOf(picker.catalogType)
            onSelected: function (index) {
                picker.selectSection(index);
            }
        }
        // — Поиск: тот же дебаунс 300 мс, что и на вкладке каталога —
        AppInput {
            id: searchField
            visible: picker.expandedIndex < 0

            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredHeight: Theme.touchMin
            leftPadding: Spacing.screenPadding
            objectName: "exercisePickerSearch"
            placeholderText: qsTr("Поиск упражнения")
            rightPadding: Spacing.screenPadding

            surfaceColor: Theme.surface

            onTextChanged: {
                picker.expandedIndex = -1;

                if (picker.hasCatalog)
                    picker.catalog.setQuery(text);
                list.positionViewAtBeginning();
            }
        }
        RowLayout {
            Layout.fillWidth: true
            visible: picker.expandedIndex < 0
            AppButton {
                Layout.fillWidth: true
                text: picker.filterCount > 0 ? qsTr("Фильтры · %1").arg(picker.filterCount) : qsTr("Мышцы и инвентарь")
                variant: AppButton.Secondary
                onClicked: picker.openFilters()
            }
            AppButton {
                visible: picker.filterCount > 0
                variant: AppButton.Ghost
                text: qsTr("Сбросить")
                onClicked: picker.clearFilters()
            }
        }
        // — Каталог плитками по две в строке; выбранная плитка открывает панель добавления —
        GridView {
            id: list
            visible: picker.expandedIndex < 0

            readonly property real gap: Spacing.listGap

            // Тест-хук: список ищется по objectName из теста окна выбора.
            objectName: "exercisePickerList"
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumHeight: addPanel.visible ? 48 : 120
            Layout.minimumWidth: 0
            cellHeight: tileSizer.implicitHeight + list.gap
            cellWidth: Math.floor(list.width / 2)
            clip: true
            model: picker.showList ? picker.catalog.exercise_list : null

            // Образец плитки: высота ячейки считается по нему, а не по делегатам.
            CatalogExerciseTile {
                id: tileSizer
                imageHeightRatio: 0.7

                visible: false
                width: list.cellWidth - list.gap
            }

            delegate: Item {
                id: row

                // Тест-хук: плитки каталога ищутся по objectName.
                objectName: "exercisePickerRow"
                required property int index
                required property string title
                required property var muscleGroups
                required property var model

                height: list.cellHeight
                width: list.cellWidth

                CatalogExerciseTile {
                    imageHeightRatio: 0.7
                    height: tileSizer.implicitHeight
                    photoUrl: row.model.photoUrl !== undefined && row.model.photoUrl !== null ? row.model.photoUrl : ""
                    selected: picker.expandedIndex === row.index
                    title: row.title
                    width: list.cellWidth - list.gap
                    x: Math.round(list.gap / 2)

                    onClicked: {
                        picker.expandedIndex = picker.expandedIndex === row.index ? -1 : row.index;
                    }
                }
            }
        }
        // — Панель добавления: подходы, вес и «Добавить» для выбранной плитки —
        Flickable {
            id: addPanel
            readonly property bool hasSelection: picker.expandedIndex >= 0 && picker.selectedIdText.length > 0
            readonly property string selectedId: hasSelection ? picker.selectedIdText : ""
            readonly property string selectedTitle: hasSelection ? picker.selectedTitleText : ""
            property string displayTitle: ""
            onSelectedTitleChanged: {
                if (selectedTitle.length > 0)
                    displayTitle = selectedTitle;
            }
            Layout.fillWidth: true
            Layout.fillHeight: hasSelection
            contentHeight: prescriptionBody.implicitHeight
            boundsBehavior: Flickable.StopAtBounds
            flickableDirection: Flickable.VerticalFlick
            flickDeceleration: Theme.flickDeceleration
            ScrollIndicator.vertical: ScrollIndicator {}
            Layout.minimumHeight: 0
            Layout.preferredHeight: hasSelection ? prescriptionBody.implicitHeight : 0
            visible: Layout.preferredHeight > 0
            clip: true
            Behavior on Layout.preferredHeight {
                NumberAnimation {
                    duration: Theme.popupEnterDuration
                    easing.type: Easing.OutCubic
                }
            }
            ColumnLayout {
                id: prescriptionBody
                width: parent.width
                height: implicitHeight
                spacing: Spacing.itemGap
                Rectangle {
                    Layout.fillWidth: true
                    Layout.preferredHeight: 1
                    color: Theme.borderSubtle
                }
                Item {
                    Layout.fillWidth: true
                    implicitHeight: Theme.touchMin
                    Label {
                        anchors.left: parent.left
                        anchors.right: collapseButton.left
                        anchors.rightMargin: Spacing.itemGap
                        anchors.verticalCenter: collapseButton.verticalCenter
                        text: addPanel.displayTitle
                        font: Typography.bodyStrong
                        color: Theme.textPrimary
                        elide: Text.ElideRight
                    }
                    AppButton {
                        id: collapseButton
                        anchors.right: parent.right
                        anchors.bottom: parent.bottom
                        objectName: "exercisePickerCollapse"
                        text: qsTr("К списку")
                        variant: AppButton.Ghost
                        onClicked: picker.expandedIndex = -1
                    }
                }
                // Подходы и вес — крупные степперы: «−/+» под большой палец, удержание
                // повторяет шаг. Вес шагает по 2,5 кг, как линейка на записи подхода.
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Spacing.listGap

                    CompactStepper {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.preferredWidth: 1
                        from: 1
                        label: qsTr("Подходы")
                        step: 1
                        to: 20
                        value: picker.pickedSets

                        onEdited: function (newValue) {
                            picker.pickedSets = Math.round(newValue);
                        }
                    }
                    CompactStepper {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.preferredWidth: 1
                        decimals: 1
                        from: 0
                        label: qsTr("Вес, кг")
                        step: 2.5
                        to: 200
                        value: picker.pickedWeight

                        onEdited: function (newValue) {
                            picker.pickedWeight = newValue;
                        }
                    }
                }
                RowLayout {
                    Layout.fillWidth: true
                    spacing: Spacing.listGap
                    CompactStepper {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.preferredWidth: 1
                        objectName: "exerciseRepsMin"
                        label: qsTr("Повторы от")
                        from: 1
                        to: 1000
                        value: picker.pickedRepsMin
                        onEdited: function (value) {
                            picker.pickedRepsMin = Math.round(value);
                            picker.pickedRepsMax = Math.max(picker.pickedRepsMax, picker.pickedRepsMin);
                        }
                    }
                    CompactStepper {
                        Layout.fillWidth: true
                        Layout.minimumWidth: 0
                        Layout.preferredWidth: 1
                        objectName: "exerciseRepsMax"
                        label: qsTr("Повторы до")
                        from: picker.pickedRepsMin
                        to: 1000
                        value: picker.pickedRepsMax
                        onEdited: value => picker.pickedRepsMax = Math.round(value)
                    }
                }
                Label {
                    Layout.fillWidth: true
                    text: qsTr("Для точного числа повторений задайте одинаковые значения.")
                    font: Typography.caption
                    color: Theme.textSecondary
                    wrapMode: Text.WordWrap
                }
            }
        }
        AppButton {
            Layout.fillWidth: true
            visible: addPanel.hasSelection
            objectName: "exercisePickerAdd"
            enabled: addPanel.hasSelection && picker.pickedRepsMin > 0 && picker.pickedRepsMax >= picker.pickedRepsMin
            onClicked: picker.picked(addPanel.selectedId, addPanel.selectedTitle, picker.pickedWeight, picker.pickedSets, picker.pickedRepsMin, picker.pickedRepsMax)
            text: qsTr("Добавить в день")
            variant: AppButton.Primary
        }
        // — Загрузка первой страницы —
        Label {
            Layout.fillWidth: true
            color: Theme.textMuted
            font: Typography.caption
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Загрузка…")
            visible: picker.hasCatalog && picker.catalog.loading && picker.showList === false && picker.failed === false
        }
        // — Пусто: поиск или фильтр не дали строк —
        Label {
            Layout.fillWidth: true
            color: Theme.textMuted
            font: Typography.caption
            horizontalAlignment: Text.AlignHCenter
            text: qsTr("Ничего не найдено")
            visible: picker.hasCatalog && picker.catalog.loading === false && picker.failed === false && picker.showList === false
        }
        // — Ошибка сервера: офлайн-кэш уже показан выше, повтор доступен —
        Label {
            Layout.fillWidth: true
            color: Theme.negative
            font: Typography.caption
            horizontalAlignment: Text.AlignHCenter
            text: picker.failed ? ApiErrorText.textFor(picker.catalog.errorKey) : ""
            visible: picker.failed
            wrapMode: Text.WordWrap
        }
        // — Пагинация: сервер не отдаёт total, «Загрузить ещё» до конца списка —
        AppButton {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            objectName: "exercisePickerLoadMore"
            onClicked: picker.catalog.loadMore()
            text: qsTr("Загрузить ещё")
            variant: AppButton.Secondary
            visible: picker.hasCatalog && picker.catalog.canLoadMore && picker.catalog.loading === false
        }
    }
    ExerciseFiltersPopup {
        id: filterSheet
        parent: picker.parent
        muscleGroups: picker.hasCatalog ? picker.catalog.muscleGroups : []
        equipments: picker.hasCatalog && picker.catalog.equipments !== undefined ? picker.catalog.equipments : []
        onApplied: function (groups, equipment) {
            picker.groupFilters = groups;
            picker.equipmentFilters = equipment;
            if (picker.hasCatalog)
                picker.catalog.setFilters(groups, equipment);
            list.positionViewAtBeginning();
        }
    }
}
