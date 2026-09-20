# Контракт mock-данных и mock-API

Документ фиксирует публичный QML-контракт фазы S. Имена полей списков должны
стать именами ролей будущих `QAbstractListModel`; имена auth/users-полей должны
сохраняться на границе UI и DTO-маппинга. Серверные схемы пока не согласованы,
поэтому такие поля помечены **assumed** и могут быть преобразованы внутри
`AAuth_Api` / `AUsers_Api`, но не в delegates.

Источник предметных данных — `qml/Mock/MockCatalog.qml`. Auth-состояние и API
разделены на `MockSession`, `MockTokenStore`, `MockApi` и `ApiErrorText`.

Страницы не обращаются к этим mock-синглтонам напрямую. Между UI и mock-слоем
находятся стабильные presentation-контракты из `qml/Presentation/`:
`AuthViewModel`, `TodayViewModel`, `PlansViewModel`, `ExercisesViewModel` и
`ProfileViewModel`. В фазе V/A их реализации заменяются на `Avm_*`, а входные
свойства и delegates страниц остаются без изменений.
Состояние и async-координация формы находятся в `AuthFormController`, поэтому
`AuthPage` не владеет transport/repository-логикой.

## 1. Общие правила

- Идентификаторы — непустые строки, стабильные между запусками seed-данных.
- Время — ISO 8601 UTC (`2026-09-12T19:22:00Z`), дата — `YYYY-MM-DD`.
- Вес хранится числом в килограммах (`weight`, `weightKg`, `tonnageKg`),
  длительность — числом секунд или минут согласно суффиксу.
- `*Text` — готовый текст представления. Поле без `Text` содержит исходное
  значение для сортировки, фильтрации и расчётов.
- Enum — стабильная непереводимая ASCII-строка; перевод выполняет UI.
- У каждого элемента будущей list model есть роль `id`. Связи задаются ролями
  `planId`, `exerciseId`, `planDayId`, `sessionExerciseId`, `userId`.
- Вложенные массивы допустимы в detail DTO. Для больших списков C++-слой
  предоставит отдельные list model с теми же именами полей.

## 2. Списки и роли моделей

### 2.1. Сегодня и каталоги

| Mock property | Будущая модель / DTO | Роли / поля |
|---|---|---|
| `todayPlan` | `Avm_Today.currentPlan` | `id`, `title`, `badges`, `weekIndex`, `weeksTotal`, `exerciseCount`, `durationText`, `restDay` |
| `todayExercises` | модель упражнений дня | `id` (= `exerciseId`), `title`, `setsText`, `weightText` |
| `exercises` | каталог упражнений | `id`, `title`, `subtitle`, `muscleGroup`, `equipment`, `isCustom` |
| `plans` | каталог планов | `id`, `title`, `subtitle`, `goal`, `level`, `reviewText`, `ratingAvg`, `ratingCount`, `reviewCount`, `createdAt` |
| `todayRecord` | `Avm_Today.record` | `label`, `title`, `dateText` |

`goal`, `level`, `muscleGroup` и `equipment` в существующих списках пока
содержат локализованный текст ради текущего UI. В схеме v2 появятся стабильные
id/enum, а отображаемые поля сохранятся как `*Text`.

### 2.2. Прогресс и профиль

| Mock property | Будущая модель / DTO | Роли / поля |
|---|---|---|
| `profile` | `Avm_Profile` | `id`, `initials`, `name`, `email`, `age`, `gender`, `heightCm`, `weightKg`, `level`, `goal`, `workoutsPerWeek`, `details`, `syncState` |
| `kpi` | KPI list model | `id`, `label`, `valueText`, `deltaText`, `deltaKind` |
| `profileProgressPoints` | progress list model | `id`, `date`, `value` |
| `regularityWeeks` | heatmap list model | `id`, `weekStart`, `workoutCount` |
| `records` | personal records list model | `id`, `exerciseId`, `title`, `valueText`, `dateText` |
| `profileParams` | read-only parameter rows | `id`, `label`, `value` |
| `profileEditFields` | editable parameter model | `id`, `section`, `label`, `value`, `valueType`, `options`, `minimum`, `maximum`, `unit` |

Совместимые свойства `chartPoints` и `heatmapWeeks` остаются для текущих
визуальных компонентов; `profileProgressPoints` и `regularityWeeks` являются
нормализованным контрактом будущих моделей.

Enum профиля:

- `gender`: `male`, `female`; незаполненный профиль отсутствует целиком;
- `level`: `beginner`, `intermediate`, `advanced`;
- `goal`: `strength`, `muscle`, `fitness`;
- `syncState`: `synced`, `local`, `syncing`, `offline`;
- `section`: `server` или `local`;
- `valueType`: `integer`, `real`, `choice`.

### 2.3. Запись тренировки и таймер

| Mock property | Будущая модель / DTO | Роли / поля |
|---|---|---|
| `workoutSession` | `Avm_Workout_Session` | `id`, `planId`, `planDayId`, `title`, `state`, `startedAt`, `currentExerciseIndex`, `exerciseCount`, `offline` |
| `workoutSessionExercises` | session exercise list model | `id`, `exerciseId`, `title`, `orderIndex`, `targetSets`, `targetRepsText`, `suggestedWeight`, `completedSets` |
| `workoutSets` | set list model | `id`, `sessionExerciseId`, `setNumber`, `weight`, `reps`, `rpe`, `note`, `done` |
| `restTimer` | `Avm_Rest_Timer` | `durationSeconds`, `remainingSeconds`, `state`, `autoStart`, `addSecondsStep`, `defaultSeconds` |

`workoutSession.state`: `draft`, `completed`, `cancelled`.
`restTimer.state`: `idle`, `running`, `paused`, `finished`.
`rpe` — число 5–10 с шагом 0,5; значение `0` означает, что RPE ещё не введён.
Незавершённый подход имеет `done: false` и может иметь `reps: 0`.

### 2.4. План и отзывы

| Mock property | Будущая модель / DTO | Роли / поля |
|---|---|---|
| `planDetails` | `Avm_Plan_Detail` | `id`, `title`, `authorName`, `description`, `goal`, `level`, `ratingAvg`, `ratingCount`, `reviewCount`, `isCurrent`, `infoTiles`, `days` |
| `planDetails.infoTiles` | plan info list model | `id`, `label`, `valueText` |
| `planDetails.days` | plan day list model | `id`, `title`, `subtitle`, `exerciseCount` |
| `planDayExercises` | day exercise list model | `id`, `planDayId`, `exerciseId`, `title`, `orderIndex`, `sets`, `repsText`, `restSeconds` |
| `planReviews` | review list model | `id`, `planId`, `authorName`, `rating`, `text`, `createdAt`, `isOwn` |

`rating` — целое 1–5; пустой `text` допустим. `ratingAvg` хранится числом,
`ratingCount` — число всех оценок, `reviewCount` — число текстовых отзывов.

### 2.5. Карточка упражнения

| Mock property | Будущая модель / DTO | Роли / поля |
|---|---|---|
| `exerciseDetails` | `Avm_Exercise_Detail` | `id`, `title`, `muscleGroup`, `equipment`, `description`, `tags`, `technique`, `recordTiles`, `progressPoints`, `isCustom` |
| `exerciseDetails.recordTiles` | record tile list model | `id`, `label`, `valueText` |
| `exerciseDetails.progressPoints` | exercise progress list model | `id`, `date`, `value` |

`tags` и `technique` — списки строк detail DTO. `progressPoints.value` — рабочий
вес в кг. Другой тип метрики должен сопровождаться отдельным `metric/units`.

### 2.6. Подбор плана

`planWizard.steps` — список шагов с полями `id`, `title`, `options`; option
имеет `id`, `label`. Ответы хранятся как объект `step.id → option.id`.
`defaultAnswers` содержит валидный начальный набор, `resultPlanId` связывает
результат с каталогом планов.

Стабильные step id: `experience`, `days`, `equipment`, `goal`.

### 2.7. История

| Mock property | Будущая модель / DTO | Роли / поля |
|---|---|---|
| `workoutHistory` | history list model | `id`, `planId`, `title`, `completedAt`, `dateText`, `durationMinutes`, `durationText`, `tonnageKg`, `tonnageText`, `exerciseCount` |
| `workoutDetails` | workout detail DTO | `id`, `planId`, `title`, `completedAt`, `durationMinutes`, `tonnageKg`, `note`, `exercises` |
| `workoutDetails.exercises` | workout exercise list model | `id`, `exerciseId`, `title`, `orderIndex`, `sets` |
| `workoutDetails.exercises[].sets` | completed set list model | `id`, `setNumber`, `weight`, `reps`, `rpe`, `note`, `done` |

Фильтры истории работают по исходным `completedAt` и `exerciseId`, а не по
локализованным `dateText` / `title`.

## 3. Mock auth/users API

### 3.1. Источник контракта

`http://94.228.166.134:8181/api/v1/swagger/doc.json` (`host`:
`fitness.nought.ru`, `basePath`: `/api/v1`). Схемы wire DTO и заявленные
HTTP-коды ниже берутся из Swagger. Требование `Authorization: Bearer
<access_token>` для приватных endpoint и rotation refresh-токенов подтверждено
бэкендом отдельно: Swagger пока не содержит `securityDefinitions`, security у
операций и path-параметр `id` для `/users/{id}` — это дефекты спецификации.

### 3.2. Асинхронный QML-интерфейс

Каждый метод возвращает числовой `requestId`. Завершение приходит сигналом:

```qml
MockApi.requestFinished(requestId, operation, ok, data, error)
```

`operation`: `checkEmail`, `signup`, `login`, `refresh`, `getUser`.
Сигнал `requestStarted(requestId, operation)` и свойства `busy` /
`pendingCount` нужны для progress UI. Запросы выполняются по очереди с
задержкой `responseDelayMs` (по умолчанию 180 мс).

| Метод | Запрос | Успешный `data` |
|---|---|---|
| `checkEmail(email)` | email строкой | `{ free }`; занятый email — `ok: true`, `free: false` |
| `signup(request)` | `{ email, name, password }` | `{ user, tokens }` |
| `login(email, password)` | две строки | `{ user, tokens }` |
| `refresh(refreshToken)` | refresh token строкой | `{ tokens }` |
| `getUser(userId, withProfile)` | id и bool | плоский `user`; поле `profile` добавляется только при запросе и наличии профиля |
| `logout()` | — | синхронно очищает локальную mock-сессию |

Методы совпадают с будущим QML-фасадом C++ API; сетевые пути:
`POST /auth/check-email`, `POST /auth/signup`, `POST /auth/login`,
`POST /auth/refresh`, `GET /users/{id}?withProfile=true`.

### 3.3. Wire DTO и QML DTO

| Wire JSON бэкенда | Нормализованное поле QML | Статус |
|---|---|---|
| `user.id` | `user.id` | Swagger; строка UUID |
| `user.name` | `user.name` | Swagger; имя не обязано быть уникальным |
| `user.email` | `user.email` | Swagger |
| `user.created_at` | `user.createdAt` | Swagger + DTO mapping |
| `tokens.access_token` | `tokens.accessToken` | Swagger + DTO mapping |
| `tokens.refresh_token` | `tokens.refreshToken` | Swagger + DTO mapping |
| `profile.age` | `profile.age` | Swagger; integer |
| `profile.gender` | `profile.gender` | Swagger; `male` / `female` |
| `profile.height` | `profile.heightCm` | Swagger + DTO mapping; integer, единица по продукту — см |
| `profile.weight` | `profile.weightKg` | Swagger + DTO mapping; integer, единица по продукту — кг |

`POST /auth/signup`, `POST /auth/login` возвращают `{ user, tokens }`.
`POST /auth/refresh` принимает wire body `{ refresh_token }` и возвращает
`{ tokens }`. Сервер не возвращает `token_type`, `expires_at` или `expires_in`;
`Bearer` — правило формирования заголовка, а не поле token DTO.

`GET /users/{id}` возвращает поля `User` на верхнем уровне. При
`withProfile=true` поле `profile` добавляется только если профиль заполнен;
если профиль отсутствует, само поле также отсутствует. Поля `level`, `goal`,
`workoutsPerWeek` локальные и не входят в `signup`/server profile.

Wire-запросы:

```json
POST /auth/check-email { "email": "user@example.com" }
POST /auth/signup      { "email": "user@example.com", "name": "Имя", "password": "Strong1!" }
POST /auth/login       { "email": "user@example.com", "password": "Strong1!" }
POST /auth/refresh     { "refresh_token": "..." }
```

Все auth endpoint публичные. `GET /users/{id}` приватный и требует:

```http
Authorization: Bearer <access_token>
```

Тестовая запись:

```text
email:    igor@example.com
password: Strong1!
userId:   2e8cbeec-3528-45bc-908b-cdf76944d9c3
```

### 3.4. Ошибки и failMode

При ошибке `ok === false`, `data === null`, а `error` имеет форму:

```js
{ key: "incorrect_password", message: "Password is incorrect", httpStatus: 401 }
```

UI показывает `ApiErrorText.textFor(error.key)`, а не серверный `message`.
Неизвестный ключ и `undefined_error` дают «Не удалось выполнить операцию» и
пишутся в лог.

Swagger фиксирует форму ошибки, но не enum значений `key`. Живой API подтвердил
`invalid_token` для отсутствующего/невалидного access token и невалидного
refresh token; невалидный email в `check-email` возвращает
`400/undefined_error`; `undefined_error` также используется для части internal
errors. Остальные ключи ниже пока являются клиентскими UX-mapping или assumed
до публикации серверного enum:

На момент проверки login с гарантированно отсутствующим email возвращал
`500/undefined_error`. Mock повторяет это как default. Сценарий будущего
`email_not_found` остаётся доступен через `failMode="login:email_not_found"`,
но не считается подтверждённым backend-контрактом.

- auth/validation: `email_taken`, `email_not_found`, `incorrect_password`,
  `invalid_email`, `invalid_name`, `invalid_password`, `password_too_short`,
  `password_too_long`, `password_missing_letter`, `password_missing_digit`,
  `password_missing_special`, `validation_error`;
- session/HTTP: `invalid_token`, `token_expired`, `not_found`,
  `network_error`, `timeout`, `rate_limited`, `server_error`,
  `undefined_error`;
- зарезервированы для issue 33: `recovery_code_invalid`,
  `recovery_code_expired`.

`MockApi.failMode`:

- `none` — штатная логика;
- `<key>` — ошибка для любой следующей операции;
- `<operation>:<key>` — ошибка только указанной операции, например
  `login:incorrect_password` или `refresh:invalid_token`;
- `failOnce: true` сбрасывает режим после первого совпавшего запроса.

### 3.5. Валидация mock API

- email нормализуется через `trim().toLowerCase()`;
- Swagger подтверждает только password length 8–64 для signup/login;
- клиент дополнительно требует печатные ASCII-символы, минимум одну латинскую
  букву, одну цифру и один специальный символ — это UX-валидация **assumed**;
- ограничения длины `name` и enum ключей validation errors не описаны Swagger.

## 4. Сессия и токены

`MockTokenStore` хранит только `accessToken` и `refreshToken` в памяти,
предоставляет `save(tokens)`, `clear()`, `snapshot()`, `hasTokens` и вычисляемый
`authorizationHeader`. Production refresh token должен храниться в защищённом
платформенном хранилище. SQLite в Android internal storage допустима только как
промежуточная реализация issue 7; Android Keystore остаётся целевым усилением.

Успешный refresh атомарно заменяет **оба** токена. Старый refresh становится
одноразовым; его повторное использование инвалидирует всю token family,
очищает локальные токены и переводит сессию в `sessionExpired`. Параллельные
401 в реальном клиенте должны использовать single-flight refresh и повторять
каждый исходный запрос не более одного раза.

`MockSession`:

- `state`: `anonymous`, `authenticated`, `sessionExpired`;
- `user`, `profile`, `userId`, `offline`, `authenticated`;
- `authenticate(...)`, `restore(...)`, `logout()`, `expire()`, `reset()`;
- сигналы `authenticatedChangedByApi`, `loggedOut`, `sessionExpired`.

Неудачный refresh с `invalid_token` / `token_expired` очищает токены и переводит
сессию в `sessionExpired`. Роутинг (issue 2): гость работает во вкладках без
сессии; вход открывается из «Профиля»; `sessionExpired` принудительно показывает
экран входа.

## 5. Правило миграции на C++

1. Delegate обращается к именованным ролям из этого документа, а не к
   внутреннему объекту репозитория.
2. C++ list model публикует те же lowerCamelCase role names.
3. Несовпадение backend JSON преобразуется в API/DTO-слое.
4. Форматированные `*Text` может вычислять ViewModel, исходные числовые поля
   остаются доступны для сортировки и расчётов.
5. Новая роль сначала добавляется сюда и в mock seed, затем используется UI.