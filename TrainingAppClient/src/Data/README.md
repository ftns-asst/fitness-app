# Data

Инфраструктурный слой: HTTP, DTO-маппинг, SQLite, token store и реализации
репозиториев.

`Network/` содержит API-ядро issue 5:

- `AsHttp_Client` — единый `QNetworkAccessManager`, request/response DTO,
  transfer timeout и настраиваемый exponential retry для network/5xx;
- `AApi_Error` — backend `{ key, message }` с fallback-маппингом transport/HTTP
  ошибок.

API-адаптеры решают, можно ли повторять конкретный запрос: generic GET включает
retry, `Post_JSON` по умолчанию его запрещает. Auto-refresh и token store не
входят в HTTP core и добавляются issue 7.

`Database/AsSql_Database` реализует SQLite schema v2 в приватном каталоге:
`auth_tokens`, `user_cache`, `profile_cache`, `profile_local`, `sync_outbox` (v1)
плюс модель глобальной программы среза W1 (docs/02-program-model.md §8):
`exercises`, `programs`, `goal_targets`, `program_schedule`, `program_days`,
`program_day_exercises`, `program_rules`, `rule_checkins`, `program_revisions`,
`workouts`, `sets`, `body_measurements`, `program_stats`, `lifetime_stats`,
резерв `nutrition_entries`/`nutrition_day_totals`; миграция идемпотентна,
инвариант «одна активная программа» — частичный уникальный индекс. Кэш
повторяет полезные backend-поля `display_name`,
`created_at`, `updated_at`, `age`, `gender`, `height_cm`, `weight_kg`, но никогда
не хранит серверные `pass_hash` и `token_hash`. Каталог `plans/…` и seed —
миграция v3 (фаза B).

`Auth/` реализует issues 7–8:

- `AsToken_Store` — хранилище пары токенов в таблице `auth_tokens`
  (единственная строка id = 1): атомарный upsert при ротации, ISO-даты expiry,
  чтение для cold start; значения токенов не логируются;
- `AsAuth_Session` — приватные запросы с `Authorization: Bearer <token>`
  (assumed до ответа backend) и single-flight refresh: первый auth failure
  отправляет один `POST /auth/refresh`, параллельные запросы ждут в очереди и
  повторяются по одному разу с новой парой токенов; HTTP-отказ refresh очищает
  токены и сигналит `sessionExpired`, transport-сбой сессию сохраняет;
- `AAuth_Api` — HTTP-адаптер `check-email`/`signup`/`login`: snake case → DTO
  `SAuth_User`/`SAuth_Session_Data`, валидация обязательных полей (неполный
  успешный ответ → `undefined_error`), passthrough backend-ключей ошибок;
- `AAuth_Repository` — оркестрация сценариев: успешный login/signup атомарно
  пишет `user_cache` и пару токенов и активирует сессию; локальный logout
  очищает токены и сохраняет кэш пользователя; `Restore_Local_Session()` —
  cold start; форвард `sessionExpired` наверх.

`src/Presentation/Auth_VM` (`Avm_Auth`) — единственный пока C++ ViewModel:
стабильные состояния `idle/checkingEmail/emailTaken/signingUp/loggingIn/
error(key)/authenticated/sessionExpired` поверх репозитория; UI не видит
transport-DTO. `src/Presentation/Auth_Qml_Bridge` (`Avm_Auth_Bridge`) — QML-мост
реального auth: при `--real-auth` AuthViewModel делегирует ему check-email/
signup/login/logout/saveLocalProfile и зеркалит события в MockSession (как это
делает сам MockApi), поэтому Main/AppShell/страницы не меняются; без флага и в
QML-тестах UI работает на mock.

`Users/` реализует issue 9:

- `AUsers_Api` — приватный `GET /users/{id}?withProfile=true` через
  `AsAuth_Session` (Bearer, single-flight refresh); DTO `SUsers_User`/
  `SUsers_Profile` с assumed-единицами height → Height_Cm, weight → Weight_Kg
  (docs/03, §4); обязательное поле `id`, отсутствие `profile` — не ошибка;
- `AUsers_Repository` — успех пишет `user_cache`/`profile_cache`; любая ошибка
  при наличии кэша отдаёт его с маркером `from_cache` (офлайн-пометка, docs/03,
  §3.5); `Get_Cached_User` читает кэш без сети; локальные параметры
  (уровень/цель/частота) пишутся в `profile_local` + запись в `sync_outbox`
  (доставка — воркер issue 34) со статусом `saved_locally`.

`Program/` реализует срезы W3–W4 (issue 23, `02-program-model.md` §3/§4/§9):

- `AProgram_Repository` — локальный репозиторий модели программы поверх
  schema v2: чтение (`Load_Program`, `Load_Program_Full` — дерево цели/дни/
  предписания/слоты, `Get_Active_Program_Id`), транзакционные правки
  (`Replace_Goal_Targets`, `Change_Term`, `Replace_Schedule`, `Replace_Structure`,
  `Change_State`). Каждая правка валидирует вход и состояние (инвариант 24:
  completed/cancelled/archived — read-only), инкрементирует `programs.revision`
  и пишет строку `program_revisions` со снимком СТАРОГО раздела: после замены
  дней FK обнуляет ссылки фактов, и план прошлых недель живёт только в снимках.
  Переходы состояний — по §3.1; активация штампует `actual_start_date`.
- `ARule_Repository` — правила программы и отметки: `Replace_Rules` заменяет
  набор транзакционно (валидация, `revision++`, снимок старых правил),
  `Save_Checkin` — upsert на пару `ruleId + date` (§3.7): расчётная отметка
  не перезаписывает `manual`, ручная правка перекрывает расчётную этого дня;
  отметки доступны и в read-only-состояниях (инвариант 24).
- `AMeasurement_Repository` — `body_measurements`: одна строка на пару
  (user, date), upsert с слиянием по `Has_*`-полям (утренний вес не теряется
  при вечерней записи обхватов).
- `AStats_Repository` — кэш `program_stats`/`lifetime_stats` (§5.4): хранение
  JSON-нагрузки с меткой `computedAt` и ревизией, проверка актуальности,
  инвалидация (удаление строки) по факту/правке/завершению программы.
  Пересчёт делает `AsProgress_Calculator` (W7).

Зависит от Qt и внешних систем; не содержит визуального QML.