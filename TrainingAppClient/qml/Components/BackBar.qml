import QtQuick
import QtQuick.Controls
import TrainingAppClient

// Панель возврата для экранов вне вкладок (срез W11). Детали программы
// открыты поверх вкладок, поэтому «назад» тут обязателен: без него
// пользователь оказывается в тупике — таб-бара нет, а назад по системе
// работает неочевидно.
Item {
    id: bar

    property string text: qsTr("Назад")
    signal clicked

    implicitHeight: Theme.buttonSecondary
    // Ширина задаётся родителем: панель лежит в обычной Column PageScaffold,
    // где Layout.fillWidth не действует, и без этой привязки элемент вместе с
    // кнопкой схлопывался в ноль — «назад» был в разметке, но не на экране.
    implicitWidth: backButton.implicitWidth
    width: parent !== null ? parent.width : implicitWidth

    AppButton {
        id: backButton

        anchors.left: parent.left
        anchors.verticalCenter: parent.verticalCenter
        height: Theme.buttonSecondary
        onClicked: bar.clicked()
        text: "← " + bar.text
        variant: AppButton.Secondary
    }
}
