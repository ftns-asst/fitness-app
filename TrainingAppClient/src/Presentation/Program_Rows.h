#pragma once

#include "Domain/Program_Model.h"
#include "Domain/Progress_Stats.h"

#include <QHash>
#include <QStringList>
#include <QVariantMap>
#include <QVector>

using namespace Program_Model;

//----------------------------------------------------------------------------
// Маппинг доменных структур среза W в роли QML (срез W8, issues 24/26,
// §7 `02-program-model.md`).
//
// Роли отделены от `AList_Model`: здесь единственное место, где известны
// имена полей контракта, и потому единственное, которое меняется вместе с
// mock-контрактом. Модели списков не знают ни про программы, ни про SQL.
//
// Два правила, общие для всех списков:
// - enum отдаётся строкой через Program_Model::To_String: значения QML
//   сопоставляет словари переводов, числа машинных констант не годятся;
// - «не задано» — это null, а не 0 и не «-1»: UI обязан различать «нет
//   данных» и «ноль выполнено» (§5.5).
//----------------------------------------------------------------------------
namespace Program_Rows
{
// Наборы ролей: порядок объявления фиксирует значения ролей в модели.
QStringList Week_Slot_Roles();
//----------------------------------------------------------------------------
QStringList Program_Day_Roles();
//----------------------------------------------------------------------------
QStringList Day_Exercise_Roles();
//----------------------------------------------------------------------------
QStringList Goal_Target_Roles();
//----------------------------------------------------------------------------
QStringList Rule_Roles();
//----------------------------------------------------------------------------
QStringList Checkin_Roles();
//----------------------------------------------------------------------------
QStringList Measurement_Roles();

// Каталог типовых правил для экрана выбора (§5.7). Пустая категория — все.
// Строки несут всё, что нужно для создания правила, чтобы экран не держал
// свою таблицу соответствия «название пресета → параметры».
QVector<QVariantMap> Build_Rule_Catalog(const QString &in_category);
//----------------------------------------------------------------------------
QStringList Rule_Catalog_Roles();

// Каталог метрик для формы произвольного правила (§5.7): у метрики своя
// единица и периодичность, и форма не даёт выбрать чужую.
QVector<QVariantMap> Build_Metric_Catalog();
//----------------------------------------------------------------------------
QStringList Metric_Catalog_Roles();
// Единица, осмысленная для метрики. Валидируется по той же таблице, что
// отдаётся форме, — иначе правило разошлось бы с интерфейсом.
bool Is_Unit_Valid_For_Metric(int in_metric, int in_unit);

// Роли карточки каталога (срез S1): то, что показывает строка списка
// программ. Отличаются от ролей сводки (Program_Summary_Roles) тем,
// что сводка — это посчитанная метрика для сравнения программ, а каталог —
// это сами строки таблицы programs с группировкой по состоянию.
QStringList Catalog_Program_Roles();
//----------------------------------------------------------------------------
// Строки каталога.
QVector<QVariantMap> Build_Catalog_Programs(const QVector<SProgram> &in_programs);
//----------------------------------------------------------------------------
QStringList Program_Summary_Roles();
//----------------------------------------------------------------------------
QStringList Rule_Stat_Roles();
//----------------------------------------------------------------------------
QStringList Exercise_Strength_Roles();
//----------------------------------------------------------------------------
QStringList Goal_Progress_Roles();

// Строки списков.
// day_titles — заголовки дней программы по programDayId (роль dayTitle слота);
// today — YYYY-MM-DD или пусто, тогда isToday = false у всех.
QVector<QVariantMap> Build_Week_Slots(const QVector<SWeek_Slot> &in_slots, const QHash<QString, QString> &in_day_titles,
                                      const QString &in_today);

// exercise_count — число предписаний в дне (роль exerciseCount).
QVector<QVariantMap> Build_Program_Days(const QVector<SProgram_Day> &in_days, int in_exercise_count);

// titles — названия упражнений по exerciseId (роль title).
QVector<QVariantMap> Build_Day_Exercises(const QVector<SDay_Exercise> &in_exercises,
                                         const QHash<QString, QString> &in_titles);

// today_state — состояние отметки правила за сегодня по ruleId (роль
// todayState); отсутствие отметки даёт «unknown».
QVector<QVariantMap> Build_Rules(const QVector<SProgram_Rule> &in_rules, const QHash<QString, QString> &in_today_state);

// current_values — текущее значение цели по goalTargetId (роль currentValue).
QVector<QVariantMap> Build_Goal_Targets(const QVector<SGoal_Target> &in_targets,
                                        const QHash<QString, double> &in_current_values);

QVector<QVariantMap> Build_Checkins(const QVector<SRule_Checkin> &in_checkins);
//----------------------------------------------------------------------------
QVector<QVariantMap> Build_Measurements(const QVector<SBody_Measurement> &in_measurements);

// best_program_id — программа с лучшей метрикой (роль isBest, §5.3).
QVector<QVariantMap> Build_Program_Summaries(const QVector<SProgram_Summary> &in_summaries,
                                             const QString &in_best_program_id);

QVector<QVariantMap> Build_Rule_Stats(const QVector<SRule_Stat_Row> &in_rows, const QHash<QString, int> &in_streaks);

// Сила по упражнениям (§5.1 workingWeightByExercise[]): рабочий вес и e1RM.
QVector<QVariantMap> Build_Exercise_Strength(const QVector<SExercise_Strength> &in_strength);

// Прогресс целей из статистики (§5.1 goalProgress[]): «сегодняшнее» значение
// и процент, посчитанные доменом.
QVector<QVariantMap> Build_Goal_Progress(const QVector<SGoal_Progress> &in_progress);

// Недельные срезы графика тоннажа (§5.1 tonnageByWeek[]).
QStringList Week_Aggregate_Roles();
//----------------------------------------------------------------------------
QVector<QVariantMap> Build_Week_Aggregates(const QVector<SWeek_Aggregate> &in_weeks);
} // namespace Program_Rows
//----------------------------------------------------------------------------