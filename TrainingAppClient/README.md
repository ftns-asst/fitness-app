# TrainingAppClient

QML-клиент приложения **«Фитнес-помощник»**: план дня, запись тренировок, каталог программ и упражнений, прогресс и история.

Целевая платформа — **Android**. Стек — **Qt 6 / QML / C++** (проект требует
Qt 6.8+, в разработке — 6.11.2). UI пока работает на mock-данных; минимальный
C++ composition root и конфигурация будущего API уже подключены.

## Продукт

Главный сценарий — быстро начать тренировку дня. Четыре раздела в нижней панели:

| Вкладка | Назначение |
|---|---|
| **Сегодня** | План на день и вход в тренировку |
| **Планы** | Каталог программ и подбор |
| **Упражнения** | Каталог, поиск, свои упражнения |
| **Профиль** | Прогресс, история, параметры |

Запись тренировки и таймер отдыха — полноэкранные оверлеи (tab bar скрыт). Прогресс и история не отдельные вкладки: они внутри «Профиля».

Клиент задуман **offline-first**: локальные данные сразу, синхронизация с бэкендом позже. До сети UI работает на заглушках `MockCatalog`.

## Демо-интерфейс

Визуальный прототип без C++-репозиториев и HTTP. Обычный запуск — гостевой: все четыре вкладки доступны без сессии. Вход и регистрация открываются из «Профиля» кнопкой «Войти» и закрываются без обязательной авторизации. Нереализованные переходы показывают toast («этап D2/D3/D4»), а не пустые экраны.

Окно: «Фитнес-помощник — UI демо». На широком десктопе контент не растягивается: максимум `Theme.contentMaxWidth` (480 px), по центру, в телефонных пропорциях.

### Экраны

**Сегодня** — hero-карточка плана (бейджи, число упражнений, длительность, неделя), CTA «Начать тренировку», список упражнений дня, равноширинные «Другой план» / «История», карточка личного рекорда.

**Планы** — promo «Не знаете, что выбрать?» с кнопкой «Подобрать», chips-сортировка (рейтинг / популярные / по времени), карточки программ с рейтингом. Карточка кликабельна целиком.

**Упражнения** — поиск и «+ Своё» в одной строке, фильтры по мышечным группам, каталог карточками с плейсхолдером «фото техники».

**Профиль** — для гостя кнопка «Войти» вместо аватара; после входа — шапка с инициалами и «Выйти из аккаунта». Сегменты **Прогресс / История / Параметры**. На «Прогрессе»: KPI, график, heatmap регулярности (2×12 недель), список рекордов.

Этап R1 (вкладки и tab bar), M1 (Android-контур) и S1 issue 2 (вход/регистрация на mock) закрыты. Не в UI: оверлей записи, таймер отдыха, карточки плана и упражнения, отзывы, подбор плана.

### Панель Dev

Кнопка **Dev** справа сверху — инструмент дизайн-ревью, не часть продукта:

- акцент: синий, бирюзовый, красно-оранжевый, фиолетовый;
- светлая / тёмная тема;
- рамка телефона 390×812 (status bar «9:41 / LTE / 87%» и home indicator);
- переход по вкладкам.

## Сборка и запуск

Нужны Qt 6.8+ и модули Quick, Quick Controls 2, Layouts, Effects, Network, Sql;
для desktop-тестов также Test и QuickTest. Сборка через Qt Creator (CMake).

Задеплоенный API: `http://fitness.nought.ru/api/v1`. Документация backend:
`http://fitness.nought.ru/api/v1/swagger`. В issue 5 готово API-ядро
`AsHttp_Client` (timeout/retry/error mapping), но endpoint-адаптеры и реальный
Auth UI подключаются в issues 7–9; текущий UI по-прежнему использует mock
auth/data.

### Desktop

Kit: `Desktop Qt 6.11.2 MinGW 64-bit`, каталог `build/Desktop_Qt_6_11_2_MinGW_64_bit_Debug`. Цель — `appTrainingAppClient`.

Флаги дизайн-ревью:

| Флаг | Назначение |
|---|---|
| `--frame` | окно-рамка 390×812 |
| `--tab <0..3>` | стартовая вкладка (0 — Сегодня) |
| `--screen <имя>` | `auth-login`, `auth-signup`, `today`, `plans`, `exercises`, `profile` |
| `--authenticated` | открыть приложение с mock-сессией |
| `--unauth` | оставить гостя даже при `--tab` / `--screen` / `--authenticated` |
| `--open-auth` | открыть вход как из «Профиля» (гость, экран закрываемый) |
| `--auth-fail <operation:key>` | принудительная ошибка auth mock API |
| `--api-url <url>` | base URL будущего API; выше QSettings/env/default |
| `--dark` | тёмная тема |
| `--accent <0..3>` | цвет акцента |
| `--width <px>` / `--height <px>` | размер окна |
| `--screenshot <путь.png>` | снимок контента и выход |
| `--diag` | аудит вёрстки в stderr, затем выход |

Аудит (`QT_FORCE_STDERR_LOGGING=1`):

```powershell
$env:QT_FORCE_STDERR_LOGGING = '1'
.\appTrainingAppClient.exe --diag --width 360 --height 780 --tab 2
```

Норматив: `problems=0` на всех четырёх вкладках при ширине 360 / 390 / 480 px, в рамке и в тёмной теме. Тени `MultiEffect` и горизонтальный скролл chips аудит не считает ошибкой.

### Тесты и локальные gates

Единый скрипт выполняет проверку `qmlformat`, `clang-format`, конфигурацию
CMake, `qmllint`, desktop-сборку и все CTest-тесты:

```powershell
.\scripts\run-gates.ps1 `
    -BuildDir build/gates `
    -Generator Ninja `
    -QtPrefix D:\Qt\6.11.2\mingw_64
```

Зарегистрированные тесты:

- `qml_tests` — auth/users mock API, ошибки, сессия, токены, роли
  `MockCatalog`, presentation-контракты и состояния `AuthPage`;
- `app_context` — Qt Test приоритетов и валидации API base URL;
- `test_http_server` — GET/POST, headers/body, очередь ответов, delay,
  disconnect, malformed и split-body сценарии fake HTTP server;
- `http_client` — единый response DTO, error mapping, timeout, network/5xx
  retry, retry exhaustion, JSON и отсутствие токенов/query/backend message в
  логах;
- `tst_auth_page.qml` — валидация, login/signup, занятый email, failMode,
  транзакционное сохранение локальных параметров, гостевой вход и logout;
- `tst_qml_object_smoke.qml` — создание `AppShell`, `AuthPage` и form controls;
- `ui_smoke_tab_0..3` — `--diag` для каждой основной вкладки;
- `ui_smoke_auth-*` — login/signup при 360/390/480 px, framed и dark.

GitHub Actions workflow `.github/workflows/training-app-ci.yml` запускает тот
же скрипт на каждом `push` в любую ветку и для каждого pull request. Workflow
выполняется после отправки коммитов на GitHub; для проверки непосредственно
перед локальным commit можно один раз явно включить версионируемый hook:

```powershell
.\scripts\install-git-hooks.ps1
```

Команда только устанавливает `core.hooksPath=.githooks` в текущем clone и не
создаёт commit/push. После включения неуспешный gate блокирует локальный commit.
Для обязательного запрета merge включите в GitHub branch protection required
status check `Format, build and test (Qt 6.11.2)`.

### Android

Portrait-only, label «Фитнес-помощник». Kit: `Qt 6.11.2 for Android x86_64`. На устройстве рамка телефона не рисуется: Safe Area, системные полосы под тему, «назад» закрывает Dev-панель и стек деталей.

Подробности и чек-лист: [`docs/android/README.md`](docs/android/README.md).

## Структура

```
qml/Main.qml              окно, рамка, CLI, аудит вёрстки
qml/AppShell.qml          вкладки, tab bar, Dev, toast
qml/DevPanel.qml          панель дизайн-ревью
qml/Pages/                Auth, Сегодня, Планы, Упражнения, Профиль
qml/Components/           кнопки, карточки, график, heatmap, tab bar
qml/Theme/                цвета, типографика Inter, отступы
qml/Mock/                 UI demo, mock-каталог, auth/users API и сессия
src/App/                  AsApp_Context и конфигурация запуска
src/Data/                 HTTP/SQLite/repository implementations (issues 5+)
src/Domain/               сущности, repository contracts, domain services
src/Presentation/         будущие C++ ViewModel и QAbstractListModel
tests/                    C++/QML unit tests, fake HTTP и UI smoke через CTest
scripts/                  единый gate и opt-in установка Git hooks
src/Platform/             цвет системных полос Android
android/                  манифест (portrait-lock)
resources/InterFont/      встроенные начертания Inter
docs/                     дизайн, референс, roadmap
```

## Документация

| Документ | Содержание |
|---|---|
| [`docs/fitness-assistant-frontend-design.md`](docs/fitness-assistant-frontend-design.md) | Навигация, токены, экраны |
| [`docs/01-architecture.md`](docs/01-architecture.md) | Слои C++, composition root, конфигурация API |
| [`docs/03-api-contract.md`](docs/03-api-contract.md) | Реальный контракт auth/users и открытые вопросы |
| [`docs/reference-ui/README.md`](docs/reference-ui/README.md) | Скриншоты-референс и CLI |
| [`docs/04-roadmap.md`](docs/04-roadmap.md) | Фазы: mock → C++ → сеть → релиз |
| [`docs/mock-data-contract.md`](docs/mock-data-contract.md) | Роли моделей, mock API и assumed DTO |
| [`docs/android/README.md`](docs/android/README.md) | Сборка на эмулятор, чек-лист |
| [`docs/CODE_STYLE.md`](docs/CODE_STYLE.md) | Стиль C++ |
