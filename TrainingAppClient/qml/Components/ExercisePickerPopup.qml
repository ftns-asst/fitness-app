import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Мини-окно выбора упражнения из каталога (экран дня программы).
//
// Выпадающий список не тянул настоящий каталог: сотни упражнений в закрытом
// списке не видно, искать перебором неудобно. Здесь — поиск (тот же дебаунс
// Avm_Exercises, что и на вкладке «Упражнения»), chips-фильтр по группам мышц
// и список, где упражнение раскрывается по нажатию: появляются подходы, вес
// и кнопка «Добавить» справа — выбрать можно, не выходя из окна.
//
// Каталог приходит контекстным свойством ExercisesVM (Avm_Exercises); в
// qmltestrunner его нет — typeof безопасен, окно откатывается в пустое
// состояние.
Popup {
    id: picker

    // Каталог: контекстное свойство приложения или подменённая заглушка теста.
    property var catalog: typeof ExercisesVM !== "undefined" ? ExercisesVM : null
    readonly property bool hasCatalog: picker.catalog !== undefined && picker.catalog !== null
    readonly property bool failed: picker.hasCatalog && picker.catalog.errorKey !== ""
    readonly property bool showList: picker.hasCatalog && picker.catalog.exercise_list.count > 0
    // Раскрытая строка: одна за раз, повторное нажатие сворачивает.
    property int expandedIndex: -1
    // Значения раскрытой строки: подходы и вес по умолчанию — как на экране дня.
    property int pickedSets: 4
    property double pickedWeight: 40

    signal picked(string exerciseId, string title, real weightKg, int sets)

    // Popup центрируется и размеряется относительно parentItem. Родителя
    // задаёт хостящий экран (ProgramDayPage — корень страницы): Popup,
    // объявленный внутри карточки, без явного родителя сжимался до ширины
    // этой карточки (~290×99) и список упражнений в него не помещался —
    // окно выбора выглядело пустым. Родитель-страница стабилен с момента
    // создания, в отличие от Overlay.overlay, который при создании окна мог
    // быть null и ронял Popup обратно в карточку-объявление.
    // Тест-хук: Popup ищется по objectName (id снаружи не виден).
    objectName: "exercisePickerPopup"
    width: picker.parent !== null ? Math.min(picker.parent.width - Spacing.screenPadding * 2, 560) : 360
    height: picker.parent !== null ? Math.min(picker.parent.height - Spacing.screenPadding * 2, 640) : 480
    x: picker.parent !== null ? Math.round((picker.parent.width - picker.width) / 2) : 0
    y: picker.parent !== null ? Math.round((picker.parent.height - picker.height) / 2) : 0
    modal: true
    padding: Spacing.listGap
    closePolicy: Popup.CloseOnEscape | Popup.CloseOnPressOutside
    dim: true

    // Окно выезжает снизу с проявлением, закрывается обратным движением:
    // резкое появление поверх экрана читалось как сбой, а не как выбор.
    enter: Transition {
        NumberAnimation {
            duration: 220
            easing.type: Easing.OutCubic
            from: 0
            property: "opacity"
            to: 1
        }
        NumberAnimation {
            duration: 260
            easing.type: Easing.OutCubic
            from: 0.94
            property: "scale"
            to: 1
        }
    }
    exit: Transition {
        NumberAnimation {
            duration: 140
            easing.type: Easing.InCubic
            property: "opacity"
            to: 0
        }
        NumberAnimation {
            duration: 140
            easing.type: Easing.InCubic
            property: "scale"
            to: 0.96
        }
    }

    background: Rectangle {
        border.color: Theme.border
        border.width: Theme.borderWidth
        color: Theme.surface
        radius: Theme.radiusLg
    }

    onOpened: {
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
            groupChips.groupKey = "";

            // Сброс фильтров или первое открытие без прогрева: перечитываем.
            // Повторные открытия без фильтров показывают уже загруженные строки.
            if ((filtersReset || picker.catalog.exercise_list.count === 0) && picker.catalog.loading === false)
                picker.catalog.reload();

            if (picker.catalog.muscleGroups.length === 0)
                picker.catalog.loadCatalogs();
        }
    }

    contentItem: ColumnLayout {
        spacing: Spacing.itemGap

        // — Заголовок окна —
        RowLayout {
            Layout.fillWidth: true
            spacing: Spacing.itemGap

            Label {
                Layout.fillWidth: true
                Layout.minimumWidth: 0
                color: Theme.textPrimary
                font: Typography.sectionTitle
                text: qsTr("Выбор упражнения")
                wrapMode: Text.WordWrap
            }
            AppButton {
                Layout.minimumWidth: 0
                onClicked: picker.close()
                padding: Spacing.itemGap
                text: qsTr("Закрыть")
                variant: AppButton.Secondary
            }
        }
        // — Поиск: тот же дебаунс 300 мс, что и на вкладке каталога —
        TextField {
            id: searchField

            Layout.fillWidth: true
            Layout.minimumWidth: 0
            Layout.preferredHeight: Theme.touchMin
            color: Theme.textPrimary
            font: Typography.body
            leftPadding: Spacing.screenPadding
            objectName: "exercisePickerSearch"
            placeholderText: qsTr("Поиск упражнения")
            placeholderTextColor: Theme.textMuted
            rightPadding: Spacing.screenPadding
            verticalAlignment: Text.AlignVCenter

            background: Rectangle {
                border.color: searchField.activeFocus ? Theme.accent : Theme.border
                border.width: Theme.borderWidth
                color: Theme.surface
                radius: Theme.radiusMd
            }

            onTextChanged: {
                picker.expandedIndex = -1;

                if (picker.hasCatalog)
                    picker.catalog.setQuery(text);
            }
        }
        // — Фильтр по группе мышц: «Все» + группы из справочника —
        FilterChips {
            id: groupChips

            Layout.fillWidth: true
            // Ключ фильтра хранится отдельно: chips показывают перевод
            // (ExerciseTexts), а в каталог уходит ключ справочника.
            property string groupKey: ""

            currentIndex: groupKey.length === 0 ? 0 : (picker.hasCatalog ? picker.catalog.muscleGroups.indexOf(groupKey) + 1 : 0)
            items: [qsTr("Все")].concat(picker.hasCatalog ? picker.catalog.muscleGroups.map(function (key) {
                return ExerciseTexts.muscleGroupTitle(key);
            }) : [])
            objectName: "exercisePickerGroups"

            onSelected: function (index) {
                groupKey = index === 0 ? "" : picker.catalog.muscleGroups[index - 1];
                picker.expandedIndex = -1;

                if (picker.hasCatalog)
                    picker.catalog.setMuscleGroups(groupKey.length === 0 ? [] : [groupKey]);
            }
        }
        // — Каталог плитками по две в строке; выбранная плитка открывает панель добавления —
        GridView {
            id: list

            readonly property real gap: Spacing.listGap

            // Тест-хук: список ищется по objectName из теста окна выбора.
            objectName: "exercisePickerList"
            Layout.fillHeight: true
            Layout.fillWidth: true
            Layout.minimumHeight: 120
            Layout.minimumWidth: 0
            cellHeight: tileSizer.implicitHeight + list.gap
            cellWidth: Math.floor(list.width / 2)
            clip: true
            model: picker.showList ? picker.catalog.exercise_list : null

            // Образец плитки: высота ячейки считается по нему, а не по делегатам.
            CatalogExerciseTile {
                id: tileSizer

                visible: false
                width: list.cellWidth - list.gap
            }

            delegate: Item {
                id: row

                // Тест-хук: плитки каталога ищутся по objectName.
                objectName: "exercisePickerRow"
                required property int index
                required property string id
                required property string title
                required property var muscleGroups
                required property var model

                height: list.cellHeight
                width: list.cellWidth

                CatalogExerciseTile {
                    height: tileSizer.implicitHeight
                    photoUrl: row.model.photoUrl !== undefined && row.model.photoUrl !== null ? row.model.photoUrl : ""
                    selected: picker.expandedIndex === row.index
                    title: row.title
                    width: list.cellWidth - list.gap
                    x: Math.round(list.gap / 2)

                    onClicked: {
                        picker.expandedIndex = picker.expandedIndex === row.index ? -1 : row.index;

                        if (picker.expandedIndex !== -1) {
                            // Значения по умолчанию — как на экране дня: выбор
                            // всегда начинается с честной заготовки, а не с прошлой.
                            picker.pickedSets = 4;
                            picker.pickedWeight = 40;
                        }
                    }
                }
            }
        }
        // — Панель добавления: подходы, вес и «Добавить» для выбранной плитки —
        ColumnLayout {
            id: addPanel

            readonly property var selectedRow: picker.expandedIndex >= 0 && picker.showList && picker.expandedIndex < picker.catalog.exercise_list.count ? picker.catalog.exercise_list.get(picker.expandedIndex) : null

            Layout.fillWidth: true
            // Панель выезжает и проявляется: выбор плитки виден по отклику, а не
            // по резкому скачку высоты списка.
            Layout.preferredHeight: addPanel.selectedRow !== null ? addPanel.implicitHeight : 0
            clip: true
            opacity: addPanel.selectedRow !== null ? 1 : 0
            spacing: Spacing.itemGap
            visible: opacity > 0.01

            Behavior on Layout.preferredHeight {
                NumberAnimation {
                    duration: 200
                    easing.type: Easing.OutCubic
                }
            }
            Behavior on opacity {
                NumberAnimation {
                    duration: 160
                }
            }

            Rectangle {
                Layout.fillWidth: true
                Layout.preferredHeight: 1
                color: Theme.borderSubtle
            }
            Label {
                Layout.fillWidth: true
                color: Theme.textPrimary
                elide: Text.ElideRight
                font: Typography.bodyStrong
                text: addPanel.selectedRow !== null ? addPanel.selectedRow.title : ""
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
            AppButton {
                Layout.fillWidth: true
                objectName: "exercisePickerAdd"
                onClicked: picker.picked(addPanel.selectedRow.id, addPanel.selectedRow.title, picker.pickedWeight, picker.pickedSets)
                text: qsTr("Добавить в день")
                variant: AppButton.Primary
            }
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
}
