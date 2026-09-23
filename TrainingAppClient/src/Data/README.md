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

`Database/AsSql_Database` реализует SQLite schema v1 в приватном каталоге:
`auth_tokens`, `user_cache`, `profile_cache`, `profile_local`, `sync_outbox` и
`schema_version`. Кэш повторяет полезные backend-поля `display_name`,
`created_at`, `updated_at`, `age`, `gender`, `height_cm`, `weight_kg`, но никогда
не хранит серверные `pass_hash` и `token_hash`.

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
transport-DTO. QML остаётся на mock до снятия backend-блокеров.

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

Зависит от Qt и внешних систем; не содержит визуального QML.