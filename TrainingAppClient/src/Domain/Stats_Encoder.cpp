#include "Domain/Stats_Encoder.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(STATS_Encoder_Log, "trainingapp.program.stats_encoder")

//----------------------------------------------------------------------------
// Значение метрики: число, если данные есть, иначе JSON null — «нечего считать»
// отличается от нуля на экране (§5.5).
//----------------------------------------------------------------------------
static QJsonValue Number_Or_Null(double in_value, bool in_has)
{
	return in_has ? QJsonValue(in_value) : QJsonValue(QJsonValue::Null);
}
//----------------------------------------------------------------------------
// AStats_Encoder
//----------------------------------------------------------------------------
AStats_Encoder::AStats_Encoder() : Error_State(&STATS_Encoder_Log())
{
}
//----------------------------------------------------------------------------
QString AStats_Encoder::Get_Last_Error() const
{
	return Error_State.Get_Last_Error();
}
//----------------------------------------------------------------------------
bool AStats_Encoder::Set_Last_Error(const QString &in_error)
{
	return Error_State.Set_Last_Error(in_error);
}
//----------------------------------------------------------------------------
void AStats_Encoder::Clear_Last_Error()
{
	Error_State.Clear_Last_Error();
}
//----------------------------------------------------------------------------
// JSON-нагрузка статистики программы для кэша `program_stats` (§5.4).
//----------------------------------------------------------------------------
bool AStats_Encoder::Encode_Program_Stats(const SProgram_Stats &in_stats, QString &out_json)
{
	QJsonObject object;
	QJsonArray tonnage_by_week;
	QJsonArray week_adherence;
	QJsonArray strengths;
	QJsonArray goals;
	QJsonArray deltas;
	QJsonArray rules;

	Clear_Last_Error();
	out_json.clear();

	if (in_stats.Program_Id.isEmpty())
		return Set_Last_Error("Кодирование статистики требует программу с id");

	object = Encode_Program_Scalars(in_stats);

	// Тоннаж по неделям и adherence разнесены в два массива: график тянет
	// первый, а список недель со значением — второй (§7).
	for (int i = 0; i < in_stats.Weeks.size(); ++i)
	{
		tonnage_by_week.append(in_stats.Weeks.at(i).Tonnage_Kg);
		week_adherence.append(Encode_Week(in_stats.Weeks.at(i)));
	}

	for (int i = 0; i < in_stats.Exercise_Strength.size(); ++i)
		strengths.append(Encode_Strength(in_stats.Exercise_Strength.at(i)));

	for (int i = 0; i < in_stats.Goal_Progress.size(); ++i)
		goals.append(Encode_Goal(in_stats.Goal_Progress.at(i)));

	for (int i = 0; i < in_stats.Measurement_Deltas.size(); ++i)
		deltas.append(Encode_Delta(in_stats.Measurement_Deltas.at(i)));

	for (int i = 0; i < in_stats.Rule_Stats.size(); ++i)
		rules.append(Encode_Rule(in_stats.Rule_Stats.at(i)));

	object.insert("tonnageByWeek", tonnage_by_week);
	object.insert("weekAdherence", week_adherence);
	object.insert("exerciseStrength", strengths);
	object.insert("goalProgress", goals);
	object.insert("measurementDeltas", deltas);
	object.insert("ruleStats", rules);

	out_json = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));

	return true;
}
//----------------------------------------------------------------------------
QJsonObject AStats_Encoder::Encode_Week(const SWeek_Aggregate &in_week) const
{
	QJsonObject object;

	object.insert("weekIndex", in_week.Week_Index);
	object.insert("plannedSessions", in_week.Planned_Sessions);
	object.insert("completedSessions", in_week.Completed_Sessions);
	object.insert("adherencePercent", Number_Or_Null(in_week.Adherence_Percent, in_week.Has_Adherence_Percent));

	return object;
}
//----------------------------------------------------------------------------
QJsonObject AStats_Encoder::Encode_Strength(const SExercise_Strength &in_strength) const
{
	QJsonObject object;

	object.insert("exerciseId", in_strength.Exercise_Id);
	object.insert("workingWeightKg", Number_Or_Null(in_strength.Working_Weight_Kg, in_strength.Has_Working_Weight_Kg));
	object.insert("e1rmKg", Number_Or_Null(in_strength.E1rm_Kg, in_strength.Has_E1rm_Kg));

	return object;
}
//----------------------------------------------------------------------------
QJsonObject AStats_Encoder::Encode_Goal(const SGoal_Progress &in_goal) const
{
	QJsonObject object;

	object.insert("goalTargetId", in_goal.Goal_Target_Id);
	object.insert("metric", To_String(in_goal.Metric));
	object.insert("exerciseId", in_goal.Exercise_Id);
	object.insert("direction", To_String(in_goal.Direction));
	object.insert("startValue", Number_Or_Null(in_goal.Start_Value, in_goal.Has_Start_Value));
	object.insert("currentValue", Number_Or_Null(in_goal.Current_Value, in_goal.Has_Current_Value));
	object.insert("targetValue", in_goal.Target_Value);
	object.insert("progressPercent", Number_Or_Null(in_goal.Progress_Percent, in_goal.Has_Progress_Percent));
	object.insert("achieved", in_goal.Is_Achieved);

	return object;
}
//----------------------------------------------------------------------------
QJsonObject AStats_Encoder::Encode_Delta(const SMeasurement_Delta &in_delta) const
{
	QJsonObject object;

	object.insert("key", To_String(in_delta.Key));
	object.insert("startCm", Number_Or_Null(in_delta.Start_Cm, in_delta.Has_Start_Cm));
	object.insert("latestCm", Number_Or_Null(in_delta.Latest_Cm, in_delta.Has_Latest_Cm));
	object.insert("deltaCm", Number_Or_Null(in_delta.Delta_Cm, in_delta.Has_Delta_Cm));

	return object;
}
//----------------------------------------------------------------------------
QJsonObject AStats_Encoder::Encode_Rule(const SRule_Stat_Row &in_rule) const
{
	QJsonObject object;

	object.insert("ruleId", in_rule.Rule_Id);
	object.insert("code", in_rule.Code);
	object.insert("title", in_rule.Title);
	object.insert("adherencePercent",
	              Number_Or_Null(in_rule.Stats.Adherence_Percent, in_rule.Stats.Has_Adherence_Percent));
	object.insert("doneDays", in_rule.Stats.Done_Count);
	object.insert("failedDays", in_rule.Stats.Failed_Count);
	object.insert("skippedDays", in_rule.Stats.Skipped_Count);
	object.insert("avgValue", Number_Or_Null(in_rule.Stats.Average_Value, in_rule.Stats.Has_Average_Value));

	return object;
}
//----------------------------------------------------------------------------
// Скалярные поля программы. Массивы добавляются отдельно и присутствуют
// всегда, даже когда пусты: UI различает «пусто» и «нет ключа» по флагам
// метрик, а не по отсутствию списка.
//----------------------------------------------------------------------------
QJsonObject AStats_Encoder::Encode_Program_Scalars(const SProgram_Stats &in_stats) const
{
	QJsonObject object;

	object.insert("programId", in_stats.Program_Id);
	object.insert("programRevision", in_stats.Program_Revision);
	object.insert("plannedSessions", in_stats.Planned_Sessions);
	object.insert("completedSessions", in_stats.Completed_Sessions);
	object.insert("adherencePercent", Number_Or_Null(in_stats.Adherence_Percent, in_stats.Has_Adherence_Percent));
	object.insert("missedSessions", in_stats.Missed_Sessions);
	object.insert("movedSessions", in_stats.Moved_Sessions);
	object.insert("extraSessions", in_stats.Extra_Sessions);
	object.insert("currentStreakWeeks", in_stats.Current_Streak_Weeks);
	object.insert("longestStreakWeeks", in_stats.Longest_Streak_Weeks);
	object.insert("tonnageKg", in_stats.Tonnage_Kg);
	object.insert("setsCount", in_stats.Sets_Count);
	object.insert("repsCount", in_stats.Reps_Count);
	object.insert("durationMinutes", in_stats.Duration_Minutes);
	object.insert("avgRpe", Number_Or_Null(in_stats.Average_Rpe, in_stats.Has_Average_Rpe));
	object.insert("maxRpe", Number_Or_Null(in_stats.Max_Rpe, in_stats.Has_Max_Rpe));
	object.insert("hardSessionsCount", in_stats.Hard_Sessions_Count);
	object.insert("rpeViolations", in_stats.Rpe_Violations);

	object.insert("bodyWeightStartKg",
	              Number_Or_Null(in_stats.Body_Weight_Start_Kg, in_stats.Has_Body_Weight_Start_Kg));
	object.insert("bodyWeightLatestKg",
	              Number_Or_Null(in_stats.Body_Weight_Latest_Kg, in_stats.Has_Body_Weight_Latest_Kg));
	object.insert("bodyWeightDeltaKg",
	              Number_Or_Null(in_stats.Body_Weight_Delta_Kg, in_stats.Has_Body_Weight_Delta_Kg));

	object.insert("ruleAdherencePercent",
	              Number_Or_Null(in_stats.Rule_Adherence_Percent, in_stats.Has_Rule_Adherence_Percent));
	object.insert("avgProteinGramsPerDay",
	              Number_Or_Null(in_stats.Average_Protein_Grams_Per_Day, in_stats.Has_Average_Protein_Grams_Per_Day));
	object.insert("avgProteinPerKg",
	              Number_Or_Null(in_stats.Average_Protein_Per_Kg, in_stats.Has_Average_Protein_Per_Kg));
	object.insert("avgSleepHours", Number_Or_Null(in_stats.Average_Sleep_Hours, in_stats.Has_Average_Sleep_Hours));
	object.insert("avgWaterMl", Number_Or_Null(in_stats.Average_Water_Ml, in_stats.Has_Average_Water_Ml));
	object.insert("avgSteps", Number_Or_Null(in_stats.Average_Steps, in_stats.Has_Average_Steps));

	object.insert("dataRangeFrom", in_stats.Data_Range_From);
	object.insert("dataRangeTo", in_stats.Data_Range_To);
	object.insert("goalAchieved", in_stats.Goal_Achieved);

	return object;
}
//----------------------------------------------------------------------------
// JSON-нагрузка общей статистики для кэша `lifetime_stats` (§5.3, §5.4).
//----------------------------------------------------------------------------
bool AStats_Encoder::Encode_Lifetime_Stats(const SLifetime_Stats &in_stats, QString &out_json)
{
	QJsonObject object;
	QJsonArray regularity;
	QJsonArray records;
	QJsonArray summaries;

	Clear_Last_Error();
	out_json.clear();

	if (in_stats.User_Id.isEmpty())
		return Set_Last_Error("Общая статистика требует userId");

	object = Encode_Lifetime_Scalars(in_stats);

	for (int i = 0; i < in_stats.Regularity.size(); ++i)
		regularity.append(Encode_Regularity(in_stats.Regularity.at(i)));

	for (int i = 0; i < in_stats.Records.size(); ++i)
		records.append(Encode_Record(in_stats.Records.at(i)));

	for (int i = 0; i < in_stats.Summaries.size(); ++i)
		summaries.append(Encode_Summary(in_stats.Summaries.at(i)));

	object.insert("regularity", regularity);
	object.insert("records", records);
	object.insert("programSummaries", summaries);

	out_json = QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));

	return true;
}
//----------------------------------------------------------------------------
QJsonObject AStats_Encoder::Encode_Lifetime_Scalars(const SLifetime_Stats &in_stats) const
{
	QJsonObject object;

	object.insert("userId", in_stats.User_Id);
	object.insert("totalPrograms", in_stats.Total_Programs);
	object.insert("completedPrograms", in_stats.Completed_Programs);
	object.insert("cancelledPrograms", in_stats.Cancelled_Programs);
	object.insert("totalSessions", in_stats.Total_Sessions);
	object.insert("totalSets", in_stats.Total_Sets);
	object.insert("totalReps", in_stats.Total_Reps);
	object.insert("lifetimeTonnageKg", in_stats.Lifetime_Tonnage_Kg);
	object.insert("lifetimeDurationMinutes", in_stats.Lifetime_Duration_Minutes);
	object.insert("firstSessionDate", in_stats.First_Session_Date);
	object.insert("lastSessionDate", in_stats.Last_Session_Date);
	object.insert("activeDaysCount", in_stats.Active_Days_Count);
	object.insert("weeksWithSessions", in_stats.Weeks_With_Sessions);
	object.insert("longestStreakWeeks", in_stats.Longest_Streak_Weeks);
	object.insert("currentStreakWeeks", in_stats.Current_Streak_Weeks);
	object.insert("avgSessionsPerWeek",
	              Number_Or_Null(in_stats.Average_Sessions_Per_Week, in_stats.Has_Average_Sessions_Per_Week));
	object.insert("bodyWeightTotalDeltaKg",
	              Number_Or_Null(in_stats.Body_Weight_Total_Delta_Kg, in_stats.Has_Body_Weight_Total_Delta_Kg));
	object.insert("bestProgramId", in_stats.Best_Program_Id);

	return object;
}
//----------------------------------------------------------------------------
QJsonObject AStats_Encoder::Encode_Regularity(const SRegularity_Week &in_week) const
{
	QJsonObject object;

	object.insert("weekStart", in_week.Week_Start);
	object.insert("workoutCount", in_week.Workout_Count);

	return object;
}
//----------------------------------------------------------------------------
QJsonObject AStats_Encoder::Encode_Record(const SExercise_Record &in_record) const
{
	QJsonObject object;

	object.insert("exerciseId", in_record.Exercise_Id);
	object.insert("weightKg", in_record.Weight_Kg);
	object.insert("reps", in_record.Reps);
	object.insert("e1rmKg", in_record.E1rm_Kg);
	object.insert("date", in_record.Date);
	object.insert("sessionId", in_record.Session_Id);
	object.insert("programId", in_record.Program_Id);

	return object;
}
//----------------------------------------------------------------------------
QJsonObject AStats_Encoder::Encode_Summary(const SProgram_Summary &in_summary) const
{
	QJsonObject object;

	object.insert("programId", in_summary.Program_Id);
	object.insert("title", in_summary.Title);
	object.insert("goal", To_String(in_summary.Goal));
	object.insert("state", To_String(in_summary.State));
	object.insert("weeksTotal", in_summary.Weeks_Total);
	object.insert("sessions", in_summary.Sessions);
	object.insert("tonnageKg", in_summary.Tonnage_Kg);
	object.insert("adherencePercent", Number_Or_Null(in_summary.Adherence_Percent, in_summary.Has_Adherence_Percent));
	object.insert("bodyWeightDeltaKg",
	              Number_Or_Null(in_summary.Body_Weight_Delta_Kg, in_summary.Has_Body_Weight_Delta_Kg));
	object.insert("keyLiftDeltaKg", Number_Or_Null(in_summary.Key_Lift_Delta_Kg, in_summary.Has_Key_Lift_Delta_Kg));

	return object;
}
//----------------------------------------------------------------------------