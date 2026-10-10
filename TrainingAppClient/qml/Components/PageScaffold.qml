import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Каркас экрана: фон, заголовок с подписью и прокручиваемая область контента.
// Дети страницы попадают в колонку контента; anchors внутри детей не использовать.
//
// Колонка контента — обычная Column, поэтому `Layout.fillWidth` у её детей
// не действует: ширину ребёнка задаёт сам ребёнок. Поэтому ряды карточек ниже
// объявляют `width: parent.width` явно, иначе схлопывались бы до
// `MetricCard.implicitWidth` и стояли узким блоком у левого края.
Rectangle {
    id: scaffold

    property real bottomInset: 0
    property string caption: ""
    default property alias content: bodyColumn.data
    // Содержимое закреплённой нижней панели: страница задаёт его как
    // `footer: RowLayout { … }`, а каркас держит панель на экране при любой
    // длине контента. Свойство не по умолчанию: по умолчанию остаётся контент,
    // иначе ни одна существующая страница не собралась бы.
    property alias footer: footerRow.data
    // Прокручиваемая область экрана. Открыта наружу, чтобы жест по «пустой»
    // области вокруг контента на широком экране тоже листал страницу
    // (`qml/Components/DragToScroll.qml`).
    readonly property alias pageFlickable: flick
    property alias spacing: bodyColumn.spacing
    property string title: ""
    property real topInset: 0

    // Закреплённая нижняя панель. Её содержимое живёт вне прокручиваемой
    // колонки и всегда на экране: кнопка завершения не должна требовать
    // прокрутки до самого низа — на длинном экране настройки дня это лишний
    // жест перед самым важным действием.
    readonly property real footerHeight: footer.visible ? footer.implicitHeight : 0

    // — Эластичный overscroll (Android-стиль) —
    //
    // На touch-платформах границы позволяют дотянуть контент за край с плавным
    // пружинным возвратом (DragAndOvershootBounds): прокрутка у конца списка не
    // обрывается на границе, а докатывается и мягко возвращается. Поверх этого
    // контент «растягивается» (Material 3 «stretch», Android 12): по вертикали
    // он вытягивается пропорционально перерасходу (`verticalOvershoot`), по
    // горизонтали чуть сжимается; точка закрепления — край, противоположный
    // overscroll, поэтому ближний к пальцу край видно тянущимся. На десктопе
    // колесо за границу не тянет — там остаётся StopAtBounds.
    readonly property bool elasticScroll: Theme.touchPlatform
    // Перерасход за границу в долях высоты области, 0…1.
    readonly property real overscrollRatio: elasticScroll ? Math.min(1, Math.abs(flick.verticalOvershoot) / Math.max(1, flick.height)) : 0
    // Сила растяжения: при перерасходе на высоту всей области контент вытянут
    // до 10% по вертикали и сжат до 3% по горизонтали — заметно, но не карикатурно.
    readonly property real stretchRatio: overscrollRatio * 0.1

    color: Theme.background

    Column {
        id: header

        anchors.left: parent.left
        anchors.leftMargin: Spacing.screenPadding
        anchors.right: parent.right
        anchors.rightMargin: Spacing.screenPadding
        anchors.top: parent.top
        anchors.topMargin: scaffold.topInset + Spacing.screenPadding
        spacing: 4

        Label {
            color: Theme.textPrimary
            font: Typography.screenTitle
            text: scaffold.title
            width: parent.width
            wrapMode: Text.WordWrap
        }
        Label {
            color: Theme.textSecondary
            font: Typography.caption
            text: scaffold.caption
            visible: text.length > 0
            width: parent.width
            wrapMode: Text.WordWrap
        }
    }
    // Панель действий: закреплена снизу и отделена от контента тонкой линией.
    // Содержимое задаёт страница через `page.footer`, поэтому порядок и вид
    // кнопок остаются её делом, а закрепление — делом каркаса.
    Item {
        id: footer

        anchors.bottom: parent.bottom
        anchors.bottomMargin: scaffold.bottomInset
        anchors.left: parent.left
        anchors.right: parent.right
        // Отступы колонки повторяют Flickable, иначе панель была бы шире
        // области контента и кнопки не совпадали бы с карточками.
        anchors.leftMargin: Spacing.screenPadding
        anchors.rightMargin: Spacing.screenPadding
        visible: footerRow.children.length > 0
        // Пустая панель не должна занимать место, иначе подвал сместил бы контент
        // на невидимый отступ.
        implicitHeight: visible ? footerRow.implicitHeight + Spacing.itemGap * 2 : 0

        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            color: Theme.background

            Rectangle {
                anchors.left: parent.left
                anchors.right: parent.right
                anchors.top: parent.top
                color: Theme.borderSubtle
                height: 1
            }
        }

        RowLayout {
            id: footerRow

            anchors.bottom: parent.bottom
            anchors.bottomMargin: Spacing.itemGap
            anchors.left: parent.left
            anchors.right: parent.right
            spacing: Spacing.itemGap
        }
    }
    Flickable {
        id: flick

        ScrollIndicator.vertical: Theme.touchPlatform ? touchIndicator : null
        anchors.bottom: parent.bottom
        // Панель действий перекрывает низ области прокрутки, поэтому под неё
        // резервируется высота: без резерва последняя карточка дня оказывалась
        // под кнопками и не доезжала до конца.
        anchors.bottomMargin: scaffold.bottomInset + scaffold.footerHeight
        anchors.left: parent.left
        // Контент отстоит от краёв экрана, как карточки в референсе (§3.3).
        anchors.leftMargin: Spacing.screenPadding
        anchors.right: parent.right
        anchors.rightMargin: Spacing.screenPadding
        anchors.top: header.bottom
        anchors.topMargin: header.visible ? Spacing.sectionGap : 0
        bottomMargin: Spacing.sectionGap
        boundsBehavior: scaffold.elasticScroll ? Flickable.DragAndOvershootBounds : Flickable.StopAtBounds
        clip: true
        contentHeight: bodyColumn.implicitHeight
        contentWidth: width
        topMargin: Spacing.safeGap

        // Десктоп: привычный скроллбар. Touch: только транзиентный индикатор
        // (у ScrollIndicator нет policy, поэтому на десктопе он не подключается).
        ScrollBar.vertical: ScrollBar {
            policy: Theme.touchPlatform ? ScrollBar.AlwaysOff : ScrollBar.AsNeeded
        }

        ScrollIndicator {
            id: touchIndicator
        }
        Column {
            id: bodyColumn

            // Растяжение при overscroll (см. scaffold.elasticScroll). Scale, а не
            // `scale`: якорь зависит от стороны overscroll — закрепляется дальний
            // от пальца край, как в Android StretchEffect. Трансформация чисто
            // визуальная: layout и contentHeight она не трогает, привязки
            // не зацикливаются, а clip Flickable не даёт растянутому контенту
            // вылезти за пределы области.
            transform: Scale {
                origin.x: bodyColumn.width / 2
                origin.y: flick.verticalOvershoot > 0 ? 0 : bodyColumn.height
                xScale: 1 - scaffold.stretchRatio * 0.3
                yScale: 1 + scaffold.stretchRatio
            }

            // Layout.* у детей этой колонки не работает: Column — не layout, и
            // привязки `Layout.fillWidth` игнорируются. Поэтому ширину ребёнка
            // задаёт сам ребёнок (`width: parent.width`), а вложенные
            // ColumnLayout/RowLayout внутри — своими Layout-свойствами.
            spacing: Spacing.sectionGap
            width: flick.width
        }
    }
}
