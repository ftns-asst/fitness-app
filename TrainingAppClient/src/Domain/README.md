# Domain

Предметные сущности, абстракции репозиториев и сервисы расчётов. Слой не зависит
от QML, HTTP, SQLite и платформенного UI.

`Program_Model` (срез W2, `02-program-model.md` §3) — структуры и перечисления
модели программы: `SProgram`, `SGoal_Target`, `STerm` (производный
`Compute_End_Date`), `SWeek_Slot`, `SProgram_Day`, `SDay_Exercise`,
`SProgression_Rule`, `SProgram_Rule`, `SRule_Checkin`, `SWorkout_Session`,
`SSet_Log` и enum-типы. `To_String` / `From_String` дают стабильные ASCII-ключи
schema v2 и mock-контракта; `To_Progression_Json` / `Parse_Progression_Json` и
`To_Json_Array` / `Parse_Json_Array` сериализуют TEXT-поля (`progression_json`,
`focus`, `substitution_ids`, `applies_days`).

W5 выполнен: `AsProgram_Service` в `Program_Service` создаёт личную программу
с нуля или из переданного неизменяемого снимка шаблона, безопасно переписывает
идентификаторы и ссылки дней/предписаний/слотов, делегирует переходы состояния
репозиторию и рассчитывает рекомендацию веса по правилу прогрессии. Полная
запись программы с целями и расписанием выполняется репозиторием одной
транзакцией с одной начальной записью `program_revisions`; тесты —
`tst_Program_Service` (`program_service`).

W6 выполнен: `AsRule_Engine` в `Rule_Engine` — чистые функции над структурами
(без QML и SQL). `Evaluate_Value` / `Evaluate_Checkin` выносят вердикт правила
по целевому диапазону (`min`/`max`/`exact`), сохраняя явные `skipped`/`partial`;
`Build_Computed_Checkin` автозаполняет расчётные отметки из фактов
(`computedFromSets` — `maxRpe` и `progressionDiscipline`; `computedFromSessions`
— `restBetweenSessions`); `Compute_Rule_Stats` / `Compute_Overall_Stats` дают
`SRule_Stats` (число `done`/`failed`/`skipped`/`partial`/`unknown`, среднее
значение и `adherencePercent` с флагом «нет данных»); `Is_Rule_Active` проверяет
период действия правила. Тесты — `tst_Rule_Engine` (`rule_engine`).
W7 выполнен: `AsProgress_Calculator` (`Progress_Calculator` + `Progress_Stats`) —
чистые функции над фактами без SQL и QML (§5.4). `Compute_Program_Stats` собирает
статистику программы: окно недель по расписанию, план/факт и adherence
(в знаменателе только ожидавшие слоты), серии недель, тоннаж/подходы/повторы/
время, нагрузку (средний и максимальный RPE, жёсткие тренировки, нарушения
`maxRpe`), силу по упражнениям (рабочий вес и e1RM по Эпли), прогресс целей
(включая `direction = decrease`), дельты замеров тела и сводку правил через
`AsRule_Engine`. `Compute_Lifetime_Stats` считает «всё время» по всем программам
и соло-тренировкам: итоги, регулярность и серии недель, рекорды по e1RM, дельту
веса тела и сводки программ для сравнения. `Encode_Program_Stats` /
`Encode_Lifetime_Stats` дают JSON-нагрузку кэша (`program_stats` /
`lifetime_stats`), где метрика без данных кодируется как `null` (§5.5).
`Recompute_Week` пересчитывает одну неделю и заново собирает итоги из массива
недель — тест-инвариант «полный пересчёт = инкрементальный». Тесты —
`tst_Progress_Calculator` (`progress_calculator`).

## Presentation (срез W8)

`Presentation` — единственный слой, который знает о ролях QML; он не знает о
SQL, а репозитории скрыты за указателями, поэтому в тестах подставляются
фейки.

- `List_Model` (`AList_Model`) — базовый `QAbstractListModel` над строками
  `QVariantMap`: выдаёт роли и данные, инвалидирует модель при смене ролей и
  строк. Не знает ни про программы, ни про SQL.
- `Program_Rows` (`namespace Program_Rows`) — единственное место, где
  доменные структуры превращаются в роли §7 `02-program-model.md`: слоты
  недели, дни, предписания, цели, правила, отметки, замеры, сводки
  программ, статистика правил, недельные срезы. Два правила на весь
  контракт: enum отдаётся строкой через `To_String`, «не задано» — это
  `null`, а не `0` (§5.5).
- `Program_VM` (`Avm_Program`) — дерево активной программы для
  экранов 41–43: скалярные свойства программы (`title`, `goal`, `state`,
  `weeksTotal`, `revision`, `startDate`) и модели-списки (`week_slots`,
  `program_days`, `day_exercises`, `goal_targets`, `rules`). `Select_Day`
  выбирает день плана; несуществующий день очищает список, чтобы UI не
  показывал упражнения прошлого выбора.
- `Program_Qml` (`AProgram_Qml`) — только макросы экспорта
  (`QML_NAMED_ELEMENT(Program)`); сам ViewModel лежит в `TrainingAppCore`,
  который не должен зависеть от Qt Qml.

Тесты — `tst_List_Model` (`list_model`), `tst_Program_Rows`
(`program_rows`), `tst_Program_VM` (`Program_VM`).

## Срез W9 — факты тренировок и статистика

- `Workout_Repository` (`AWorkout_Repository`) — чтение фактов `workouts` и
  `sets`. Записи тренировок в срезе нет: её ведёт `AsWorkout_Logger` (фаза B,
  issue 27), поэтому класс асимметричен. `Load_Program_Facts` отдаёт сессии
  программы вместе с их подходами; соло-сессии (`program_id = NULL`) в них
  не входят (§4 инвариант 18), но читаются отдельным вызовом с пустым
  `program_id` — так они попадают в «всё время».
- `Program_Stats_VM` (`Avm_Program_Stats`) — KPI программы (§5.1) и запись
  кэша `program_stats`. Считает по фактам через `AsProgress_Calculator`, кэш
  использует как ускоритель отображения, а не как источник истины (§5.4).
  `Set_Today` задаёт «сегодня» явно, чтобы расчёт не зависел от системных
  часов и был воспроизводим в тестах. Экспорт в QML — `ProgramStats`.