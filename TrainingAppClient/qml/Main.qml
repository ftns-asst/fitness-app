import QtQuick
import QtQuick.Controls
import TrainingAppClient

ApplicationWindow {
    id: window

    readonly property int deviceWidth: 390
    readonly property int deviceHeight: 812
    readonly property int bezel: 12
    readonly property bool framed: Demo.phoneFrame

    // — Режим снимка для дизайн-ревью и CI: --screenshot <путь> —
    readonly property int screenshotArgIndex: Qt.application.arguments.indexOf("--screenshot")
    readonly property bool screenshotMode: screenshotArgIndex >= 0

    // — Аргументы дизайн-ревью: --tab <0..3>, --dark, --width/--height, --diag —
    readonly property int tabArgIndex: Qt.application.arguments.indexOf("--tab")
    readonly property int initialTab: tabArgIndex >= 0 && tabArgIndex + 1 < Qt.application.arguments.length ? Number(Qt.application.arguments[tabArgIndex + 1]) : 0

    readonly property bool diagMode: Qt.application.arguments.indexOf("--diag") >= 0

    function argNumber(name, fallback) {
        const index = Qt.application.arguments.indexOf(name);

        return index >= 0 && index + 1 < Qt.application.arguments.length ? Number(Qt.application.arguments[index + 1]) : fallback;
    }

    visible: true
    title: qsTr("Фитнес-помощник — UI демо")
    width: framed ? deviceWidth + bezel * 2 : argNumber("--width", 420)
    height: framed ? deviceHeight + bezel * 2 : argNumber("--height", 820)
    minimumWidth: 360
    minimumHeight: 640
    color: framed ? "#101216" : Theme.background

    Component.onCompleted: {
        Demo.phoneFrame = Qt.application.arguments.indexOf("--frame") >= 0 || screenshotMode;
        Theme.darkMode = Qt.application.arguments.indexOf("--dark") >= 0;

        const accent_index = Qt.application.arguments.indexOf("--accent");

        if (accent_index >= 0 && accent_index + 1 < Qt.application.arguments.length) {
            const value = Number(Qt.application.arguments[accent_index + 1]);

            if (value >= 0 && value < Theme.accentPalette.length) {
                Theme.accentIndex = value;
            }
        }

        if (screenshotMode) {
            captureTimer.start();
        }

        if (window.diagMode) {
            diagTimer.start();
        }

        PlatformChrome.applyChrome(Theme.darkMode);
    }

    // Системные полосы Android перекрашиваются под тему (no-op на десктопе).
    Connections {
        target: Theme

        function onDarkModeChanged() {
            PlatformChrome.applyChrome(Theme.darkMode);
        }
    }

    Timer {
        id: captureTimer

        // Ждём первый кадр и завершение стартовых анимаций.
        interval: 700

        onTriggered: deviceScreen.grabToImage(function (result) {
            const args = Qt.application.arguments;
            const path = window.screenshotArgIndex + 1 < args.length ? args[window.screenshotArgIndex + 1] : "ui-preview.png";

            result.saveToFile(path);
            quitTimer.start();
        })
    }

    Timer {
        id: quitTimer

        interval: 300

        onTriggered: Qt.quit()
    }

    // — Аудит вёрстки (--diag): печатает в stderr переполнения текста и выход
    // элементов за границы родителя. Заменяет визуальный осмотр при проверке
    // адаптивности на разных ширинах (--width 360 / 390 / 480).
    Timer {
        id: diagTimer

        interval: 1500

        onTriggered: {
            window.runLayoutAudit();
            Qt.quit();
        }
    }

    function auditPath(item) {
        const names = [];

        let node = item;
        let guard = 0;

        while (node !== null && guard < 5) {
            names.unshift(String(node).replace(/\(0x[0-9a-fA-F]+\)/, ""));
            node = node.parent;
            ++guard;
        }

        return names.join(" > ");
    }

    function runLayoutAudit() {
        const problems = [];

        function walk(item) {
            // Ветви с нулевой геометрией не проверяем: неактивные вкладки
            // StackLayout имеют width 0, а их дети — отрицательную ширину
            // (0 минус отступы), что даёт сотни ложных срабатываний.
            if (item.width <= 0 || item.height <= 0) {
                return;
            }

            const kids = item.children;

            for (let i = 0; i < kids.length; ++i) {
                const child = kids[i];

                // Внутренности MultiEffect (QGfxSourceProxy, ShaderEffect) всегда
                // больше источника на auto-padding тени — это не дефект вёрстки.
                if (String(child.parent).indexOf("MultiEffect") >= 0) {
                    continue;
                }

                // Проверяем и скрытые ветви: вкладки StackLayout и сегменты
                // «Профиля» всё равно будут показаны пользователю.
                // Текст, который не влезает в свой элемент.
                if (child.text !== undefined && child.paintedWidth !== undefined) {
                    if (child.paintedWidth > child.width + 1) {
                        problems.push("TEXT-WIDTH " + window.auditPath(child) + " painted=" + Math.round(child.paintedWidth) + " width=" + Math.round(child.width) + " text=\"" + child.text + "\"");
                    }

                    if (child.paintedHeight > child.height + 1) {
                        problems.push("TEXT-HEIGHT " + window.auditPath(child) + " painted=" + Math.round(child.paintedHeight) + " height=" + Math.round(child.height) + " text=\"" + child.text + "\"");
                    }
                }

                // Элемент вылезает за правую границу родителя.
                const host = child.parent;

                // Контент прокручиваемого Flickable шире вьюпорта по замыслу
                // (ряд chips) — это не дефект.
                const scrollable = host !== null && host.contentWidth !== undefined && host.contentWidth > host.width + 1;

                if (host !== null && host.width !== undefined && !scrollable && child.x + child.width > host.width + 1) {
                    problems.push("OVERFLOW-X " + window.auditPath(child) + " x=" + Math.round(child.x) + " width=" + Math.round(child.width) + " parent=" + Math.round(host.width));
                }

                // Схлопнутый элемент с непустым содержимым.
                if (child.width <= 0 && child.implicitWidth > 0) {
                    problems.push("ZERO-WIDTH " + window.auditPath(child));
                }

                walk(child);
            }
        }

        walk(deviceScreen);

        // Доступность Inter: Qt.fontFamilies() включает шрифты приложения,
        // поэтому видно, зарегистрировались ли начертания и под какими
        // семействами (важно для маппинга весов Medium/DemiBold/Bold).
        const families = Qt.fontFamilies();
        const inter = families.filter(function (name) {
            return name.indexOf("Inter") >= 0;
        });

        console.log("[audit] interFamilies=" + JSON.stringify(inter) + " totalFamilies=" + families.length);
        console.log("[audit] fontWeights15 normal=" + Math.round(fontProbe.normal15) + " medium=" + Math.round(fontProbe.medium15) + " demibold=" + Math.round(fontProbe.demiBold15) + " bold=" + Math.round(fontProbe.bold15));
        console.log("[audit] fontWeights40 normal=" + Math.round(fontProbe.normal40) + " medium=" + Math.round(fontProbe.medium40) + " demibold=" + Math.round(fontProbe.demiBold40) + " bold=" + Math.round(fontProbe.bold40));
        console.log("[audit] contentWidth=" + Math.round(deviceScreen.width) + " problems=" + problems.length);

        for (let p = 0; p < problems.length; ++p) {
            console.log("[audit] " + problems[p]);
        }
    }

    // — Зонд шрифтов: измеряем advanceWidth одного и того же текста в четырёх
    // весах при 15 и 40 px (TextMetrics, без визуальных элементов). Если ширины
    // совпадают, вес не сопоставился с начертанием (например, Medium лежит
    // в отдельном семействе) — это видно в выводе --diag без визуального осмотра.
    // Контейнер — Item: у QtObject нет default-свойства для вложенных объектов.
    Item {
        id: fontProbe

        visible: false
        width: 0
        height: 0

        readonly property string sampleText: "Приседания со штангой 62,5"

        readonly property real normal15: metricsNormal15.advanceWidth
        readonly property real medium15: metricsMedium15.advanceWidth
        readonly property real demiBold15: metricsDemiBold15.advanceWidth
        readonly property real bold15: metricsBold15.advanceWidth
        readonly property real normal40: metricsNormal40.advanceWidth
        readonly property real medium40: metricsMedium40.advanceWidth
        readonly property real demiBold40: metricsDemiBold40.advanceWidth
        readonly property real bold40: metricsBold40.advanceWidth

        TextMetrics {
            id: metricsNormal15

            font: Qt.font({
                family: Typography.fontFamily,
                pixelSize: 15,
                weight: Font.Normal
            })
            text: fontProbe.sampleText
        }

        TextMetrics {
            id: metricsMedium15

            font: Qt.font({
                family: Typography.fontFamilyMedium,
                pixelSize: 15,
                weight: Font.Medium
            })
            text: fontProbe.sampleText
        }

        TextMetrics {
            id: metricsDemiBold15

            font: Qt.font({
                family: Typography.fontFamilyDemiBold,
                pixelSize: 15,
                weight: Font.DemiBold
            })
            text: fontProbe.sampleText
        }

        TextMetrics {
            id: metricsBold15

            font: Qt.font({
                family: Typography.fontFamily,
                pixelSize: 15,
                weight: Font.Bold
            })
            text: fontProbe.sampleText
        }

        TextMetrics {
            id: metricsNormal40

            font: Qt.font({
                family: Typography.fontFamily,
                pixelSize: 40,
                weight: Font.Normal
            })
            text: fontProbe.sampleText
        }

        TextMetrics {
            id: metricsMedium40

            font: Qt.font({
                family: Typography.fontFamilyMedium,
                pixelSize: 40,
                weight: Font.Medium
            })
            text: fontProbe.sampleText
        }

        TextMetrics {
            id: metricsDemiBold40

            font: Qt.font({
                family: Typography.fontFamilyDemiBold,
                pixelSize: 40,
                weight: Font.DemiBold
            })
            text: fontProbe.sampleText
        }

        TextMetrics {
            id: metricsBold40

            font: Qt.font({
                family: Typography.fontFamily,
                pixelSize: 40,
                weight: Font.Bold
            })
            text: fontProbe.sampleText
        }
    }

    Item {
        id: deviceScreen

        // Responsive: на широком десктопном окне контент не растягивается на всю
        // ширину, а остаётся в телефонной пропорции и центрируется (§6.3).
        width: window.framed ? window.deviceWidth : Math.min(window.width, Theme.contentMaxWidth)
        height: window.framed ? window.deviceHeight : window.height
        anchors.centerIn: parent
        clip: true

        // — Safe area (Qt ≥ 6.9): на устройстве реальные insets, на десктопе 0 —
        readonly property real safeTop: SafeArea.margins.top
        readonly property real safeBottom: SafeArea.margins.bottom

        readonly property real contentTopInset: window.framed ? statusBarMock.height : safeTop
        readonly property real contentBottomInset: window.framed ? homeIndicator.height + 8 : safeBottom

        Rectangle {
            anchors.fill: parent
            color: Theme.background
        }

        AppShell {
            anchors.fill: parent
            topInset: deviceScreen.contentTopInset
            bottomInset: deviceScreen.contentBottomInset
            initialTab: window.initialTab
        }

        // — Status bar mock (§4.10, референс 2080/2090): время 9:41, LTE, батарея 87%;
        // во время записи тренировки слева «офлайн» акцентным цветом —
        Item {
            id: statusBarMock

            anchors.top: parent.top
            anchors.left: parent.left
            anchors.right: parent.right
            height: 44
            visible: window.framed

            Label {
                anchors.left: parent.left
                anchors.verticalCenter: parent.verticalCenter
                anchors.leftMargin: 24
                text: Demo.sessionOffline ? qsTr("офлайн") : "9:41"
                font: Typography.captionStrong
                color: Demo.sessionOffline ? Theme.accent : Theme.textPrimary
            }

            Row {
                anchors.right: parent.right
                anchors.verticalCenter: parent.verticalCenter
                anchors.rightMargin: 20
                spacing: 5

                Label {
                    height: implicitHeight
                    text: "LTE"
                    font: Typography.caption
                    color: Theme.textPrimary
                    verticalAlignment: Text.AlignVCenter
                }

                Repeater {
                    model: 4

                    delegate: Item {
                        width: 3
                        height: 12

                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            height: 4 + index * 2.5
                            radius: 1
                            color: Theme.textPrimary
                            opacity: index < 3 ? 1.0 : 0.35
                        }
                    }
                }

                Item {
                    width: 25
                    height: 13

                    Rectangle {
                        anchors.fill: parent
                        radius: 3
                        color: "transparent"
                        border.width: 1
                        border.color: Theme.textPrimary
                        opacity: 0.6
                    }

                    Rectangle {
                        anchors.left: parent.left
                        anchors.top: parent.top
                        anchors.bottom: parent.bottom
                        anchors.margins: 2
                        width: (parent.width - 4) * 0.87
                        radius: 1.5
                        color: Theme.textPrimary
                    }
                }

                Label {
                    height: implicitHeight
                    text: "87%"
                    font: Typography.caption
                    color: Theme.textPrimary
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // — Home indicator —
        Rectangle {
            id: homeIndicator

            anchors.bottom: parent.bottom
            anchors.horizontalCenter: parent.horizontalCenter
            anchors.bottomMargin: 8
            width: 134
            height: 5
            radius: 2.5
            color: Theme.textPrimary
            opacity: 0.25
            visible: window.framed
        }
    }
}
