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

Default соответствует Swagger `host=fitness.nought.ru`,
`basePath=/api/v1`. Swagger не объявляет scheme; на 20.09.2026 HTTP доступен,
а HTTPS с текущей тестовой машины не устанавливает соединение. Production URL
и TLS остаются **запрошено у backend/DevOps**.

## 5. Владение и жизненный цикл

- `AsApp_Context` живёт на стеке `main()` дольше `QQmlApplicationEngine`.
- Будущие сервисы создаются с parent `AsApp_Context`.
- `QNetworkAccessManager` будет один на HTTP-клиент, issue 5.
- Соединение SQLite и миграции появляются в issue 6.
- Token store и single-flight refresh появляются в issue 7.
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
| `TrainingAppCore` | `AsApp_Context`, будущие Data/Domain/Presentation классы |
| `appTrainingAppClient` | executable + QML module |
| `tst_App_Context` | Qt Test конфигурации запуска |

`TrainingAppCore` связан с `Qt6::Core`, `Qt6::Network`, `Qt6::Sql`; тест связан
с `Qt6::Test`. UI executable дополнительно использует Quick/Controls/Layouts.

## 8. Следующие шаги

- issue 4: fake HTTP server и C++ API-test utilities;
- issue 5: `AsHttp_Client`, `AApi_Error`, timeout/retry;
- issue 6: `AsSql_Database` и schema v1;
- issue 7: `AsToken_Store` и single-flight refresh;
- issue 8: `AAuth_Api`, repository и `Avm_Auth`;
- issue 9: users/profile cache и offline state.