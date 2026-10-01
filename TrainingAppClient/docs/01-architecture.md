# Архитектура клиента «Фитнес-помощник»

## 1. Назначение

Документ фиксирует целевую структуру клиента и минимальный C++-каркас issue 3.
Принцип зависимостей: UI и Presentation зависят от контрактов приложения и
домена; Data реализует эти контракты. HTTP, SQLite и QML не проникают в Domain.

```text
QML Pages / Components
          │
          ▼
Presentation (Avm_*, QAbstractListModel)
          │
          ▼
Domain (сущности, use-case, абстракции репозиториев)
          ▲
          │
Data (HTTP API, DTO mapper, SQLite, repository implementations)
```

`AsApp_Context` — composition root C++-части. Он создаётся в `main.cpp` до
загрузки QML и в следующих issues будет владеть сервисами в порядке
зависимостей. Владение — иерархия `QObject`, без smart pointers.

## 2. Каталоги

| Каталог | Ответственность | Разрешённые зависимости |
|---|---|---|
| `src/App` | composition root, конфигурация запуска | Qt Core, Data/Domain/Presentation |
| `src/Data` | HTTP, DTO, SQLite, token store, реализации репозиториев | Qt Network/Sql, Domain |
| `src/Domain` | сущности, интерфейсы репозиториев, бизнес-сервисы | Qt Core; без QML/HTTP/SQL |
| `src/Presentation` | ViewModel и list models для QML | Qt Core/Qml, Domain |
| `src/Platform` | платформенные адаптеры Android/desktop | платформенные Qt API |
| `qml/Presentation` | временные mock-backed адаптеры фазы S/V | QML mock layer |

В issue 3 `src/Data`, `src/Domain`, `src/Presentation` созданы как границы
слоёв; рабочие классы добавляются issues 5–9 и 22–26.

## 3. Инициализация

Порядок старта приложения:

1. `QGuiApplication` получает аргументы процесса.
2. Устанавливаются `organizationName`, `organizationDomain` и
   `applicationName`; они определяют хранилище `QSettings`.
3. `AsApp_Context.Initialize()` разрешает и валидирует API base URL.
4. При ошибке конфигурации приложение пишет одну диагностическую строку и
   завершается с кодом `2`; fallback к другому окружению не выполняется.
5. Контекст публикуется в QML как `AppContext` через `QQmlContext`.
6. Загружается QML-модуль `TrainingAppClient`.

`AppContext` предоставляет QML только read-only свойства:

```qml
AppContext.apiBaseUrl
AppContext.apiUrlSource // commandLine | settings | environment | default
AppContext.initialized
```

## 4. API base URL

Приоритет источников, от высшего к низшему:

1. CLI: `--api-url <url>`;
2. `QSettings`: ключ `network/apiUrl`;
3. environment: `TRAINING_APP_API_URL`;
4. default: `http://fitness.nought.ru/api/v1`.

Пример:

```powershell
.\appTrainingAppClient.exe --api-url http://127.0.0.1:8080/api/v1
```

Правила валидации:

- URL абсолютный, содержит host;
- схема только `http` или `https`;
- credentials, query и fragment запрещены;
- завершающий `/` удаляется;
- повторный `--api-url` и отсутствие значения считаются ошибкой;
- если выбранный источник невалиден, переход к источнику меньшего приоритета
  запрещён, чтобы клиент не подключился к неожиданному окружению.

Default соответствует подтверждённому backend URL
`http://fitness.nought.ru/api/v1`. Документация доступна по адресу
`http://fitness.nought.ru/api/v1/swagger`, Swagger JSON — по
`http://fitness.nought.ru/api/v1/swagger/doc.json`. Отдельные staging/prod URL
и перевод на HTTPS остаются **запрошено у backend/DevOps**.

## 5. Владение и жизненный цикл

- `main.cpp` владеет `AsApp_Logger`: handler ставится до инициализации
  контекста, каждая строка пишется в stderr и в приватный `logs/app.log`
  с ротацией по размеру (2 МБ + одна резервная копия).
- `AsApp_Context` живёт на стеке `main()` дольше `QQmlApplicationEngine`.
- Будущие сервисы создаются с parent `AsApp_Context`.
- `AsHttp_Client` владеет одним `QNetworkAccessManager`; composition root владеет
  одним `AsHttp_Client` (issue 5 выполнен).
- `AsApp_Context` владеет одним `AsSql_Database`; SQLite открывается в приватном
  `QStandardPaths::AppDataLocation`, schema v1 мигрирует транзакционно (issue 6).
- `AsApp_Context` владеет одним `AsToken_Store` (поверх `AsSql_Database`) и одним
  `AsAuth_Session` (поверх `AsHttp_Client` и `AsToken_Store`); session
  single-flight refresh реализован issue 7. При переинициализации сервисы
  удаляются в порядке, обратном созданию: session → token store → HTTP → SQLite.
- `AsApp_Context` владеет одним `AAuth_Repository` (поверх HTTP/SQLite/token
  store/session; сам владеет `AAuth_Api`) и одним `Avm_Auth` (issue 8).
  Порядок создания: SQLite → HTTP → token store → session → repository →
  ViewModel; порядок удаления — обратный.
- `AsApp_Context` владеет одним `AUsers_Repository` поверх auth-сессии и SQLite
  (issue 9); сам репозиторий владеет `AUsers_Api`.
- `AsApp_Context` владеет одним `Avm_Auth_Bridge` — QML-мост реального auth
  (issue 8, UI-подключение): контракт MockApi поверх `AAuth_Repository`/
  `AUsers_Repository`. main.cpp выставляет его как контекст-свойство
  `AuthBackend` только при `--real-auth`; реальные токены остаются в C++ SQLite,
  в MockTokenStore попадают заглушки.
- QML-страницы остаются на presentation-mock до снятия backend-блокеров;
  `Avm_Auth` подключается к `AuthViewModel` при активации реального входа без
  изменения контрактов страниц.
- ViewModel реального auth появляется в issue 8 и заменяет mock-backed
  `AuthViewModel` без изменения `AuthPage`.

## 6. Правила границ

1. QML-страницы не вызывают HTTP/SQL и не знают DTO транспорта.
2. Data не содержит UI-текстов; ошибки передаются стабильным `key`.
3. Domain не зависит от Data и Presentation.
4. Snake case backend DTO преобразуется на границе Data; QML использует
   lowerCamelCase.
5. Access/refresh tokens не пишутся в обычный лог.
6. Base URL может логироваться, но URL запросов с query и заголовки
   авторизации — нет.
7. Email, пароли, значения `free` check-email и backend-сообщения не
   логируются; ключи ошибок (`error key=...`) — могут.
8. Ошибки инициализации возвращаются через `bool` + `Last_Error()`, без
   исключений.

## 7. CMake targets

| Target | Назначение |
|---|---|
| `TrainingAppCore` | `AsApp_Context`, `AsHttp_Client`, `AApi_Error`, `AsSql_Database`, `AsToken_Store`, `AsAuth_Session`, `AAuth_Api`, `AAuth_Repository`, `Avm_Auth`, `Avm_Auth_Bridge`, `AUsers_Api`, `AUsers_Repository`, будущие Domain/Presentation классы |
| `appTrainingAppClient` | executable + QML module |
| `tst_App_Context` | Qt Test конфигурации запуска и composition root |
| `tst_Sql_Database` | Qt Test SQLite schema v1 и миграций |
| `tst_Token_Store` | Qt Test хранилища auth_tokens |
| `tst_Auth_Session` | Qt Test single-flight refresh на fake HTTP server |
| `tst_Auth_Api` | Qt Test auth-адаптера: DTO, валидация, ключи ошибок |
| `tst_Auth_Repository` | Qt Test сохранения сессии, logout и cold start |
| `tst_Avm_Auth` | Qt Test состояний auth ViewModel |
| `tst_Auth_Qml_Bridge` | Qt Test QML-моста реального auth |
| `tst_Users_Api` | Qt Test users endpoint: DTO, профиль, ключи ошибок, refresh-ретрай |
| `tst_Users_Repository` | Qt Test кэша профиля, офлайн-фолбэка, profile_local и sync_outbox |
| `tst_Http_Client` | Qt Test API-ядра против fake HTTP server |

`TrainingAppCore` связан с `Qt6::Core`, `Qt6::Network`, `Qt6::Sql`; тест связан
с `Qt6::Test`. UI executable дополнительно использует Quick/Controls/Layouts.

## 8. Следующие шаги

- issue 4: выполнен — CTest/QML smoke/gates/CI и fake HTTP server;
- issue 5: выполнен — `AsHttp_Client`, `AApi_Error`, timeout/retry и безопасные логи;
- issue 6: выполнен — `AsSql_Database`, приватная SQLite и schema v1;
- issue 7: выполнен — `AsToken_Store`, `AsAuth_Session`, single-flight refresh;
- issue 8: выполнен (C++-слой) — `AAuth_Api`, `AAuth_Repository`, `Avm_Auth`;
  подключение `Avm_Auth` к QML — при снятии backend-блокеров;
- issue 9: выполнен (C++-слой) — `AUsers_Api`, `AUsers_Repository`, кэш
  профиля, офлайн-фолбэк, `profile_local`/`sync_outbox`;
- issue 10: фаза S2 — остальные экраны на mock.

Далее (см. `04-roadmap.md` §3, срез **W**): сначала C++ — issue 22 (схема v2 с
таблицами программы, правил, отметок, статистики), 23 (`AProgram_Repository`,
`ARule_Repository`, `AMeasurement_Repository`, `AStats_Repository`), 24 (роли
моделей программы), 25 (`AsRule_Engine`, `AsProgram_Service`, расширенный
`AsProgress_Calculator`), 26 (`Avm_Program`, `Avm_Program_Schedule`,
`Avm_Program_Rules`, `Avm_Program_Stats`), затем UI — issues 41–44
(«Моя программа», редактор расписания и дня, правила и отметки, статистика).
Модель описана в `02-program-model.md`.