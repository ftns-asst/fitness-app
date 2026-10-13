pragma Singleton
import QtQuick

// Design tokens: единственный источник цветов, скруглений и размеров.
// Источник значений: docs/fitness-assistant-frontend-design.md §3, §8.
QtObject {
    readonly property color accent: accentPalette[accentIndex]
    readonly property color accentForeground: "#FFFFFF"
    property int accentIndex: 0
    readonly property var accentNames: [qsTr("Синий"), qsTr("Бирюзовый"), qsTr("Красно-оранжевый"), qsTr("Фиолетовый")]
    // — Переключаемый акцент (§3.1): синий по умолчанию —
    readonly property var accentPalette: ["#505DD5", "#0FB5A5", "#F2542D", "#8B46E0"]
    readonly property color accentPressed: Qt.darker(accent, 1.2)
    readonly property color accentSoft: Qt.rgba(accent.r, accent.g, accent.b, 0.30)
    readonly property color accentTint: Qt.rgba(accent.r, accent.g, accent.b, darkMode ? 0.20 : 0.12)

    // — Поверхности (§3.1) —
    readonly property color background: darkMode ? "#0F1115" : "#F8F9FB"
    readonly property color border: darkMode ? "#262B36" : "#E5E7EB"
    // Вложенные карточки (цели, правила, упражнения): рамка светлее основной,
    // иначе вложенная карточка читалась как ещё одна карточка того же уровня.
    readonly property color borderSubtle: darkMode ? "#2A2F3A" : "#EEF0F4"
    readonly property int borderWidth: 1
    readonly property int buttonPrimary: 56
    readonly property int buttonSecondary: 44

    // Ограничение ширины контента на десктопе: интерфейс рассчитан на 390 dp,
    // растягивание на всю ширину окна ломает пропорции карточек и графиков.
    readonly property int contentMaxWidth: 480
    property bool darkMode: false

    // Тач-платформы: на них выключаются blur-тени карточек (MultiEffect гонит
    // по каждой карточке отдельный текстурный проход на каждый кадр — десятки
    // карточек на страницу мобильный GPU при прокрутке не тянет, скролл
    // рассыпается на рывки), а постоянный скроллбар заменяется транзиентным
    // индикатором.
    readonly property bool touchPlatform: Qt.platform.os === "android" || Qt.platform.os === "ios"

    // Скорость отклика на нажатие — единый токен: мягкая анимация 120 мс с
    // OutCubic на цветовых переходах. Принудительно короткие 60 мс пробовали и
    // откатили: мгновенный цвет читался как мигание, а не отклик; плавность
    // перехода и есть «отзывчивость», наряду с честным скроллом.
    readonly property int pressFeedbackDuration: 120

    // — Непрозрачные «смешанные» аналоги tint-заливок —
    // Крупные тонированные поверхности (hero-карточка, highlight-плитка метрик)
    // не полагаются на альфу: на некоторых Android-устройствах и сборках
    // транслюцентная заливка теряет альфу при композитинге (surface без
    // альфа-канала, RGB_565), и карточка выглядит неокрашенной — «подсветка
    // пропадает». Заранее смешанный непрозрачный цвет даёт тот же визуальный
    // результат и от возможностей рендера не зависит.
    function mixIntoSurface(top, ratio) {
        return Qt.rgba(top.r * ratio + surface.r * (1 - ratio), top.g * ratio + surface.g * (1 - ratio), top.b * ratio + surface.b * (1 - ratio), 1.0);
    }
    readonly property color accentSurface: mixIntoSurface(accent, darkMode ? 0.26 : 0.16)
    // Мягкая линия акцента — рамка тонированной карточки, читается на обоих фонах.
    readonly property color accentSurfaceLine: mixIntoSurface(accent, darkMode ? 0.55 : 0.45)

    readonly property color negative: darkMode ? "#F87171" : "#DC2626"

    // — Семантика —
    readonly property color positive: darkMode ? "#4ADE80" : "#16A34A"
    readonly property int radiusLg: 18
    readonly property int radiusMd: 14

    // — Тонированные заливки: цвет на 10–20 % подложкой, а не «тот же цвет,
    // но прозрачный» в компоненте. Альфа разная для светлой и тёмной темы:
    // на тёмном фоне тот же процент даёт менее заметное пятно.
    readonly property color positiveTint: Qt.rgba(positive.r, positive.g, positive.b, darkMode ? 0.30 : 0.14)
    readonly property color warningTint: Qt.rgba(warning.r, warning.g, warning.b, darkMode ? 0.22 : 0.16)

    // Предупреждение — «не заполнен», «нужно внимание»: состояние, которое
    // пользователь должен увидеть, но которое ещё не ошибка. Отдельный токен,
    // а не negative: жёлтым помечают незакрытое, красным — сломанное.
    readonly property color warning: darkMode ? "#FBBF24" : "#F59E0B"

    // — Геометрия (§3.3–3.5, §8) —
    readonly property int radiusSm: 10
    readonly property real shadowBlur: 0.45

    // — Тени: только карточки и primary CTA (§3.5) —
    readonly property color shadowColor: "#0B1020"
    readonly property int shadowOffset: 4
    readonly property real shadowOpacity: darkMode ? 0.0 : 0.12
    readonly property color surface: darkMode ? "#171A21" : "#FFFFFF"
    readonly property color surfaceMuted: darkMode ? "#1E222B" : "#F5F5F5"
    readonly property int tabBarHeight: 60
    readonly property color textMuted: darkMode ? "#6B7280" : "#9CA3AF"

    // — Текст (§3.1) —
    readonly property color textPrimary: darkMode ? "#F3F4F6" : "#1A1A1A"
    readonly property color textSecondary: darkMode ? "#9CA3AF" : "#6B7280"
    readonly property int touchMin: 44
}
