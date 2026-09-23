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
7. Ошибки инициализации возвращаются через `bool` + `Last_Error()`, без
   исключений.

## 7. CMake targets

| Target | Назначение |
|---|---|
| `TrainingAppCore` | `AsApp_Context`, `AsHttp_Client`, `AApi_Error`, `AsSql_Database`, `AsToken_Store`, `AsAuth_Session`, будущие Domain/Presentation классы |
| `appTrainingAppClient` | executable + QML module |
| `tst_App_Context` | Qt Test конфигурации запуска и composition root |
| `tst_Sql_Database` | Qt Test SQLite schema v1 и миграций |
| `tst_Token_Store` | Qt Test хранилища auth_tokens |
| `tst_Auth_Session` | Qt Test single-flight refresh на fake HTTP server |
| `tst_Http_Client` | Qt Test API-ядра против fake HTTP server |

`TrainingAppCore` связан с `Qt6::Core`, `Qt6::Network`, `Qt6::Sql`; тест связан
с `Qt6::Test`. UI executable дополнительно использует Quick/Controls/Layouts.

## 8. Следующие шаги

- issue 4: выполнен — CTest/QML smoke/gates/CI и fake HTTP server;
- issue 5: выполнен — `AsHttp_Client`, `AApi_Error`, timeout/retry и безопасные логи;
- issue 6: выполнен — `AsSql_Database`, приватная SQLite и schema v1;
- issue 7: выполнен — `AsToken_Store`, `AsAuth_Session`, single-flight refresh;
- issue 8: `AAuth_Api`, repository и `Avm_Auth`;
- issue 9: users/profile cache и offline state.