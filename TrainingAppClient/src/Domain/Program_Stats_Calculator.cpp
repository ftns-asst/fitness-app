#include "Domain/Program_Stats_Calculator.h"

#include "Domain/Progress_Support.h"
#include "Domain/Rule_Engine.h"

#include <QHash>
#include <QLoggingCategory>
#include <QSet>

#include <cmath>

Q_LOGGING_CATEGORY(PROGRAM_Stats_Calculator_Log, "trainingapp.program.progress")

//----------------------------------------------------------------------------
// Допуск значения цели: вес и часы измеряются с шагом 0,5, поэтому «достигнуто»
// и «удержано» определяются в пределах половины шага.
//----------------------------------------------------------------------------
static const double Goal_Value_Tolerance = 0.5;

//----------------------------------------------------------------------------
// Значение обхвата по ключу замера: пара «значение + Has_».
//----------------------------------------------------------------------------
static bool Measurement_Read_Key(const SBody_Measurement &in_measurement, EMeasurement_Key in_key, double &out_value)
{
	switch (in_key)
	{
	case EMeasurement_Key::Chest:
		out_value = in_measurement.Chest_Cm;

		return in_measurement.Has_Chest_Cm;

	case EMeasurement_Key::Waist:
		out_value = in_measurement.Waist_Cm;

		return in_measurement.Has_Waist_Cm;

	case EMeasurement_Key::Hip:
		out_value = in_measurement.Hip_Cm;

		return in_measurement.Has_Hip_Cm;

	case EMeasurement_Key::Arm:
		out_value = in_measurement.Arm_Cm;

		return in_measurement.Has_Arm_Cm;

	case EMeasurement_Key::Thigh:
		out_value = in_measurement.Thigh_Cm;

		return in_measurement.Has_Thigh_Cm;

	default:
		return false;
	}
}
//----------------------------------------------------------------------------
// Текущий рабочий вес упражнения по уже посчитанной силе (§5.1).
//----------------------------------------------------------------------------
static bool Lookup_Strength(const SProgram_Stats &in_stats, const QString &in_exercise_id, double &out_value)
{
	for (int i = 0; i < in_stats.Exercise_Strength.size(); ++i)
	{
		const SExercise_Strength &strength = in_stats.Exercise_Strength.at(i);

		if (strength.Exercise_Id != in_exercise_id)
			continue;
		if (strength.Has_Working_Weight_Kg == false)
			return false;

		out_value = strength.Working_Weight_Kg;

		return true;
	}

	return false;
}
//----------------------------------------------------------------------------
// Текущее значение обхвата: последнее измерение ключа (§5.1).
//----------------------------------------------------------------------------
static bool Lookup_Delta_Latest(const SProgram_Stats &in_stats, EMeasurement_Key in_key, bool in_has_key,
                                double &out_value)
{
	if (in_has_key == false)
		return false;

	for (int i = 0; i < in_stats.Measurement_Deltas.size(); ++i)
	{
		const SMeasurement_Delta &delta = in_stats.Measurement_Deltas.at(i);

		if (delta.Key != in_key)
			continue;

		if (delta.Has_Latest_Cm == false)
			return false;

		out_value = delta.Latest_Cm;

		return true;
	}

	return false;
}
//----------------------------------------------------------------------------
// Прогресс измеримой цели в процентах (§5.2). false — знаменатель нулевой
// (старт совпадает с целью), процент посчитать нельзя.
//----------------------------------------------------------------------------
static bool Goal_Progress_Percent(EGoal_Direction in_direction, double in_start, double in_target, double in_current,
                                  double &out_percent)
{
	double denominator = 0;
	double numerator = 0;

	switch (in_direction)
	{
	case EGoal_Direction::Increase:
		numerator = in_current - in_start;
		denominator = in_target - in_start;
		break;

	case EGoal_Direction::Decrease:
		numerator = in_start - in_current;
		denominator = in_start - in_target;
		break;

	default:
		out_percent = std::fabs(in_current - in_start) <= Goal_Value_Tolerance ? 100 : 0;

		return true;
	}

	if (std::fabs(denominator) < 1e-9)
		return false;

	out_percent = numerator / denominator * 100;

	return true;
}
//----------------------------------------------------------------------------
// Достижение цели с учётом направления и допуска.
//----------------------------------------------------------------------------
static bool Goal_Reached(EGoal_Direction in_direction, double in_start, double in_target, double in_current)
{
	switch (in_direction)
	{
	case EGoal_Direction::Increase:
		return in_current >= in_target;

	case EGoal_Direction::Decrease:
		return in_current <= in_target;

	default:
		return std::fabs(in_current - in_start) <= Goal_Value_Tolerance;
	}
}
//----------------------------------------------------------------------------
// Окно программы: от фактического старта (или планового) до min(сегодня,
// плановый конец срока). Открытый срок заканчивается «сегодня» (§3.3, §5.5).
//----------------------------------------------------------------------------
static bool Resolve_Window(const SProgress_Facts &in_facts, QDate &out_start, QDate &out_end)
{
	QString start_text = in_facts.Program.Actual_Start_Date;
	QDate today = QDate::fromString(in_facts.Today, Qt::ISODate);
	QDate term_end = QDate::fromString(in_facts.Program.Term.Compute_End_Date(), Qt::ISODate);
	QDate start;

	// Окно отсчитывается от фактического старта, если он был (§5.5).
	if (start_text.isEmpty())
		start_text = in_facts.Program.Term.Start_Date;

	start = QDate::fromString(start_text, Qt::ISODate);

	if (start.isValid() == false || today.isValid() == false)
		return false;

	out_start = start;
	out_end = (term_end.isValid() && term_end < today) ? term_end : today;

	return true;
}
//----------------------------------------------------------------------------
// Число слотов, ожидаемых программой за неделю: тренировка или опциональный
// день (§5.1 plannedSessions; дни отдыха в знаменатель не входят, §5.5).
//----------------------------------------------------------------------------
static int Slots_Per_Week(const QVector<SWeek_Slot> &in_schedule)
{
	int count = 0;

	for (int i = 0; i < in_schedule.size(); ++i)
	{
		if (in_schedule.at(i).Kind == ESlot_Kind::Workout || in_schedule.at(i).Kind == ESlot_Kind::Optional)
			++count;
	}

	return count;
}
//----------------------------------------------------------------------------
// Правило верхней границы RPE, если программа его задаёт. Нарушение считается
// через движок правил (W6), а не сравнением «на глаз».
//----------------------------------------------------------------------------
static const SProgram_Rule *Find_Max_Rpe_Rule(const QVector<SProgram_Rule> &in_rules)
{
	const SProgram_Rule *found = 0;

	for (int i = 0; i < in_rules.size(); ++i)
	{
		if (in_rules.at(i).Metric == ERule_Metric::MaxRpe &&
		    in_rules.at(i).Check_Source == ERule_Check_Source::ComputedFromSets)
			found = &in_rules.at(i);
	}

	return found;
}
//----------------------------------------------------------------------------
// Неделя выполнена полностью: выполнено не меньше, чем запланировано.
//----------------------------------------------------------------------------
static bool Week_Met(const SWeek_Aggregate &in_week)
{
	return in_week.Has_Adherence_Percent && in_week.Adherence_Percent >= 100;
}
//----------------------------------------------------------------------------
// Подходы тренировки. Собираются один раз на сессию: и объём, и нагрузка, и
// проверка правила читают один и тот же список.
//----------------------------------------------------------------------------
static void Collect_Session_Sets(const SProgress_Facts &in_facts, const QString &in_session_id,
                                 QVector<SSet_Log> &out_sets)
{
	for (int i = 0; i < in_facts.Sets.size(); ++i)
	{
		if (in_facts.Sets.at(i).Session_Id == in_session_id)
			out_sets.append(in_facts.Sets.at(i));
	}
}
//----------------------------------------------------------------------------
// Правило проверяет себя по фактам тренировок (§3.6).
//----------------------------------------------------------------------------
static bool Is_Computed_Rule(const SProgram_Rule &in_rule)
{
	return in_rule.Check_Source == ERule_Check_Source::ComputedFromSets ||
	       in_rule.Check_Source == ERule_Check_Source::ComputedFromSessions;
}
//----------------------------------------------------------------------------
// Отметка правила за дату уже есть: сохранённая запись пользователя имеет
// приоритет над расчётной и не перезаписывается (§3.7).
//----------------------------------------------------------------------------
static bool Has_Checkin(const QVector<SRule_Checkin> &in_checkins, const QString &in_rule_id, const QString &in_date)
{
	for (int i = 0; i < in_checkins.size(); ++i)
	{
		if (in_checkins.at(i).Rule_Id == in_rule_id && in_checkins.at(i).Date == in_date)
			return true;
	}

	return false;
}
//----------------------------------------------------------------------------
// AsProgram_Stats_Calculator
//----------------------------------------------------------------------------
AsProgram_Stats_Calculator::AsProgram_Stats_Calculator() : Error_State(&PROGRAM_Stats_Calculator_Log())
{
}
//----------------------------------------------------------------------------
QString AsProgram_Stats_Calculator::Get_Last_Error() const
{
	return Error_State.Get_Last_Error();
}
//----------------------------------------------------------------------------
bool AsProgram_Stats_Calculator::Set_Last_Error(const QString &in_error)
{
	return Error_State.Set_Last_Error(in_error);
}
//----------------------------------------------------------------------------
void AsProgram_Stats_Calculator::Clear_Last_Error()
{
	Error_State.Clear_Last_Error();
}
//----------------------------------------------------------------------------
bool AsProgram_Stats_Calculator::Prepare(const SProgress_Facts &in_facts, QDate &out_start, QDate &out_end,
                                         int &out_slots_per_week, int &out_weeks_in_window)
{
	if (in_facts.Program.ID.isEmpty())
		return Set_Last_Error("Статистика требует программу с id");

	if (Resolve_Window(in_facts, out_start, out_end) == false)
		return Set_Last_Error("Статистика требует корректные даты старта программы и «сегодня»");

	out_slots_per_week = Slots_Per_Week(in_facts.Schedule);
	out_weeks_in_window = Week_Index(out_start, out_end);

	if (out_weeks_in_window < 1)
		return Set_Last_Error("Окно программы не содержит ни одной полной недели");

	return true;
}
//----------------------------------------------------------------------------
bool AsProgram_Stats_Calculator::Compute(const SProgress_Facts &in_facts, SProgram_Stats &out_stats)
{
	QDate start;
	QDate end;
	int slots_per_week = 0;
	int weeks_in_window = 0;

	Clear_Last_Error();
	out_stats = SProgram_Stats();

	if (Prepare(in_facts, start, end, slots_per_week, weeks_in_window) == false)
		return false;

	out_stats.Program_Id = in_facts.Program.ID;
	out_stats.Program_Revision = in_facts.Program.Revision;

	for (int week = 1; week <= weeks_in_window; ++week)
	{
		SWeek_Aggregate aggregate;

		Fill_Week(in_facts, start, end, slots_per_week, week, aggregate);
		out_stats.Weeks.append(aggregate);
	}

	Derive_Totals(out_stats);
	Finish_Progress(in_facts, start, end, out_stats);

	return true;
}
//----------------------------------------------------------------------------
bool AsProgram_Stats_Calculator::Recompute_Week(const SProgress_Facts &in_facts, int in_week_index,
                                                SProgram_Stats &in_out_stats)
{
	QDate start;
	QDate end;
	int slots_per_week = 0;
	int weeks_in_window = 0;
	int position = -1;
	SWeek_Aggregate aggregate;

	Clear_Last_Error();

	if (Prepare(in_facts, start, end, slots_per_week, weeks_in_window) == false)
		return false;

	if (in_out_stats.Program_Id != in_facts.Program.ID)
		return Set_Last_Error("Инкрементальный пересчёт требует базовую статистику той же программы");

	if (in_week_index < 1 || in_week_index > weeks_in_window)
		return Set_Last_Error(QString("Неделя %1 вне окна программы").arg(in_week_index));

	for (int i = 0; i < in_out_stats.Weeks.size(); ++i)
	{
		if (in_out_stats.Weeks.at(i).Week_Index == in_week_index)
			position = i;
	}

	if (position < 0)
		return Set_Last_Error("Базовая статистика не содержит недельного среза для пересчёта");

	Fill_Week(in_facts, start, end, slots_per_week, in_week_index, aggregate);

	in_out_stats.Weeks.replace(position, aggregate);
	in_out_stats.Program_Revision = in_facts.Program.Revision;

	Derive_Totals(in_out_stats);
	Finish_Progress(in_facts, start, end, in_out_stats);

	return true;
}
// Недельный срез: план (слоты недели), факт (сессии недели), объём и нагрузка.
//----------------------------------------------------------------------------
void AsProgram_Stats_Calculator::Fill_Week(const SProgress_Facts &in_facts, const QDate &in_start, const QDate &in_end,
                                           int in_slots_per_week, int in_week_index, SWeek_Aggregate &out_week) const
{
	const SProgram_Rule *rpe_rule = Find_Max_Rpe_Rule(in_facts.Rules);

	out_week = SWeek_Aggregate();
	out_week.Week_Index = in_week_index;
	out_week.Planned_Sessions = in_slots_per_week;

	for (int i = 0; i < in_facts.Sessions.size(); ++i)
	{
		const SWorkout_Session &session = in_facts.Sessions.at(i);
		QDate date = QDate::fromString(Session_Date(session), Qt::ISODate);

		if (session.State != EWorkout_State::Completed)
			continue;

		if (Week_Index(in_start, date) != in_week_index)
			continue;

		// Номер недели проверяет принадлежность к срезу, а окно — что тренировка
		// уже состоялась: сессия будущей недели в статистику не попадает.
		if (In_Window(date, in_start, in_end) == false)
			continue;

		Accumulate_Session(in_facts, rpe_rule, session, out_week);
	}

	if (out_week.Planned_Sessions > 0)
	{
		out_week.Adherence_Percent = out_week.Completed_Sessions * 100.0 / out_week.Planned_Sessions;
		out_week.Has_Adherence_Percent = true;
	}
}
//----------------------------------------------------------------------------
void AsProgram_Stats_Calculator::Accumulate_Session(const SProgress_Facts &in_facts, const SProgram_Rule *in_rpe_rule,
                                                    const SWorkout_Session &in_session,
                                                    SWeek_Aggregate &in_out_week) const
{
	AsRule_Engine engine;
	QVector<SSet_Log> session_sets;
	double average = 0;
	SRule_Checkin checkin;

	Collect_Session_Sets(in_facts, in_session.ID, session_sets);

	for (int i = 0; i < session_sets.size(); ++i)
		Accumulate_Set(session_sets.at(i), in_out_week);

	++in_out_week.Completed_Sessions;

	if (in_session.Deviation == EDeviation::MovedDay)
		++in_out_week.Moved_Sessions;

	if (in_session.Deviation == EDeviation::ExtraDay)
		++in_out_week.Extra_Sessions;

	in_out_week.Duration_Minutes += in_session.Duration_Seconds / 60.0;

	if (Session_Average_Rpe(in_session, session_sets, average) && average >= 9)
		++in_out_week.Hard_Sessions;

	// Нарушение верхней границы RPE (правило `maxRpe`, §3.6).
	if (in_rpe_rule == 0)
		return;

	if (engine.Build_Computed_Checkin(*in_rpe_rule, in_session, session_sets, QVector<SWorkout_Session>(), checkin) &&
	    checkin.State == ECheckin_State::Failed)
		++in_out_week.Rpe_Violations;
}
//----------------------------------------------------------------------------
void AsProgram_Stats_Calculator::Accumulate_Set(const SSet_Log &in_set, SWeek_Aggregate &in_out_week) const
{
	if (Set_Counts_In_Volume(in_set))
	{
		in_out_week.Tonnage_Kg += Set_Tonnage(in_set);
		++in_out_week.Sets_Count;
		in_out_week.Reps_Count += in_set.Reps;
	}

	if (in_set.Has_Rpe == false || in_set.Is_Warmup)
		return;

	in_out_week.Rpe_Sum += in_set.Rpe;
	++in_out_week.Rpe_Count;

	if (in_out_week.Has_Max_Rpe && in_set.Rpe <= in_out_week.Max_Rpe)
		return;

	in_out_week.Max_Rpe = in_set.Rpe;
	in_out_week.Has_Max_Rpe = true;
}
//----------------------------------------------------------------------------
// Итоговые метрики выводятся только из массива недель: так инкрементальный
// пересчёт одной недели и полный пересчёт дают одинаковый результат (§5.4).
//----------------------------------------------------------------------------
void AsProgram_Stats_Calculator::Derive_Totals(SProgram_Stats &in_out_stats) const
{
	double rpe_sum = 0;
	int rpe_count = 0;

	Reset_Totals(in_out_stats);

	for (int i = 0; i < in_out_stats.Weeks.size(); ++i)
		Sum_Week_Volume(in_out_stats.Weeks.at(i), in_out_stats, rpe_sum, rpe_count);

	Track_Streaks(in_out_stats.Weeks, in_out_stats);

	if (rpe_count > 0)
	{
		in_out_stats.Average_Rpe = rpe_sum / rpe_count;
		in_out_stats.Has_Average_Rpe = true;
	}

	if (in_out_stats.Planned_Sessions == 0)
		return;

	in_out_stats.Adherence_Percent = in_out_stats.Completed_Sessions * 100.0 / in_out_stats.Planned_Sessions;
	in_out_stats.Has_Adherence_Percent = true;
}
//----------------------------------------------------------------------------
void AsProgram_Stats_Calculator::Reset_Totals(SProgram_Stats &in_out_stats) const
{
	in_out_stats.Planned_Sessions = 0;
	in_out_stats.Completed_Sessions = 0;
	in_out_stats.Missed_Sessions = 0;
	in_out_stats.Moved_Sessions = 0;
	in_out_stats.Extra_Sessions = 0;
	in_out_stats.Tonnage_Kg = 0;
	in_out_stats.Sets_Count = 0;
	in_out_stats.Reps_Count = 0;
	in_out_stats.Duration_Minutes = 0;
	in_out_stats.Average_Rpe = 0;
	in_out_stats.Has_Average_Rpe = false;
	in_out_stats.Max_Rpe = 0;
	in_out_stats.Has_Max_Rpe = false;
	in_out_stats.Hard_Sessions_Count = 0;
	in_out_stats.Rpe_Violations = 0;
	in_out_stats.Longest_Streak_Weeks = 0;
	in_out_stats.Current_Streak_Weeks = 0;
}
//----------------------------------------------------------------------------
void AsProgram_Stats_Calculator::Sum_Week_Volume(const SWeek_Aggregate &in_week, SProgram_Stats &in_out_stats,
                                                 double &in_out_rpe_sum, int &in_out_rpe_count) const
{
	in_out_stats.Planned_Sessions += in_week.Planned_Sessions;
	in_out_stats.Completed_Sessions += in_week.Completed_Sessions;
	in_out_stats.Missed_Sessions += qMax(0, in_week.Planned_Sessions - in_week.Completed_Sessions);
	in_out_stats.Moved_Sessions += in_week.Moved_Sessions;
	in_out_stats.Extra_Sessions += in_week.Extra_Sessions;
	in_out_stats.Tonnage_Kg += in_week.Tonnage_Kg;
	in_out_stats.Sets_Count += in_week.Sets_Count;
	in_out_stats.Reps_Count += in_week.Reps_Count;
	in_out_stats.Duration_Minutes += in_week.Duration_Minutes;
	in_out_stats.Hard_Sessions_Count += in_week.Hard_Sessions;
	in_out_stats.Rpe_Violations += in_week.Rpe_Violations;

	in_out_rpe_sum += in_week.Rpe_Sum;
	in_out_rpe_count += in_week.Rpe_Count;

	if (in_week.Has_Max_Rpe == false || (in_out_stats.Has_Max_Rpe && in_week.Max_Rpe <= in_out_stats.Max_Rpe))
		return;

	in_out_stats.Max_Rpe = in_week.Max_Rpe;
	in_out_stats.Has_Max_Rpe = true;
}
//----------------------------------------------------------------------------
// Серии недель подряд: неделя засчитана, если её adherence ≥ 100 %.
// Текущая серия — непрерывный хвост в конце окна, поэтому считается наоборот.
//----------------------------------------------------------------------------
void AsProgram_Stats_Calculator::Track_Streaks(const QVector<SWeek_Aggregate> &in_weeks,
                                               SProgram_Stats &in_out_stats) const
{
	int longest = 0;
	int current = 0;

	for (int i = 0; i < in_weeks.size(); ++i)
	{
		if (Week_Met(in_weeks.at(i)) == false)
		{
			current = 0;

			continue;
		}

		++current;

		if (current > longest)
			longest = current;
	}

	in_out_stats.Longest_Streak_Weeks = longest;
	current = 0;

	for (int i = in_weeks.size() - 1; i >= 0; --i)
	{
		if (Week_Met(in_weeks.at(i)) == false)
			break;

		++current;
	}

	in_out_stats.Current_Streak_Weeks = current;
}
//----------------------------------------------------------------------------
// Части статистики, не привязанные к неделе. Векторы пересобираются целиком:
// функции идемпотентны и одинаково работают при полном и инкрементальном
// пересчёте (§5.4).
//----------------------------------------------------------------------------
void AsProgram_Stats_Calculator::Finish_Progress(const SProgress_Facts &in_facts, const QDate &in_start,
                                                 const QDate &in_end, SProgram_Stats &in_out_stats) const
{
	QVector<SRule_Checkin> checkins = in_facts.Checkins;

	in_out_stats.Exercise_Strength.clear();
	in_out_stats.Measurement_Deltas.clear();
	in_out_stats.Goal_Progress.clear();
	in_out_stats.Goal_Achieved = false;
	in_out_stats.Rule_Stats.clear();

	Collect_Strength(in_facts, in_start, in_end, in_out_stats);
	Collect_Measurements(in_facts, in_start, in_end, in_out_stats);
	Collect_Goals(in_facts, in_out_stats);
	Collect_Computed_Checkins(in_facts, checkins);
	Collect_Rule_Stats(in_facts.Rules, checkins, in_out_stats);
}
//----------------------------------------------------------------------------
// Силовые показатели и границы данных: рабочий вес и e1RM по упражнениям за
// окно программы. Порядок упражнений — по первому появлению в фактах.
//----------------------------------------------------------------------------
void AsProgram_Stats_Calculator::Collect_Strength(const SProgress_Facts &in_facts, const QDate &in_start,
                                                  const QDate &in_end, SProgram_Stats &in_out_stats) const
{
	QHash<QString, SExercise_Strength> strengths;
	QStringList order;
	SExtremes_Value data_range;

	for (int i = 0; i < in_facts.Sessions.size(); ++i)
	{
		const SWorkout_Session &session = in_facts.Sessions.at(i);
		QDate date = QDate::fromString(Session_Date(session), Qt::ISODate);

		if (session.State != EWorkout_State::Completed || In_Window(date, in_start, in_end) == false)
			continue;

		data_range.Add(date.toString("yyyy-MM-dd"), 0);

		for (int j = 0; j < in_facts.Sets.size(); ++j)
		{
			const SSet_Log &set = in_facts.Sets.at(j);

			if (set.Session_Id != session.ID || Set_Counts_In_Volume(set) == false || set.Exercise_Id.isEmpty())
				continue;

			if (strengths.contains(set.Exercise_Id) == false)
			{
				SExercise_Strength empty;

				empty.Exercise_Id = set.Exercise_Id;
				strengths.insert(set.Exercise_Id, empty);
				order.append(set.Exercise_Id);
			}

			SExercise_Strength &strength = strengths[set.Exercise_Id];

			// Рабочий вес — максимум за период; e1RM — максимум оценки.
			if (strength.Has_Working_Weight_Kg == false || set.Weight_Kg > strength.Working_Weight_Kg)
			{
				strength.Working_Weight_Kg = set.Weight_Kg;
				strength.Has_Working_Weight_Kg = true;
			}

			if (strength.Has_E1rm_Kg == false || E1rm(set) > strength.E1rm_Kg)
			{
				strength.E1rm_Kg = E1rm(set);
				strength.Has_E1rm_Kg = true;
			}
		}
	}

	for (int i = 0; i < order.size(); ++i)
		in_out_stats.Exercise_Strength.append(strengths.value(order.at(i)));

	in_out_stats.Data_Range_From = data_range.First_Date;
	in_out_stats.Data_Range_To = data_range.Last_Date;
}
//----------------------------------------------------------------------------
// Вес тела на границах окна и дельты обхватов по каждому ключу замера.
//----------------------------------------------------------------------------
void AsProgram_Stats_Calculator::Collect_Measurements(const SProgress_Facts &in_facts, const QDate &in_start,
                                                      const QDate &in_end, SProgram_Stats &in_out_stats) const
{
	QVector<SBody_Measurement> window_measurements;
	SExtremes_Value weight;

	for (int i = 0; i < in_facts.Measurements.size(); ++i)
	{
		const SBody_Measurement &measurement = in_facts.Measurements.at(i);
		QDate date = QDate::fromString(measurement.Date, Qt::ISODate);

		if (In_Window(date, in_start, in_end) == false)
			continue;

		window_measurements.append(measurement);

		if (measurement.Has_Weight_Kg)
			weight.Add(measurement.Date, measurement.Weight_Kg);
	}

	if (weight.Has_First)
	{
		in_out_stats.Body_Weight_Start_Kg = weight.First_Value;
		in_out_stats.Has_Body_Weight_Start_Kg = true;
	}

	if (weight.Has_Last)
	{
		in_out_stats.Body_Weight_Latest_Kg = weight.Last_Value;
		in_out_stats.Has_Body_Weight_Latest_Kg = true;
	}

	Collect_Measurement_Deltas(window_measurements, in_out_stats);

	if (weight.Has_Both() == false)
		return;

	in_out_stats.Body_Weight_Delta_Kg = weight.Delta();
	in_out_stats.Has_Body_Weight_Delta_Kg = true;
}
//----------------------------------------------------------------------------
// Дельта обхвата «старт → последнее измерение» для каждого ключа (§5.1).
// Ключ без данных остаётся в списке с пустыми флагами — UI покажет «нет данных».
//----------------------------------------------------------------------------
void AsProgram_Stats_Calculator::Collect_Measurement_Deltas(const QVector<SBody_Measurement> &in_measurements,
                                                            SProgram_Stats &in_out_stats) const
{
	for (int key = 0; key < (int)EMeasurement_Key::Max; ++key)
	{
		EMeasurement_Key measurement_key = (EMeasurement_Key)key;
		SExtremes_Value extremes;
		SMeasurement_Delta delta;

		for (int i = 0; i < in_measurements.size(); ++i)
		{
			double value = 0;

			if (Measurement_Read_Key(in_measurements.at(i), measurement_key, value) == false)
				continue;

			extremes.Add(in_measurements.at(i).Date, value);
		}

		delta.Key = measurement_key;

		if (extremes.Has_First)
		{
			delta.Start_Cm = extremes.First_Value;
			delta.Has_Start_Cm = true;
		}

		if (extremes.Has_Last)
		{
			delta.Latest_Cm = extremes.Last_Value;
			delta.Has_Latest_Cm = true;
		}

		if (extremes.Has_Both())
		{
			delta.Delta_Cm = extremes.Delta();
			delta.Has_Delta_Cm = true;
		}

		in_out_stats.Measurement_Deltas.append(delta);
	}
}
//----------------------------------------------------------------------------
// Прогресс целей: текущее значение метрики, процент и отметка достижения.
//----------------------------------------------------------------------------
void AsProgram_Stats_Calculator::Collect_Goals(const SProgress_Facts &in_facts, SProgram_Stats &in_out_stats) const
{
	for (int i = 0; i < in_facts.Goal_Targets.size(); ++i)
	{
		const SGoal_Target &target = in_facts.Goal_Targets.at(i);
		SGoal_Progress progress;
		double current = 0;

		progress.Goal_Target_Id = target.ID;
		progress.Metric = target.Metric;
		progress.Exercise_Id = target.Exercise_Id;
		progress.Measurement_Key = target.Measurement_Key;
		progress.Has_Measurement_Key = target.Has_Measurement_Key;
		progress.Direction = target.Direction;
		progress.Start_Value = target.Start_Value;
		progress.Has_Start_Value = target.Has_Start_Value;
		progress.Target_Value = target.Target_Value;
		progress.State = target.State;

		progress.Has_Current_Value = Read_Goal_Current(target, in_out_stats, current);
		progress.Current_Value = current;

		// Процент и отметка достижения считаются только при известных
		// текущем значении и старте: иначе цель «не измерена», а не «не выполнена».
		if (progress.Has_Current_Value && target.Has_Start_Value)
			Apply_Goal_Progress(target, current, progress);

		if (progress.Is_Achieved)
			in_out_stats.Goal_Achieved = true;

		in_out_stats.Goal_Progress.append(progress);
	}
}
//----------------------------------------------------------------------------
void AsProgram_Stats_Calculator::Apply_Goal_Progress(const SGoal_Target &in_target, double in_current,
                                                     SGoal_Progress &in_out_progress) const
{
	double percent = 0;

	if (Goal_Progress_Percent(in_target.Direction, in_target.Start_Value, in_target.Target_Value, in_current, percent))
	{
		in_out_progress.Progress_Percent = percent;
		in_out_progress.Has_Progress_Percent = true;
	}

	in_out_progress.Is_Achieved =
	    Goal_Reached(in_target.Direction, in_target.Start_Value, in_target.Target_Value, in_current);
}
//----------------------------------------------------------------------------
// Текущее значение цели по её метрике. Метрики без данных дают false — на
// экране это «нет данных», а не ноль (§5.5).
//----------------------------------------------------------------------------
bool AsProgram_Stats_Calculator::Read_Goal_Current(const SGoal_Target &in_target, const SProgram_Stats &in_stats,
                                                   double &out_value) const
{
	switch (in_target.Metric)
	{
	case EGoal_Metric::BodyWeightKg:
		if (in_stats.Has_Body_Weight_Latest_Kg == false)
			return false;

		out_value = in_stats.Body_Weight_Latest_Kg;

		return true;

	case EGoal_Metric::WorkingWeightKg:
		return Lookup_Strength(in_stats, in_target.Exercise_Id, out_value);

	case EGoal_Metric::MeasurementCm:
		return Lookup_Delta_Latest(in_stats, in_target.Measurement_Key, in_target.Has_Measurement_Key, out_value);

	case EGoal_Metric::WorkoutsPerWeek:
		if (in_stats.Weeks.isEmpty())
			return false;

		out_value = in_stats.Completed_Sessions * 1.0 / in_stats.Weeks.size();

		return true;

	case EGoal_Metric::TonnagePerWeekKg:
		if (in_stats.Weeks.isEmpty())
			return false;

		out_value = in_stats.Tonnage_Kg / in_stats.Weeks.size();

		return true;

	default:
		return false;
	}
}
//----------------------------------------------------------------------------
SMeasurement_Delta delta;

//----------------------------------------------------------------------------
// Сводка правил и средние значения метрик (белок, сон, вода, шаги).
//----------------------------------------------------------------------------
void AsProgram_Stats_Calculator::Collect_Rule_Stats(const QVector<SProgram_Rule> &in_rules,
                                                    const QVector<SRule_Checkin> &in_checkins,
                                                    SProgram_Stats &in_out_stats) const
{
	AsRule_Engine engine;
	SRule_Stats overall;

	if (engine.Compute_Overall_Stats(in_rules, in_checkins, overall) && overall.Has_Adherence_Percent)
	{
		in_out_stats.Rule_Adherence_Percent = overall.Adherence_Percent;
		in_out_stats.Has_Rule_Adherence_Percent = true;
	}

	for (int i = 0; i < in_rules.size(); ++i)
	{
		const SProgram_Rule &rule = in_rules.at(i);
		SRule_Stat_Row row;
		SRule_Stats stats;

		if (engine.Compute_Rule_Stats(rule, in_checkins, stats) == false)
			continue;

		row.Rule_Id = rule.ID;
		row.Code = rule.Code;
		row.Title = rule.Title;
		row.Metric = rule.Metric;
		row.Stats = stats;
		in_out_stats.Rule_Stats.append(row);

		if (stats.Has_Average_Value)
			Apply_Average_Metric(rule, stats.Average_Value, in_out_stats);
	}

	// Норма белка «на кг» — от последнего веса тела (§5.1).
	if (in_out_stats.Has_Average_Protein_Grams_Per_Day == false || in_out_stats.Has_Body_Weight_Latest_Kg == false ||
	    in_out_stats.Body_Weight_Latest_Kg <= 0)
		return;

	in_out_stats.Average_Protein_Per_Kg =
	    in_out_stats.Average_Protein_Grams_Per_Day / in_out_stats.Body_Weight_Latest_Kg;
	in_out_stats.Has_Average_Protein_Per_Kg = true;
}
//----------------------------------------------------------------------------
// Среднее по метрике правила попадает в отдельный счётчик программы. Метрики
// без своего счётчика (например, custom) учитываются только в ruleStats[].
//----------------------------------------------------------------------------
void AsProgram_Stats_Calculator::Apply_Average_Metric(const SProgram_Rule &in_rule, double in_average,
                                                      SProgram_Stats &in_out_stats) const
{
	switch (in_rule.Metric)
	{
	case ERule_Metric::ProteinGrams:
		in_out_stats.Average_Protein_Grams_Per_Day = in_average;
		in_out_stats.Has_Average_Protein_Grams_Per_Day = true;

		break;

	case ERule_Metric::SleepHours:
		in_out_stats.Average_Sleep_Hours = in_average;
		in_out_stats.Has_Average_Sleep_Hours = true;

		break;

	case ERule_Metric::WaterMl:
		in_out_stats.Average_Water_Ml = in_average;
		in_out_stats.Has_Average_Water_Ml = true;

		break;

	case ERule_Metric::StepCount:
		in_out_stats.Average_Steps = in_average;
		in_out_stats.Has_Average_Steps = true;

		break;

	default:
		break;
	}
}
//----------------------------------------------------------------------------
// Расчётные правила проверяют себя сами (§3.6): к сохранённым отметкам
// добавляются авто-отметки из фактов тренировок, иначе в статистике они
// выглядели бы «без данных».
//----------------------------------------------------------------------------
void AsProgram_Stats_Calculator::Collect_Computed_Checkins(const SProgress_Facts &in_facts,
                                                           QVector<SRule_Checkin> &in_out_checkins) const
{
	AsRule_Engine engine;

	for (int i = 0; i < in_facts.Rules.size(); ++i)
	{
		const SProgram_Rule &rule = in_facts.Rules.at(i);

		if (Is_Computed_Rule(rule) == false)
			continue;

		for (int j = 0; j < in_facts.Sessions.size(); ++j)
		{
			const SWorkout_Session &session = in_facts.Sessions.at(j);
			QVector<SSet_Log> session_sets;
			SRule_Checkin checkin;

			if (session.State != EWorkout_State::Completed)
				continue;

			Collect_Session_Sets(in_facts, session.ID, session_sets);

			if (engine.Build_Computed_Checkin(rule, session, session_sets, QVector<SWorkout_Session>(), checkin) ==
			    false)
				continue;

			if (Has_Checkin(in_out_checkins, rule.ID, checkin.Date) == false)
				in_out_checkins.append(checkin);
		}
	}
}
//----------------------------------------------------------------------------
