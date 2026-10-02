import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Выбор даты (срез W11). Ручной ввод «2026-11-02» заменён календарём:
// на телефоне ввод с клавиатуры — лишние шаги и источник опечаток, из-за
// которых программа просто не создаётся.
//
// Календарь собран на QtQuick.Controls, а не на DateDialog: модуль
// QtQuick.Dialogs не подключён к сборке, а своя сетка даёт нужное — крупные
// цели касания и предсказуемый вид на всех платформах.
//
// Значение — YYYY-MM-DD, формат, который ждёт C++ (§3.3). Месяцы и дни
// берутся из локали QML, поэтому подписи переводятся вместе с приложением.
//-------------------------------------------------------------------------------
Item {
    id: field

    property string value: ""
    property string label: ""
    readonly property bool expanded: picker.visible

    // Год и месяц, показываемые календарём.
    property int shownYear: value.length === 10 ? Number(value.substring(0, 4)) : (new Date()).getFullYear()
    property int shownMonth: value.length === 10 ? Number(value.substring(5, 7)) - 1 : (new Date()).getMonth()

    signal edited(string isoDate)

    // Две строки: подпись сверху, значение с кнопкой снизу. Высота задана
    // явно, потому что у Item нет содержимого для расчёта implicitHeight.
    implicitHeight: 72
    implicitWidth: 240

    readonly property int cellSize: Math.floor((width - Spacing.listGap * 6) / 7)
    // Дни месяца: високосный год считает сам Date, а не деление на 4.
    readonly property int daysInMonth: new Date(shownYear, shownMonth + 1, 0).getDate()
    // Пустые клетки до первого числа: неделя начинается с понедельника (ISO).
    readonly property int leadingBlanks: (new Date(shownYear, shownMonth, 1).getDay() + 6) % 7

    // — Поле: подпись, значение и кнопка раскрытия —
    Rectangle {
        anchors.fill: parent
        border.color: field.expanded ? Theme.accent : Theme.border
        border.width: Theme.borderWidth
        color: Theme.surface
        radius: Theme.radiusMd
    }
    // Подпись стоит над значением, а не слева: в строку на 360 px подпись,
    // дата и кнопка не помещались и обрезали друг друга.
    ColumnLayout {
        anchors.fill: parent
        anchors.leftMargin: Spacing.listGap
        anchors.rightMargin: Spacing.listGap
        spacing: 2

        Label {
            color: Theme.textMuted
            font: Typography.caption
            text: field.label
            visible: field.label.length > 0
        }
        RowLayout {
            Layout.fillWidth: true
            spacing: Spacing.itemGap

            Label {
                Layout.fillWidth: true
                color: Theme.textPrimary
                elide: Text.ElideRight
                font: Typography.body
                text: field.readableText
            }
            AppButton {
                onClicked: field.expanded ? field.closePicker() : field.openPicker()
                text: field.expanded ? qsTr("Скрыть") : qsTr("Выбрать")
                variant: AppButton.Secondary
            }
        }
    }
    // Сокращения дней недели берутся из локали и режутся до двух букв:
    // перечисление QLocale.FormatName в QML недоступно, а написанный вручную
    // список не переводился бы вместе с приложением.
    readonly property var weekdayInitials: {
        const names = [];

        for (let day = 0; day < 7; ++day)
            names.push(Qt.locale().dayName(day).substring(0, 2));

        return names;
    }

    // — Календарь: месяц, дни недели, сетка чисел —
    Popup {
        id: picker

        y: field.height + Spacing.itemGap
        width: field.width
        padding: Spacing.listGap

        background: Rectangle {
            border.color: Theme.border
            border.width: Theme.borderWidth
            color: Theme.surface
            radius: Theme.radiusLg
        }
        contentItem: ColumnLayout {
            spacing: Spacing.itemGap

            RowLayout {
                Layout.fillWidth: true
                spacing: Spacing.itemGap

                AppButton {
                    Layout.preferredWidth: Theme.touchMin
                    onClicked: field.shiftMonth(-1)
                    text: qsTr("‹")
                    variant: AppButton.Secondary
                }
                Label {
                    Layout.fillWidth: true
                    color: Theme.textPrimary
                    font: Typography.bodyStrong
                    horizontalAlignment: Text.AlignHCenter
                    text: Qt.locale().standaloneMonthName(field.shownMonth) + " " + field.shownYear
                }
                AppButton {
                    Layout.preferredWidth: Theme.touchMin
                    onClicked: field.shiftMonth(1)
                    text: qsTr("›")
                    variant: AppButton.Secondary
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
                        color: Theme.textMuted
                        font: Typography.caption
                        horizontalAlignment: Text.AlignHCenter
                        text: field.weekdayInitials[index]
                    }
                }
            }
            GridLayout {
                Layout.fillWidth: true
                columns: 7

                Repeater {
                    model: field.leadingBlanks

                    delegate: Item {
                        Layout.preferredHeight: field.cellSize
                        Layout.preferredWidth: field.cellSize
                    }
                }
                Repeater {
                    model: field.daysInMonth

                    delegate: AppButton {
                        id: dayCell

                        required property int index

                        Layout.preferredHeight: field.cellSize
                        Layout.preferredWidth: field.cellSize
                        readonly property int day: index + 1
                        readonly property string iso: field.iso_For(field.shownYear, field.shownMonth, day)
                        font.pixelSize: 14
                        onClicked: field.pick(dayCell.iso)
                        padding: 0
                        text: String(day)
                        variant: field.value === dayCell.iso ? AppButton.Primary : AppButton.Secondary
                    }
                }
            }
        }
    }

    function openPicker() {
        picker.open();
    }

    function closePicker() {
        picker.close();
    }

    function shiftMonth(delta) {
        const shifted = new Date(shownYear, shownMonth + delta, 1);

        shownYear = shifted.getFullYear();
        shownMonth = shifted.getMonth();
    }

    function pick(iso) {
        edited(iso);
        closePicker();
    }

    // Дата собирается вручную, а не через toISOString(): тот сдвигает день
    // назад в отрицательных временных зонах.
    function iso_For(year, monthIndex, day) {
        return year + "-" + String(monthIndex + 1).padStart(2, "0") + "-" + String(day).padStart(2, "0");
    }

    readonly property string readableText: value.length !== 10 ? qsTr("дата не выбрана") : qsTr("%1 %2 %3").arg(Number(value.substring(8, 10))).arg(Qt.locale().standaloneMonthName(Number(value.substring(5, 7)) - 1)).arg(value.substring(0, 4))
}
