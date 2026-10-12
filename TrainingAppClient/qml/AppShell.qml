import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import TrainingAppClient

// Каркас приложения: 4 вкладки (StackLayout) + стек деталей (экраны модуля
// программы). Инструментов дизайн-ревью в каркасе нет: экраны и рамка телефона
// открываются ключами командной строки (`--screen`, `--tab`, `--frame`, `--accent`,
// `--dark`) — см. `docs/07-programs-ui-spec.md` §2 и `docs/04-roadmap.md`.
// Оверлеи записи тренировки и таймера отдыха добавляются на этапе D2 — они будут
// братями StackView и будут скрывать tab bar (§5.2, §5.3).
Item {
    id: shell

    property var auth: AuthViewModel
    property real bottomInset: 0
    readonly property int currentTab: tabBar.currentIndex
    // Каталог упражнений — контекстное свойство ExercisesVM (Avm_Exercises,
    // спринт U4). typeof: в qmltestrunner контекст-свойств нет.
    property var exercisesViewModel: typeof ExercisesVM !== "undefined" ? ExercisesVM : null
    property string initialProgramScreen: ""
    property int initialTab: 0

    // — Память вкладок (отзыв: «вернулся на вкладку — а там корень») —
    // Полная история переходов каждой вкладки: массив записей
    // {name: "program-edit", id: …} / {name: "program-day", weekday, kind}
    // в порядке открытия. Возвращение на вкладку восстанавливает ВСЮ цепочку
    // (назад из восстановленного экрана не ведёт в корень), «назад» снимает
    // верхний экран, повторный тап по своей вкладке сбрасывает к корню.
    property var tabHistory: ({})
    // Отложенная навигация: push во время перехода отбрасывается guard'ом
    // pushPage, поэтому восстановление экрана после pop живёт в очереди и
    // разбирается в onBusyChanged стека.
    property var pendingRestoreChain: null
    property var pendingComponentEntry: null
    // Глубина стека деталей: тесты навигации смотрят сюда, потому что идентификатор
    // StackView из оболочки наружу недоступен.
    readonly property int pushedDepth: pages.depth
    // Верхний экран стека: тестам нужно отличать «обзор» от «редактора» при
    // одинаковой глубине. undefined у корня (вкладки).
    readonly property var topItem: pages.depth > 1 ? pages.currentItem : null
    // Идёт переход (пуш/поп с анимацией): в это время команды навигации
    // отбрасываются, и тестам нужно дождаться тишины перед следующим шагом.
    readonly property bool stackBusy: pages.busy
    property var plansViewModel: PlansViewModel
    property var profileViewModel: ProfileViewModel
    property var todayViewModel: TodayVM

    // Отступы под safe area / status bar mock передаёт Main.qml.
    property real topInset: 0

    // Отступ низа для pushed-экранов (открываются поверх вкладок): панель
    // навигации теперь видна всегда, и её высота резервируется так же, как
    // safe area, иначе контент и подвал экрана заезжали бы под панель.
    readonly property real pageBottomInset: bottomInset + Theme.tabBarHeight

    // Запрос на открытие экрана входа из «Профиля» (гость): Main.qml решает,
    // показывать ли экран и куда вернуться после входа.
    signal authPageRequested(int originTab)

    // Прокручиваемая область активного экрана. Нужна каркасу окна: на широком
    // экране вокруг контента остаётся «пустая» область, и жест по ней должен
    // листать страницу, а не пропадать (см. `qml/Main.qml`, `DragToScroll`).
    //
    // Экран в стеке деталей и текущая вкладка лежат в разных контейнерах:
    // у `pages` (StackView) текущий элемент — сам экран, а внутри вкладок это
    // StackLayout, чьи дети и есть страницы в порядке вкладок.
    readonly property var activeFlickable: {
        const top = pages.currentItem;

        if (top === null || top === undefined)
            return null;

        if (pages.depth > 1)
            return top.pageFlickable !== undefined ? top.pageFlickable : null;

        const tab = top.children !== undefined ? top.children[tabBar.currentIndex] : undefined;

        return tab !== undefined && tab.pageFlickable !== undefined ? tab.pageFlickable : null;
    }

    function showTab(index) {
        // Переход уже идёт (пуш/поп с анимацией): второй тап в этот момент
        // не переключает вкладку, а игнорируется — иначе вкладка менялась бы
        // «под» pushed-экраном, и назад возвращал не туда, откуда пришли.
        if (pages.busy)
            return;

        const sameTab = tabBar.currentIndex === index;
        const chain = sameTab === false && shell.tabHistory[index] !== undefined ? shell.tabHistory[index] : [];

        if (pages.depth > 1)
            pages.pop(null);

        if (sameTab) {
            // Повторный тап по своей вкладке — к её корню, как в системных
            // таб-барах: открытый поток пользователь закрыл сам, и помнить
            // его больше не нужно.
            shell.forgetTabScreen(index);

            return;
        }

        tabBar.currentIndex = index;
        shell.pendingRestoreChain = chain.slice();
        shell.flushPendingNavigation();
    }

    // Запись экрана в историю вкладки — точка, через которую проходят все
    // open*-функции. История растёт с каждым push; повторная запись того же
    // экрана (тот же name/id/weekday/kind) не дублируется, чтобы повторный
    // вызов open* поверх живого экрана не удваивал цепочку.
    function noteTabScreen(entry) {
        const history = shell.tabHistory[tabBar.currentIndex] || [];

        if (history.length > 0 && shell.sameEntry(history[history.length - 1], entry))
            return;

        history.push(entry);
        shell.tabHistory[tabBar.currentIndex] = history;
    }

    // Замена верха истории: сохранение программы заменяет редактор обзором
    // в стеке (pages.replace), и история обязана отражать то же самое —
    // иначе возврат на вкладку восстановил бы закрытую форму создания.
    function replaceTopScreen(entry) {
        const history = shell.tabHistory[tabBar.currentIndex] || [];

        if (history.length > 0)
            history[history.length - 1] = entry;
        else
            history.push(entry);

        shell.tabHistory[tabBar.currentIndex] = history;
    }

    function forgetTabScreen(index) {
        shell.tabHistory[index] = [];
    }

    // Сравнение записей: одинаковый экран (имя и параметры) не попадает
    // в историю дважды.
    function sameEntry(a, b) {
        return (a.name === b.name) && ((a.id || "") === (b.id || "")) && ((a.weekday || 0) === (b.weekday || 0)) && ((a.kind || 0) === (b.kind || 0));
    }

    // Компонент экрана по имени записи. Только для восстановления цепочки:
    // живые переходы идут через open*-функции с их сайд-эффектами.
    function componentFor(name) {
        const map = {
            "macro-calculator": macroCalculatorComponent,
            "program-catalog": programCatalogComponent,
            "program-day": programDayComponent,
            "program-edit": programEditorComponent,
            "program-overview": programOverviewComponent,
            "program-rules": programRulesComponent,
            "program-rules-picker": programRulesPickerComponent,
            "program-stats": programStatsComponent,
            "set-entry": setEntryComponent
        };

        return map[name] !== undefined ? map[name] : null;
    }

    // Восстановление всей цепочки экранов вкладки одним push'ем: страницы
    // сами загружают свои данные при создании (Component.onCompleted), поэтому
    // перед созданием достаточно выставить общие параметры — идентификатор
    // программы и открытый день. Разные программы в одной цепочке не
    // поддерживаются: реальный поток всегда смотрит одну программу.
    function restoreChain(entries) {
        const components = [];
        let dayEntry = null;

        for (var i = 0; i < entries.length; ++i) {
            const entry = entries[i];

            if (entry.id !== undefined && entry.id !== null && entry.id !== "")
                shell.programId = entry.id;

            if (entry.name === "program-day")
                dayEntry = entry;

            const component = shell.componentFor(entry.name);

            if (component !== null)
                components.push(component);
        }

        if (dayEntry !== null) {
            shell.programDayWeekday = dayEntry.weekday;
            shell.programDayKind = dayEntry.kind;
        }

        if (components.length > 0)
            pages.push(components);
    }

    // Разбор отложенной навигации, когда стек снова свободен: push во время
    // перехода (busy) отбрасывается без ошибки, и без этой очереди экран,
    // восстанавливаемый сразу после pop, просто не открылся бы.
    function flushPendingNavigation() {
        if (pages.busy)
            return;

        if (shell.pendingRestoreChain !== null) {
            const chain = shell.pendingRestoreChain;

            shell.pendingRestoreChain = null;
            shell.restoreChain(chain);

            return;
        }

        if (shell.pendingComponentEntry !== null) {
            const task = shell.pendingComponentEntry;

            shell.pendingComponentEntry = null;
            shell.noteTabScreen(task.entry);
            shell.pushPage(task.component);
        }
    }

    // Открытие экрана в стеке деталей. Страница создаётся при пуше, и это
    // заметная работа: пока идёт переход (`busy`), повторный тап по CTA
    // игнорируется. Без guard'а двойной тап толкал один экран дважды —
    // человек видел это как «фантомное перелистывание»: нажатие назад
    // показывало тот же экран ещё раз.
    function pushPage(component) {
        if (pages.busy)
            return;

        pages.push(component);
    }
    // Редактор своего упражнения (спринт U5): закрытие после сохранения
    // перезагружает каталог — новая строка обязана появиться без ручного
    // обновления.
    function closeExerciseEditor() {
        pages.pop();
        shell.exercisesViewModel.reload();
    }

    Component.onCompleted: {
        if (shell.initialProgramScreen.length > 0) {
            shell.openProgramScreen(shell.initialProgramScreen);
        } else if (shell.initialTab > 0 && shell.initialTab < 4) {
            shell.showTab(shell.initialTab);
        }
    }

    StackView {
        id: pages

        anchors.fill: parent
        initialItem: tabsView

        // Переход закончился — стек снова принимает push: очередь отложенной
        // навигации разбирается здесь, потому что pushPage во время busy
        // отбрасывает команду молча.
        onBusyChanged: shell.flushPendingNavigation()
    }
    BackHandler {
        enabled: pages.depth > 1
        priority: 10

        onBackPerformed: shell.goBack()
    }
    Component {
        id: tabsView

        StackLayout {
            currentIndex: tabBar.currentIndex

            TodayPage {
                bottomInset: shell.bottomInset + Theme.tabBarHeight
                topInset: shell.topInset
                viewModel: shell.todayViewModel

                // Расчёт КБЖУ (спринт S7) открывается с главной: человек узнаёт
                // о нём, когда программа уже есть, а не отправляется искать его
                // в профиль.
                onMacroRequested: shell.openMacroCalculator()
                // День отдыха ведёт в обзор активной программы, а пустой
                // главный экран — на вкладку «Планы», где программу создают.
                onPlansRequested: shell.showTab(1)
                onProgramRequested: shell.openProgramOverview()
            }
            PlansPage {
                bottomInset: shell.bottomInset + Theme.tabBarHeight
                topInset: shell.topInset
                viewModel: shell.plansViewModel

                onMyProgramRequested: shell.openProgramCatalog()
            }
            ExercisesPage {
                bottomInset: shell.bottomInset + Theme.tabBarHeight
                topInset: shell.topInset

                onCreateOwnRequested: shell.pushPage(exerciseEditorComponent)
            }
            ProfilePage {
                auth: shell.auth
                bottomInset: shell.bottomInset + Theme.tabBarHeight
                sectionIndex: shell.profileSection
                topInset: shell.topInset
                viewModel: shell.profileViewModel

                onLoginRequested: shell.authPageRequested(3)
                onLogoutRequested: shell.auth.logout()
            }
        }
    }
    // Запрос на открытие экрана программы из «Планов» — своей глобальной
    // программы пользователь не ищет среди шаблонов, он открывает её сам.
    signal myProgramRequested

    // — Экраны глобальной программы (срез W11, issues 41–44) —
    // Отдельный стек поверх вкладок: детали программы живут вне неё, поэтому
    // вкладок не видно, а «назад» возвращает на вкладку, из которой пришли.
    function openProgramCatalog() {
        shell.noteTabScreen({
            name: "program-catalog"
        });
        shell.pushPage(programCatalogComponent);
    }
    function openProgramOverview(id) {
        shell.programId = id === undefined ? "" : id;
        shell.noteTabScreen({
            name: "program-overview",
            id: shell.programId
        });
        shell.pushPage(programOverviewComponent);
    }

    function openProgramEditor(id) {
        shell.programId = id === undefined ? "" : id;
        shell.noteTabScreen({
            name: "program-edit",
            id: shell.programId
        });
        shell.pushPage(programEditorComponent);
    }

    function openProgramRules() {
        shell.noteTabScreen({
            name: "program-rules"
        });
        shell.pushPage(programRulesComponent);
    }

    // Экран выбора правил открывается из редактора: он меняет тот же черновик
    // правил, поэтому идёт поверх, а не вместо него.
    function openProgramRulesPicker() {
        shell.noteTabScreen({
            name: "program-rules-picker"
        });
        shell.pushPage(programRulesPickerComponent);
    }

    function openProgramStats(id) {
        shell.programId = id === undefined ? "" : id;
        shell.noteTabScreen({
            name: "program-stats",
            id: shell.programId
        });
        shell.pushPage(programStatsComponent);
    }

    // — Экран авторасчёта КБЖУ (спринт S7) —
    // Отдельный стек поверх вкладок, как и экраны программы: расчёт нужен из
    // профиля и с главной, но сам по себе он не вкладка.
    function openSetEntry() {
        shell.openAfterPop(setEntryComponent, {
            name: "set-entry"
        });
    }

    // -- Запись подхода (спринт S8) --
    function openMacroCalculator() {
        shell.openAfterPop(macroCalculatorComponent, {
            name: "macro-calculator"
        });
    }

    // Инструменты (запись подхода, КБЖУ) открываются вместо текущего стека:
    // сперва pop, и только после завершения перехода — push, потому что push
    // во время busy отбрасывается. Запись памяти тоже отложена: «последний
    // экран» фиксируется в момент фактического открытия, а не отброшенной
    // попытки.
    function openAfterPop(component, entry) {
        if (pages.depth > 1) {
            pages.pop(null);
            // Поток вкладки закрыт (инструмент открыли с корня): прежняя
            // цепочка экранов больше не лежит в стеке, и хранить её нельзя —
            // возврат на вкладку восстановил бы несуществующий стек.
            shell.forgetTabScreen(tabBar.currentIndex);
            shell.pendingComponentEntry = {
                component: component,
                entry: entry
            };

            return;
        }

        shell.noteTabScreen(entry);
        shell.pushPage(component);
    }

    // — «Профиль → Параметры» (экран 32) —
    // Оформление клиента (акцент и тема) живёт в параметрах профиля, а не в
    // инструменте ревью: это настройка пользователя. Именованный экран нужен,
    // чтобы блок проверялся аудитом вёрстки на всех компоновках.
    function openProfileSettings() {
        profileSection = 2;
        shell.showTab(3);
    }

    // — Завершение сохранения программы —
    // Экран редактора закрывается, а результат показывается явно: созданная
    // программа открывается обзором, правка возвращает туда, откуда пришли.
    // Без этого нажатие «Создать программу» выглядело как «ничего не произошло»
    // — экран исчезал, а сохранённой программы пользователь не видел.
    function finishProgramSave(programId, wasEditing, previousPaused) {
        // §2.3: активная программа у пользователя одна, поэтому включение новой
        // ставит прежнюю на паузу. Об этом говорится в том же сообщении: иначе
        // прежняя программа молча ушла бы из «Активных» в «Паузу».
        if (previousPaused) {
            Demo.notify(wasEditing ? qsTr("Изменения сохранены, прежняя программа — на паузе") : qsTr("Программа создана, прежняя — на паузе"));
        } else {
            Demo.notify(wasEditing ? qsTr("Изменения сохранены") : qsTr("Программа создана"));
        }

        if (wasEditing || programId.length === 0) {
            shell.goBack();

            return;
        }

        // replace, а не push: обзор встаёт на место редактора, поэтому «назад»
        // из обзора возвращает туда, откуда редактор открыли, а не в уже
        // закрытую форму создания. Память вкладки тоже обновляется: последним
        // открытым экраном теперь обзор, а не редактор.
        shell.programId = programId;
        shell.replaceTopScreen({
            name: "program-overview",
            id: programId
        });
        pages.replace(pages.currentItem, programOverviewComponent);
    }

    // Раздел «Профиля», который открывает именованный экран. «Параметры» нужны
    // аудиту вёрстки: в них живёт блок «Оформление», и без такого экрана он не
    // попадал бы в снимки и smoke-проверки.
    property int profileSection: 0

    // Открытие экрана по имени: используется дизайн-ревью (--screen) и
    // восстановление состояния при возврате в приложение.
    function openProgramScreen(name) {
        const map = {
            "macro-calculator": openMacroCalculator,
            "profile-settings": openProfileSettings,
            "set-entry": openSetEntry,
            "exercise-edit": function () {
                if (typeof ExercisesEditorVM !== "undefined" && ExercisesEditorVM !== null)
                    ExercisesEditorVM.reset();

                shell.pushPage(exerciseEditorComponent);
            },
            "program-catalog": openProgramCatalog,
            "program-overview": openProgramOverview,
            "program-edit": openProgramEditor,
            "program-rules": openProgramRules,
            "program-rules-picker": openProgramRulesPicker,
            "program-stats": openProgramStats,
            // Экран дня для аудита вёрстки открывается на первом дне недели
            // активной программы. Данные грузятся здесь же: редактор перед ним не
            // открывался, поэтому ViewModel оставалась с пустым черновиком, и
            // снимок показывал «Отдых» и ноль упражнений вместо реального дня.
            "program-day": function () {
                const weekday = 1;
                const userId = AppContext !== undefined && AppContext !== null ? AppContext.currentUserId : "u1";

                if (ProgramVM !== undefined && ProgramVM !== null)
                    ProgramVM.reload(userId, "");

                const programId = ProgramVM !== undefined && ProgramVM !== null && ProgramVM.hasProgram ? ProgramVM.programId : "";

                if (ProgramEditVM === undefined || ProgramEditVM === null || programId.length === 0 || ProgramEditVM.load(userId, programId) === false) {
                    openProgramDay(weekday, 1);

                    return;
                }

                ProgramEditVM.selectDay(weekday);

                // Вид дня берётся у дня плана, а не задаётся константой: иначе
                // подпись разошлась бы с содержимым дня. `selectDay` возвращает
                // номер дня, поэтому вид читается из модели дней по orderIndex.
                const dayRow = ProgramEditVM.program_days.get(weekday - 1);
                const kindByName = {
                    workout: 0,
                    rest: 1,
                    cardio: 2,
                    mobility: 3
                };

                openProgramDay(weekday, dayRow !== null && dayRow !== undefined && kindByName[dayRow.kind] !== undefined ? kindByName[dayRow.kind] : 1);
            }
        };
        const opener = map[name];

        if (typeof opener === "function")
            opener();
    }

    // День и вид дня, открываемые на экране дня. Экрану нужны оба значения до его
    // создания, а задать их напрямую нельзя: страница живёт внутри Component, и
    // её идентификатор из оболочки недоступен.
    // Программа, открытая из каталога (срез S2). Пусто — активная: вход из
    // вкладок открывает текущую, а каталог передаёт идентификатор.
    property string programId: ""

    property int programDayWeekday: 1
    property int programDayKind: 1

    // Открытый экран дня недели.
    function openProgramDay(weekday, kind) {
        programDayWeekday = weekday;
        programDayKind = kind;
        shell.noteTabScreen({
            name: "program-day",
            kind: kind,
            weekday: weekday
        });
        shell.pushPage(programDayComponent);
    }

    // Открытый экран программы всегда над вкладками, поэтому «назад» просто
    // снимает верхний элемент стека (см. BackHandler ниже). Поп во время
    // идущего перехода не повторяется: двойная команда назад (системный жест
    // + кнопка) иначе снимала бы два экрана сразу.
    function goBack() {
        if (pages.busy === false && pages.depth > 1) {
            pages.pop();

            // Снятый экран уходит из истории вкладки: «назад» по цепочке
            // обязан идти синхронно со стеком, иначе восстановление вернуло
            // бы уже закрытый экран.
            const history = shell.tabHistory[tabBar.currentIndex] || [];

            if (history.length > 0)
                history.pop();

            shell.tabHistory[tabBar.currentIndex] = history;

            // Возврат к корню закрывает поток целиком.
            if (pages.depth === 1)
                shell.forgetTabScreen(tabBar.currentIndex);
        }
    }

    Component {
        id: exerciseEditorComponent

        ExercisesEditorPage {
            bottomInset: shell.pageBottomInset
            topInset: shell.topInset

            onBackRequested: shell.closeExerciseEditor()
            onFinished: shell.closeExerciseEditor()
        }
    }
    Component {
        id: programCatalogComponent

        ProgramCatalogPage {
            bottomInset: shell.pageBottomInset
            topInset: shell.topInset

            onBackRequested: shell.goBack()
            onCreateRequested: shell.openProgramEditor()
            // Открытие, правка и статистика приходят идентификатором: каталог
            // показывает все программы, а не только активную, поэтому и переход
            // должен нести конкретную программу.
            onOpenRequested: function (programId) {
                shell.openProgramOverview(programId);
            }
            onEditRequested: function (programId) {
                shell.openProgramEditor(programId);
            }
            onStatsRequested: function (programId) {
                shell.openProgramStats(programId);
            }
        }
    }
    Component {
        id: programOverviewComponent

        ProgramOverviewPage {
            bottomInset: shell.pageBottomInset
            topInset: shell.topInset
            programId: shell.programId

            onBackRequested: shell.goBack()
            onCreateRequested: shell.openProgramEditor()
            onEditRequested: function (programId) {
                shell.openProgramEditor(programId);
            }
            onRulesRequested: shell.openProgramRules()
            onStatsRequested: shell.openProgramStats()
        }
    }
    Component {
        id: programEditorComponent

        ProgramEditPage {
            bottomInset: shell.pageBottomInset
            topInset: shell.topInset
            programId: shell.programId

            onBackRequested: shell.goBack()
            onCreated: function (programId, wasEditing, previousPaused) {
                shell.finishProgramSave(programId, wasEditing, previousPaused);
            }
            onDayRequested: function (weekday, dayKind) {
                shell.openProgramDay(weekday, dayKind);
            }
            onRulesPickerRequested: shell.openProgramRulesPicker()
        }
    }
    Component {
        id: programDayComponent

        ProgramDayPage {
            bottomInset: shell.pageBottomInset
            topInset: shell.topInset

            // Экран создаётся в момент открытия, поэтому привязки успевают
            // получить актуальные день и вид дня.
            kind: shell.programDayKind
            weekday: shell.programDayWeekday

            onBackRequested: shell.goBack()
            onCloseRequested: shell.goBack()
        }
    }
    Component {
        id: programRulesComponent

        ProgramRulesPage {
            bottomInset: shell.pageBottomInset
            topInset: shell.topInset

            onBackRequested: shell.goBack()
        }
    }
    Component {
        id: programRulesPickerComponent

        ProgramRulesPickerPage {
            bottomInset: shell.pageBottomInset
            topInset: shell.topInset

            onBackRequested: shell.goBack()
        }
    }
    Component {
        id: programStatsComponent

        ProgramStatsPage {
            bottomInset: shell.pageBottomInset
            topInset: shell.topInset
            programId: shell.programId

            onBackRequested: shell.goBack()
        }
    }
    Component {
        id: macroCalculatorComponent

        MacroCalculatorPage {
            bottomInset: shell.pageBottomInset
            topInset: shell.topInset
            viewModel: MacroVM

            onBackRequested: shell.goBack()
        }
    }
    Component {
        id: setEntryComponent

        SetEntryPage {
            bottomInset: shell.pageBottomInset
            topInset: shell.topInset

            onBackRequested: shell.goBack()
        }
    }
    BottomTabBar {
        id: tabBar

        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        bottomInset: shell.bottomInset
        // Панель видна всегда (отзыв): на pushed-экранах она раньше пряталась,
        // и «назад» был единственным способом вернуться к разделам — теперь
        // разделы доступны с любого экрана одним тапом, как в нативных
        // приложениях. Pushed-страницы резервируют под панель место
        // (shell.pageBottomInset), контент не заезжает под неё.
        visible: true

        // Сигнал панели раньше никто не слушал: тап менял только подсветку
        // вкладки, а сама страница оставалась на месте — панель выглядела
        // сломанной. Здесь навигация и происходит: showTab снимает стек
        // и восстанавливает последний экран выбранной вкладки.
        onTabSelected: shell.showTab(index)
    }

    // — Временное сообщение о ещё не реализованном экране —
    Rectangle {
        id: toast

        anchors.bottom: tabBar.top
        anchors.bottomMargin: Spacing.sectionGap
        anchors.horizontalCenter: parent.horizontalCenter
        color: Theme.textPrimary
        height: toastLabel.implicitHeight + Spacing.itemGap * 2
        opacity: 0
        radius: Theme.radiusLg
        visible: opacity > 0
        width: Math.min(parent.width - Spacing.screenPadding * 2, toastLabel.implicitWidth + Spacing.screenPadding * 2)

        Behavior on opacity {
            NumberAnimation {
                duration: 180
            }
        }

        Label {
            id: toastLabel

            anchors.centerIn: parent
            color: Theme.background
            font: Typography.captionStrong
            horizontalAlignment: Text.AlignHCenter
            text: ""
            width: Math.min(implicitWidth, parent.width - Spacing.screenPadding * 2)
            wrapMode: Text.WordWrap
        }
        Timer {
            id: toastTimer

            interval: 2200

            onTriggered: toast.opacity = 0
        }
    }
    Connections {
        function onToastRequested(message) {
            toastLabel.text = message;
            toast.opacity = 0.94;
            toastTimer.restart();
        }

        target: Demo
    }
}
