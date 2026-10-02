import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Числовое поле в стиле приложения: та же рамка, скругление и высота, что у
// AppTextField. Штатный SpinBox рисовался системным стилем Qt (серые
// квадратные кнопки) и выбивался из общей картины.
//
// Значение — обычное свойство компонента, а не алиас TextInput: у TextInput
// такого свойства нет, и поле со значением «0 кг» требовало бы отдельного
// состояния в каждом экранном файле.
Column {
    id: field

    // Сколько знаков после запятой показывать: вес «62,5», подходы «4».
    property int decimals: 0
    property real from: 0
    property string label: ""
    property real step: 1
    property string suffix: ""
    property real to: 100
    // Текущее значение. Присваивается извне (страница) и кнопками.
    property real value: 0

    // Имя не совпадает с valueChanged: авто-сигнал свойства value уже
    // существует, и объявление своего сломало бы компонент.
    signal edited(real value)

    // Ширина по умолчанию — как у AppTextField; конкретные экраны переопределяют
    // её preferredWidth, потому что число занимает мало места.
    spacing: 6
    width: 280

    Label {
        color: Theme.textSecondary
        font: Typography.captionStrong
        text: field.label
        visible: text.length > 0
        width: parent.width
        wrapMode: Text.WordWrap
    }
    RowLayout {
        width: parent.width
        spacing: 0

        Rectangle {
            Layout.fillWidth: true
            Layout.minimumWidth: 0
            color: Theme.surfaceMuted
            implicitHeight: Theme.buttonPrimary
            // Рамка рисуется на самом прямоугольнике: у вложенного скругление
            // не совпало бы с радиусом поля.
            border.color: Theme.border
            border.width: Theme.borderWidth
            radius: Theme.radiusMd

            Label {
                anchors.centerIn: parent
                // Метка занимает ширину поля и обрезается многоточием: без
                // этого длинное значение (например, «500 кг») вылезало за
                // рамку.
                elide: Text.ElideRight
                horizontalAlignment: Text.AlignHCenter
                width: parent.width
                color: Theme.textPrimary
                font: Typography.body
                // Значение форматируется в одном месте, поэтому внешний код
                // задаёт только число, а не строку.
                text: Number(field.value).toFixed(field.decimals) + field.suffix
            }
        }
        // Кнопки встроены в поле: отдельная пара «−/＋» рядом с ним выглядела
        // бы как чужой элемент формы, а внутри — как часть поля ввода.
        Repeater {
            model: [
                {
                    text: "−",
                    step: -field.step,
                    radius: 0
                },
                {
                    text: "+",
                    step: field.step,
                    radius: Theme.radiusMd
                }
            ]

            delegate: Button {
                id: stepButton

                required property var modelData

                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: Theme.buttonPrimary
                Layout.preferredWidth: Theme.buttonPrimary
                text: modelData.text

                background: Rectangle {
                    id: stepBackground

                    border.color: Theme.border
                    border.width: Theme.borderWidth
                    // pressed читается у самой кнопки: внутри фона свойства с
                    // таким именем нет, и обращение давало ReferenceError.
                    color: stepButton.pressed ? Theme.accentTint : Theme.surfaceMuted
                    // Правый внешний угол повторяет скругление поля, левые
                    // кнопки квадратные и стыкуются с ним без зазора.
                    radius: modelData.radius

                    Behavior on color {
                        ColorAnimation {
                            duration: 100
                        }
                    }
                }
                contentItem: Label {
                    color: Theme.textSecondary
                    font: Typography.bodyStrong
                    horizontalAlignment: Text.AlignHCenter
                    text: parent.text
                    verticalAlignment: Text.AlignVCenter
                }
                // Значение зажимается в границы здесь: за её пределами цель или
                // вес недостижимы, и такую цифру нельзя ни сохранить, ни показать.
                onClicked: field.setValue(field.value + modelData.step)
            }
        }
    }

    // Единственная точка записи значения: и кнопки, и внешний код идут через
    // неё, поэтому зажим в границы не может быть обойдён.
    function setValue(newValue) {
        const clamped = Math.max(field.from, Math.min(field.to, newValue));

        if (field.value === clamped)
            return;

        field.value = clamped;
        field.edited(clamped);
    }
}
