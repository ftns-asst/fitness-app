import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Карточка «Сколько мне есть» на главной (спринт S7, issue 50).
//
// Показывает числа авторасчёта КБЖУ и предлагает пересчитать. Пересчёт —
// отдельное действие (§9.2 доклада): при изменении веса цель молча не должна
// ехать, и решение о новом расчёте остаётся за человеком. Экран только сообщает
// намерение, навигацию выполняет каркас.
//
// Формат чисел повторяет экран расчёта: «1 843 ккал» и «132 г» — один и тот же
// результат не должен выглядеть по-разному на двух экранах.
AppCard {
    id: card

    property real caloriesKcal: 0
    property real carbsG: 0
    // Дата расчёта строкой YYYY-MM-DD: так её отдаёт profile_local, и разметка
    // не тащит сюда QDate.
    property string calculatedAt: ""
    property real fatG: 0
    // Расчёт уже делали: показываются числа, а не приглашение.
    property bool hasPlan: false
    property real proteinG: 0

    signal recalculateRequested

    // Подпись показывает, когда считали: числа зависят от веса, и без даты
    // «82 кг» из расчёта полугодовой давности выглядело бы свежим.
    readonly property string calculatedCaption: {
        if (card.calculatedAt.length !== 10)
            return "";

        const day = Number(card.calculatedAt.substring(8, 10));
        const month = Qt.locale().standaloneMonthName(Number(card.calculatedAt.substring(5, 7)) - 1);

        return qsTr("Посчитано %1 %2").arg(day).arg(month);
    }
    readonly property string caloriesText: card.hasPlan ? card.caloriesKcal + " " + qsTr("ккал") : ""
    // Макросы одной строкой: они сопоставимы между собой, и ряд читается сразу.
    readonly property var macroRows: [
        {
            caption: qsTr("Белки"),
            gram: card.proteinG,
            objectName: "nutritionProtein"
        },
        {
            caption: qsTr("Жиры"),
            gram: card.fatG,
            objectName: "nutritionFat"
        },
        {
            caption: qsTr("Углеводы"),
            gram: card.carbsG,
            objectName: "nutritionCarbs"
        }
    ]

    objectName: "nutritionCard"
    spacing: Spacing.itemGap
    width: parent.width

    Label {
        color: Theme.textPrimary
        font: Typography.bodyStrong
        text: qsTr("Сколько мне есть")
        width: parent.width
        wrapMode: Text.WordWrap
    }
    Label {
        objectName: "nutritionCaption"

        color: Theme.textMuted
        font: Typography.caption
        text: card.hasPlan ? card.calculatedCaption : qsTr("Калории и белки, жиры, углеводы — по весу, росту, возрасту и активности.")
        width: parent.width
        wrapMode: Text.WordWrap
    }
    // Числа: калорийность крупно, макросы тремя равными колонками — как в
    // карточке результата на экране расчёта.
    ColumnLayout {
        // Ширина — явно по родителю: карточка держит детей в обычной Column, где
        // `Layout.fillWidth` не работает (см. PageScaffold.qml), и без привязки
        // числа вставали бы узким блоком у левого края.
        //
        // objectName нужен тесту: у скрытого блока элементы остаются в дереве, и
        // проверять приходится его видимость, а не наличие.
        objectName: "nutritionNumbers"
        spacing: 0
        visible: card.hasPlan
        width: parent.width

        Label {
            Layout.fillWidth: true
            color: Theme.textSecondary
            font: Typography.caption
            text: qsTr("Калорийность")
        }
        Label {
            Layout.fillWidth: true
            color: Theme.accent
            font: Typography.metric
            objectName: "nutritionCalories"
            text: card.caloriesText
        }
    }
    RowLayout {
        // То же, что у колонки выше: ширина задаётся явно.
        spacing: Spacing.itemGap
        visible: card.hasPlan
        width: parent.width

        Repeater {
            model: card.macroRows

            delegate: ColumnLayout {
                required property var modelData

                Layout.fillWidth: true
                spacing: 0

                Label {
                    Layout.fillWidth: true
                    color: Theme.textMuted
                    font: Typography.caption
                    horizontalAlignment: Text.AlignHCenter
                    text: modelData.caption
                }
                Label {
                    Layout.fillWidth: true
                    color: Theme.textPrimary
                    font: Typography.bodyStrong
                    horizontalAlignment: Text.AlignHCenter
                    objectName: modelData.objectName
                    text: modelData.gram + " " + qsTr("г")
                }
            }
        }
    }
    AppButton {
        objectName: "nutritionRecalculateButton"

        text: card.hasPlan ? qsTr("Пересчитать") : qsTr("Рассчитать")
        variant: AppButton.Secondary
        width: parent.width

        onClicked: card.recalculateRequested()
    }
}
