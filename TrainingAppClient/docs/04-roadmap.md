# Дорожная карта разработки клиента («Фитнес-помощник»)

> Стратегия: сначала все экраны на
> mock-данных, затем замена mock на C++-слой, затем сеть; при этом **ранний
> вертикальный срез реального auth** выполняется сразу после auth-экранов.
> Связанные документы: `fitness-assistant-frontend-design.md` (спецификация),
> `reference-ui/README.md` (референс UI и статусы этапов), `android/README.md`
> (мобильный контур), `CODE_STYLE.md` (стиль C++).

## 1. Стратегия и принципы

**Точка старта (выполнено до issue 1):** этапы R1 (вкладки приведены к
референсу) и M1 (Android-контур: portrait-lock, `BackHandler`/`BackRouter`,
`PlatformChrome`, touch-полировка) закрыты; десктоп-запуск восстановлен
(устранены `BackHandler is not a type` и `ScrollIndicator policy`).
Существующие экраны работают на mock-синглтонах `MockCatalog`/`Demo`.

1. **Фаза S** — все экраны строятся на mock-данных (быстрый визуальный
   прогресс); **фаза A/B** заменяет mock на C++-слой (SQLite, репозитории,
   модели, сервисы, ViewModel); **фаза E** — интеграция с бэкендом;
   **фаза F** — качество, платформа, релиз.
2. **Ранний auth-срез (фаза V)**: сразу после auth-экранов на mock делается
   реальный auth/users (`check-email`, `signup`, `login`, `refresh`,
   `GET /users/{id}?withProfile=true`) — вход, токены, профиль с сервера.
3. **Контракт ролей mock = роли будущих `QAbstractListModel`** (конвенция
   `qml/Mock/MockCatalog.qml`): замена mock → C++ не переписывает delegates.
4. **Mock-API повторяет ключи ошибок бэкенда** (`{ key, message }`), поэтому
   UI входа/регистрации не переписывается при переходе на реальный API.
5. **Единая таблица ошибок**: `key → локализованный текст` живёт в одном месте
   (QML-синглтон в S1 → `AApi_Error` в V); `undefined_error` → «Не удалось
   выполнить операцию» + запись в лог.
6. **Клиентская валидация пароля** до запроса (8–64 символа, латиница,
   цифры, спецсимволы); истина остаётся за ответом сервера.
7. Каждая точка-заглушка `Demo.notify(...)` закрывается своим issue.

## 2. Единые gates (DoD каждого issue)

- `qmlformat` и `qmllint` чистые (допустим только известный `[import]`).
- C++: `clang-format --dry-run --Werror`; стиль по `docs/CODE_STYLE.md`
  (AStyle-UE адаптация: префиксы `A`/`As`/`Avm`, `Words_With_Underscores`,
  без исключений/умных указателей, Qt-контейнеры).
- Сборка desktop (MinGW, `build/Desktop_Qt_6_11_2_MinGW_64_bit_Debug`);
  Android-сборка — если затронута платформа.
- `ctest` зелёный.
- Скриншот в `docs/ui-preview/` при изменении UI.
- Аудит вёрстки `--diag` (см. `reference-ui/README.md`): `problems=0` на всех
  четырёх вкладках при 360/390/480 px, в framed-режиме и в тёмной теме.
- Обновление статуса в `docs/reference-ui/README.md`.

## 3. Фазы и issue

Размеры: S — малый, M — средний, L — большой.

### Фаза S1 — вход и mock-API (issues 1–2)

| # | Issue | Состав | Размер |
|---|---|---|---|
| 1 | Контракт mock-данных и mock-API | Расширение `qml/Mock/MockCatalog.qml`; синглтоны `qml/Mock/MockSession.qml` и `qml/Mock/MockApi.qml` (`check-email`/`signup`/`login`/`refresh`/`GET /users/{id}` с ключами ошибок бэкенда и переключаемым `failMode`); `MockTokenStore`; синглтон `ApiErrorText` (`key → qsTr`, включая зарезервированные `recovery_code_*`); док `docs/mock-data-contract.md` («поле → роль модели / поле DTO»). Данные для всех экранов S2: сессия и подходы (`weight/reps/rpe/note/done`), таймер, карточка плана (`description/infoTiles/days`), отзывы, карточка упражнения (`tags/technique/recordTiles/progressPoints`), анкета подбора, детали истории, параметры профиля | L |
| 2 | Auth-экраны на mock | `qml/Pages/Auth/AuthPage.qml`: Логин + Регистрация. Регистрация: email → `check-email` (потеря фокуса/debounce), занят → ошибка + «Войти»; имя + пароль (валидация 8–64, латиница/цифры/спецсимволы); параметры тела и цель — на том же экране (локально); `signup` → токены → возврат на вкладку-источник. Вход: `incorrect_password` → «Неверный пароль», подтверждённый в будущем `email_not_found` → «Проверьте email», текущий backend `500/undefined_error` → общая ошибка. «Забыли пароль?» — под флагом (issue 33). Роутинг: обычный запуск — гость (вкладки без сессии); вход из «Профиля» («Войти» вместо аватара), экран закрываемый; `sessionExpired` → принудительный вход; выход. Состояния через `MockApi.failMode` | L |

### Фаза V — вертикальный срез реального auth (issues 3–9)

| # | Issue | Состав | Размер |
|---|---|---|---|
| 3 | Каркас C++-слоя (минимальный) | `src/Data`, `src/Domain`, `src/Presentation`; подключение `Qt6::Network`, `Qt6::Sql`, `Qt6::Test`; `AsApp_Context`; инициализация в `main.cpp`; доки `docs/01-architecture.md` и `docs/03-api-contract.md` (реальные auth/users + ключи + сценарии 1–5 + «запрошено у бэкенда»); base URL через `--api-url` / QSettings / env | M |
| 4 | Тестовая инфраструктура | `enable_testing()`, `tests/` (Qt Test + `qmltestrunner`), smoke-тест QML-объектов, fake HTTP-сервер на `QTcpServer`, скрипт gates | M |
| 5 | API-ядро | `AsHttp_Client`: таймауты, экспоненциальные ретраи (сеть/5xx), `AApi_Error` (парсинг `{ key, message }`, маппинг HTTP→key: 401 → `invalid_token`/`token_expired`, 404 → `not_found`, 400 → валидация), единый DTO-ответ; логирование без токенов; тесты на fake-сервере | L |
| 6 | `AsSql_Database` + минимальная схема | Открытие/создание, `schema_version`, `Migrate()`, `Last_Error()`; схема v1: `auth_tokens`, `user_cache`, `profile_cache`, `profile_local`, `sync_outbox`; тесты на временной БД | M |
| 7 | Токены и авто-refresh | `AsToken_Store` (SQLite, приватный каталог); single-flight refresh (очередь параллельных запросов, один `POST /auth/refresh`, повтор исходного запроса один раз); при неудаче refresh → очистка токенов + `sessionExpired` → экран входа; cold start без сети — вход по локальным токенам с кэшем; тесты (истёк access/refresh, гонка) | L |
| 8 | Auth-интеграция | `AAuth_Api` (`check-email`, `signup`, `login`, `refresh`, локальный logout), `AAuth_Repository`, `Avm_Auth` (`idle/checkingEmail/emailTaken/signingUp/loggingIn/error(key)/authenticated/sessionExpired`); замена `MockApi` без правок UI; сохранение сессии; выход | M |
| 9 | Профиль с сервера | `AUsers_Api.Get_User(id, withProfile=true)` → `profile_cache`; показ имени, email, возраста, пола, роста, веса; локальные поля (уровень, цель, частота) из `profile_local`; изменения → БД + `sync_outbox` (воркер — issue 34) + статус «сохранено локально»; офлайн — кэш с пометкой | M |

**Блокеры issues 5–9 (ответы бэкенда):** заголовок авторизации; staging/prod
URL, HTTPS и тестовые учётки; семантика 404 при login; набор спецсимволов
пароля и ключ ошибки; точные refresh error keys; серверный logout. Deployed
URL и схемы `user`/`tokens`/`profile` уже получены. До остальных ответов —
разработка против fake-сервера, поля в
`docs/03-api-contract.md` помечены «assumed».

### Фаза S2 — остальные экраны на mock (issues 10–21)

| # | Issue | Состав | Размер |
|---|---|---|---|
| 10 | D2 Оверлей записи — каркас | `qml/Overlays/WorkoutOverlay.qml`: полноэкранный брат `StackView`, tab bar скрыт, `BackHandler` priority **40**, «офлайн» в status bar, chips упражнений, «Следующее упражнение»/«Завершить», пустое состояние «Подходы»; вход из CTA «Сегодня» | L |
| 11 | D2 Ввод подхода | `WeightRuler.qml` (шаг 2,5 кг, метки каждые 10 кг, акцентное деление), `RepsWheel.qml` (до 10 000, accent-рамка), `RpeSelector.qml` (5–10), раскрывающаяся заметка (IME не перекрывает CTA); подход ≤ 3 касаний | L |
| 12 | D2 Таймер отдыха | `qml/Overlays/RestTimerOverlay.qml`: priority **50**, реальный отсчёт, автозапуск после подхода, «+15 с»/«Пропустить»/«Стоп»; Back → возврат в запись | M |
| 13 | D2 Состояния «Сегодня» | День отдыха / план не выбран / «Продолжить» (draft); hero-CTA по состоянию; переключение через DevPanel и CLI | M |
| 14 | D3 Карточка плана | `PlanDetailPage.qml` (detail, tab bar виден): назад, рейтинг, описание, info-tiles, «День A», «Взять этот план», «Скопировать и изменить», «Отзывы» | M |
| 15 | D3 Отзыв на план | `PlanReviewsPage.qml` (вне вкладок, свой `BackHandler`): звёзды 1–5, необязательный текст, список отзывов | M |
| 16 | D3 Карточка упражнения | `ExerciseDetailPage.qml`: фото техники, теги, описание, tiles рекорда, «График прогресса»; входы из Сегодня/Упражнения/История | M |
| 17 | D3 Своё упражнение | Форма (название, группа, инвентарь, описание), валидация, IME; появление в каталоге с меткой «своё» | M |
| 18 | D3 Подбор плана | `PlanWizardPage.qml`: анкета (опыт, дни, инвентарь, цель), шаги, Back → шаг назад, результат → карточка плана | M |
| 19 | D4 История | Фильтры по периоду/упражнению, список тренировок, `WorkoutDetailPage.qml` (подходы), переходы к упражнению/плану | M |
| 20 | D1 Редактирование профиля | `ProfileEditPage.qml`: серверные поля (возраст, пол, рост, вес) и локальные (уровень, цель, частота); валидация; запись в кэш/`sync_outbox`; шапка и «Параметры» обновляются | M |
| 21 | Дизайн-ревью и документация этапа S | DevPanel: доступ ко всем экранам, состояниям и оверлеям; CLI `--screen <имя>`; скриншоты всех экранов в `docs/ui-preview/`; обновление `reference-ui/README.md` и `android/README.md`; мобильный чек-лист на эмуляторе | M |

### Фаза A — данные и домен на C++ (issues 22–26)

| # | Issue |
|---|---|
| 22 | **Полная схема v2 + миграции + seed**: `exercises`, `muscle_groups`, `equipment`, `plans`, `plan_days`, `day_exercises`, `workouts`, `sets`, `personal_records`, `plan_reviews`, `settings`; seed 1:1 с mock (UI визуально не меняется) |
| 23 | **Репозитории**: `AExercise_Repository`, `APlan_Repository`, `AWorkout_Repository`, `AProfile_Repository`, `AReview_Repository`, `ASettings_Repository` + абстракции и fake-реализации; тесты CRUD |
| 24 | **Модели-списки Presentation**: `QAbstractListModel` с ролями из контракта issue 1 (каталог упражнений с поиском/фильтром, план дня, планы, подходы сессии, история, детали тренировки, рекорды, heatmap, KPI, график, отзывы, детали плана/упражнения) |
| 25 | **Сервисы домена**: `AsWorkout_Logger` (сессия/draft/восстановление), `AsRest_Timer` (C++-таймер, живёт при свёрнутом окне), `AsProgress_Calculator` (тоннаж, частота, рабочий вес, дельты, рекорды), `AsPlan_Matcher`; unit-тесты расчётов |
| 26 | **ViewModels и регистрация в QML**: `Avm_Today`, `Avm_Plans`, `Avm_Exercises`, `Avm_Profile`, `Avm_Workout_Session`, `Avm_Rest_Timer`, `Avm_Plan_Detail`, `Avm_Exercise_Detail`, `Avm_Settings`; `QML_ELEMENT`/`QML_SINGLETON`; `--mock` остаётся для дизайн-ревью; QML-тесты привязок |

### Фаза B — экраны на реальных данных (issues 27–32)

| # | Issue |
|---|---|
| 27 | Сегодня + оверлей записи + таймер на данных: сессия, draft, завершение → история, пересчёт рекордов/KPI, восстановление после перезапуска |
| 28 | Планы: каталог, chips-сортировки, карточка плана, «взять», «скопировать и изменить», подбор через `AsPlan_Matcher` |
| 29 | Отзывы: запись/чтение, пересчёт `ratingAvg`/`reviewCount` |
| 30 | Упражнения: каталог/поиск/фильтры, карточка, график прогресса, своё упражнение в БД |
| 31 | Профиль: Прогресс (расчёты), История (фильтры + детали), Параметры (чтение/редактирование + отложенная синхронизация) |
| 32 | Настройки: тема, акцент, единицы, отдых по умолчанию, шрифт-скейл; персистентность; применение без рестарта |

### Фаза E — остальная сеть (issues 33–35)

| # | Issue |
|---|---|
| 33 | **Восстановление пароля (feature-flag)**: email → код → новый пароль; состояния `recovery_code_invalid`/`recovery_code_expired`; включается после реализации бэкенда |
| 34 | **Offline-first синхронизация (полная)**: фоновый воркер над `sync_outbox`, last-write-wins/версии, ретраи при появлении сети, индикатор «офлайн/синхронизация»; обслуживает профиль, затем тренировки |
| 35 | **Заготовка plans / workouts / exercises / reviews / медиа**: чек-лист полей и сценариев (`GET /plans/current`, `GET /workouts?state=draft`, каталог, отзывы, фото техники); активируется после согласования контракта |

### Фаза F — качество, платформа, релиз (issues 36–40)

| # | Issue |
|---|---|
| 36 | Покрытие тестами: unit (API-ядро, refresh-логика, репозитории, расчёты), QML-тесты auth-состояний и оверлеев, golden-скриншоты через `--screenshot` |
| 37 | CI (GitHub Actions): desktop + Android APK артефактом, линтеры, `ctest`; contract-тесты против staging при наличии URL и тестовой учётки |
| 38 | Локализация (ru/en, включая тексты ошибок API) и брендирование (иконка/splash Android, sp-размеры) |
| 39 | Android-полировка по чек-листу `android/README.md` + релиз-сборка (keystore, `versionName`, AAB/APK) |
| 40 | Документация и сквозная приёмка: README (сборка/запуск/тесты), статусы в docs, сценарий «регистрация → вход → профиль → тренировка → таймер → история» против реального бэкенда |

## 4. Зависимости

```
1 → 2 → 3 → 4 → 5 → 6 → 7 → 8 → 9   (вертикальный срез auth)
9 → 10 → 11 → 12 → 13                (S2: оверлеи и состояния)
14 → 15; 16, 17, 18 после 14
19 после 11; 20 после 9; 21 завершает S2
21 → 22 → 23 → 24 → 25 → 26 → 27 … 32 → 33–35 → 36 … 40
```

Issue 33 блокирован бэкендом (восстановление пароля), issue 35 — контрактом
по plans/workouts/exercises/reviews. Остальная сеть подключается только после
получения контракта; до этого клиент полностью работоспособен офлайн.

## 5. Запросы к бэкенду

**Критично до issues 5–9:**

1. **Получено:** Swagger 2.0 описывает `user`, `tokens`, `profile` и auth DTO;
   wire mapping зафиксирован в `docs/mock-data-contract.md`. В token DTO только
   `access_token` / `refresh_token`; profile: `age`, `gender`, `height`, `weight`.
2. **Получено:** приватные endpoint используют `Authorization: Bearer
   <access_token>`; при 401 — `POST /auth/refresh` с `refresh_token`, новая пара
   атомарно заменяет старую. Swagger security metadata пока неполна.
3. **Получено:** deployed API `http://fitness.nought.ru/api/v1`, Swagger UI
   `http://fitness.nought.ru/api/v1/swagger`. Осталось: staging/prod URL, HTTPS,
   тестовые учётки и рателимиты.
4. **Наблюдается:** `POST /auth/login` при несуществующем email возвращает
   `500/undefined_error`. Запрошено у бэкенда: заменить на стабильный 4xx и
   отдельный key (`email_not_found`) либо формально закрепить текущую семантику.
5. Правила пароля: точный набор спецсимволов, обязательные классы символов,
   ключ ошибки при невалидном пароле.
6. **Получено:** refresh одноразовый с ротацией пары; replay старого refresh
   должен завершать сессию. Точный серверный key replay-ошибки ещё не описан.
7. Серверный logout / отзыв токенов: есть ли эндпоинт.

**Для фаз S2/B:**

8. Write-эндпоинт профиля (`PUT/PATCH /users/{id}/profile`) — без него
   параметры тела живут локально в `sync_outbox`; входят ли в профиль сервера
   уровень, цель, тренировок в неделю.
9. **Получено:** параметры тела не входят в `signup`; профиль возвращается
   только из `GET /users/{id}?withProfile=true` и может отсутствовать.
10. Правила валидации `name` / `email` (лимиты, ключи ошибок).

**Для фаз E/F:**

11. Контракт и сроки по plans / workouts / exercises / reviews / медиа
    (блокер issue 35 и серверных KPI).
12. Восстановление пароля: сроки, формат кода (длина, срок жизни), эндпоинты.

## 6. Зафиксированные клиентские решения

- **Хранение refresh-токена**: защищённое платформенное хранилище; SQLite в
  Android internal storage — промежуточная реализация issue 7, Android Keystore
  через JNI — целевое усиление до production-релиза.
- **Офлайн-старт**: с сохранёнными токенами пользователь входит без сети,
  данные из кэша, синхронизация — при появлении сети.
- **Single-flight refresh**: при 401 параллельные запросы встают в очередь,
  выполняется один `POST /auth/refresh`, исходный запрос повторяется один раз;
  неудача refresh → `sessionExpired` → экран входа.
- **Единая таблица ошибок**: `key → текст`; `undefined_error` → «Не удалось
  выполнить операцию» + лог; `recovery_code_*` зарезервированы.
- **Клиентская валидация** пароля/email до запроса; истина — ответ сервера.
- **Mock-API повторяет подтверждённые ключи ошибок** бэкенда; неподтверждённые
  UX-ключи явно помечены assumed. UI не переписывается при переходе к реальному
  API: расхождения закрываются слоем DTO/маппинга (`AApi_Error`, структуры DTO).

## 7. Ведение статуса

Статус issue фиксируется в этой таблице при завершении работы над ним
(коммит, закрывающий issue, ссылается на номер в сообщении: `#N …`).

| Issue | Статус | Примечание |
|---|---|---|
| 1 | выполнен | MockCatalog для S2, MockApi/MockSession/MockTokenStore, ApiErrorText, `docs/mock-data-contract.md` |
| 2 | выполнен | AuthPage: login/signup, check-email, валидация, локальные параметры, роутинг сессии, logout, failMode, QML/UI-тесты |
| 3 | выполнен | `AsApp_Context`, Data/Domain/Presentation, Network/Sql/Test, base URL, архитектура и API-контракт, Qt Test |
| 4 | выполнен | CTest, Qt Test, qmltestrunner/object smoke, fake HTTP server на QTcpServer, gates, pre-commit hook и CI |
| 5–40 | не начат | — |

Backlog (вне scope плана): хаптика/звуки, уведомления о тренировках, экспорт
данных, планшеты/landscape, Android Keystore (JNI), серверная реализация
backlog-эндпоинтов.



