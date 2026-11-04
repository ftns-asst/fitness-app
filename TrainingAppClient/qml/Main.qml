import QtQuick
import QtQuick.Controls
import TrainingAppClient

ApplicationWindow {
    id: window

    // Вкладка, на которую возвращаемся после входа или закрытия экрана входа.
    property int activeTab: initialTab
    readonly property int authFailArgIndex: Qt.application.arguments.indexOf("--auth-fail")

    // Режим экрана входа: «login» либо «signup»; при истечении сессии
    // принудительно возвращается в «login».
    property string authMode: initialAuthMode
    property bool authOpen: openAuthInitially
    property int authOriginTab: 3
    readonly property int bezel: 12
    readonly property int deviceHeight: 812
    readonly property int deviceWidth: 390
    readonly property bool diagMode: Qt.application.arguments.indexOf("--diag") >= 0
    // --fps: измерение частоты кадров в лог (этап качества). Нужно, чтобы
    // проверить отзывчивость на телефоне числами, а не ощущением.
    readonly property bool frameRateMode: Qt.application.arguments.indexOf("--fps") >= 0
    readonly property bool forceAuthenticated: Qt.application.arguments.indexOf("--authenticated") >= 0 || tabArgIndex >= 0 || screenTab >= 0
    readonly property bool framed: Demo.phoneFrame
    readonly property string initialAuthMode: initialScreen === "auth-signup" ? "signup" : "login"
    readonly property string initialScreen: screenArgIndex >= 0 && screenArgIndex + 1 < Qt.application.arguments.length ? String(Qt.application.arguments[screenArgIndex + 1]) : ""
    readonly property int initialTab: screenTab >= 0 ? screenTab : tabArgIndex >= 0 && tabArgIndex + 1 < Qt.application.arguments.length ? Number(Qt.application.arguments[tabArgIndex + 1]) : 0

    // --open-auth: вход открывается так же, как по кнопке в «Профиле» —
    // гость остаётся гостем, а закрытие экрана возвращает на вкладку «Профиль».
    readonly property bool openAuthFromProfile: Qt.application.arguments.indexOf("--open-auth") >= 0
    readonly property bool openAuthInitially: initialScreen === "auth-login" || initialScreen === "auth-signup"

    // — Аргументы дизайн-ревью: --screen, --tab, --dark, --width/--height, --diag —
    readonly property int screenArgIndex: Qt.application.arguments.indexOf("--screen")
    readonly property int screenTab: ["today", "plans", "exercises", "profile"].indexOf(initialScreen)
    // Экраны глобальной программы живут вне вкладок, поэтому --screen умеет
    // открывать и их: без этого аудит вёрстки их не видел.
    // Экран авторасчёта КБЖУ (спринт S7) живёт вне вкладок, как и экраны
    // программы, поэтому --screen открывает и его.
    // profile-settings — раздел «Параметры» профиля: в нём блок «Оформление».
    // Без имени в списке экран молча открывался бы как вкладка «Сегодня», и
    // smoke-проверка проходила бы, ничего не проверив.
    readonly property string initialProgramScreen: ["program-catalog", "program-overview", "program-edit", "program-rules", "program-rules-picker", "program-stats", "program-day", "macro-calculator", "set-entry", "profile-settings", "exercise-edit", "exercise-detail"].indexOf(initialScreen) >= 0 ? initialScreen : ""

    // — Режим снимка для дизайн-ревью и CI: --screenshot <путь> —
    readonly property int screenshotArgIndex: Qt.application.arguments.indexOf("--screenshot")
    readonly property bool screenshotMode: screenshotArgIndex >= 0
    // --popup-shot открывает окно выбора упражнения на экране дня (см. AppShell);
    // снимок ждёт и его: 900 мс на открытие окна плюс запас на загрузку каталога.
    readonly property bool popupShot: Qt.application.arguments.indexOf("--popup-shot") >= 0
    readonly property int captureDelayMs: popupShot ? 2600 : 700
    readonly property bool startUnauthenticated: Qt.application.arguments.indexOf("--unauth") >= 0
    readonly property int tabArgIndex: Qt.application.arguments.indexOf("--tab")

    // — Тема: по умолчанию следует системной схеме (Android — системный
    // «ночной режим», десктоп — настройка ОС), а флаги --dark/--light её
    // переопределяют: ими пользуются дизайн-ревью и CI-снимки.
    // Прежде тема читалась только из --dark, поэтому на Android приложение
    // оставалось светлым даже при включённом системном тёмном режиме.
    readonly property bool forceDark: Qt.application.arguments.indexOf("--dark") >= 0
    readonly property bool forceLight: Qt.application.arguments.indexOf("--light") >= 0

    function argNumber(name, fallback) {
        const index = Qt.application.arguments.indexOf(name);

        return index >= 0 && index + 1 < Qt.application.arguments.length ? Number(Qt.application.arguments[index + 1]) : fallback;
    }
    // Единая точка выбора темы: явный флаг важнее системы, система — важнее
    // дефолта. В режиме --screenshot без --dark всегда светлая схема, чтобы
    // снимки дизайн-ревью и CI не зависели от темы машины сборки.
    function resolveDarkMode() {
        if (forceDark) {
            return true;
        }

        if (forceLight) {
            return false;
        }

        if (screenshotMode) {
            return false;
        }

        return Qt.styleHints.colorScheme === Qt.ColorScheme.Dark;
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

    // — Закрытие экранной клавиатуры тапом мимо поля ввода —
    // Qt Quick не снимает фокус с текстового поля по тапу мимо него, поэтому
    // на Android клавиатура остаётся открытой после завершения ввода. По тапу
    // ищется самый верхний элемент под пальцем: если он не берёт фокус по
    // нажатию (то есть не поле ввода), клавиатура прячется. Тап по соседнему
    // полю клавиатуру сохраняет — фокус уходит этому полю.
    function topItemUnderPoint(rootItem, x, y) {
        let node = rootItem;
        let guard = 0;

        while (node !== null && node !== undefined && guard < 24) {
            const child = node.childAt(x, y);

            if (child === null || child === undefined)
                break;

            const point = child.mapFromItem(node, x, y);

            node = child;
            x = point.x;
            y = point.y;
            ++guard;
        }

        return node;
    }
    // Поле ввода берёт фокус по нажатию: у TextInput и TextEdit свойство
    // включено, у обычных Item выключено. Проверяется и цепочка родителей:
    // верхним элементом под пальцем бывает обёртка поля.
    function takesFocusOnPress(item) {
        let node = item;
        let guard = 0;

        while (node !== null && node !== undefined && guard < 24) {
            if (node.activeFocusOnPress === true)
                return true;

            node = node.parent;
            ++guard;
        }

        return false;
    }

    // Закрывает экран входа и возвращает пользователя на исходную вкладку.
    function closeAuth() {
        activeTab = authOriginTab;
        authOpen = false;
    }

    // Открывает экран входа поверх каркаса: originTab — вкладка возврата.
    function openAuth(originTab) {
        authOriginTab = originTab;
        authOpen = true;
    }
    // — Аудит цветов (--diag): печатает resolved-токены темы и фактический
    // surfaceColor каждой карточки. Повод — «пропадающая» подсветка hero-карточки
    // на некоторых Android-сборках: по логу видно, какой цвет реально разрешён
    // (акцент, тема, альфа в составе #AARRGGBB) и что стоит в каждой карточке,
    // без скриншотов. Строка surfaceColor с альфой меньше ff — верный признак
    // транслюцентной заливки.
    function runColorAudit() {
        console.warn("diag colors: darkMode=" + Theme.darkMode + " accentIndex=" + Theme.accentIndex + " accent=" + Theme.accent + " accentSurface=" + Theme.accentSurface + " accentTint=" + Theme.accentTint + " surface=" + Theme.surface + " background=" + Theme.background);

        function walkCards(item) {
            if (item.surfaceColor !== undefined)
                console.warn("diag colors: " + window.auditPath(item) + " surfaceColor=" + item.surfaceColor);

            const kids = item.children;

            for (let i = 0; i < kids.length; ++i)
                walkCards(kids[i]);
        }

        walkCards(deviceScreen);
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

                // Блок уже родителя при непустом содержимом. Это ровно тот
                // случай, когда AppCard в обычной Column PageScaffold берёт
                // свой implicitWidth (240) вместо ширины колонки: элемент не
                // вылезает за границу, поэтому OVERFLOW-X его не видит, а
                // выглядит обрезанным наполовину.
                //
                // Проверяются только дети обычной Column: внутри RowLayout и
                // ColumnLayout сжатие детей — их штатная работа, и без этого
                // ограничения проверка дала бы сотни ложных срабатываний.
                //
                // Текст с переносом и с многоточием отбрасывается: у него
                // implicitWidth всегда больше ширины (строка рассчитана без
                // переноса), и такой элемент по определению «сжат».
                const host_name = String(host);
                const plain_column = host_name.indexOf("QQuickColumn") >= 0 && host_name.indexOf("QQuickRowLayout") < 0 && host_name.indexOf("QQuickColumnLayout") < 0;

                const wraps = child.wrapMode !== undefined && child.wrapMode !== 0;
                const elides = child.elide !== undefined && child.elide !== Text.ElideNone;

                // Layout сам по себе подстраивается под родителя, и его
                // implicitWidth — это ширина содержимого, а не заданная ширина.
                // Считать его «схлопнутым» бессмысленно.
                const is_layout = String(child).indexOf("Layout") >= 0;

                if (plain_column && !is_layout && !wraps && !elides && child.implicitWidth > 0 && child.width > 0 && child.width < child.implicitWidth - 1 && child.height > 0) {
                    problems.push("SHRUNK-WIDTH " + window.auditPath(child) + " text=\"" + (child.text === undefined ? "" : child.text) + "\" width=" + Math.round(child.width) + " implicit=" + Math.round(child.implicitWidth));
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

    color: framed ? "#101216" : Theme.background
    height: framed ? deviceHeight + bezel * 2 : argNumber("--height", 820)
    minimumHeight: 640
    minimumWidth: 360
    title: qsTr("Фитнес-помощник — UI демо")
    visible: true
    width: framed ? deviceWidth + bezel * 2 : argNumber("--width", 420)

    Component.onCompleted: {
        MockApi.reset();
        Demo.phoneFrame = Qt.application.arguments.indexOf("--frame") >= 0 || screenshotMode;
        Theme.darkMode = window.resolveDarkMode();

        if (window.authFailArgIndex >= 0 && window.authFailArgIndex + 1 < Qt.application.arguments.length) {
            MockApi.failMode = String(Qt.application.arguments[window.authFailArgIndex + 1]);
        }

        // Обычный запуск — гостевая работа без сессии: вкладки, дневник и каталог
        // доступны офлайн, а вход открывается из «Профиля» (issue 2). Сессия
        // поднимается только для дизайн-ревью (--authenticated/--tab/--screen);
        // --unauth принудительно оставляет гостя даже в этих режимах.
        if (window.forceAuthenticated && !window.startUnauthenticated) {
            MockApi.startDemoSession();
        }

        if (window.openAuthFromProfile) {
            window.openAuth(3);
        }

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
        function onDarkModeChanged() {
            PlatformChrome.applyChrome(Theme.darkMode);
        }

        target: Theme
    }

    // Живая реакция на смену системной темы (Android «Ночной режим», десктоп):
    // пользователь переключает тему в системе — приложение следует за ней без
    // перезапуска. Явные --dark/--light отключают слежение: тему выбрали руками.
    Connections {
        function onColorSchemeChanged() {
            if (!window.forceDark && !window.forceLight) {
                Theme.darkMode = window.resolveDarkMode();
            }
        }

        target: Qt.styleHints
    }
    // — Снимок для ревью и CI: цель захвата —
    //
    // Кадр берётся с «рамки телефона» (deviceScreen): превью экранов — без
    // безеля. Окно выбора упражнения живёт в Overlay окна — соседе рамки, и в
    // такой кадр оно не попадает: `grabToImage` по Overlay и по contentItem
    // окна в этом рантайме не завершается вовсе. Состояние окна выбора
    // проверяется отчётом `[popup-shot]` (строки каталога, размер, позиция) и
    // сквозным добавлением упражнения в день.
    readonly property Item captureTarget: deviceScreen

    Timer {
        id: captureTimer

        // Ждём первый кадр и завершение стартовых анимаций; с --popup-shot —
        // ещё и открытие окна выбора упражнения.
        interval: window.captureDelayMs

        onTriggered: window.captureTarget.grabToImage(function (result) {
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
            window.runColorAudit();
            window.runLayoutAudit();
            Qt.quit();
        }
    }

    // — Частота кадров (--fps, этап качества) —
    // 120 Гц у Qt Quick не настройка, а следствие vsync дисплея: приложение
    // рисует по сигналу композитора, и убедиться в этом можно только числами с
    // реального экрана. Строка в секунду уходит в лог приложения (stderr и
    // logs/app.log), поэтому отладчик для проверки не нужен.
    FrameRateMeter {
        id: frameMeter

        running: window.frameRateMode || Theme.fpsCounter
    }
    Timer {
        id: frameRateTimer

        interval: 1000
        repeat: true
        running: window.frameRateMode

        onTriggered: console.info("fps", frameMeter.framesPerSecond.toFixed(1), "frame-ms", (frameMeter.frameSeconds * 1000).toFixed(2))
    }
    // Плашка счётчика поверх экрана: включается настройкой в «Профиль →
    // Параметры» (Theme.fpsCounter), клик сквозь неё не проходит в страницу
    // и не нужен — плашка диагностическая. Стоит над всем контентом.
    FpsOverlay {
        anchors.right: parent.right
        anchors.top: parent.top
        anchors.topMargin: Spacing.safeGap + 40
    }

    // — Зонд шрифтов: измеряем advanceWidth одного и того же текста в четырёх
    // весах при 15 и 40 px (TextMetrics, без визуальных элементов). Если ширины
    // совпадают, вес не сопоставился с начертанием (например, Medium лежит
    // в отдельном семействе) — это видно в выводе --diag без визуального осмотра.
    // Контейнер — Item: у QtObject нет default-свойства для вложенных объектов.
    Item {
        id: fontProbe

        readonly property real bold15: metricsBold15.advanceWidth
        readonly property real bold40: metricsBold40.advanceWidth
        readonly property real demiBold15: metricsDemiBold15.advanceWidth
        readonly property real demiBold40: metricsDemiBold40.advanceWidth
        readonly property real medium15: metricsMedium15.advanceWidth
        readonly property real medium40: metricsMedium40.advanceWidth
        readonly property real normal15: metricsNormal15.advanceWidth
        readonly property real normal40: metricsNormal40.advanceWidth
        readonly property string sampleText: "Приседания со штангой 62,5"

        height: 0
        visible: false
        width: 0

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
    // — Жесты по «пустой» области вокруг контента листают страницу —
    //
    // Контент живёт в центрированной колонке (`Theme.contentMaxWidth`, рамка
    // телефона в режиме ревью), и вокруг остаётся область окна, которая
    // странице не принадлежит: нажатие по ней пропадало, и до прокрутки
    // приходилось тянуться в центр экрана. Ловушка объявлена до `deviceScreen`,
    // поэтому нажатия самой страницы, кнопок и ползунков до неё не доходят.
    DragToScroll {
        id: backgroundScroll

        anchors.fill: parent
        target: appShell.activeFlickable
    }

    Item {
        id: deviceScreen

        readonly property real contentBottomInset: window.framed ? homeIndicator.height + 8 : safeBottom
        readonly property real contentTopInset: window.framed ? statusBarMock.height : safeTop
        readonly property real safeBottom: SafeArea.margins.bottom

        // — Safe area (Qt ≥ 6.9): на устройстве реальные insets, на десктопе 0 —
        readonly property real safeTop: SafeArea.margins.top

        anchors.centerIn: parent
        clip: true
        height: window.framed ? window.deviceHeight : window.height

        // Responsive: на широком десктопном окне контент не растягивается на всю
        // ширину, а остаётся в телефонной пропорции и центрируется (§6.3).
        width: window.framed ? window.deviceWidth : Math.min(window.width, Theme.contentMaxWidth)

        Rectangle {
            anchors.fill: parent
            color: Theme.background
        }
        AppShell {
            id: appShell

            anchors.fill: parent
            auth: AuthViewModel
            bottomInset: deviceScreen.contentBottomInset
            enabled: !window.authOpen
            initialProgramScreen: window.initialProgramScreen
            initialTab: window.activeTab
            topInset: deviceScreen.contentTopInset

            onAuthPageRequested: function (originTab) {
                window.openAuth(originTab);
            }

            // Вкладка запоминается для дизайн-ревью, но сам каркас больше не
            // уничтожается при открытии auth overlay.
            onCurrentTabChanged: window.activeTab = appShell.currentTab
        }
        Loader {
            id: authOverlay

            anchors.fill: parent
            active: window.authOpen
            sourceComponent: Component {
                AuthPage {
                    auth: AuthViewModel
                    bottomInset: deviceScreen.contentBottomInset
                    // Из «Профиля» / --open-auth экран можно закрыть без входа.
                    // Прямой --screen auth-* — ревью формы без «×»; sessionExpired
                    // всё равно блокирует закрытие внутри AuthPage.
                    closable: !window.openAuthInitially
                    initialMode: window.authMode
                    objectName: "authPage"
                    topInset: deviceScreen.contentTopInset

                    onAuthenticated: window.closeAuth()
                    onCancelRequested: window.closeAuth()
                }
            }
            z: 100
        }
        // — Тап мимо поля ввода закрывает экранную клавиатуру (Android) —
        // Обработчик объявлен в каркасе экрана: он видит нажатия и вкладок, и
        // открываемых поверх экранов, включая экран входа.
        TapHandler {
            // Захват пассивный (gesturePolicy по умолчанию): кнопки, ползунки
            // и прокрутка продолжают работать как раньше. Тап, а не press:
            // прокрутку списка клавиатура закрывать не должна.
            onTapped: function (eventPoint) {
                if (Qt.inputMethod.visible === false)
                    return;

                const top = window.topItemUnderPoint(deviceScreen, eventPoint.position.x, eventPoint.position.y);

                if (top !== null && window.takesFocusOnPress(top))
                    return;

                Qt.inputMethod.hide();
            }
        }
        Connections {
            function onSessionExpired() {
                window.authMode = "login";
                window.openAuth(window.activeTab);
            }

            target: AuthViewModel
        }

        // — Status bar mock (§4.10, референс 2080/2090): время 9:41, LTE, батарея 87%;
        // во время записи тренировки слева «офлайн» акцентным цветом —
        Item {
            id: statusBarMock

            anchors.left: parent.left
            anchors.right: parent.right
            anchors.top: parent.top
            height: 44
            visible: window.framed

            Label {
                anchors.left: parent.left
                anchors.leftMargin: 24
                anchors.verticalCenter: parent.verticalCenter
                color: Demo.sessionOffline ? Theme.accent : Theme.textPrimary
                font: Typography.captionStrong
                text: Demo.sessionOffline ? qsTr("офлайн") : "9:41"
            }
            Row {
                anchors.right: parent.right
                anchors.rightMargin: 20
                anchors.verticalCenter: parent.verticalCenter
                spacing: 5

                Label {
                    color: Theme.textPrimary
                    font: Typography.caption
                    height: implicitHeight
                    text: "LTE"
                    verticalAlignment: Text.AlignVCenter
                }
                Repeater {
                    model: 4

                    delegate: Item {
                        height: 12
                        width: 3

                        Rectangle {
                            anchors.bottom: parent.bottom
                            anchors.left: parent.left
                            anchors.right: parent.right
                            color: Theme.textPrimary
                            height: 4 + index * 2.5
                            opacity: index < 3 ? 1.0 : 0.35
                            radius: 1
                        }
                    }
                }
                Item {
                    height: 13
                    width: 25

                    Rectangle {
                        anchors.fill: parent
                        border.color: Theme.textPrimary
                        border.width: 1
                        color: "transparent"
                        opacity: 0.6
                        radius: 3
                    }
                    Rectangle {
                        anchors.bottom: parent.bottom
                        anchors.left: parent.left
                        anchors.margins: 2
                        anchors.top: parent.top
                        color: Theme.textPrimary
                        radius: 1.5
                        width: (parent.width - 4) * 0.87
                    }
                }
                Label {
                    color: Theme.textPrimary
                    font: Typography.caption
                    height: implicitHeight
                    text: "87%"
                    verticalAlignment: Text.AlignVCenter
                }
            }
        }

        // — Home indicator —
        Rectangle {
            id: homeIndicator

            anchors.bottom: parent.bottom
            anchors.bottomMargin: 8
            anchors.horizontalCenter: parent.horizontalCenter
            color: Theme.textPrimary
            height: 5
            opacity: 0.25
            radius: 2.5
            visible: window.framed
            width: 134
        }
    }
}
