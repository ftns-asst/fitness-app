# Контракт API: auth, users, exercises

## 1. Источники и статусы достоверности

Backend задеплоен и проверен 20.09.2026:

- API base URL: `http://fitness.nought.ru/api/v1` — **подтверждено backend**;
- Swagger UI: `http://fitness.nought.ru/api/v1/swagger`; тот же сервис отвечает по
  адресу из публикации — `http://94.228.166.134:8181/api/v1/swagger/index.html`
  (**observed** 08.10.2026: `doc.json` обоих адресов совпадает байт в байт, 42 141
  байт), поэтому базовый URL клиента не меняется;
- Swagger JSON: `http://fitness.nought.ru/api/v1/swagger/doc.json`;
- Swagger `host`: `fitness.nought.ru`;
- Swagger `basePath`: `/api/v1`;
- Swagger `schemes`: пустой массив;
- опубликованное дополнение к документации сервиса («Fitness Assistant API»,
  получено 08.10.2026) — **документация backend**, полный текст сохранён в
  `docs/03-api-backend-published.md`.

Метки документа:

- **Swagger** — поле или статус опубликованы в спецификации;
- **документация backend** — опубликованное дополнение (соглашения, семантика
  ошибок, специфика по тегам): источник `docs/03-api-backend-published.md`;
- **подтверждено backend** — передано владельцем backend вне Swagger;
- **observed** — фактический безопасный запрос к текущему API;
- **assumed** — временное клиентское предположение;
- **запрошено у backend** — блокер до включения реальной интеграции.

Default клиента совпадает с задеплоенным API:
`http://fitness.nought.ru/api/v1`. Корень `/api/v1/` не является отдельным
endpoint и отвечает `404`; это не признак недоступности API. Отдельные
staging/prod URL и HTTPS — **запрошено у backend/DevOps**.

Объём контракта: теги `auth`, `users`, `exercises` и справочники
`muscle-groups`/`equipments` — всё, что перечислено в Swagger и в опубликованном
дополнении. Клиентские сценарии 1–5 (§6) описаны для auth/users; exercises
попадут в них по мере реализации (issue фазы B).

## 2. Общие правила

- `Content-Type: application/json` — **Swagger**.
- Публичные endpoints: `check-email`, `signup`, `login`, `refresh`,
  `password-recovery`, `password-recovery/verify`, `password-recovery/reset` и
  справочники `muscle-groups`/`equipments` — **документация backend**.
  `GET /exercises` — с опциональной авторизацией; `GET /users/{id}`,
  `GET /users/me`, `PATCH /users/me/profile` и весь CRUD `/exercises/{id}` —
  приватные (**документация backend**; в Swagger `security` по-прежнему нет).
- `Authorization: Bearer <access_token>` — **документация backend**: формат
  заголовка закрыт, клиентское «assumed» в `AsAuth_Session` переведено в
  подтверждённое. Swagger не содержит `securityDefinitions` и не объявляет
  security ни глобально, ни для `/users/{id}` — **observed** 08.10.2026.
- JWT — три части `header.payload.signature`; сервис использует `exp` и приватный
  `userID`. Клиент не разбирает payload и хранит токены как непрозрачные строки —
  **документация backend**.
- Сроки жизни: `access_token` — 30 минут, `refresh_token` — 10 дней
  (**документация backend**). Проактивно обновлять токен по `exp` не нужно:
  истечение обрабатывается отказом запроса и refresh (сценарий 4).
- `refresh_token` одноразовый: после обмена он инвалидируется, повторное
  использование → `401 invalid_token` — **документация backend**; совпадает с
  клиентской моделью ротации (§6, сценарии 4–5).
- **Опциональная авторизация:** без заголовка запрос идёт анонимно, но переданный
  невалидный или истёкший токен даёт `401`, а не «молча анонимный» режим —
  **документация backend**. Заведомо истёкший токен клиент не отправляет.
- Ошибка: `{ "key": string, "message": string }` — **Swagger** (`ErrorResponse`).
  `500` всегда приходит как `key=undefined_error` с текстом `internal server
  error` без деталей (**документация backend**), поэтому диагностика серверных
  сбоев у клиента ограничена статусом и request id из лога.
- `400` — общая валидация: невалидное тело/path/query либо бизнес-валидация.
  Собственный `key` есть только у части случаев (`email_already_used`,
  `incorrect_password`, `recovery_code_*`, `reset_token_*`,
  `user_id_required_for_filter`), остальные приходят как `undefined_error` с
  деталями только в `message` — **документация backend**. Клиент показывает
  локализованный текст по `key`, а `message` хранит для диагностики.
- `omitempty`: пустые поля в ответ **не включаются** (не `null` и не `0`), в том
  числе `profile` целиком и его поля — **документация backend**. Клиент проверяет
  наличие поля, а не значение-заглушку (§3.6, §4).
- Параметры: path (обязательны), query (обычно необязательны, значение по
  умолчанию — нулевое для типа), body (часть полей `required` в Swagger);
  параметры-списки передаются повторением ключа
  (`?muscle_group=chest&muscle_group=triceps`) — **документация backend**.
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

Ошибок с собственным `key` у эндпоинта нет: только общая валидация (`400`, если
email не строка/не email) — **документация backend**. Какие случаи дают `404`
(статус объявлен в Swagger) публикация не поясняет — вопрос остаётся открытым
(§8).

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

Ограничение password `8..64` есть в Swagger — **Swagger**. Специфика валидации —
**документация backend**: занятый email даёт `400` `email_already_used`, а
невалидные поля (`password` короче 8 или длиннее 64 символов, недопустимые
символы, пустые `name`/`email`/`password`) — `400` с `key=undefined_error` и
деталями только в `message`. Перечень недопустимых символов публикация не
приводит — **запрошено у backend** (§8).

Клиентскому реестру нужен ключ `email_already_used`: сейчас и `ApiErrorText`, и
мок используют `email_taken`, из-за чего занятый email показывался бы общим
текстом (§5).

Observed (live-check 23.09.2026, клиент issue 8): `signup` создаёт пользователя
и возвращает `user` + `tokens`; повторный `login` созданной учёткой возвращает
ту же пару сущностей; `GET /users/{id}` с `Bearer` работает (сценарий 3).

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

Observed для отсутствующего email: `500` с key `undefined_error` — поведение
старой сборки. По опубликованной документации текущее поведение такое:
неверный пароль → `400 incorrect_password`, отсутствующий email → `404` с
`key=undefined_error` (**документация backend**). Отдельного ключа «email не
найден» у сервиса нет: клиент различает случай по статусу `404`, а не по `key`
(§5). Тестовая учётная запись больше не блокирует сценарий: оба исхода
подтверждены документацией.

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

Ответы и семантика — **документация backend**: пара токенов ротируется,
переданный `refresh_token` инвалидируется (одноразовый), повторное использование
даёт `401 invalid_token`, истёкший (старше 10 дней) — `401 token_expired`.
Прежнее «запрошено у backend» (ротация, одноразовость, expiry, ключи) закрыто;
клиентская модель — одна очередь single-flight, атомарная замена пары (§6,
сценарий 4) — совпадает с этим и не меняется.

### 3.5. Восстановление пароля (три шага)

Три последовательных шага, каждый зависит от предыдущего — **документация
backend**. В клиенте сценарий пока не реализован (issue 33 зарезервирован):
контракт фиксируется здесь, чтобы форма восстановления и тексты ошибок не
изобретались заново.

1. `POST /auth/password-recovery` — тело `{ email }`. Сервис находит
   пользователя, генерирует 6-значный числовой код, сохраняет его хэш (прежние
   неиспользованные коды пользователя становятся недействительными) и отправляет
   письмо асинхронно. Код живёт **10 минут**, ответ — `200` без тела.
   Ошибка: `404` (`key: undefined_error`) — пользователь не найден.
2. `POST /auth/password-recovery/verify` — тело `{ email, code }`, `code` строго
   6 цифр (**Swagger**: оба поля required). При совпадении код помечается
   использованным и выдаётся одноразовый `reset_token`
   (`VerifyRecoveryCodeResponse.reset_token` — **Swagger**), срок жизни
   **15 минут**. Ошибки: `recovery_code_invalid` (`400`) — нет неиспользованного
   кода либо введённое значение не совпало; `recovery_code_expired` (`400`) —
   код старше 10 минут.
3. `POST /auth/password-recovery/reset` — тело `{ new_password, reset_token }`.
   При успехе пароль меняется, `reset_token` инвалидируется, а ответ сразу
   содержит пару токенов и `user` (**Swagger**), поэтому повторный вход не нужен.
   Ошибки: `reset_token_invalid` (`400`) — токен не найден, использован или
   пользователь не существует; `reset_token_expired` (`400`) — старше 15 минут.

Swagger statuses у всех трёх: `200`, `400`, `404`, `500`; `401` в спецификации
нет — **Swagger**.

Клиенту: сроки кода и токена (10 и 15 минут) — единственный источник для таймера
и текста «код истёк»; своего `exp` у непрозрачного `reset_token` нет.

### 3.6. `GET /users/{id}`

Приватный — **подтверждено backend**; `401` с `invalid_token`/`token_expired` при
отсутствующем, невалидном или истёкшем токене — **документация backend**.

Parameters:

- `id`: path, string UUID, required — **Swagger**;
- `withProfile`: query, boolean, optional, по умолчанию `false` — **Swagger**.

Без профиля или при `withProfile=false` (**Swagger**, схема `User`):

```json
{
  "id": "2e8cbeec-3528-45bc-908b-cdf76944d9c3",
  "name": "somename",
  "email": "test@gmail.com",
  "created_at": "0001-01-01T00:00:00Z"
}
```

С профилем (**Swagger**, схема `UserWithProfile`). Поля профиля включаются
**только заполненные** — **документация backend**, семантика `omitempty`:

```json
{
  "id": "2e8cbeec-3528-45bc-908b-cdf76944d9c3",
  "name": "somename",
  "email": "test@gmail.com",
  "created_at": "0001-01-01T00:00:00Z",
  "profile": {
    "gender": "male",
    "height": 180
  }
}
```

Прежний observed-пример с нулями (`"age": 0`, `"height": 0`) снят: нули в ответе
были поведением сборки до `omitempty`, и клиент не должен ждать их как признак
«не заполнено».

Если `withProfile=true`, но поле `profile` отсутствует, профиль ещё не заполнен
— **подтверждено backend**; это не ошибка.

`gender`: `male | female` — **Swagger**. Диапазоны и единицы — **документация
backend**: `age` 1–150, `height` 1–300 см, `weight` 1–500 кг. Единицы
подтверждены, прежний «assumed unit» в §4 закрыт.

Swagger statuses: `200`, `400`, `404`, `500`; `401`/`403` в спецификации
по-прежнему нет, но поведение описано документацией (**документация backend**):
`401` — при отсутствующем/невалидном/истёкшем токене, `403` — для чужой сущности.
`securityDefinitions` в Swagger отсутствуют (**observed** 08.10.2026): это вопрос
к спецификации, а не к поведению сервиса.

### 3.7. `GET /users/me`

То же, что §3.6, но без `id` в пути: пользователь определяется по `userID` из
access token — **документация backend**. Параметр `withProfile` (query, optional,
по умолчанию `false`) — **Swagger**; statuses в спецификации `200`, `400`, `404`,
`500`, `401` — по росписи документации.

Клиенту: `GET /users/me` снимает необходимость держать свой `id` рядом с
токенами, но текущая реализация (issue 9) ходит в `GET /users/{id}` с `id` из
сессии и кэширует его. Переход на `/users/me` — отдельное решение авторизации, а
не следствие публикации.

### 3.8. `PATCH /users/me/profile`

Частичное обновление профиля текущего пользователя — **документация backend**.
Все поля тела необязательны (`UpdateUserProfileRequestBody` в Swagger без
`required`) и обновляют только переданное, остальное остаётся как было.

| Поле | Тип | Допустимо |
|---|---|---|
| `age` | integer | 1–150 |
| `gender` | string | `male` \| `female` (**Swagger**) |
| `height` | integer | 1–300, см |
| `weight` | integer | 1–500, кг |

Ответ — объект профиля целиком после обновления, с той же семантикой `omitempty`,
что и в §3.6 (**документация backend**). Ошибка: `404` (`key: undefined_error`) —
профиль текущего пользователя не найден.

Клиенту: отправлять только изменённые поля — иначе непонятно, что было пустым; в
ответе профиль (`omitempty`) кладётся в `profile_cache` целиком (§7).

### 3.9. Упражнения (`/exercises`)

Три вида упражнений — **документация backend**: базовые (общий каталог, у них нет
`owner_id` в ответе), свои (владелец — пользователь из токена) и публичные
(`is_public = true`).

`GET /exercises` — **опциональная авторизация**: без токена доступны базовые и
публичные, с токеном — ещё и свои. Параметры (**Swagger**):

- `type` (required): `basic` | `public` | `own` (нужен токен) | `all`;
- `muscle_group` (optional, повторяемый) — хотя бы одна из указанных групп;
- `equipment` (optional, повторяемый) — весь требуемый инвентарь упражнения
  входит в переданный набор;
- `search` (optional) — подстрока названия, без учёта регистра;
- `limit` (required, максимум 1000), `offset` (required, максимум 100000).

Результат отсортирован по названию, дубликаты в списках игнорируются. Специфика
ошибок — **документация backend**: `400` при отсутствующем или невалидном
`type`/`limit`/`offset`; `user_id_required_for_filter` (`400`) — нужен токен, а
его нет; `401 invalid_token`/`token_expired` — токен передан, но негоден. В
Swagger у эндпоинта только `200`, `400`, `500`.

`GET /exercises/{id}` — требует авторизации: `403` и для публичного упражнения
другого автора (его можно получить только списком `type=public`), `404` — не
найдено, `400` — невалидный UUID (**документация backend**).

`POST /exercises` — тело `name`, `is_public` (по умолчанию `false`),
`description` (необязателен), `muscle_groups`, `equipment`. В
`CreateExerciseRequestBody` ни одно поле не помечено `required`, поэтому
обязательность `name`/`muscle_groups`/`equipment` задана только текстом
публикации (**документация backend**); владелец — пользователь из токена.

`PATCH /exercises/{id}` — частичное обновление своего упражнения; `id` в теле
обязан совпадать с `id` в пути, иначе `400`. `DELETE /exercises/{id}` — удаление
«мягкое»: упражнение исчезает из списков, но остаётся в БД ради будущих ссылок из
тренировок, ответ `200` без тела (**документация backend**).

Поля ответа (**Swagger**, схема `Exercise`): `id`, `name`, `description`
(`omitempty`), `owner_id` (`omitempty`, у базовых отсутствует), `muscle_groups`,
`equipment`, `created_at`.

### 3.10. Справочники `/muscle-groups`, `/equipments`

Публичные (авторизация не нужна), без параметров, возвращают массив строк —
например `["chest", "biceps", ...]` — **документация backend**. Значения
используются и в фильтрах `GET /exercises`, и в телах `POST`/`PATCH`
(**документация backend**). Swagger statuses: только `200`.

Клиенту: справочники — единственный источник допустимых значений для UI каталога
упражнений (чипы групп мышц и инвентаря); хардкод в QML расходился бы с бэкендом.

## 4. DTO mapping

| Backend JSON | Data DTO / QML |
|---|---|
| `created_at` | `createdAt` |
| `access_token` | `accessToken` |
| `refresh_token` | `refreshToken` |
| `withProfile` | `withProfile` |
| profile `height` | `heightCm` — **документация backend**: сантиметры |
| profile `weight` | `weightKg` — **документация backend**: килограммы |
| profile `age` | `age` — годы, 1–150 |
| profile `gender` | `gender` — `male`/`female` |
| `reset_token` | `resetToken` — шаг verify восстановления пароля (§3.5) |
| `is_public`, `muscle_groups`, `equipment`, `owner_id` | те же имена (exercises, §3.9); DTO появится вместе с экраном каталога |

Snake case преобразуется только в Data mapper. Репозитории, ViewModel и QML не
должны зависеть от JSON-написания.

Единицы профиля подтверждены публикацией — рост в сантиметрах, вес в килограммах,
прежние метки **assumed unit** сняты. Range-валидация профиля (1–150 / 1–300 /
1–500) — **документация backend**, и её достаточно, чтобы не выдумывать свои
границы.

`omitempty` (§2) меняет чтение профиля: «не заполнено» — это **отсутствие ключа**,
а не `0`. Клиентский кэш `profile_cache` хранит то же состояние (значение + флаг
заполненности), а не нули-заглушки.

## 5. Ключи ошибок

Swagger задаёт только тип string. Реестр после публикации 08.10.2026:

| Key | Статус |
|---|---|
| `undefined_error` | **документация backend**: общая валидация `400` и любой `500` |
| `email_already_used` | **документация backend** (signup, `400`) |
| `incorrect_password` | **документация backend** (login, `400`) |
| `invalid_token` | **документация backend** (`401`; им же отвечает повторно использованный `refresh_token`) |
| `token_expired` | **документация backend** (`401`, истёкший access/refresh) |
| `recovery_code_invalid`, `recovery_code_expired` | **документация backend** (шаг verify, `400`) |
| `reset_token_invalid`, `reset_token_expired` | **документация backend** (шаг reset, `400`) |
| `user_id_required_for_filter` | **документация backend** (`GET /exercises`, `400`) |
| `email_taken` | клиентский/мок-ключ: backend его **не отдаёт** (следствие — ниже) |
| `email_not_found` | клиентский ключ для «пользователь не найден»: backend отдаёт `404` с `undefined_error`, поэтому сопоставление — по статусу |
| `invalid_email`, `invalid_name`, `invalid_password`, `password_*` | client/mock validation; собственных backend-ключей нет — валидация приходит как `undefined_error` |
| `network_error`, `timeout`, `server_error`, `rate_limited` | client mapping |

Следствия для клиента (отдельные задачи, здесь только фиксируются):

- реестр `ApiErrorText` и мок должны знать `email_already_used`: сейчас занятый
  email вернул бы общий текст `undefined_error`, хотя локализованная строка для
  него уже есть под другим ключом;
- «пользователь не найден» при входе различается по статусу `404`, а не по `key`:
  отдельного ключа сервис не отдаёт;
- `500` по `key` не различить (всегда `undefined_error`), поэтому текст «ошибка
  сервера» строится по статусу, а `message` остаётся диагностикой (§2).

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
`token_expired`, он сохраняется. Публикация 08.10.2026 подтвердила, что `401`
различает `invalid_token` и `token_expired`, поэтому fallback по статусу нужен
только для случаев без собственного ключа (login с неизвестным email → `404`).

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

1. Клиент добавляет `Authorization: Bearer <access_token>` — формат подтверждён
   (**документация backend**, §2).
2. `GET /users/{id}?withProfile=true`.
3. User сохраняется в cache.
4. Отсутствующий `profile` трактуется как «не заполнен».
5. При отсутствии сети используется cache с offline-маркером (issue 9).

Реализовано в issue 9: `AUsers_Api` отдаёт DTO через auth-сессию;
`AUsers_Repository` пишет `user_cache`/`profile_cache`, при любой ошибке
возвращает сохранённый кэш с маркером `from_cache` и исходной ошибкой как
офлайн-пометкой. Единицы height/weight подтверждены — сантиметры и килограммы
(§4).

### Сценарий 4. Истёк access token

1. Приватный запрос получает `401` с `invalid_token` или `token_expired` —
   **документация backend**.
2. Запрос ставится в очередь single-flight refresh.
3. Выполняется один `POST /auth/refresh` с сохранённым refresh token.
4. Новая пара токенов атомарно заменяет старую.
5. Исходный запрос повторяется один раз.

Реализовано в issue 7 (`AsAuth_Session`). Зафиксированные клиентские решения:
один `POST /auth/refresh` на очередь параллельных auth failure; запросы,
пришедшие во время refresh, отправляются после него; второй auth failure после
refresh не ретраится повторно и не сбрасывает сохранённые токены; transport-сбой
refresh и невалидный JSON 200-ответ трактуются как transport-сбой (сессия
сохраняется); только HTTP-подтверждённый отказ refresh завершает сессию.

### Сценарий 5. Refresh неуспешен

1. Refresh возвращает `401 invalid_token`/`token_expired` либо невосстановимую
   ошибку (**документация backend**, §3.4).
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
внутренний `id` клиенту неизвестны. Запись, ротация и очистка реализованы в
issue 7 (`AsToken_Store`, `AsAuth_Session`).

`user_cache` после успешного login/signup пишет `AAuth_Repository` (issue 8) в
фиксированном порядке: кэш пользователя → пара токенов → активация сессии;
локальный logout очищает только токены и сохраняет кэш (сценарий 5).

## 8. Запрошено у backend

Публикацией 08.10.2026 закрыто (см. §2–§5): формат `Authorization`, сроки жизни и
одноразовость токенов, статусы и ключи для «неверный пароль», «неизвестный email»
и «невалидный/истёкший токен», семантика `omitempty`, диапазоны и единицы
профиля, контракт восстановления пароля, справочники групп мышц и инвентаря.

Остаётся открытым:

1. Staging/prod URL и переход на HTTPS. Dev/deployed URL уже получен
   (`http://fitness.nought.ru/api/v1`); публикация указывает тот же сервис по
   `http://94.228.166.134:8181/api/v1` — какой адрес канонический для
   конфигурации клиента, не уточнено.
2. Наличие server-side logout/revocation endpoint: в публикации не описан, а
   клиентский выход пока локальный (§7).
3. Полный набор допустимых символов и обязательных классов пароля: публикация
   говорит только «содержит недопустимые символы», поэтому клиентская валидация
   остаётся строже или равна серверной, но не выведена из неё.
4. Почему `POST /auth/check-email` объявляет `404` (по публикации у него только
   общая валидация `400`).
5. `securityDefinitions` и `401` в Swagger для приватных эндпоинтов: поведение
   описано документацией, спецификация отстаёт — вопрос к спецификации, а не к
   сервису.
6. Required-поля успешных ответов: Swagger их не помечает, а `omitempty` делает
   «обязательность» контекстной — понадобится при жёстком DTO-валидаторе.
