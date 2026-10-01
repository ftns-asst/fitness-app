#pragma once

#include "Domain/Error_State.h"
#include "Domain/Program_Model.h"

#include <QString>
#include <QVector>

using namespace Program_Model;

//----------------------------------------------------------------------------
// Сводка adherence одного правила или их набора (02-program-model.md §5.1
// `ruleStats[]`, `ruleAdherencePercent`). `Evaluated_Count` — число отметок с
// вердиктом (done/failed/skipped/partial); оно стоит в знаменателе adherence,
// `Unknown_Count` в знаменатель не входит. `Has_Adherence_Percent = false`
// означает «данных для расчёта нет» (§5.5), а не ноль выполненных.
//----------------------------------------------------------------------------
struct SRule_Stats
{
	int Done_Count = 0;
	int Failed_Count = 0;
	int Skipped_Count = 0;
	int Partial_Count = 0;
	int Unknown_Count = 0;
	int Evaluated_Count = 0;
	double Average_Value = 0;
	bool Has_Average_Value = false;
	double Adherence_Percent = 0;
	bool Has_Adherence_Percent = false;
};

//----------------------------------------------------------------------------
// Доменный движок правил программы (срез W6, 02-program-model.md §3.6, §5.4).
//
// Чистые функции над структурами — без QML, SQL и сети:
// - `Evaluate_Value` / `Evaluate_Checkin` — вердикт правила по целевому
//   диапазону (`min` / `max` / `exact`, §3.6);
// - `Build_Computed_Checkin` — автозаполнение расчётной отметки из фактов:
//   `computedFromSets` (`maxRpe`, `progressionDiscipline`) и
//   `computedFromSessions` (`restBetweenSessions`);
// - `Compute_Rule_Stats` / `Compute_Overall_Stats` — adherence правил.
//
// Правила с источником `selfReport`, `mixed`, `computedFromProfile` и
// `computedFromLog` движок оценивает, но не заполняет из фактов тренировки:
// их значение приходит от пользователя, профиля или дневника питания.
//----------------------------------------------------------------------------
class AsRule_Engine
{
public:
	AsRule_Engine();

	QString Get_Last_Error() const;

	// Правило действует на дату (Active_From / Active_To; пустые — без границ).
	bool Is_Rule_Active(const SProgram_Rule &in_rule, const QString &in_date);

	// Вердикт по значению метрики: done, если значение попадает в цель правила.
	bool Evaluate_Value(const SProgram_Rule &in_rule, double in_value, ECheckin_State &out_state);

	// Вердикт по сохранённой отметке: явные skipped/partial сохраняются,
	// отметка со значением оценивается заново, отметка без значения
	// сохраняет собственное состояние (unknown — если оно не задано).
	bool Evaluate_Checkin(const SProgram_Rule &in_rule, const SRule_Checkin &in_checkin, ECheckin_State &out_state);

	// Расчётная отметка из фактов тренировки. Отсутствие данных — не ошибка:
	// возвращается отметка с состоянием unknown и без значения.
	bool Build_Computed_Checkin(const SProgram_Rule &in_rule, const SWorkout_Session &in_session, const QVector<SSet_Log> &in_sets,  const QVector<SWorkout_Session> &in_earlier_sessions, SRule_Checkin &out_checkin);

	// Сводка adherence одного правила по его отметкам.
	bool Compute_Rule_Stats(const SProgram_Rule &in_rule, const QVector<SRule_Checkin> &in_checkins, SRule_Stats &out_stats);

	// Сводка adherence по всем правилам (агрегат отметок всех правил).
	bool Compute_Overall_Stats(const QVector<SProgram_Rule> &in_rules, const QVector<SRule_Checkin> &in_checkins, SRule_Stats &out_stats);

private:
	// Хранилище ошибки (рефакторинг ошибок): состояние и логирование в
	// RULE_Engine_Log живут в AError_State, здесь — только делегирование.
	bool Set_Last_Error(const QString &in_error);
	void Clear_Last_Error();
	void Compute_Max_Rpe_Value(const QVector<SSet_Log> &in_sets, double &out_value, bool &out_has_value) const;
	void Compute_Progression_Value(const QVector<SSet_Log> &in_sets, double &out_value, bool &out_has_value) const;
	void Compute_Rest_Hours(const SWorkout_Session &in_session, const QVector<SWorkout_Session> &in_earlier_sessions,
	                        double &out_value, bool &out_has_value) const;
	void Accumulate_State(ECheckin_State in_state, SRule_Stats &out_stats) const;
	void Finalize_Stats(SRule_Stats &out_stats) const;

	AError_State Error_State;
};
//----------------------------------------------------------------------------
