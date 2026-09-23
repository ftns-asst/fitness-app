# Тестовая инфраструктура

## Контуры

- `cpp/tst_App_Context.cpp` — конфигурация composition root;
- `cpp/tst_Test_Http_Server.cpp` — самотестирование fake HTTP server;
- `cpp/tst_Http_Client.cpp` — API error mapping, DTO, timeout/retry и redaction;
- `cpp/tst_Sql_Database.cpp` — schema v1, миграции и constraints;
- `cpp/tst_Token_Store.cpp` — auth_tokens: save/load/ротация/clear и cold start;
- `cpp/tst_Auth_Session.cpp` — single-flight refresh: гонка 401, один refresh,
  один повтор, `sessionExpired`, transport-сбой и redaction логов;
- `qml/tst_mock_contract.qml` — mock API и data roles;
- `qml/tst_auth_page.qml` — auth state/UI interaction;
- `qml/tst_qml_object_smoke.qml` — создание ключевых production QML-типов;
- `run_ui_smoke.cmake` — запуск настоящего приложения в `--diag` режиме.

Все тесты регистрируются в CTest. `scripts/run-gates.ps1` сначала собирает все
default targets, затем `qmllint`, после чего запускает весь CTest-набор.

## Fake HTTP server

`Support/Test_Http_Server.h` предоставляет test-only `ATest_Http_Server` на
`QTcpServer`. Production target с ним не линкуется.

Поддерживается:

- loopback и случайный свободный порт;
- очередь детерминированных HTTP/1.1 responses;
- status, content type, headers и body;
- задержка ответа для timeout-сценариев;
- disconnect без ответа для network error;
- чтение method, target/query, lowercase headers и body запроса;
- буферизация тела до полного `Content-Length`;
- ограничение запроса 1 MiB;
- fallback `500 undefined_error`, если response не был поставлен в очередь.

Не поддерживается намеренно: TLS, chunked encoding, streaming и HTTP
pipelining. Эти возможности не нужны для unit-тестов API core.

Пример:

```cpp
ATest_Http_Server server;
STest_Http_Response response;

server.Start();
response.Status_Code = 401;
response.Body = "{\"key\":\"token_expired\",\"message\":\"expired\"}";
server.Enqueue_Response(response);

QUrl endpoint = server.Base_URL().resolved(QUrl("/api/v1/users/id"));
```

Fake server не пишет запросы в лог. В частности, `Authorization` и token body
не должны попадать в CTest output. Тест может явно читать captured headers для
assertion, но не должен выводить их через `qDebug`/`QCOMPARE` при успехе.