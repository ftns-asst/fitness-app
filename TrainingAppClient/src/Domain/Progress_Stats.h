#pragma once

#include "Domain/Program_Model.h"
#include "Domain/Rule_Engine.h"

#include <QString>
#include <QVector>

using namespace Program_Model;

//----------------------------------------------------------------------------
// Выходные структуры статистики (02-program-model.md §5). Это «витрина»
// производных метрик: первичными данными остаются факты (сессии, подходы,
// отметки, замеры), поэтому любую метрику можно пересчитать заново (§5.4).
//
// Пара «значение + Has_» отличает «нет данных» от нуля: для метрик без данных
// UI показывает состояние «нечего считать» (§5.5).
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// Недельный срез метрик: минимальная единица инкрементального пересчёта (§5.4).
// Все итоговые метрики сессий, объёма и нагрузки выводятся из массива недель,
// а не считаются отдельно, поэтому «полный пересчёт = сумма недель».
//----------------------------------------------------------------------------
struct SWeek_Aggregate
{
	int Week_Index = 0;
	int Planned_Sessions = 0;
	int Completed_Sessions = 0;
	double Adherence_Percent = 0;
	bool Has_Adherence_Percent = false;
	double Tonnage_Kg = 0;
	int Sets_Count = 0;
	int Reps_Count = 0;
	double Duration_Minutes = 0;
	double Rpe_Sum = 0;
	int Rpe_Count = 0;
	double Max_Rpe = 0;
	bool Has_Max_Rpe = false;
	int Moved_Sessions = 0;
	int Extra_Sessions = 0;
	int Hard_Sessions = 0;
	int Rpe_Violations = 0;
};

//----------------------------------------------------------------------------
// Силовой показатель упражнения за период (§5.1 `workingWeightByExercise[]`).
// Рабочий вес — максимум веса рабочих подходов; e1RM — максимум оценки по
// формуле Эпли (§5.2).
//----------------------------------------------------------------------------
struct SExercise_Strength
{
	QString Exercise_Id;
	double Working_Weight_Kg = 0;
	bool Has_Working_Weight_Kg = false;
	double E1rm_Kg = 0;
	bool Has_E1rm_Kg = false;
};

//----------------------------------------------------------------------------
// Прогресс измеримой цели (§5.1 `goalProgress[]`). Current_Value — «сегодняшнее»
// значение метрики цели: последний замер, лучший рабочий вес, частота и т. п.
//----------------------------------------------------------------------------
struct SGoal_Progress
{
	QString Goal_Target_Id;
	EGoal_Metric Metric = EGoal_Metric::BodyWeightKg;
	QString Exercise_Id;
	EMeasurement_Key Measurement_Key = EMeasurement_Key::Chest;
	bool Has_Measurement_Key = false;
	EGoal_Direction Direction = EGoal_Direction::Increase;
	double Start_Value = 0;
	bool Has_Start_Value = false;
	double Current_Value = 0;
	bool Has_Current_Value = false;
	double Target_Value = 0;
	double Progress_Percent = 0;
	bool Has_Progress_Percent = false;
	bool Is_Achieved = false;
	EGoal_Target_State State = EGoal_Target_State::Active;
};

//----------------------------------------------------------------------------
// Дельта обхвата «старт → последнее измерение» (§5.1 `measurementDeltas[]`).
//----------------------------------------------------------------------------
struct SMeasurement_Delta
{
	EMeasurement_Key Key = EMeasurement_Key::Chest;
	double Start_Cm = 0;
	bool Has_Start_Cm = false;
	double Latest_Cm = 0;
	bool Has_Latest_Cm = false;
	double Delta_Cm = 0;
	bool Has_Delta_Cm = false;
};

//----------------------------------------------------------------------------
// Сводка правила для блока «Правила» (§5.1 `ruleStats[]`): правило и его
// adherence, посчитанный движком правил (W6).
//----------------------------------------------------------------------------
struct SRule_Stat_Row
{
	QString Rule_Id;
	QString Code;
	QString Title;
	ERule_Metric Metric = ERule_Metric::Custom;
	SRule_Stats Stats;
};

//----------------------------------------------------------------------------
// Статистика одной программы (§5.1 `SProgram_Stats`).
//----------------------------------------------------------------------------
struct SProgram_Stats
{
	QString Program_Id;
	int Program_Revision = 0;

	// Выполнение.
	int Planned_Sessions = 0;
	int Completed_Sessions = 0;
	double Adherence_Percent = 0;
	bool Has_Adherence_Percent = false;
	int Missed_Sessions = 0;
	int Moved_Sessions = 0;
	int Extra_Sessions = 0;
	int Current_Streak_Weeks = 0;
	int Longest_Streak_Weeks = 0;

	// Объём.
	double Tonnage_Kg = 0;
	int Sets_Count = 0;
	int Reps_Count = 0;
	double Duration_Minutes = 0;

	// Нагрузка.
	double Average_Rpe = 0;
	bool Has_Average_Rpe = false;
	double Max_Rpe = 0;
	bool Has_Max_Rpe = false;
	int Hard_Sessions_Count = 0;
	int Rpe_Violations = 0;

	// Прогресс.
	QVector<SExercise_Strength> Exercise_Strength;
	QVector<SGoal_Progress> Goal_Progress;
	bool Goal_Achieved = false;

	// Тело.
	double Body_Weight_Start_Kg = 0;
	bool Has_Body_Weight_Start_Kg = false;
	double Body_Weight_Latest_Kg = 0;
	bool Has_Body_Weight_Latest_Kg = false;
	double Body_Weight_Delta_Kg = 0;
	bool Has_Body_Weight_Delta_Kg = false;
	QVector<SMeasurement_Delta> Measurement_Deltas;

	// Правила.
	double Rule_Adherence_Percent = 0;
	bool Has_Rule_Adherence_Percent = false;
	QVector<SRule_Stat_Row> Rule_Stats;
	double Average_Protein_Grams_Per_Day = 0;
	bool Has_Average_Protein_Grams_Per_Day = false;
	double Average_Protein_Per_Kg = 0;
	bool Has_Average_Protein_Per_Kg = false;
	double Average_Sleep_Hours = 0;
	bool Has_Average_Sleep_Hours = false;
	double Average_Water_Ml = 0;
	bool Has_Average_Water_Ml = false;
	double Average_Steps = 0;
	bool Has_Average_Steps = false;

	// Служебное.
	QString Data_Range_From;
	QString Data_Range_To;

	// Недельные срезы — источник итоговых метрик выше.
	QVector<SWeek_Aggregate> Weeks;
};

//----------------------------------------------------------------------------
// Входные факты программы: структура, цели, правила, расписание и записи
// фактов. Считающая часть не ходит в SQL — всё передаётся структурами (§5.4).
// Today — локальная дата устройства в формате `YYYY-MM-DD`.
//----------------------------------------------------------------------------
struct SProgress_Facts
{
	SProgram Program;
	QVector<SGoal_Target> Goal_Targets;
	QVector<SProgram_Rule> Rules;
	QVector<SWeek_Slot> Schedule;
	QVector<SWorkout_Session> Sessions;
	QVector<SSet_Log> Sets;
	QVector<SRule_Checkin> Checkins;
	QVector<SBody_Measurement> Measurements;
	QString Today;
};

//----------------------------------------------------------------------------
// Неделя heatmap регулярности (§5.3 `regularityHeatmap[]`; роль mock-контракта
// `workoutCount`, §2.2 `mock-data-contract.md`).
//----------------------------------------------------------------------------
struct SRegularity_Week
{
	QString Week_Start; // ISO-понедельник недели, YYYY-MM-DD
	int Workout_Count = 0;
};

//----------------------------------------------------------------------------
// Личный рекорд по упражнению (§5.3 `records[]`) — лучший подход по e1RM.
//----------------------------------------------------------------------------
struct SExercise_Record
{
	QString Exercise_Id;
	double Weight_Kg = 0;
	int Reps = 0;
	double E1rm_Kg = 0;
	QString Date; // YYYY-MM-DD
	QString Session_Id;
	QString Program_Id;
};

//----------------------------------------------------------------------------
// Сводка программы для экрана сравнения (§5.3 `programSummaries[]`).
//----------------------------------------------------------------------------
struct SProgram_Summary
{
	QString Program_Id;
	QString Title;
	EProgram_Goal Goal = EProgram_Goal::Strength;
	EProgram_State State = EProgram_State::Draft;
	int Weeks_Total = 0;
	int Sessions = 0;
	double Tonnage_Kg = 0;
	double Adherence_Percent = 0;
	bool Has_Adherence_Percent = false;
	double Body_Weight_Delta_Kg = 0;
	bool Has_Body_Weight_Delta_Kg = false;
	double Key_Lift_Delta_Kg = 0;
	bool Has_Key_Lift_Delta_Kg = false;
};

//----------------------------------------------------------------------------
// Входные факты «всё время»: программы пользователя и все его сессии (включая
// соло-тренировки вне программы, §4 инвариант 18).
//----------------------------------------------------------------------------
struct SLifetime_Facts
{
	QString User_Id;
	QVector<SProgram> Programs;
	QVector<SWorkout_Session> Sessions;
	QVector<SSet_Log> Sets;
	QVector<SBody_Measurement> Measurements;
	QString Today;
};

//----------------------------------------------------------------------------
// Общая статистика пользователя (§5.3 `SLifetime_Stats`).
//----------------------------------------------------------------------------
struct SLifetime_Stats
{
	QString User_Id;

	// Итоги.
	int Total_Programs = 0;
	int Completed_Programs = 0;
	int Cancelled_Programs = 0;
	int Total_Sessions = 0;
	int Total_Sets = 0;
	int Total_Reps = 0;
	double Lifetime_Tonnage_Kg = 0;
	double Lifetime_Duration_Minutes = 0;
	QString First_Session_Date;
	QString Last_Session_Date;
	int Active_Days_Count = 0;

	// Регулярность.
	int Weeks_With_Sessions = 0;
	int Longest_Streak_Weeks = 0;
	int Current_Streak_Weeks = 0;
	double Average_Sessions_Per_Week = 0;
	bool Has_Average_Sessions_Per_Week = false;
	QVector<SRegularity_Week> Regularity;

	// Сила.
	QVector<SExercise_Record> Records;

	// Тело.
	double Body_Weight_Total_Delta_Kg = 0;
	bool Has_Body_Weight_Total_Delta_Kg = false;

	// Программы.
	QVector<SProgram_Summary> Summaries;
	QString Best_Program_Id;
};
