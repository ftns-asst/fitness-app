#pragma once

#include "Domain/Error_State.h"
#include "Domain/Progress_Stats.h"

#include <QDate>

//-------------------------------------------------------------------------------------------------------------
// Расчёт статистики одной программы (срез W7, 02-program-model.md §5.1).
//
// Чистые функции над структурами — без QML, SQL и сети (§5.4). Первичные
// данные — только факты; результат кэшируется репозиторием как JSON.
//
// Инкрементальный пересчёт (§5.4): `Recompute_Week` обновляет один недельный
// срез и заново собирает итоги из массива недель, поэтому результат совпадает
// с полным пересчётом (тест-инвариант «полный пересчёт = инкрементальный»).
//
// Отделён от «всё время» (AsLifetime_Calculator) и от сериализации
// (AStats_Encoder): у каждой части своя причина для изменения.
//-------------------------------------------------------------------------------------------------------------
class AsProgram_Stats_Calculator
{
public:
	AsProgram_Stats_Calculator();

	QString Get_Last_Error() const;

	// Полный пересчёт статистики программы по её фактам.
	bool Compute(const SProgress_Facts &in_facts, SProgram_Stats &out_stats);

	// Инкрементальный пересчёт одной недели: её срез пересчитывается из фактов,
	// итоги заново собираются из массива недель. Требует, чтобы массив недель
	// уже содержал срезы остальных недель (результат полного пересчёта).
	bool Recompute_Week(const SProgress_Facts &in_facts, int in_week_index, SProgram_Stats &in_out_stats);

private:
	bool Set_Last_Error(const QString &in_error);
	void Clear_Last_Error();

	// Общие шаги обоих пересчётов: проверка входа, окно, слоты и недели.
	bool Prepare(const SProgress_Facts &in_facts, QDate &out_start, QDate &out_end, int &out_slots_per_week,
	             int &out_weeks_in_window);

	// Недели: срез по номеру недели. Дата сессии должна лежать в окне, иначе
	// тренировка будущей недели попала бы в статистику.
	void Fill_Week(const SProgress_Facts &in_facts, const QDate &in_start, const QDate &in_end, int in_slots_per_week,
	               int in_week_index, SWeek_Aggregate &out_week) const;
	void Accumulate_Session(const SProgress_Facts &in_facts, const SProgram_Rule *in_rpe_rule,
	                        const SWorkout_Session &in_session, SWeek_Aggregate &in_out_week) const;
	void Accumulate_Set(const SSet_Log &in_set, SWeek_Aggregate &in_out_week) const;

	// Итоги — только из массива недель, поэтому оба пересчёта сходятся.
	void Derive_Totals(SProgram_Stats &in_out_stats) const;
	void Reset_Totals(SProgram_Stats &in_out_stats) const;
	void Sum_Week_Volume(const SWeek_Aggregate &in_week, SProgram_Stats &in_out_stats, double &in_out_rpe_sum,
	                     int &in_out_rpe_count) const;
	void Track_Streaks(const QVector<SWeek_Aggregate> &in_weeks, SProgram_Stats &in_out_stats) const;

	// Части, не привязанные к неделе.
	void Collect_Strength(const SProgress_Facts &in_facts, const QDate &in_start, const QDate &in_end,
	                      SProgram_Stats &in_out_stats) const;
	void Collect_Measurements(const SProgress_Facts &in_facts, const QDate &in_start, const QDate &in_end,
	                          SProgram_Stats &in_out_stats) const;
	void Collect_Measurement_Deltas(const QVector<SBody_Measurement> &in_measurements,
	                                SProgram_Stats &in_out_stats) const;
	void Collect_Goals(const SProgress_Facts &in_facts, SProgram_Stats &in_out_stats) const;
	void Apply_Goal_Progress(const SGoal_Target &in_target, double in_current, SGoal_Progress &in_out_progress) const;

	// Правила: расчётные отметки из фактов плюс агрегация движком правил.
	void Collect_Computed_Checkins(const SProgress_Facts &in_facts, QVector<SRule_Checkin> &in_out_checkins) const;
	void Collect_Rule_Stats(const QVector<SProgram_Rule> &in_rules, const QVector<SRule_Checkin> &in_checkins,
	                        SProgram_Stats &in_out_stats) const;
	void Apply_Average_Metric(const SProgram_Rule &in_rule, double in_average, SProgram_Stats &in_out_stats) const;
	void Finish_Progress(const SProgress_Facts &in_facts, const QDate &in_start, const QDate &in_end,
	                     SProgram_Stats &in_out_stats) const;

	// Текущее значение цели по её метрике. false — данных нет.
	bool Read_Goal_Current(const SGoal_Target &in_target, const SProgram_Stats &in_stats, double &out_value) const;

	AError_State Error_State;
};
//-------------------------------------------------------------------------------------------------------------