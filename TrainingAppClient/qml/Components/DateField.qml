import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// A compact month calendar shared by forms and diary date navigation.
Item {
    id: field

    property string value: ""
    property string label: ""
    property bool pickerOnly: false
    property string minimumDate: ""
    property string maximumDate: ""
    readonly property bool expanded: picker.visible
    property int shownYear: value.length === 10 ? Number(value.slice(0, 4)) : new Date().getFullYear()
    property int shownMonth: value.length === 10 ? Number(value.slice(5, 7)) - 1 : new Date().getMonth()
    property bool periodExpanded: false
    readonly property string today: Qt.formatDate(new Date(), Qt.ISODate)
    readonly property int daysInMonth: new Date(shownYear, shownMonth + 1, 0).getDate()
    readonly property int leadingBlanks: (new Date(shownYear, shownMonth, 1).getDay() + 6) % 7
    readonly property int cellSize: Math.floor((picker.width - picker.padding * 2) / 7)
    readonly property var weekdayInitials: {
        const names = [];
        for (let day = 0; day < 7; ++day)
            names.push(new Date(2024, 0, day + 1).toLocaleDateString(Qt.locale(), "ddd"));
        return names;
    }
    readonly property var months: {
        const result = [];
        for (let month = 0; month < 12; ++month)
            result.push({
                id: month,
                title: Qt.locale().standaloneMonthName(month)
            });
        return result;
    }
    readonly property string readableText: value.length !== 10 ? qsTr("Дата не выбрана") : field.readableDate(value)

    signal edited(string isoDate)
    implicitHeight: pickerOnly ? 0 : 72
    implicitWidth: 240

    function isDateAvailable(iso) {
        return (minimumDate.length === 0 || iso >= minimumDate) && (maximumDate.length === 0 || iso <= maximumDate);
    }
    function iso_For(year, monthIndex, day) {
        return year + "-" + String(monthIndex + 1).padStart(2, "0") + "-" + String(day).padStart(2, "0");
    }
    function shiftMonth(delta) {
        const shifted = new Date(shownYear, shownMonth + delta, 1);
        shownYear = shifted.getFullYear();
        shownMonth = shifted.getMonth();
    }
    function shiftYear(delta) {
        shownYear += delta;
    }
    function openPicker() {
        const selected = value.length === 10 ? value : today;
        shownYear = Number(selected.slice(0, 4));
        shownMonth = Number(selected.slice(5, 7)) - 1;
        periodExpanded = false;
        picker.open();
    }
    function closePicker() {
        picker.close();
    }
    function pick(iso) {
        if (!isDateAvailable(iso))
            return;
        edited(iso);
        closePicker();
    }

    Rectangle {
        visible: !field.pickerOnly
        anchors.fill: parent
        color: Theme.surface
        border.color: field.expanded ? Theme.accent : Theme.border
        border.width: Theme.borderWidth
        radius: Theme.radiusMd
    }
    ColumnLayout {
        visible: !field.pickerOnly
        anchors.fill: parent
        anchors.leftMargin: Spacing.listGap
        anchors.rightMargin: Spacing.listGap
        spacing: 2
        Label {
            visible: field.label.length > 0
            text: field.label
            color: Theme.textMuted
            font: Typography.caption
        }
        RowLayout {
            Layout.fillWidth: true
            Label {
                Layout.fillWidth: true
                text: field.readableText
                color: Theme.textPrimary
                font: Typography.body
                elide: Text.ElideRight
            }
            AppButton {
                text: field.expanded ? qsTr("Скрыть") : qsTr("Выбрать")
                variant: AppButton.Ghost
                onClicked: field.expanded ? field.closePicker() : field.openPicker()
            }
        }
    }
    SwipePopup {
        id: picker
        parent: Overlay.overlay
        dragHandle: calendarGrip
        modal: true
        dim: true
        padding: Spacing.listGap
        width: Math.min(380, parent !== null ? parent.width - Spacing.safeGap * 2 : field.width)
        background: Rectangle {
            color: Theme.surface
            border.color: Theme.border
            border.width: Theme.borderWidth
            radius: Theme.radiusLg
        }
        contentItem: ColumnLayout {
            spacing: Spacing.itemGap
            SwipeDismissHandle {
                id: calendarGrip
                Layout.fillWidth: true
                objectName: "calendarSwipeGrip"
                onDismissed: field.closePicker()
            }
            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    objectName: "datePrevMonthButton"
                    Layout.preferredWidth: Theme.touchMin
                    implicitWidth: Theme.touchMin
                    text: "‹"
                    variant: AppButton.Ghost
                    Accessible.name: qsTr("Предыдущий месяц")
                    onClicked: field.shiftMonth(-1)
                }
                AppButton {
                    id: monthYearButton
                    objectName: "dateMonthYearButton"
                    Layout.fillWidth: true
                    labelFont: Typography.bodyStrong
                    text: Qt.locale().standaloneMonthName(field.shownMonth) + " " + field.shownYear
                    variant: AppButton.Ghost
                    Accessible.name: qsTr("Выбрать месяц и год")
                    onClicked: field.periodExpanded = !field.periodExpanded
                    contentItem: RowLayout {
                        spacing: Spacing.itemGap
                        Label {
                            Layout.fillWidth: true
                            text: monthYearButton.text
                            color: Theme.textPrimary
                            font: Typography.bodyStrong
                            horizontalAlignment: Text.AlignHCenter
                            elide: Text.ElideRight
                        }
                        AppIcon {
                            name: "chevronDown"
                            iconColor: Theme.textMuted
                            rotation: field.periodExpanded ? 180 : 0
                        }
                    }
                }
                AppButton {
                    objectName: "dateNextMonthButton"
                    Layout.preferredWidth: Theme.touchMin
                    implicitWidth: Theme.touchMin
                    text: "›"
                    variant: AppButton.Ghost
                    Accessible.name: qsTr("Следующий месяц")
                    onClicked: field.shiftMonth(1)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                visible: field.periodExpanded
                AppButton {
                    objectName: "datePrevYearButton"
                    Layout.preferredWidth: Theme.touchMin
                    implicitWidth: Theme.touchMin
                    text: "‹"
                    variant: AppButton.Ghost
                    Accessible.name: qsTr("Предыдущий год")
                    onClicked: field.shiftYear(-1)
                }
                AppComboBox {
                    id: monthSelector
                    objectName: "calendarMonthSelector"
                    Layout.fillWidth: true
                    model: field.months
                    onActivated: field.shownMonth = currentIndex
                    Binding {
                        target: monthSelector
                        property: "currentIndex"
                        value: field.shownMonth
                    }
                }
                AppButton {
                    objectName: "dateNextYearButton"
                    Layout.preferredWidth: Theme.touchMin
                    implicitWidth: Theme.touchMin
                    text: "›"
                    variant: AppButton.Ghost
                    Accessible.name: qsTr("Следующий год")
                    onClicked: field.shiftYear(1)
                }
            }
            RowLayout {
                Layout.fillWidth: true
                spacing: 0
                Repeater {
                    model: 7
                    delegate: Label {
                        required property int index
                        Layout.fillWidth: true
                        text: field.weekdayInitials[index]
                        font: Typography.caption
                        color: Theme.textMuted
                        horizontalAlignment: Text.AlignHCenter
                    }
                }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 7
                rowSpacing: 0
                columnSpacing: 0
                Repeater {
                    model: 42
                    delegate: AbstractButton {
                        id: dayCell
                        required property int index
                        readonly property int day: index - field.leadingBlanks + 1
                        readonly property bool inMonth: day > 0 && day <= field.daysInMonth
                        readonly property string iso: field.iso_For(field.shownYear, field.shownMonth, day)
                        objectName: inMonth ? "calendarDate_" + iso : "calendarBlank_" + index
                        Layout.fillWidth: true
                        Layout.preferredWidth: field.cellSize
                        Layout.preferredHeight: Math.max(Theme.touchMin, field.cellSize)
                        enabled: inMonth && field.isDateAvailable(iso)
                        text: inMonth ? String(day) : ""
                        Accessible.name: inMonth ? field.readableDate(iso) : ""
                        Accessible.role: Accessible.Button
                        Accessible.onPressAction: clicked()
                        onClicked: field.pick(iso)
                        contentItem: Label {
                            text: dayCell.text
                            color: field.value === dayCell.iso ? Theme.accentForeground : dayCell.enabled ? Theme.textPrimary : Theme.textMuted
                            opacity: dayCell.enabled ? 1 : 0.4
                            font: field.value === dayCell.iso ? Typography.bodyStrong : Typography.body
                            horizontalAlignment: Text.AlignHCenter
                            verticalAlignment: Text.AlignVCenter
                        }
                        background: Rectangle {
                            width: Math.min(parent.width - 4, 40)
                            height: width
                            anchors.centerIn: parent
                            radius: width / 2
                            color: field.value === dayCell.iso && dayCell.inMonth ? Theme.accent : dayCell.down ? Theme.accentSurface : "transparent"
                            border.width: dayCell.inMonth && dayCell.iso === field.today ? Theme.borderWidth : 0
                            border.color: Theme.accent
                        }
                    }
                }
            }
            RowLayout {
                Layout.fillWidth: true
                AppButton {
                    objectName: "dateTodayButton"
                    Layout.fillWidth: true
                    text: qsTr("Сегодня")
                    variant: AppButton.Ghost
                    enabled: field.isDateAvailable(field.today)
                    onClicked: field.pick(field.today)
                }
                AppButton {
                    objectName: "dateCloseButton"
                    Layout.fillWidth: true
                    text: qsTr("Готово")
                    variant: AppButton.Ghost
                    onClicked: field.closePicker()
                }
            }
        }
    }
    function readableDate(iso) {
        return Qt.formatDate(new Date(Number(iso.slice(0, 4)), Number(iso.slice(5, 7)) - 1, Number(iso.slice(8, 10))), "d MMMM yyyy");
    }
}
