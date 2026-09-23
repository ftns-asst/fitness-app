# Контракт API auth/users

## 1. Источники и статусы достоверности

Backend задеплоен и проверен 20.09.2026:

- API base URL: `http://fitness.nought.ru/api/v1` — **подтверждено backend**;
- Swagger UI: `http://fitness.nought.ru/api/v1/swagger`;
- Swagger JSON: `http://fitness.nought.ru/api/v1/swagger/doc.json`;
- Swagger `host`: `fitness.nought.ru`;
- Swagger `basePath`: `/api/v1`;
- Swagger `schemes`: пустой массив.

Метки документа:

- **Swagger** — поле или статус опубликованы в спецификации;
- **подтверждено backend** — передано владельцем backend вне Swagger;
- **observed** — фактический безопасный запрос к текущему dev API;
- **assumed** — временное клиентское предположение;
- **запрошено у backend** — блокер до включения реальной интеграции.

Default клиента совпадает с задеплоенным API:
`http://fitness.nought.ru/api/v1`. Корень `/api/v1/` не является отдельным
endpoint и отвечает `404`; это не признак недоступности API. Отдельные
staging/prod URL и HTTPS — **запрошено у backend/DevOps**.

## 2. Общие правила

- `Content-Type: application/json` — **Swagger**.
- Публичные endpoints: `check-email`, `signup`, `login`, `refresh` —
  **подтверждено backend**.
- `GET /users/{id}` приватный и требует access token — **подтверждено backend**.
- Точное значение `Authorization` (`Bearer <access_token>` или raw token) —
  **запрошено у backend**: Swagger не содержит `securityDefinitions` и не
  объявляет security ни глобально, ни для `/users/{id}`.
- Ответ ошибки имеет форму `{ "key": string, "message": string }` —
  **Swagger**. Перечень допустимых `key` Swagger не задаёт.
- Клиент показывает локализованный текст по `key`; backend `message` хранится
  для диагностики и не является пользовательским текстом.
- Все response properties в Swagger формально не помечены `required`; клиент
  валидирует обязательные для сценария поля и возвращает `undefined_error`,
  если успешный ответ неполон — **assumed** до уточнения схем.

## 3. Endpoints

### 3.1. `POST /auth/check-email`

Публичный.

Request (**Swagger**, required `email`):

```json
{
  "email": "user@example.com"
}
```

Response 200:

```json
{
  "free": true
}
```

`free=false` — email занят, это успешный ответ, не API error.

Swagger statuses: `200`, `400`, `404`, `500`.

Observed:

- свободный корректный email → `200 { "free": true }`;
- некорректный email → `400 { "key": "undefined_error", ... }`.

Почему endpoint документирует `404` — **запрошено у backend**.

### 3.2. `POST /auth/signup`

Публичный. Имя не уникально — **подтверждено backend**.

Request (**Swagger**, все поля required):

```json
{
  "email": "user@example.com",
  "name": "Имя пользователя",
  "password": "Strong1!"
}
```

Параметры тела не входят в signup — **подтверждено backend**.

Response 200:

```json
{
  "user": {
    "id": "2e8cbeec-3528-45bc-908b-cdf76944d9c3",
    "name": "Имя пользователя",
    "email": "user@example.com",
    "created_at": "2026-09-20T12:00:00Z"
  },
  "tokens": {
    "access_token": "...",
    "refresh_token": "..."
  }
}
```

Swagger statuses: `200`, `400`, `404`, `500`.

Ограничение password `8..64` есть в Swagger. Точный набор разрешённых
спецсимволов, обязательные классы символов и error key —
**запрошено у backend**.

### 3.3. `POST /auth/login`

Публичный.

Request (**Swagger**, `email` и `password` required):

```json
{
  "email": "user@example.com",
  "password": "Strong1!"
}
```

Response 200 совпадает с signup: `{ user, tokens }`.

Swagger statuses: `200`, `400`, `404`, `500`.

Observed для отсутствующего email: `500` с key `undefined_error`. Целевой
стабильный статус/key (`404 email_not_found` или иной) —
**запрошено у backend**. Key неверного пароля `incorrect_password` пока
**assumed** на основании UI-контракта; требуется тестовая учётная запись.

### 3.4. `POST /auth/refresh`

Публичный в смысле отсутствия access token; принимает refresh token.

Request (**Swagger**, `refresh_token` required):

```json
{
  "refresh_token": "..."
}
```

Response 200:

```json
{
  "tokens": {
    "access_token": "...",
    "refresh_token": "..."
  }
}
```

Swagger statuses: `200`, `400`, `404`, `500`; `401` не документирован.

Ротация refresh token, одноразовость старого токена, expiry claims/поля и
ключи ошибок — **запрошено у backend**. До ответа клиент проектируется под
ротацию: успешный ответ атомарно заменяет всю пару токенов.

### 3.5. `GET /users/{id}`

Приватный — **подтверждено backend**.

Parameters:

- `id`: path, string UUID, required — **Swagger**;
- `withProfile`: query, boolean, optional — **Swagger**.

Без профиля или при `withProfile=false`:

```json
{
  "id": "2e8cbeec-3528-45bc-908b-cdf76944d9c3",
  "name": "somename",
  "email": "test@gmail.com",
  "created_at": "0001-01-01T00:00:00Z"
}
```

С профилем:

```json
{
  "id": "2e8cbeec-3528-45bc-908b-cdf76944d9c3",
  "name": "somename",
  "email": "test@gmail.com",
  "created_at": "0001-01-01T00:00:00Z",
  "profile": {
    "age": 0,
    "gender": "male",
    "height": 0,
    "weight": 0
  }
}
```

Если `withProfile=true`, но поле `profile` отсутствует, профиль ещё не заполнен
— **подтверждено backend**; это не ошибка.

`gender`: `male | female` — **Swagger**. Единицы `height`/`weight`, допустимые
диапазоны и смысл нулей — **запрошено у backend**.

Swagger statuses: `200`, `400`, `404`, `500`; `401/403` и security declaration
отсутствуют — **запрошено у backend**.

## 4. DTO mapping

| Backend JSON | Data DTO / QML |
|---|---|
| `created_at` | `createdAt` |
| `access_token` | `accessToken` |
| `refresh_token` | `refreshToken` |
| `withProfile` | `withProfile` |
| profile `height` | `heightCm` — **assumed unit** |
| profile `weight` | `weightKg` — **assumed unit** |

Snake case преобразуется только в Data mapper. Репозитории, ViewModel и QML не
должны зависеть от JSON-написания.

## 5. Ключи ошибок

Swagger задаёт только тип string. Текущий клиентский реестр:

| Key | Статус |
|---|---|
| `undefined_error` | observed; fallback для неизвестной ошибки |
| `email_taken` | assumed |
| `email_not_found` | assumed, ожидает решение backend |
| `incorrect_password` | assumed |
| `invalid_email`, `invalid_name` | client/mock validation; backend key не подтверждён |
| `invalid_password`, `password_*` | client/mock validation; backend keys не подтверждены |
| `invalid_token`, `token_expired` | reserved для issues 5–8; backend keys не подтверждены |
| `network_error`, `timeout`, `server_error`, `rate_limited` | client mapping |
| `recovery_code_invalid`, `recovery_code_expired` | reserved issue 33 |

Маппинг HTTP status → key реализован в `AApi_Error` issue 5. Валидный непустой
backend `key` имеет приоритет. Если DTO отсутствует/невалиден или key пустой:

| Условие | Fallback key |
|---|---|
| network error без HTTP status | `network_error` |
| transfer timeout / HTTP 408 | `timeout` |
| HTTP 400 | `validation_error` |
| HTTP 401 | `invalid_token` |
| HTTP 404 | `not_found` |
| HTTP 429 | `rate_limited` |
| HTTP 5xx | `server_error` |
| остальное | `undefined_error` |

`401 → invalid_token` — только transport fallback. Если backend вернул
`token_expired`, он сохраняется. До подтверждения нельзя считать один status
достаточным для конкретного UX-сценария.

`AsHttp_Client` по умолчанию использует timeout 10 секунд и максимум два retry
(250/500 мс) для network errors без HTTP status и HTTP 5xx. Generic GET retry
разрешает; JSON POST запрещает его по умолчанию, пока endpoint-адаптер явно не
подтвердит повторяемость операции. `401`, `400`, `404`, `408`, `429` не
повторяются. Auto-refresh не является transport retry и реализуется issue 7.

Логи содержат request id, method, path без query, status/network code, attempt и
retry delay. Body, headers, query, access/refresh token и backend message не
логируются.

## 6. Сценарии 1–5

### Сценарий 1. Проверка email и регистрация

1. UI валидирует email/password локально.
2. `POST /auth/check-email`.
3. При `free=false` UI предлагает вход.
4. При `free=true` выполняется `POST /auth/signup`.
5. Клиент атомарно сохраняет пару токенов и пользователя; параметры тела
   сохраняются локально и не входят в signup DTO.

### Сценарий 2. Вход

1. `POST /auth/login`.
2. Валидируется наличие `user.id`, `tokens.access_token`,
   `tokens.refresh_token`.
3. Пара токенов сохраняется до публикации authenticated state.
4. UI возвращается на вкладку, с которой был открыт вход.

### Сценарий 3. Получение пользователя и optional profile

1. Клиент добавляет Authorization с access token — точный формат ожидается.
2. `GET /users/{id}?withProfile=true`.
3. User сохраняется в cache.
4. Отсутствующий `profile` трактуется как «не заполнен».
5. При отсутствии сети используется cache с offline-маркером (issue 9).

### Сценарий 4. Истёк access token

1. Приватный запрос получает auth failure (точный status/key ожидается).
2. Запрос ставится в очередь single-flight refresh.
3. Выполняется один `POST /auth/refresh` с сохранённым refresh token.
4. Новая пара токенов атомарно заменяет старую.
5. Исходный запрос повторяется один раз.

Реализация — issue 7; повторные ретраи после второго auth failure запрещены.

### Сценарий 5. Refresh неуспешен

1. Refresh возвращает invalid/expired token либо невосстановимую ошибку.
2. Клиент очищает пару токенов.
3. Session переходит в `sessionExpired`.
4. Открывается обязательный экран входа без кнопки «Назад».
5. Cache не удаляется; после нового входа он может быть показан до синхронизации.

Сетевой timeout сам по себе не должен уничтожать валидную локальную сессию —
точная offline policy фиксируется в issues 7 и 9.

## 7. Отображение backend БД в клиентскую SQLite v1

Клиент не копирует серверную схему буквально. Полезные поля `users` сохраняются
в `user_cache`: `id`, `email`, `display_name`, `created_at`, `updated_at`; поля
`user_profiles` — в `profile_cache`: `age`, `gender`, `height_cm`, `weight_kg`,
`updated_at`. Серверный `pass_hash` никогда не передаётся и не хранится.

`auth_tokens` содержит полученные access/refresh token и их клиентские сроки.
Это не копия серверной таблицы `tokens`: серверные `token_hash`, `used_at` и
внутренний `id` клиенту неизвестны. Запись и ротация токенов реализуются issue 7.

## 8. Запрошено у backend

1. Staging/prod URL, сроки перехода на HTTPS и тестовые учётки. Dev/deployed
   URL уже получен: `http://fitness.nought.ru/api/v1`.
2. Точный формат Authorization и обязательные headers.
3. Required-поля успешных responses и формат/expiry токенов.
4. Refresh rotation/reuse semantics.
5. Стабильные status/key для unknown email, wrong password, invalid/expired token.
6. Полные password/name/email validation rules.
7. Единицы и диапазоны profile, семантика нулевых значений.
8. Наличие server logout/revocation endpoint.
9. Почему публичные endpoints и user endpoint документируют `404`, но не `401`.