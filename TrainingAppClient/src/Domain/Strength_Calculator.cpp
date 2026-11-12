#include "Domain/Strength_Calculator.h"

#include <QDate>
#include <QMap>
#include <QtMath>

//-------------------------------------------------------------------------------------------------------------
SStrength_Workload AStrength_Calculator::Summarize(const QVector<SSet_Log> &in_sets)
{
	SStrength_Workload result;
	double total_reps = 0;
	for (const SSet_Log &set : in_sets)
	{
		if (!set.Done || set.Is_Warmup || !qIsFinite(set.Weight_Kg) || set.Weight_Kg < 0 || set.Reps <= 0)
			continue;

		result.Has_Data = true;
		result.Weight_Kg = qMax(result.Weight_Kg, set.Weight_Kg);
		total_reps += set.Reps;
		++result.Set_Count;
	}
	if (result.Has_Data)
		result.Average_Reps = total_reps / result.Set_Count;

	return result;
}
//-------------------------------------------------------------------------------------------------------------
SExercise_Trend AStrength_Calculator::Evaluate_Exercise_Trend(const SStrength_Workload &in_previous,
                                                              const SStrength_Workload &in_current, int in_reps_min,
                                                              int in_reps_max, int in_tolerance)
{
	SExercise_Trend result;
	if (in_tolerance < 0 || in_tolerance > 20 || in_reps_min < 0 || in_reps_max < in_reps_min)
	{
		result.Reason = "invalidSettings";
		return result;
	}
	if (!in_current.Has_Data || !qIsFinite(in_current.Weight_Kg) || !qIsFinite(in_current.Average_Reps) ||
	    in_current.Weight_Kg < 0 || in_current.Average_Reps <= 0)
		return result;

	if (!in_previous.Has_Data)
	{
		result.Status = "start";
		result.Reason = "firstRecord";
		return result;
	}
	if (!qIsFinite(in_previous.Weight_Kg) || !qIsFinite(in_previous.Average_Reps) || in_previous.Weight_Kg < 0 ||
	    in_previous.Average_Reps <= 0)
	{
		result.Reason = "invalidPrevious";
		return result;
	}

	result.Is_Evaluated = true;
	result.Has_Delta = true;
	result.Delta_Kg = in_current.Weight_Kg - in_previous.Weight_Kg;
	result.Has_Delta_Percent = in_previous.Weight_Kg > 0;
	if (result.Has_Delta_Percent)
		result.Delta_Percent = result.Delta_Kg / in_previous.Weight_Kg * 100;

	double rep_change = in_current.Average_Reps - in_previous.Average_Reps;
	if (in_reps_min > 0 && in_current.Average_Reps < in_reps_min)
	{
		result.Status = "worse";
		result.Reason = "belowRange";
	}
	else if (in_reps_max > 0 && in_current.Average_Reps > in_reps_max)
	{
		result.Status = "attention";
		result.Reason = "aboveRange";
	}
	else if (result.Delta_Kg > 0)
	{
		result.Status = rep_change >= -in_tolerance ? "better" : "worse";
		result.Reason = rep_change >= -in_tolerance ? "weightInTolerance" : "repDrop";
	}
	else if (result.Delta_Kg < 0)
	{
		result.Status = rep_change > 0 ? "attention" : "worse";
		result.Reason = rep_change > 0 ? "lighterMoreReps" : "lessWeight";
	}
	else
	{
		result.Status = rep_change > 0 ? "better" : (rep_change < 0 ? "worse" : "same");
		result.Reason = rep_change > 0 ? "moreReps" : (rep_change < 0 ? "fewerReps" : "unchanged");
	}

	return result;
}
//-------------------------------------------------------------------------------------------------------------
void AStrength_Calculator::Accumulate(SProgress_Index &io_index, const QString &in_status)
{
	if (in_status != "better" && in_status != "worse" && in_status != "same" && in_status != "attention")
		return;

	++io_index.Evaluated;
	if (in_status == "better")
		++io_index.Better;
	if (in_status == "worse")
		++io_index.Worse;
	io_index.Percent = 100.0 * (io_index.Better - io_index.Worse) / io_index.Evaluated;
}
//-------------------------------------------------------------------------------------------------------------
SStrength_Report AStrength_Calculator::Compute(const SProgram &in_program, const QVector<SProgram_Day> &in_days,
                                               const QVector<SDay_Exercise> &in_exercises,
                                               const QVector<SWorkout_Session> &in_sessions,
                                               const QVector<SSet_Log> &in_sets)
{
	SStrength_Report report;
	for (const SProgram_Day &day : in_days)
	{
		SStrength_Split split;
		split.ID = day.ID;
		split.Title = day.Title;
		report.Splits << split;
	}
	for (const SDay_Exercise &prescription : in_exercises)
	{
		if (prescription.Exercise_Id.isEmpty() ||
		    Find_Exercise(report, prescription.Program_Day_Id, prescription.Exercise_Id) >= 0)
			continue;

		SStrength_Exercise exercise;
		exercise.Split_Id = prescription.Program_Day_Id;
		exercise.Exercise_Id = prescription.Exercise_Id;
		exercise.Reps_Min = prescription.Reps_Min;
		exercise.Reps_Max = prescription.Reps_Max;
		exercise.Progression = prescription.Progression;
		exercise.Has_Progression = prescription.Has_Progression;
		report.Exercises << exercise;
		Ensure_Split(report, exercise.Split_Id);
	}

	QVector<SWorkout_Session> sessions;
	for (const SWorkout_Session &session : in_sessions)
	{
		if (session.State == EWorkout_State::Completed && session.Program_Id == in_program.ID &&
		    session.User_Id == in_program.User_Id && !session.ID.isEmpty() &&
		    QDate::fromString(session.Date, Qt::ISODate).isValid())
			sessions << session;
	}
	QMap<QString, SWorkout_Session> ordered;
	for (const SWorkout_Session &session : sessions)
		ordered.insert(session.Date + "/" + session.Started_At + "/" + session.ID, session);
	sessions = ordered.values();

	for (const SWorkout_Session &session : sessions)
	{
		const QString split_id = session.Snapshot_Day_Id.isEmpty() ? session.Program_Day_Id : session.Snapshot_Day_Id;
		QHash<QString, QVector<SSet_Log>> sets_by_exercise;
		QStringList exercise_ids;
		for (const SSet_Log &set : in_sets)
		{
			if (set.Session_Id != session.ID || set.Exercise_Id.isEmpty())
				continue;
			if (!sets_by_exercise.contains(set.Exercise_Id))
				exercise_ids << set.Exercise_Id;
			sets_by_exercise[set.Exercise_Id] << set;
		}
		// QHash iteration order must never determine display or accumulation order.
		exercise_ids.sort();
		for (const QString &exercise_id : exercise_ids)
		{
			SStrength_Workload workload = Summarize(sets_by_exercise.value(exercise_id));
			if (!workload.Has_Data)
				continue;

			int index = Find_Exercise(report, split_id, exercise_id);
			if (index < 0)
			{
				SStrength_Exercise exercise;
				exercise.Split_Id = split_id;
				exercise.Exercise_Id = exercise_id;
				report.Exercises << exercise;
				index = report.Exercises.size() - 1;
			}
			SStrength_Exercise &exercise = report.Exercises[index];
			exercise.Previous = exercise.Current;
			exercise.Current = workload;
			if (!exercise.Start.Has_Data)
				exercise.Start = workload;
			SStrength_Observation observation;
			Resolve_Range(sets_by_exercise.value(exercise_id), exercise, observation);
			exercise.Trend = Evaluate_Exercise_Trend(exercise.Previous, workload, observation.Reps_Min,
			                                         observation.Reps_Max, in_program.Rep_Tolerance);
			Accumulate(exercise.Index, exercise.Trend.Status);
			Accumulate(report.Index, exercise.Trend.Status);
			int split_index = Ensure_Split(report, exercise.Split_Id);
			if (!session.Snapshot_Day_Title.isEmpty())
				report.Splits[split_index].Title = session.Snapshot_Day_Title;
			Accumulate(report.Splits[split_index].Index, exercise.Trend.Status);
			observation.Session_Id = session.ID;
			observation.Date = session.Date;
			observation.Split_Id = exercise.Split_Id;
			observation.Exercise_Id = exercise_id;
			observation.Previous = exercise.Previous;
			observation.Current = workload;
			observation.Trend = exercise.Trend;
			report.History << observation;
		}
	}

	return report;
}
//-------------------------------------------------------------------------------------------------------------
int AStrength_Calculator::Find_Exercise(const SStrength_Report &in_report, const QString &in_split_id,
                                        const QString &in_exercise_id)
{
	for (int i = 0; i < in_report.Exercises.size(); ++i)
		if (in_report.Exercises.at(i).Split_Id == in_split_id &&
		    in_report.Exercises.at(i).Exercise_Id == in_exercise_id)
			return i;

	return -1;
}
//-------------------------------------------------------------------------------------------------------------
int AStrength_Calculator::Ensure_Split(SStrength_Report &io_report, const QString &in_split_id)
{
	for (int i = 0; i < io_report.Splits.size(); ++i)
		if (io_report.Splits.at(i).ID == in_split_id)
			return i;

	SStrength_Split split;
	split.ID = in_split_id;
	io_report.Splits << split;

	return io_report.Splits.size() - 1;
}
//-------------------------------------------------------------------------------------------------------------
void AStrength_Calculator::Resolve_Range(const QVector<SSet_Log> &in_sets, const SStrength_Exercise &in_exercise,
                                         SStrength_Observation &out_observation)
{
	bool has_snapshot = false;
	bool has_legacy = false;
	int legacy_min = 0;
	int legacy_max = 0;
	for (const SSet_Log &set : in_sets)
	{
		if (!set.Done || set.Is_Warmup || !qIsFinite(set.Weight_Kg) || set.Weight_Kg < 0 || set.Reps <= 0)
			continue;
		if (set.Has_Target_Reps_Range && set.Target_Reps_Min > 0 && set.Target_Reps_Max >= set.Target_Reps_Min)
		{
			if (!has_snapshot)
			{
				out_observation.Reps_Min = set.Target_Reps_Min;
				out_observation.Reps_Max = set.Target_Reps_Max;
			}
			else if (out_observation.Reps_Min != set.Target_Reps_Min || out_observation.Reps_Max != set.Target_Reps_Max)
			{
				// Conflicting snapshots do not describe one comparable range.
				out_observation.Reps_Min = 0;
				out_observation.Reps_Max = 0;
				out_observation.Range_Source = "conflictingSnapshots";
				return;
			}
			has_snapshot = true;
		}
		if (set.Has_Target_Reps && set.Target_Reps > 0)
		{
			legacy_min = has_legacy ? qMin(legacy_min, set.Target_Reps) : set.Target_Reps;
			legacy_max = qMax(legacy_max, set.Target_Reps);
			has_legacy = true;
		}
	}
	if (has_snapshot)
		out_observation.Range_Source = "snapshot";
	else if (in_exercise.Reps_Min > 0 && in_exercise.Reps_Max >= in_exercise.Reps_Min)
	{
		out_observation.Reps_Min = in_exercise.Reps_Min;
		out_observation.Reps_Max = in_exercise.Reps_Max;
		out_observation.Range_Source = "currentPlan";
	}
	else if (has_legacy)
	{
		out_observation.Reps_Min = legacy_min;
		out_observation.Reps_Max = legacy_max;
		out_observation.Range_Source = "legacyTarget";
	}
}
//-------------------------------------------------------------------------------------------------------------
