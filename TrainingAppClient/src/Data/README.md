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

`Auth/` реализует issue 7:

- `AsToken_Store` — хранилище пары токенов в таблице `auth_tokens`
  (единственная строка id = 1): атомарный upsert при ротации, ISO-даты expiry,
  чтение для cold start; значения токенов не логируются;
- `AsAuth_Session` — приватные запросы с `Authorization: Bearer <token>`
  (assumed до ответа backend) и single-flight refresh: первый auth failure
  отправляет один `POST /auth/refresh`, параллельные запросы ждут в очереди и
  повторяются по одному разу с новой парой токенов; HTTP-отказ refresh очищает
  токены и сигналит `sessionExpired`, transport-сбой сессию сохраняет.

Зависит от Qt и внешних систем; не содержит визуального QML.