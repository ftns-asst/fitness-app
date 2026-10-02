#include "Lifetime_Calculator.h"

#include "Program_Stats_Calculator.h"
#include "Progress_Support.h"

#include <QHash>
#include <QLoggingCategory>
#include <QSet>

Q_LOGGING_CATEGORY(LIFETIME_Calculator_Log, "trainingapp.program.lifetime")

//----------------------------------------------------------------------------
// Дельта ключевого упражнения программы: упражнение с максимальным e1RM, а
// дельта — между его лучшим и худшим e1RM (§5.3 `keyLiftDeltaKg`).
//----------------------------------------------------------------------------
static bool Key_Lift_Delta(const SProgress_Facts &in_facts, double &out_delta)
{
	QHash<QString, double> best_by_exercise;
	QHash<QString, double> worst_by_exercise;
	QStringList order;
	QString key_exercise;
	double best_value = 0;
	bool has_best = false;

	for (int i = 0; i < in_facts.Sessions.size(); ++i)
	{
		const SWorkout_Session &session = in_facts.Sessions.at(i);

		if (session.State != EWorkout_State::Completed)
			continue;

		for (int j = 0; j < in_facts.Sets.size(); ++j)
		{
			const SSet_Log &set = in_facts.Sets.at(j);
			double e1rm = 0;

			if (set.Session_Id != session.ID || Set_Counts_In_Volume(set) == false || set.Exercise_Id.isEmpty())
				continue;

			if (best_by_exercise.contains(set.Exercise_Id) == false)
			{
				best_by_exercise.insert(set.Exercise_Id, 0);
				worst_by_exercise.insert(set.Exercise_Id, 0);
				order.append(set.Exercise_Id);
			}

			e1rm = E1rm(set);

			if (e1rm > best_by_exercise.value(set.Exercise_Id))
				best_by_exercise[set.Exercise_Id] = e1rm;
			if (worst_by_exercise.value(set.Exercise_Id) <= 0 || e1rm < worst_by_exercise.value(set.Exercise_Id))
				worst_by_exercise[set.Exercise_Id] = e1rm;
		}
	}

	for (int i = 0; i < order.size(); ++i)
	{
		double value = best_by_exercise.value(order.at(i));

		if (has_best == false || value > best_value)
		{
			best_value = value;
			key_exercise = order.at(i);
			has_best = true;
		}
	}

	if (has_best == false)
		return false;

	out_delta = best_by_exercise.value(key_exercise) - worst_by_exercise.value(key_exercise);

	return true;
}
//----------------------------------------------------------------------------
// AsLifetime_Calculator
//----------------------------------------------------------------------------
AsLifetime_Calculator::AsLifetime_Calculator() : Error_State(&LIFETIME_Calculator_Log())
{
}
//----------------------------------------------------------------------------
QString AsLifetime_Calculator::Get_Last_Error() const
{
	return Error_State.Get_Last_Error();
}
//----------------------------------------------------------------------------
bool AsLifetime_Calculator::Set_Last_Error(const QString &in_error)
{
	return Error_State.Set_Last_Error(in_error);
}
//----------------------------------------------------------------------------
void AsLifetime_Calculator::Clear_Last_Error()
{
	Error_State.Clear_Last_Error();
}
//----------------------------------------------------------------------------
bool AsLifetime_Calculator::Compute(const SLifetime_Facts &in_facts, const QVector<SProgress_Facts> &in_programs,
                                    AsProgram_Stats_Calculator *in_program_calculator, SLifetime_Stats &out_stats)
{
	QSet<QString> completed_ids;
	QSet<QString> active_dates;
	QHash<QString, int> week_counts;
	QHash<QString, SExercise_Record> records;
	QStringList record_order;

	Clear_Last_Error();
	out_stats = SLifetime_Stats();

	if (in_facts.User_Id.isEmpty())
		return Set_Last_Error("Общая статистика требует userId");

	out_stats.User_Id = in_facts.User_Id;

	Count_Programs(in_facts.Programs, out_stats);
	Count_Sessions(in_facts, completed_ids, active_dates, week_counts, out_stats);
	Count_Sets(in_facts, completed_ids, records, record_order, out_stats);

	Build_Regularity(week_counts, out_stats);
	Resolve_Records(in_facts, records, record_order, out_stats);
	Collect_Body_Weight(in_facts, out_stats);
	Build_Summaries(in_facts, in_programs, in_program_calculator, out_stats);

	return true;
}
//----------------------------------------------------------------------------
void AsLifetime_Calculator::Count_Programs(const QVector<SProgram> &in_programs, SLifetime_Stats &in_out_stats) const
{
	for (int i = 0; i < in_programs.size(); ++i)
	{
		++in_out_stats.Total_Programs;

		if (in_programs.at(i).State == EProgram_State::Completed)
			++in_out_stats.Completed_Programs;

		if (in_programs.at(i).State == EProgram_State::Cancelled)
			++in_out_stats.Cancelled_Programs;
	}
}
//----------------------------------------------------------------------------
// Сессии: итоги, границы данных, активные дни и счётчик тренировок по неделям
// ISO. Черновики и незавершённые тренировки не считаются фактом (§3.8).
//----------------------------------------------------------------------------
void AsLifetime_Calculator::Count_Sessions(const SLifetime_Facts &in_facts, QSet<QString> &out_completed_ids,
                                           QSet<QString> &out_active_dates, QHash<QString, int> &out_week_counts,
                                           SLifetime_Stats &in_out_stats) const
{
	SExtremes_Value dates;

	for (int i = 0; i < in_facts.Sessions.size(); ++i)
	{
		const SWorkout_Session &session = in_facts.Sessions.at(i);
		QDate date;

		if (session.State != EWorkout_State::Completed)
			continue;

		out_completed_ids.insert(session.ID);

		if (session.Date.isEmpty())
		{
			++in_out_stats.Total_Sessions;
			in_out_stats.Lifetime_Duration_Minutes += session.Duration_Seconds / 60.0;

			continue;
		}

		date = QDate::fromString(session.Date, Qt::ISODate);

		if (date.isValid())
		{
			dates.Add(session.Date, 0);
			out_active_dates.insert(session.Date);
			++out_week_counts[Week_Start(date).toString("yyyy-MM-dd")];
		}

		++in_out_stats.Total_Sessions;
		in_out_stats.Lifetime_Duration_Minutes += session.Duration_Seconds / 60.0;
	}

	in_out_stats.First_Session_Date = dates.First_Date;
	in_out_stats.Last_Session_Date = dates.Last_Date;
	in_out_stats.Active_Days_Count = out_active_dates.size();
}
//----------------------------------------------------------------------------
// Подходы завершённых тренировок: объём «всё время» и лучший подход по e1RM
// для каждого упражнения. Порядок упражнений — по первому появлению.
//----------------------------------------------------------------------------
void AsLifetime_Calculator::Count_Sets(const SLifetime_Facts &in_facts, const QSet<QString> &in_completed_ids,
                                       QHash<QString, SExercise_Record> &in_out_records,
                                       QStringList &in_out_record_order, SLifetime_Stats &in_out_stats) const
{
	for (int i = 0; i < in_facts.Sets.size(); ++i)
	{
		const SSet_Log &set = in_facts.Sets.at(i);
		double e1rm = 0;

		if (in_completed_ids.contains(set.Session_Id) == false || Set_Counts_In_Volume(set) == false ||
		    set.Exercise_Id.isEmpty())
			continue;

		++in_out_stats.Total_Sets;
		in_out_stats.Total_Reps += set.Reps;
		in_out_stats.Lifetime_Tonnage_Kg += Set_Tonnage(set);

		if (in_out_records.contains(set.Exercise_Id) == false)
		{
			SExercise_Record empty;

			empty.Exercise_Id = set.Exercise_Id;
			in_out_records.insert(set.Exercise_Id, empty);
			in_out_record_order.append(set.Exercise_Id);
		}

		e1rm = E1rm(set);

		{
			SExercise_Record &record = in_out_records[set.Exercise_Id];

			if (record.E1rm_Kg > 0 && e1rm <= record.E1rm_Kg)
				continue;

			// Дата и программа рекорда восстанавливаются позже: на этом
			// шаге известна только сессия.
			record.Weight_Kg = set.Weight_Kg;
			record.Reps = set.Reps;
			record.E1rm_Kg = e1rm;
			record.Session_Id = set.Session_Id;
		}
	}
}
//----------------------------------------------------------------------------
// Регулярность: неделя ISO от первой до последней тренировки, счётчик
// тренировок в каждой и серии недель подряд (§5.3 `regularityHeatmap[]`).
//----------------------------------------------------------------------------
void AsLifetime_Calculator::Build_Regularity(const QHash<QString, int> &in_week_counts,
                                             SLifetime_Stats &in_out_stats) const
{
	QDate cursor = QDate::fromString(in_out_stats.First_Session_Date, Qt::ISODate);
	QDate finish = QDate::fromString(in_out_stats.Last_Session_Date, Qt::ISODate);
	int longest = 0;
	int current = 0;

	if (cursor.isValid())
		cursor = Week_Start(cursor);

	if (finish.isValid())
		finish = Week_Start(finish);

	while (cursor.isValid() && cursor <= finish)
	{
		SRegularity_Week entry;

		entry.Week_Start = cursor.toString("yyyy-MM-dd");
		entry.Workout_Count = in_week_counts.value(entry.Week_Start, 0);
		in_out_stats.Regularity.append(entry);

		if (entry.Workout_Count > 0)
		{
			++in_out_stats.Weeks_With_Sessions;
			++current;

			if (current > longest)
				longest = current;
		}
		else
			current = 0;

		cursor = cursor.addDays(7);
	}

	in_out_stats.Longest_Streak_Weeks = longest;
	in_out_stats.Current_Streak_Weeks = current;

	if (in_out_stats.Regularity.isEmpty())
		return;

	in_out_stats.Average_Sessions_Per_Week = in_out_stats.Total_Sessions * 1.0 / in_out_stats.Regularity.size();
	in_out_stats.Has_Average_Sessions_Per_Week = true;
}
//----------------------------------------------------------------------------
// Личные рекорды: подход известен, а его дата и программа берутся из сессии
// рекорда. Соло-тренировка оставляет Program_Id пустым.
//----------------------------------------------------------------------------
void AsLifetime_Calculator::Resolve_Records(const SLifetime_Facts &in_facts,
                                            const QHash<QString, SExercise_Record> &in_records,
                                            const QStringList &in_record_order, SLifetime_Stats &in_out_stats) const
{
	QHash<QString, const SWorkout_Session *> sessions_by_id;

	for (int i = 0; i < in_facts.Sessions.size(); ++i)
	{
		if (in_facts.Sessions.at(i).State == EWorkout_State::Completed)
			sessions_by_id.insert(in_facts.Sessions.at(i).ID, &in_facts.Sessions.at(i));
	}

	for (int i = 0; i < in_record_order.size(); ++i)
	{
		SExercise_Record record = in_records.value(in_record_order.at(i));
		const SWorkout_Session *session = sessions_by_id.value(record.Session_Id, 0);

		if (session)
		{
			record.Date = session->Date;
			record.Program_Id = session->Program_Id;
		}

		in_out_stats.Records.append(record);
	}
}
//----------------------------------------------------------------------------
// Дельта веса тела за всё время: первый и последний известные замеры.
//----------------------------------------------------------------------------
void AsLifetime_Calculator::Collect_Body_Weight(const SLifetime_Facts &in_facts, SLifetime_Stats &in_out_stats) const
{
	SExtremes_Value weight;

	for (int i = 0; i < in_facts.Measurements.size(); ++i)
	{
		if (in_facts.Measurements.at(i).Has_Weight_Kg == false)
			continue;

		weight.Add(in_facts.Measurements.at(i).Date, in_facts.Measurements.at(i).Weight_Kg);
	}

	if (weight.Has_Both() == false)
		return;

	in_out_stats.Body_Weight_Total_Delta_Kg = weight.Delta();
	in_out_stats.Has_Body_Weight_Total_Delta_Kg = true;
}
//----------------------------------------------------------------------------
// Сводка каждой программы для экрана сравнения и лучшая программа по
// выполнению. Метрики берутся из того же расчёта, что и экран программы.
//----------------------------------------------------------------------------
void AsLifetime_Calculator::Build_Summaries(const SLifetime_Facts &in_facts,
                                            const QVector<SProgress_Facts> &in_programs,
                                            AsProgram_Stats_Calculator *in_program_calculator,
                                            SLifetime_Stats &in_out_stats) const
{
	double best_adherence = 0;
	bool has_best_adherence = false;

	for (int i = 0; i < in_facts.Programs.size(); ++i)
	{
		const SProgram &program = in_facts.Programs.at(i);
		const SProgress_Facts *program_facts = 0;
		SProgram_Summary summary;

		summary.Program_Id = program.ID;
		summary.Title = program.Title;
		summary.Goal = program.Goal;
		summary.State = program.State;
		summary.Weeks_Total = program.Term.Weeks_Total;

		for (int j = 0; j < in_programs.size(); ++j)
		{
			if (in_programs.at(j).Program.ID == program.ID)
				program_facts = &in_programs.at(j);
		}

		if (program_facts && in_program_calculator)
			Fill_Summary_Metrics(*program_facts, *in_program_calculator, summary);

		in_out_stats.Summaries.append(summary);

		if (summary.Has_Adherence_Percent == false)
			continue;

		if (has_best_adherence && summary.Adherence_Percent <= best_adherence)
			continue;

		best_adherence = summary.Adherence_Percent;
		in_out_stats.Best_Program_Id = summary.Program_Id;
		has_best_adherence = true;
	}
}
//----------------------------------------------------------------------------
void AsLifetime_Calculator::Fill_Summary_Metrics(const SProgress_Facts &in_facts,
                                                 AsProgram_Stats_Calculator &in_calculator,
                                                 SProgram_Summary &in_out_summary) const
{
	SProgram_Stats stats;
	double key_lift = 0;

	if (in_calculator.Compute(in_facts, stats) == false)
		return;

	in_out_summary.Sessions = stats.Completed_Sessions;
	in_out_summary.Tonnage_Kg = stats.Tonnage_Kg;
	in_out_summary.Adherence_Percent = stats.Adherence_Percent;
	in_out_summary.Has_Adherence_Percent = stats.Has_Adherence_Percent;
	in_out_summary.Body_Weight_Delta_Kg = stats.Body_Weight_Delta_Kg;
	in_out_summary.Has_Body_Weight_Delta_Kg = stats.Has_Body_Weight_Delta_Kg;

	if (Key_Lift_Delta(in_facts, key_lift) == false)
		return;

	in_out_summary.Key_Lift_Delta_Kg = key_lift;
	in_out_summary.Has_Key_Lift_Delta_Kg = true;
}
//----------------------------------------------------------------------------