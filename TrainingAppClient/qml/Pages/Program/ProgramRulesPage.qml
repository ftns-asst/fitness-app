import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Правила программы и отметки пользователя (экран 43, §3.6–3.7).
//
// Состояние за сегодня приходит ролью todayState из Avm_Program_Rules, окно
// отметок — periodDays. Расчётные правила (computedFrom*) показывают
// «неизвестно»: их проверяет движок правил по фактам тренировок, а не сам
// пользователь, поэтому отметить их здесь нельзя (§3.6).
PageScaffold {
    id: page

    property var viewModel: ProgramRulesVM
    // Панель возврата обязательна: экраны программы открыты поверх вкладок, где
    // таб-бара нет.
    BackBar {
        Layout.fillWidth: true
        onClicked: page.backRequested()
        text: qsTr("Назад")
        visible: true
    }

    caption: page.hasProgram ? qsTr("%1 · окно %2…%3").arg(page.today).arg(page.rangeFrom).arg(page.rangeTo) : qsTr("Нет активной программы")
    title: qsTr("Правила и отметки")

    readonly property bool hasProgram: page.viewModel !== undefined && page.viewModel !== null && page.viewModel.hasProgram
    readonly property string rangeFrom: page.hasProgram ? page.viewModel.rangeFrom : ""
    readonly property string rangeTo: page.hasProgram ? page.viewModel.rangeTo : ""
    readonly property string today: page.hasProgram ? page.viewModel.today : ""

    // Загрузка идёт при создании и при каждом показе: у страницы, открытой
    // прямо в StackView (например, по --screen), visible не меняется, и без
    // Component.onCompleted правила остались бы незагруженными.
    Component.onCompleted: page.reload()
    onVisibleChanged: {
        if (visible)
            page.reload();
    }

    // Идентификатор берётся из AppContext: литерал «u1» повторялся в четырёх
    // экранах, и любое другое значение делало программу невидимой.
    function currentUserId() {
        return AppContext !== undefined && AppContext !== null ? AppContext.currentUserId : "u1";
    }

    function reload() {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        page.viewModel.reload(page.currentUserId());
    }

    Label {
        color: Theme.textMuted
        font: Typography.sectionTitle
        text: qsTr("Выполнено за сегодня")
        visible: page.hasProgram
        width: parent.width
        wrapMode: Text.WordWrap
    }
    // — Нет активной программы: объяснение вместо пустого экрана —
    // Раньше здесь была только подпись заголовка, и экран выглядел как
    // сломанный: ни карточек, ни объяснения, что делать дальше.
    Label {
        color: Theme.textSecondary
        font: Typography.body
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("Создайте программу, чтобы вести правила и отметки.")
        visible: page.hasProgram === false
        width: parent.width
        wrapMode: Text.WordWrap
    }
    AppCard {
        visible: page.hasProgram
        // Ширину задаёт колонка страницы: без неё карточка брала бы
        // собственный implicitWidth (240) и обрезала содержимое.
        width: parent.width

        RowLayout {
            width: parent.width
            spacing: Spacing.itemGap

            Label {
                Layout.fillWidth: true
                // Метрика «40 %» не сжимается сама: без нулевого минимума
                // строка запрашивала бы ширину по тексту и вылезала за
                // правый край карточки на узких экранах.
                Layout.minimumWidth: 0
                color: Theme.textPrimary
                font: Typography.metric
                text: page.adherenceText()
            }
            AppButton {
                onClicked: page.cyclePeriod()
                text: page.periodText()
                variant: AppButton.Secondary
            }
        }
    }
    // — Группировка по категориям —
    // Правила идут списком по модели, а на экране они сгруппированы: человек
    // думает «что у меня по питанию», а не «правило номер два». Заголовок
    // группы несёт и итог («2 из 2»), поэтому прогресс виден, не читая каждое
    // правило (макет 05).
    Label {
        color: Theme.textMuted
        font: Typography.sectionTitle
        text: qsTr("Правила и отметки")
        visible: page.hasProgram
        width: parent.width
        wrapMode: Text.WordWrap
    }
    Label {
        color: Theme.textSecondary
        font: Typography.caption
        text: qsTr("Ручные правила отмечаются за сегодня; расчётные считаются по данным программы.")
        visible: page.hasProgram
        width: parent.width
        wrapMode: Text.WordWrap
    }
    Repeater {
        // count передаётся аргументом ради зависимости привязки: внутри функции
        // обращения к свойству не отслеживаются, и группы не перестроились бы
        // ни после загрузки, ни после отметки. Property `count`, а не вызов
        // `rowCount()`: вызов метода зависимости не создаёт, и группы замирали
        // бы на значении первого вычисления.
        model: page.hasProgram ? page.ruleGroups(page.viewModel.rules.count, page.viewModel.adherencePercent) : []

        delegate: AppCard {
            id: groupCard

            // modelData, а не required property: модель группы — массив
            // объектов QML, и именованной инъекции у него нет.
            readonly property var group: modelData

            readonly property var rows: groupCard.group.rows

            paddingHorizontal: 0
            paddingVertical: Spacing.itemGap
            spacing: 0
            width: parent.width

            // — Заголовок группы: категория, точки состояний и счётчик —
            RowLayout {
                Layout.fillWidth: true
                anchors.left: parent.left
                anchors.leftMargin: Spacing.screenPadding
                anchors.right: parent.right
                anchors.rightMargin: Spacing.screenPadding
                spacing: Spacing.itemGap

                Label {
                    Layout.fillWidth: true
                    Layout.minimumWidth: 0
                    color: Theme.textPrimary
                    elide: Text.ElideRight
                    font: Typography.bodyStrong
                    text: groupCard.group.title
                }
                Row {
                    id: dotsRow

                    spacing: 3

                    // Точки повторяют состояния правил группы: иконка без
                    // подписи ничего не скажет, а текст «Выполнено» в каждой
                    // точке превратил бы её в пилюлю.
                    Repeater {
                        model: groupCard.rows.length

                        delegate: Rectangle {
                            // checkState, а не state: state уже есть у QQuickItem (состояния и
                            // переходы), и перекрытие молча ломало бы механизм состояний.
                            readonly property string checkState: groupCard.rows[index].todayState

                            color: page.dotColor(checkState)
                            height: 6
                            radius: 3
                            width: 6
                        }
                    }
                }
                Label {
                    color: Theme.textMuted
                    font: Typography.caption
                    // Знаменатель — число проверяемых правил, а не всех строк
                    // группы: рекомендация не имеет отметки, и с ней в знаменателе
                    // группа не могла бы закрыться полностью.
                    text: groupCard.group.doneCount + " " + qsTr("из") + " " + groupCard.group.checkableCount
                }
            }
            // — Правила группы: точка состояния, название, пилюля и кнопки —
            Repeater {
                // Модель — число, а не массив: у вложенного Repeater `modelData`
                // разрешается в модель внешнего (то есть группы), и строка
                // читалась как объект группы — заголовком категории вместо
                // правила. С числом `index` точно принадлежит этому Repeater, а
                // строка берётся из массива группы по нему.
                model: groupCard.rows.length

                delegate: Item {
                    id: ruleRow

                    required property int index
                    readonly property var row: groupCard.rows[index]

                    readonly property bool canCheck: row.isAdvice === false
                    readonly property bool isDone: row.todayState === "done"

                    // Высота и содержимое — одной колонкой: кнопки отметки стоят
                    // под текстом отдельным рядом (макет 05), и считать высоту
                    // строки по одной только текстовой части значило бы
                    // наезжать кнопок на следующее правило.
                    height: ruleColumn.implicitHeight
                    width: parent.width

                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.leftMargin: Spacing.screenPadding
                        anchors.right: parent.right
                        anchors.rightMargin: Spacing.screenPadding
                        color: Theme.border
                        height: Theme.borderWidth
                        // index у массива Repeater есть, но qmllint не видит его у Item:
                        // разделитель под первым правилом группы не рисуется.
                        visible: ruleRow.index > 0
                    }
                    ColumnLayout {
                        id: ruleColumn

                        anchors.left: parent.left
                        anchors.leftMargin: Spacing.screenPadding
                        anchors.right: parent.right
                        anchors.rightMargin: Spacing.screenPadding
                        anchors.top: parent.top
                        spacing: Spacing.itemGap

                        RowLayout {
                            Layout.fillWidth: true
                            Layout.minimumWidth: 0
                            spacing: Spacing.itemGap

                            ColumnLayout {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                spacing: 2

                                Label {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    // Название правила — произвольный текст
                                    // пользователя: без нулевого минимума строка
                                    // не сжималась бы и вылезала за край карточки.
                                    color: Theme.textPrimary
                                    font: Typography.bodyStrong
                                    text: ruleRow.row.title
                                    wrapMode: Text.WordWrap
                                }
                                Label {
                                    Layout.fillWidth: true
                                    Layout.minimumWidth: 0
                                    // Формулировка правила — длинный текст: без
                                    // заполнения и нулевого минимума он задавал бы
                                    // ширину всей колонке.
                                    color: Theme.textSecondary
                                    font: Typography.caption
                                    text: ruleRow.row.displayText
                                    wrapMode: Text.WordWrap
                                }
                            }
                            CheckinStatePill {
                                checkinState: ruleRow.row.todayState
                                // У рекомендации нет отметки и не может быть: пилюля
                                // «Нет отметки» читалась бы как недочёт программы.
                                visible: ruleRow.canCheck
                            }
                        }
                        // Отметка вручную: чужой сессией это не считается,
                        // поэтому кнопки есть только у правил, которые
                        // пользователь ведёт сам.
                        RowLayout {
                            Layout.fillWidth: true
                            spacing: Spacing.itemGap
                            visible: ruleRow.canCheck && ruleRow.row.todayState !== "computed"

                            AppButton {
                                Layout.fillWidth: true
                                // Подпись — «Выполнено»/«Не выполнено»: без нулевого
                                // минимума пара кнопок шире карточки на узком экране.
                                Layout.minimumWidth: 0
                                onClicked: page.save(ruleRow.row.ruleId, "done")
                                text: qsTr("Выполнено")
                                variant: ruleRow.isDone ? AppButton.Primary : AppButton.Secondary
                            }
                            AppButton {
                                Layout.fillWidth: true
                                Layout.minimumWidth: 0
                                onClicked: page.save(ruleRow.row.ruleId, "failed")
                                text: qsTr("Не выполнено")
                                variant: ruleRow.row.todayState === "failed" ? AppButton.Primary : AppButton.Secondary
                            }
                        }
                    }
                }
            }
        }
    }
    Label {
        color: Theme.textSecondary
        font: Typography.body
        horizontalAlignment: Text.AlignHCenter
        text: qsTr("Отметок за это окно нет")
        visible: page.hasProgram && page.checkinCount === 0
        width: parent.width
        wrapMode: Text.WordWrap
    }

    // Запрос возврата: экраны программы открыты поверх вкладок, где таб-бара нет.
    signal backRequested
    readonly property int checkinCount: page.hasProgram ? page.viewModel.checkinCount : 0
    readonly property int periodDays: page.hasProgram ? page.viewModel.periodDays : 7

    // Порядок категорий повторяет модель (Nutrition → … → Other, §3.6): свой
    // порядок в разметке означал бы, что группы на экране разойдутся с тем,
    // что считает домен.
    readonly property var categoryOrder: ["nutrition", "training", "recovery", "lifestyle", "technique", "other"]

    // Правила, собранные по категориям для отрисовки группами. Группировка
    // делается здесь, а не в C++: модель отдаёт правила по порядку создания,
    // а экран нужен по категориям, и это чисто представление.
    // Аргументы не используются в логике: они нужны, чтобы привязка модели
    // пересчитывалась. Обращения к модели внутри функции QML не отслеживает,
    // поэтому без них группы не перестроились бы ни после загрузки правил, ни
    // после отметки — экран остался бы с пустым списком (QM.06).
    function ruleGroups(rowCount, adherence) {
        if (page.viewModel === undefined || page.viewModel === null)
            return [];

        const model = page.viewModel.rules;

        // adherence — только ради отслеживания привязкой (см. вызов).
        const buckets = [];

        for (let i = 0; i < page.categoryOrder.length; ++i) {
            const key = page.categoryOrder[i];
            const rows = [];

            for (let j = 0; j < model.rowCount(); ++j) {
                const row = model.get(j);

                if (row.category === key)
                    rows.push(row);
            }

            if (rows.length === 0)
                continue;

            buckets.push({
                checkableCount: page.checkableCount(rows),
                doneCount: page.doneCount(rows),
                key: key,
                rows: rows,
                title: page.categoryTitle(key)
            });
        }

        return buckets;
    }

    function doneCount(rows) {
        let count = 0;

        for (let i = 0; i < rows.length; ++i) {
            // Рекомендация не проверяется и отметки не имеет, поэтому в счёт
            // «N из M» она не входит: иначе группу нельзя было бы закрыть
            // полностью (§3.6).
            if (rows[i].isAdvice === true)
                continue;

            if (rows[i].todayState === "done")
                count += 1;
        }

        return count;
    }

    function checkableCount(rows) {
        let count = 0;

        for (let i = 0; i < rows.length; ++i) {
            if (rows[i].isAdvice === true)
                continue;

            count += 1;
        }

        return count;
    }

    function categoryTitle(key) {
        const titles = {
            lifestyle: qsTr("Активность"),
            nutrition: qsTr("Питание"),
            other: qsTr("Другое"),
            recovery: qsTr("Восстановление"),
            technique: qsTr("Техника"),
            training: qsTr("Тренировки")
        };

        return titles[key] !== undefined ? titles[key] : key;
    }

    // Цвет точки состояния в заголовке группы. Совпадает с палитрой пилюли
    // (CheckinStatePill): один смысл — один цвет, иначе точка и пилюля об одном
    // и том же говорили бы разное.
    function dotColor(state) {
        if (state === "done")
            return Theme.accent;

        if (state === "failed")
            return Theme.negative;

        return Theme.border;
    }

    function save(ruleId, state) {
        if (page.viewModel === undefined || page.viewModel === null)
            return;

        page.viewModel.saveCheckin(ruleId, state, 0, false, "");
    }

    // Окно отметок переключается по кругу: неделя → месяц → квартал (§5.1).
    function cyclePeriod() {
        const order = [7, 30, 90];
        const index = order.indexOf(page.periodDays);

        page.viewModel.setPeriodDays(order[(index + 1) % order.length]);
    }

    function periodText() {
        if (page.periodDays === 30)
            return qsTr("Месяц");

        if (page.periodDays === 90)
            return qsTr("Квартал");

        return qsTr("Неделя");
    }

    function adherenceText() {
        if (page.hasProgram === false || page.viewModel.hasAdherencePercent === false)
            return "—";

        return qsTr("%1 %").arg(Math.round(page.viewModel.adherencePercent));
    }
}
