# Тестовая инфраструктура

## Контуры

- `cpp/tst_App_Context.cpp` — конфигурация composition root;
- `cpp/tst_Test_Http_Server.cpp` — самотестирование fake HTTP server;
- `cpp/tst_Http_Client.cpp` — API error mapping, DTO, timeout/retry и redaction;
- `cpp/tst_Sql_Database.cpp` — schema v1, миграции и constraints;
- `cpp/tst_Token_Store.cpp` — auth_tokens: save/load/ротация/clear и cold start;
- `cpp/tst_Auth_Session.cpp` — single-flight refresh: гонка 401, один refresh,
  один повтор, `sessionExpired`, transport-сбой и redaction логов;
- `cpp/tst_Auth_Api.cpp` — auth DTO, валидация обязательных полей и ключи;
- `cpp/tst_Auth_Repository.cpp` — сохранение сессии, logout, cold start;
- `cpp/tst_Avm_Auth.cpp` — состояния auth ViewModel;
- `cpp/tst_Auth_Qml_Bridge.cpp` — QML-мост реального auth (контракт MockApi,
  camelCase DTO, cold start, локальный профиль);
- `cpp/tst_App_Logger.cpp` — файловый логгер: формат строки, ротация,
   восстановление handler'а;
- `cpp/tst_Users_Api.cpp` — users DTO, профиль, ключи, refresh-ретрай;
- `cpp/tst_Users_Repository.cpp` — кэш профиля, офлайн-фолбэк, outbox;
- `cpp/tst_Program_Model.cpp` — структуры модели программы, enum ↔ строка, JSON;
- `cpp/tst_Program_Repository.cpp` — дерево программы, ревизии и транзакции,
  удаление черновика (каскад FK, отказ для не-черновика);
- `cpp/tst_Rule_Repository.cpp` — набор правил и отметки (защита `computed`);
- `cpp/tst_Measurement_Repository.cpp` — замеры тела (upsert-слияние);
- `cpp/tst_Stats_Repository.cpp` — кэш статистики и инвалидация;
- `cpp/tst_Program_Service.cpp` — создание программы (снимок/с нуля),
  переходы состояния и рекомендация веса;
- `cpp/tst_Program_Edit_VM.cpp` — редактор программы: валидация до записи,
  семь дней, автосохранение черновика (`autoSaveDraft`/`resumeDraft` —
  каждое действие дня в БД, повторное открытие продолжает черновик без
  дублей), цели, правила, каталог пресетов и повторное «Создать»;
- `cpp/tst_Rule_Engine.cpp` — оценка правил-целей, расчётные отметки
  (`computedFromSets`/`computedFromSessions`) и adherence;
- `cpp/tst_Error_State.cpp` — общее хранилище ошибки: запись, чтение,
  сброс в начале вызова;
- `cpp/tst_List_Model.cpp` — базовый список Presentation: роли, данные,
  инвалидация, копия строк;
- `cpp/tst_Program_Rows.cpp` — маппинг домена в роли §7, `null` вместо
  «не задано», тексты диапазонов и подписи целей;
- `cpp/tst_Program_VM.cpp` — дерево программы из БД: скалярные свойства,
  списки, выбор дня, сигнал `changed`;
- `cpp/tst_Workout_Repository.cpp` — факты тренировок: фильтры по программе и
  датам, снимок предписания, исключение соло-сессий;
- `cpp/tst_Program_Rules_VM.cpp` — правила и ручные отметки: состояние за сегодня,
  окно отметок 7/30/90 дней, adherence, защита §3.7.
- `cpp/tst_Program_Stats_VM.cpp` — KPI программы по фактам, запись кэша
  `program_stats`, отказ на неизвестной программе;
- `cpp/tst_Macro_VM.cpp` — результат авторасчёта КБЖУ: сохранение в
  `profile_local` (миграция v6), подстановка при следующем открытии и замена
  чисел при пересчёте;
- `cpp/tst_Progress_Calculator.cpp` — статистика программы и «всё время»,
  JSON кэша, инвариант «полный пересчёт = инкрементальный», отказ на
  некорректных входных данных;
- `qml/tst_mock_contract.qml` — mock API и data roles;
- `qml/tst_auth_page.qml` — auth state/UI interaction;
- `qml/tst_qml_object_smoke.qml` — создание ключевых production QML-типов;
- `qml/tst_app_shell_tabs.qml` — навигация каркаса: панель переключает
  вкладки с pushed-экрана, вкладка восстанавливает последний открытый экран,
  повторный тап по своей вкладке возвращает к корню и снимает память;
- `qml/tst_nutrition_plan_card.qml` — карточка КБЖУ на главной: числа расчёта,
  дата расчёта и кнопка «Рассчитать»/«Пересчитать»;
- `qml/tst_drag_to_scroll.qml` — прокрутка жестом и колесом по «пустой» области
  вокруг контента: попадание жеста, границы прокрутки и инерция;
- `qml/tst_frame_rate_meter.qml` — измерение частоты кадров (`--fps`);
- `qml/tst_weight_ruler_snap.qml` — привязка ползунка к шагу инвентаря и жест в
  окружении `Flickable`: перетаскивание не отбирает страница, черта растёт под
  пальцем (`gripActive`), колесо повторов ведёт номер под пальцем;
- `qml/tst_program_edit_term.qml` — срок программы ползунком 1…52 недели:
  длительности, недостижимые кнопками 4/8/12/16, ярлыки частых сроков,
  подсветка выбора и производная дата окончания под шкалой;
- `qml/tst_program_rules_picker.qml` — вид «Другое» в форме своего правила:
  текст уходит в `addAdvice`, форма не закрывается на отказе и сбрасывается к
  измеримому виду при повторном открытии;
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