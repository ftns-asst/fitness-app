#include "Domain/Progress_Calculator.h"
#include "Domain/Progress_Stats.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QtTest>

class AProgress_Calculator_Test : public QObject
{
	Q_OBJECT

private slots:
	void programStatsAggregateWindowAndVolume();
	void programStatsStrengthBodyAndGoals();
	void programStatsRulesUseRuleEngine();
	void lifetimeStatsAggregateAllPrograms();
	void encodeProducesCachePayload();
	void incrementalEqualsFullRecalculation();
	void rejectsInvalidInput();
	void windowBoundariesAreInclusiveAndTermLimits();
	void extremesAreResolvedByDateNotByInputOrder();
	void measurementsOutsideWindowAreIgnored();
	void draftSessionsAndWarmupSetsNeverCount();
	void encodeKeepsZeroWhenDataExists();

private:
	SProgram Make_Program();
	QVector<SWeek_Slot> Make_Schedule();
	SWorkout_Session Make_Session(const QString &in_id, const QString &in_date, int in_duration,
	                              EDeviation in_deviation);
	SSet_Log Make_Set(const QString &in_session_id, const QString &in_exercise_id, double in_weight, int in_reps,
	                  double in_rpe, bool in_done, bool in_warmup);
	SProgress_Facts Make_Facts();
};

//-------------------------------------------------------------------------------------------------------------
SProgram AProgress_Calculator_Test::Make_Program()
{
	SProgram program;

	program.ID = "p1";
	program.User_Id = "u1";
	program.Title = "Сила 4 недели";
	program.Source_Kind = EProgram_Source_Kind::Scratch;
	program.Goal = EProgram_Goal::Strength;
	program.Level = EProgram_Level::Beginner;
	program.Schedule_Kind = ESchedule_Kind::Weekday;
	program.State = EProgram_State::Active;
	program.Revision = 3;
	program.Term.Start_Date = "2026-09-28";
	program.Term.Weeks_Total = 4;
	program.Term.Mode = ETerm_Mode::FixedWeeks;

	return program;
}
//-------------------------------------------------------------------------------------------------------------
QVector<SWeek_Slot> AProgress_Calculator_Test::Make_Schedule()
{
	ESlot_Kind kinds[7] = {ESlot_Kind::Workout, ESlot_Kind::Rest, ESlot_Kind::Workout, ESlot_Kind::Rest,
	                       ESlot_Kind::Workout, ESlot_Kind::Rest, ESlot_Kind::Rest};
	QVector<SWeek_Slot> schedule;

	for (int day = 1; day <= 7; ++day)
	{
		SWeek_Slot slot;

		slot.ID = QString("slot_%1").arg(day);
		slot.Program_Id = "p1";
		slot.Day_Of_Week = day;
		slot.Kind = kinds[day - 1];
		schedule.append(slot);
	}

	return schedule;
}
//-------------------------------------------------------------------------------------------------------------
SWorkout_Session AProgress_Calculator_Test::Make_Session(const QString &in_id, const QString &in_date, int in_duration,
                                                         EDeviation in_deviation)
{
	SWorkout_Session session;

	session.ID = in_id;
	session.User_Id = "u1";
	session.Program_Id = "p1";
	session.Date = in_date;
	session.Started_At = in_date + "T18:00:00Z";
	session.Completed_At = in_date + "T19:00:00Z";
	session.State = EWorkout_State::Completed;
	session.Deviation = in_deviation;
	session.Has_Deviation = true;
	session.Duration_Seconds = in_duration;

	return session;
}
//-------------------------------------------------------------------------------------------------------------
SSet_Log AProgress_Calculator_Test::Make_Set(const QString &in_session_id, const QString &in_exercise_id,
                                             double in_weight, int in_reps, double in_rpe, bool in_done, bool in_warmup)
{
	SSet_Log set;

	set.ID = QString("%1_%2").arg(in_session_id).arg(in_exercise_id);
	set.Session_Id = in_session_id;
	set.Exercise_Id = in_exercise_id;
	set.Weight_Kg = in_weight;
	set.Reps = in_reps;
	set.Rpe = in_rpe;
	set.Has_Rpe = in_rpe >= 0;
	set.Done = in_done;
	set.Is_Warmup = in_warmup;

	return set;
}
//-------------------------------------------------------------------------------------------------------------
SProgress_Facts AProgress_Calculator_Test::Make_Facts()
{
	SProgress_Facts facts;
	SWorkout_Session draft;

	facts.Program = Make_Program();
	facts.Schedule = Make_Schedule();
	facts.Today = "2026-10-19";

	facts.Sessions.append(Make_Session("s1", "2026-09-28", 3600, EDeviation::AsPlanned));
	facts.Sessions.append(Make_Session("s2", "2026-09-30", 1800, EDeviation::AsPlanned));
	facts.Sessions.append(Make_Session("s3", "2026-10-02", 900, EDeviation::AsPlanned));
	facts.Sessions.append(Make_Session("s4", "2026-10-06", 1200, EDeviation::MovedDay));
	facts.Sessions.append(Make_Session("s5", "2026-10-12", 1800, EDeviation::AsPlanned));
	facts.Sessions.append(Make_Session("s6", "2026-10-14", 1800, EDeviation::AsPlanned));
	facts.Sessions.append(Make_Session("s7", "2026-10-16", 1800, EDeviation::AsPlanned));
	facts.Sessions.append(Make_Session("s8", "2026-10-17", 600, EDeviation::ExtraDay));
	facts.Sessions.append(Make_Session("s10", "2026-11-01", 600, EDeviation::AsPlanned));

	draft = Make_Session("s9", "2026-10-19", 600, EDeviation::AsPlanned);
	draft.State = EWorkout_State::Draft;
	facts.Sessions.append(draft);

	facts.Sets.append(Make_Set("s1", "squat", 100, 5, 7, true, false));
	facts.Sets.append(Make_Set("s1", "squat", 100, 5, 8, true, false));
	facts.Sets.append(Make_Set("s2", "bench", 60, 10, 8, true, false));
	facts.Sets.append(Make_Set("s3", "squat", 40, 10, -1, true, true));
	facts.Sets.append(Make_Set("s3", "squat", 105, 5, -1, true, false));
	facts.Sets.append(Make_Set("s4", "bench", 62.5, 10, 9, true, false));
	facts.Sets.append(Make_Set("s8", "squat", 100, 5, -1, true, false));

	{
		SProgram_Rule protein;

		protein.ID = "r_protein";
		protein.Program_Id = "p1";
		protein.Category = ERule_Category::Nutrition;
		protein.Code = "protein";
		protein.Title = "Белок 140 г";
		protein.Metric = ERule_Metric::ProteinGrams;
		protein.Periodicity = ERule_Periodicity::PerDay;
		protein.Target_Min = 140;
		protein.Has_Target_Min = true;
		protein.Target_Unit = ERule_Display_Unit::G;
		protein.Display_Unit = ERule_Display_Unit::G;
		protein.Applies_To = ERule_Applies_To::AllDays;
		protein.Check_Source = ERule_Check_Source::SelfReport;
		protein.Enforcement = ERule_Enforcement::Soft;
		facts.Rules.append(protein);

		SProgram_Rule rpe;

		rpe.ID = "r_rpe";
		rpe.Program_Id = "p1";
		rpe.Category = ERule_Category::Training;
		rpe.Code = "maxRpe";
		rpe.Title = "RPE не выше 8";
		rpe.Metric = ERule_Metric::MaxRpe;
		rpe.Periodicity = ERule_Periodicity::PerWorkout;
		rpe.Target_Max = 8;
		rpe.Has_Target_Max = true;
		rpe.Target_Unit = ERule_Display_Unit::Rpe;
		rpe.Display_Unit = ERule_Display_Unit::Rpe;
		rpe.Applies_To = ERule_Applies_To::TrainingDays;
		rpe.Check_Source = ERule_Check_Source::ComputedFromSets;
		rpe.Enforcement = ERule_Enforcement::Soft;
		facts.Rules.append(rpe);
	}

	{
		SGoal_Target weight_goal;

		weight_goal.ID = "g_weight";
		weight_goal.Program_Id = "p1";
		weight_goal.Metric = EGoal_Metric::BodyWeightKg;
		weight_goal.Unit = EGoal_Unit::Kg;
		weight_goal.Start_Value = 80;
		weight_goal.Has_Start_Value = true;
		weight_goal.Target_Value = 76;
		weight_goal.Direction = EGoal_Direction::Decrease;
		weight_goal.State = EGoal_Target_State::Active;
		facts.Goal_Targets.append(weight_goal);

		SGoal_Target lift_goal;

		lift_goal.ID = "g_lift";
		lift_goal.Program_Id = "p1";
		lift_goal.Metric = EGoal_Metric::WorkingWeightKg;
		lift_goal.Exercise_Id = "squat";
		lift_goal.Unit = EGoal_Unit::Kg;
		lift_goal.Start_Value = 100;
		lift_goal.Has_Start_Value = true;
		lift_goal.Target_Value = 110;
		lift_goal.Direction = EGoal_Direction::Increase;
		lift_goal.State = EGoal_Target_State::Active;
		facts.Goal_Targets.append(lift_goal);
	}

	{
		SBody_Measurement first;
		SBody_Measurement latest;

		first.ID = "m1";
		first.User_Id = "u1";
		first.Date = "2026-09-28";
		first.Weight_Kg = 80;
		first.Has_Weight_Kg = true;
		first.Chest_Cm = 100;
		first.Has_Chest_Cm = true;
		facts.Measurements.append(first);

		latest.ID = "m2";
		latest.User_Id = "u1";
		latest.Date = "2026-10-16";
		latest.Weight_Kg = 78;
		latest.Has_Weight_Kg = true;
		latest.Chest_Cm = 101;
		latest.Has_Chest_Cm = true;
		facts.Measurements.append(latest);
	}

	{
		SRule_Checkin checkin;

		checkin.Rule_Id = "r_protein";
		checkin.Program_Id = "p1";
		checkin.Date = "2026-09-28";
		checkin.Value = 150;
		checkin.Has_Value = true;
		checkin.Unit = "g";
		checkin.Source = ECheckin_Source::Manual;
		facts.Checkins.append(checkin);

		checkin.Date = "2026-09-29";
		checkin.Value = 120;
		facts.Checkins.append(checkin);

		checkin.Date = "2026-10-01";
		checkin.Value = 160;
		facts.Checkins.append(checkin);
	}

	return facts;
}
//-------------------------------------------------------------------------------------------------------------
void AProgress_Calculator_Test::programStatsAggregateWindowAndVolume()
{
	AsProgress_Calculator calculator;
	SProgress_Facts facts = Make_Facts();
	SProgram_Stats stats;

	QVERIFY(calculator.Compute_Program_Stats(facts, stats));
	QCOMPARE(stats.Program_Id, QString("p1"));
	QCOMPARE(stats.Program_Revision, 3);
	QCOMPARE(stats.Weeks.size(), 4);
	QCOMPARE(stats.Weeks.at(0).Planned_Sessions, 3);
	QCOMPARE(stats.Weeks.at(0).Completed_Sessions, 3);
	QCOMPARE(stats.Weeks.at(1).Completed_Sessions, 1);
	QCOMPARE(stats.Weeks.at(2).Completed_Sessions, 4);
	QCOMPARE(stats.Weeks.at(3).Completed_Sessions, 0);

	QCOMPARE(stats.Planned_Sessions, 12);
	QCOMPARE(stats.Completed_Sessions, 8);
	QVERIFY(stats.Has_Adherence_Percent);
	QCOMPARE(stats.Adherence_Percent, 800.0 / 12.0);
	QCOMPARE(stats.Missed_Sessions, 5);
	QCOMPARE(stats.Moved_Sessions, 1);
	QCOMPARE(stats.Extra_Sessions, 1);
	QCOMPARE(stats.Longest_Streak_Weeks, 1);
	QCOMPARE(stats.Current_Streak_Weeks, 0);

	QCOMPARE(stats.Tonnage_Kg, 3250.0);
	QCOMPARE(stats.Sets_Count, 6);
	QCOMPARE(stats.Reps_Count, 40);
	QCOMPARE(stats.Duration_Minutes, 225.0);
	QVERIFY(stats.Has_Average_Rpe);
	QCOMPARE(stats.Average_Rpe, 8.0);
	QVERIFY(stats.Has_Max_Rpe);
	QCOMPARE(stats.Max_Rpe, 9.0);
	QCOMPARE(stats.Hard_Sessions_Count, 1);

	QCOMPARE(stats.Data_Range_From, QString("2026-09-28"));
	QCOMPARE(stats.Data_Range_To, QString("2026-10-17"));
}
//-------------------------------------------------------------------------------------------------------------
void AProgress_Calculator_Test::programStatsStrengthBodyAndGoals()
{
	AsProgress_Calculator calculator;
	SProgress_Facts facts = Make_Facts();
	SProgram_Stats stats;

	QVERIFY(calculator.Compute_Program_Stats(facts, stats));
	QCOMPARE(stats.Exercise_Strength.size(), 2);
	QCOMPARE(stats.Exercise_Strength.at(0).Exercise_Id, QString("squat"));
	QVERIFY(stats.Exercise_Strength.at(0).Has_Working_Weight_Kg);
	QCOMPARE(stats.Exercise_Strength.at(0).Working_Weight_Kg, 105.0);
	QCOMPARE(stats.Exercise_Strength.at(0).E1rm_Kg, 105.0 * (1 + 5.0 / 30.0));
	QCOMPARE(stats.Exercise_Strength.at(1).Exercise_Id, QString("bench"));
	QCOMPARE(stats.Exercise_Strength.at(1).Working_Weight_Kg, 62.5);
	QCOMPARE(stats.Exercise_Strength.at(1).E1rm_Kg, 62.5 * (1 + 10.0 / 30.0));

	QVERIFY(stats.Has_Body_Weight_Start_Kg);
	QVERIFY(stats.Has_Body_Weight_Latest_Kg);
	QVERIFY(stats.Has_Body_Weight_Delta_Kg);
	QCOMPARE(stats.Body_Weight_Start_Kg, 80.0);
	QCOMPARE(stats.Body_Weight_Latest_Kg, 78.0);
	QCOMPARE(stats.Body_Weight_Delta_Kg, -2.0);

	QCOMPARE(stats.Measurement_Deltas.size(), 5);
	QVERIFY(stats.Measurement_Deltas.at(0).Has_Delta_Cm);
	QCOMPARE(stats.Measurement_Deltas.at(0).Delta_Cm, 1.0);
	QVERIFY(!stats.Measurement_Deltas.at(1).Has_Delta_Cm);

	QCOMPARE(stats.Goal_Progress.size(), 2);
	QVERIFY(stats.Goal_Progress.at(0).Has_Current_Value);
	QCOMPARE(stats.Goal_Progress.at(0).Current_Value, 78.0);
	QVERIFY(stats.Goal_Progress.at(0).Has_Progress_Percent);
	QCOMPARE(stats.Goal_Progress.at(0).Progress_Percent, 50.0);
	QVERIFY(!stats.Goal_Progress.at(0).Is_Achieved);
	QVERIFY(stats.Goal_Progress.at(1).Has_Current_Value);
	QCOMPARE(stats.Goal_Progress.at(1).Current_Value, 105.0);
	QCOMPARE(stats.Goal_Progress.at(1).Progress_Percent, 50.0);
	QVERIFY(!stats.Goal_Achieved);
}
//-------------------------------------------------------------------------------------------------------------
void AProgress_Calculator_Test::programStatsRulesUseRuleEngine()
{
	AsProgress_Calculator calculator;
	SProgress_Facts facts = Make_Facts();
	SProgram_Stats stats;

	QVERIFY(calculator.Compute_Program_Stats(facts, stats));
	QVERIFY(stats.Has_Rule_Adherence_Percent);
	QCOMPARE(stats.Rule_Adherence_Percent, 400.0 / 6.0);
	QCOMPARE(stats.Rule_Stats.size(), 2);

	QCOMPARE(stats.Rule_Stats.at(0).Rule_Id, QString("r_protein"));
	QCOMPARE(stats.Rule_Stats.at(0).Stats.Done_Count, 2);
	QCOMPARE(stats.Rule_Stats.at(0).Stats.Failed_Count, 1);
	QCOMPARE(stats.Rule_Stats.at(0).Stats.Adherence_Percent, 200.0 / 3.0);

	// Расчётное правило RPE проверяет себя по фактам тренировок (§3.6).
	QCOMPARE(stats.Rule_Stats.at(1).Rule_Id, QString("r_rpe"));
	QCOMPARE(stats.Rule_Stats.at(1).Stats.Done_Count, 2);
	QCOMPARE(stats.Rule_Stats.at(1).Stats.Failed_Count, 1);
	QCOMPARE(stats.Rule_Stats.at(1).Stats.Adherence_Percent, 200.0 / 3.0);

	QCOMPARE(stats.Rpe_Violations, 1);

	QVERIFY(stats.Has_Average_Protein_Grams_Per_Day);
	QCOMPARE(stats.Average_Protein_Grams_Per_Day, 430.0 / 3.0);
	QVERIFY(stats.Has_Average_Protein_Per_Kg);
	QCOMPARE(stats.Average_Protein_Per_Kg, (430.0 / 3.0) / 78.0);
}
//-------------------------------------------------------------------------------------------------------------
void AProgress_Calculator_Test::lifetimeStatsAggregateAllPrograms()
{
	AsProgress_Calculator calculator;
	SProgress_Facts program_facts = Make_Facts();
	SLifetime_Facts lifetime;
	QVector<SProgress_Facts> programs;
	SLifetime_Stats stats;

	lifetime.User_Id = "u1";
	lifetime.Programs.append(Make_Program());
	lifetime.Sessions = program_facts.Sessions;
	lifetime.Sets = program_facts.Sets;
	lifetime.Measurements = program_facts.Measurements;
	lifetime.Today = "2026-10-19";
	programs.append(program_facts);

	QVERIFY(calculator.Compute_Lifetime_Stats(lifetime, programs, stats));
	QCOMPARE(stats.User_Id, QString("u1"));
	QCOMPARE(stats.Total_Programs, 1);
	QCOMPARE(stats.Completed_Programs, 0);
	QCOMPARE(stats.Cancelled_Programs, 0);

	// Соло-сессия вне окна программы (2026-11-01) входит в «всё время» (§4.18).
	QCOMPARE(stats.Total_Sessions, 9);
	QCOMPARE(stats.Total_Sets, 6);
	QCOMPARE(stats.Total_Reps, 40);
	QCOMPARE(stats.Lifetime_Tonnage_Kg, 3250.0);
	QCOMPARE(stats.Lifetime_Duration_Minutes, 235.0);
	QCOMPARE(stats.First_Session_Date, QString("2026-09-28"));
	QCOMPARE(stats.Last_Session_Date, QString("2026-11-01"));
	QCOMPARE(stats.Active_Days_Count, 9);

	QCOMPARE(stats.Regularity.size(), 5);
	QCOMPARE(stats.Weeks_With_Sessions, 4);
	QCOMPARE(stats.Longest_Streak_Weeks, 3);
	QCOMPARE(stats.Current_Streak_Weeks, 1);
	QVERIFY(stats.Has_Average_Sessions_Per_Week);
	QCOMPARE(stats.Average_Sessions_Per_Week, 9.0 / 5.0);

	QCOMPARE(stats.Records.size(), 2);
	QCOMPARE(stats.Records.at(0).Exercise_Id, QString("squat"));
	QCOMPARE(stats.Records.at(0).E1rm_Kg, 105.0 * (1 + 5.0 / 30.0));
	QCOMPARE(stats.Records.at(1).Exercise_Id, QString("bench"));

	QVERIFY(stats.Has_Body_Weight_Total_Delta_Kg);
	QCOMPARE(stats.Body_Weight_Total_Delta_Kg, -2.0);

	QCOMPARE(stats.Summaries.size(), 1);
	QCOMPARE(stats.Summaries.at(0).Program_Id, QString("p1"));
	QCOMPARE(stats.Summaries.at(0).Sessions, 8);
	QCOMPARE(stats.Summaries.at(0).Tonnage_Kg, 3250.0);
	QVERIFY(stats.Summaries.at(0).Has_Adherence_Percent);
	QVERIFY(stats.Summaries.at(0).Has_Key_Lift_Delta_Kg);
	QCOMPARE(stats.Summaries.at(0).Key_Lift_Delta_Kg, 105.0 * (1 + 5.0 / 30.0) - 100.0 * (1 + 5.0 / 30.0));
	QCOMPARE(stats.Best_Program_Id, QString("p1"));
}
//-------------------------------------------------------------------------------------------------------------
void AProgress_Calculator_Test::encodeProducesCachePayload()
{
	AsProgress_Calculator calculator;
	SProgress_Facts facts = Make_Facts();
	SProgram_Stats stats;
	QString program_json;
	QString lifetime_json;
	SLifetime_Facts lifetime;
	QVector<SProgress_Facts> programs;
	SLifetime_Stats lifetime_stats;

	QVERIFY(calculator.Compute_Program_Stats(facts, stats));
	QVERIFY(calculator.Encode_Program_Stats(stats, program_json));

	const QJsonDocument document = QJsonDocument::fromJson(program_json.toUtf8());
	const QJsonObject object = document.object();

	QCOMPARE(object.value("programId").toString(), QString("p1"));
	QCOMPARE(object.value("programRevision").toInt(), 3);
	QCOMPARE(object.value("completedSessions").toInt(), 8);
	QCOMPARE(object.value("plannedSessions").toInt(), 12);
	QVERIFY(object.value("adherencePercent").isDouble());
	QCOMPARE(object.value("tonnageKg").toDouble(), 3250.0);
	QCOMPARE(object.value("tonnageByWeek").toArray().size(), 4);
	QCOMPARE(object.value("weekAdherence").toArray().size(), 4);
	QCOMPARE(object.value("exerciseStrength").toArray().size(), 2);
	QCOMPARE(object.value("goalProgress").toArray().size(), 2);
	QCOMPARE(object.value("measurementDeltas").toArray().size(), 5);
	QCOMPARE(object.value("ruleStats").toArray().size(), 2);

	// Метрика без данных кодируется как null, а не ноль (§5.5).
	lifetime.User_Id = "u1";
	lifetime.Programs.append(facts.Program);
	lifetime.Sessions = facts.Sessions;
	lifetime.Sets = facts.Sets;
	lifetime.Measurements = facts.Measurements;
	lifetime.Today = facts.Today;
	programs.append(facts);

	QVERIFY(calculator.Compute_Lifetime_Stats(lifetime, programs, lifetime_stats));
	QVERIFY(calculator.Encode_Lifetime_Stats(lifetime_stats, lifetime_json));

	const QJsonObject lifetime_object = QJsonDocument::fromJson(lifetime_json.toUtf8()).object();

	QCOMPARE(lifetime_object.value("userId").toString(), QString("u1"));
	// Соло-сессия 2026-11-01 вне окна программы входит в «всё время» (§4.18).
	QCOMPARE(lifetime_object.value("totalSessions").toInt(), 9);
	QCOMPARE(lifetime_object.value("records").toArray().size(), 2);
	// Неделя соло-сессии тоже попадает в heatmap регулярности (§5.3).
	QCOMPARE(lifetime_object.value("regularity").toArray().size(), 5);
	QCOMPARE(lifetime_object.value("programSummaries").toArray().size(), 1);
}
//-------------------------------------------------------------------------------------------------------------
void AProgress_Calculator_Test::incrementalEqualsFullRecalculation()
{
	AsProgress_Calculator calculator;
	SProgress_Facts facts = Make_Facts();
	SProgram_Stats incremental;
	SProgram_Stats full;
	SSet_Log new_set;
	SWorkout_Session new_session;

	QVERIFY(calculator.Compute_Program_Stats(facts, incremental));

	// Первая неделя остаётся прежней, третья получает новую завершённую
	// сессию — обычная запись факта (§5.4).
	new_session = Make_Session("s11", "2026-10-13", 1800, EDeviation::AsPlanned);
	facts.Sessions.append(new_session);
	new_set = Make_Set("s11", "squat", 107.5, 3, 7.5, true, false);
	facts.Sets.append(new_set);

	QVERIFY(calculator.Recompute_Week(facts, 3, incremental));
	QVERIFY(calculator.Compute_Program_Stats(facts, full));

	QCOMPARE(incremental.Program_Id, full.Program_Id);
	QCOMPARE(incremental.Completed_Sessions, full.Completed_Sessions);
	QCOMPARE(incremental.Planned_Sessions, full.Planned_Sessions);
	QCOMPARE(incremental.Tonnage_Kg, full.Tonnage_Kg);
	QCOMPARE(incremental.Sets_Count, full.Sets_Count);
	QCOMPARE(incremental.Reps_Count, full.Reps_Count);
	QCOMPARE(incremental.Duration_Minutes, full.Duration_Minutes);
	QCOMPARE(incremental.Average_Rpe, full.Average_Rpe);
	QCOMPARE(incremental.Max_Rpe, full.Max_Rpe);
	QCOMPARE(incremental.Adherence_Percent, full.Adherence_Percent);
	QCOMPARE(incremental.Missed_Sessions, full.Missed_Sessions);
	QCOMPARE(incremental.Current_Streak_Weeks, full.Current_Streak_Weeks);
	QCOMPARE(incremental.Weeks.size(), full.Weeks.size());
	QCOMPARE(incremental.Exercise_Strength.size(), full.Exercise_Strength.size());
	QCOMPARE(incremental.Goal_Progress.size(), full.Goal_Progress.size());

	for (int week = 0; week < full.Weeks.size(); ++week)
	{
		QCOMPARE(incremental.Weeks.at(week).Completed_Sessions, full.Weeks.at(week).Completed_Sessions);
		QCOMPARE(incremental.Weeks.at(week).Tonnage_Kg, full.Weeks.at(week).Tonnage_Kg);
	}
}
//-------------------------------------------------------------------------------------------------------------
void AProgress_Calculator_Test::rejectsInvalidInput()
{
	AsProgress_Calculator calculator;
	SProgress_Facts facts = Make_Facts();
	SProgram_Stats stats;
	SLifetime_Facts lifetime;
	SLifetime_Stats lifetime_stats;
	QString json;

	facts.Program.ID.clear();
	QVERIFY(!calculator.Compute_Program_Stats(facts, stats));
	QVERIFY(!calculator.Get_Last_Error().isEmpty());

	facts = Make_Facts();
	facts.Program.Term.Start_Date = "не-дата";
	QVERIFY(!calculator.Compute_Program_Stats(facts, stats));
	QVERIFY(!calculator.Get_Last_Error().isEmpty());

	// Инкрементальный пересчёт чужой или пустой статистики отвергается.
	facts = Make_Facts();
	SProgram_Stats other;
	other.Program_Id = "p_other";
	QVERIFY(!calculator.Recompute_Week(facts, 1, other));
	QVERIFY(!calculator.Recompute_Week(facts, 99, stats));
	QVERIFY(!calculator.Get_Last_Error().isEmpty());

	QVERIFY(!calculator.Compute_Lifetime_Stats(lifetime, QVector<SProgress_Facts>(), lifetime_stats));
	QVERIFY(!calculator.Encode_Program_Stats(stats, json));
}
//-------------------------------------------------------------------------------------------------------------
// Границы окна программы: обе включительные, а плановый конец срока обрезает
// окно. Рефакторинг расчёта окна не должен изменить эти правила (§5.5).
//-------------------------------------------------------------------------------------------------------------
void AProgress_Calculator_Test::windowBoundariesAreInclusiveAndTermLimits()
{
	AsProgress_Calculator calculator;
	SProgress_Facts facts = Make_Facts();
	SProgram_Stats stats;

	// Сессия ровно на дату старта и ровно на «сегодня» входят в окно.
	facts.Sessions.clear();
	facts.Sets.clear();
	facts.Sessions.append(Make_Session("edge_start", "2026-09-28", 1800, EDeviation::AsPlanned));
	facts.Sessions.append(Make_Session("edge_end", "2026-10-19", 1800, EDeviation::AsPlanned));
	facts.Sessions.append(Make_Session("edge_next", "2026-10-20", 1800, EDeviation::AsPlanned));

	QVERIFY(calculator.Compute_Program_Stats(facts, stats));
	QCOMPARE(stats.Completed_Sessions, 2);
	QCOMPARE(stats.Data_Range_From, QString("2026-09-28"));
	QCOMPARE(stats.Data_Range_To, QString("2026-10-19"));

	// Срок 2 недели обрезает окно: тренировка 19.10 (после конца срока 11.10)
	// не входит, а массив недель содержит ровно 2 среза.
	facts.Sessions.clear();
	facts.Program.Term.Weeks_Total = 2;
	facts.Sessions.append(Make_Session("w1", "2026-09-28", 1800, EDeviation::AsPlanned));
	facts.Sessions.append(Make_Session("w2", "2026-10-05", 1800, EDeviation::AsPlanned));
	facts.Sessions.append(Make_Session("after_term", "2026-10-19", 1800, EDeviation::AsPlanned));

	QVERIFY(calculator.Compute_Program_Stats(facts, stats));
	QCOMPARE(stats.Weeks.size(), 2);
	QCOMPARE(stats.Completed_Sessions, 2);
}
//-------------------------------------------------------------------------------------------------------------
// «Первое» и «последнее» значение находятся по датам, а не по порядку во
// входных данных: факты приходят из БД без гарантии сортировки.
//-------------------------------------------------------------------------------------------------------------
void AProgress_Calculator_Test::extremesAreResolvedByDateNotByInputOrder()
{
	AsProgress_Calculator calculator;
	SProgress_Facts facts = Make_Facts();
	SProgram_Stats stats;
	SBody_Measurement measurement;

	facts.Sessions.clear();
	facts.Sets.clear();

	// Сессии поданы в обратном хронологическом порядке.
	facts.Sessions.append(Make_Session("late", "2026-10-17", 1800, EDeviation::AsPlanned));
	facts.Sessions.append(Make_Session("early", "2026-09-28", 1800, EDeviation::AsPlanned));

	// Замеры тела тоже в обратном порядке.
	facts.Measurements.clear();
	measurement = SBody_Measurement();
	measurement.User_Id = "u1";
	measurement.Date = "2026-10-17";
	measurement.Weight_Kg = 78;
	measurement.Has_Weight_Kg = true;
	facts.Measurements.append(measurement);

	measurement.Date = "2026-09-28";
	measurement.Weight_Kg = 80;
	facts.Measurements.append(measurement);

	QVERIFY(calculator.Compute_Program_Stats(facts, stats));
	QCOMPARE(stats.Data_Range_From, QString("2026-09-28"));
	QCOMPARE(stats.Data_Range_To, QString("2026-10-17"));
	QVERIFY(stats.Has_Body_Weight_Start_Kg);
	QVERIFY(stats.Has_Body_Weight_Latest_Kg);
	QCOMPARE(stats.Body_Weight_Start_Kg, 80.0);
	QCOMPARE(stats.Body_Weight_Latest_Kg, 78.0);
	QCOMPARE(stats.Body_Weight_Delta_Kg, -2.0);
}
//-------------------------------------------------------------------------------------------------------------
// Замеры вне окна программы не участвуют ни в старте, ни в последнем значении
// веса тела, ни в дельтах обхватов (§5.5 — окно программы).
//-------------------------------------------------------------------------------------------------------------
void AProgress_Calculator_Test::measurementsOutsideWindowAreIgnored()
{
	AsProgress_Calculator calculator;
	SProgress_Facts facts = Make_Facts();
	SProgram_Stats stats;
	SBody_Measurement outside;

	QVERIFY(calculator.Compute_Program_Stats(facts, stats));
	QCOMPARE(stats.Body_Weight_Start_Kg, 80.0);
	QCOMPARE(stats.Body_Weight_Latest_Kg, 78.0);

	// Замер после конца окна не должен стать «последним».
	outside = SBody_Measurement();
	outside.User_Id = "u1";
	outside.Date = "2026-12-01";
	outside.Weight_Kg = 95;
	outside.Has_Weight_Kg = true;
	outside.Waist_Cm = 100;
	outside.Has_Waist_Cm = true;
	facts.Measurements.append(outside);

	QVERIFY(calculator.Compute_Program_Stats(facts, stats));
	QCOMPARE(stats.Body_Weight_Latest_Kg, 78.0);
	QVERIFY(!stats.Measurement_Deltas.at(1).Has_Delta_Cm);
}
//-------------------------------------------------------------------------------------------------------------
// Черновики сессий и разминочные подходы не попадают ни в программу, ни в
// «всё время» (§3.8: фактом считается только завершённая рабочая работа).
//-------------------------------------------------------------------------------------------------------------
void AProgress_Calculator_Test::draftSessionsAndWarmupSetsNeverCount()
{
	AsProgress_Calculator calculator;
	SProgress_Facts facts = Make_Facts();
	SProgram_Stats stats;
	SLifetime_Facts lifetime;
	SLifetime_Stats lifetime_stats;
	SWorkout_Session draft;
	SSet_Log warmup_only;

	// Единственная сессия недели — черновик с одним разминочным подходом.
	facts.Sessions.clear();
	facts.Sets.clear();
	draft = Make_Session("draft", "2026-09-28", 1800, EDeviation::AsPlanned);
	draft.State = EWorkout_State::Draft;
	facts.Sessions.append(draft);
	warmup_only = Make_Set("draft", "squat", 60, 10, 6, true, true);
	facts.Sets.append(warmup_only);

	QVERIFY(calculator.Compute_Program_Stats(facts, stats));
	QCOMPARE(stats.Completed_Sessions, 0);
	QCOMPARE(stats.Sets_Count, 0);
	QCOMPARE(stats.Tonnage_Kg, 0.0);
	QVERIFY(stats.Exercise_Strength.isEmpty());

	lifetime.User_Id = "u1";
	lifetime.Programs.append(facts.Program);
	lifetime.Sessions = facts.Sessions;
	lifetime.Sets = facts.Sets;
	lifetime.Today = facts.Today;

	QVERIFY(calculator.Compute_Lifetime_Stats(lifetime, QVector<SProgress_Facts>(), lifetime_stats));
	QCOMPARE(lifetime_stats.Total_Sessions, 0);
	QCOMPARE(lifetime_stats.Total_Sets, 0);
	QVERIFY(lifetime_stats.Records.isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
// Метрика с данными, равными нулю, кодируется числом, а не null: «измерили и
// получили 0» отличается от «нечего считать» (§5.5).
//-------------------------------------------------------------------------------------------------------------
void AProgress_Calculator_Test::encodeKeepsZeroWhenDataExists()
{
	AsProgress_Calculator calculator;
	SProgram_Stats stats;
	QString json;
	QJsonObject zero_object;

	stats.Program_Id = "p_zero";
	stats.Program_Revision = 1;
	stats.Adherence_Percent = 0;
	stats.Has_Adherence_Percent = true;
	stats.Average_Rpe = 0;
	stats.Has_Average_Rpe = false;
	stats.Tonnage_Kg = 0;

	QVERIFY(calculator.Encode_Program_Stats(stats, json));
	zero_object = QJsonDocument::fromJson(json.toUtf8()).object();

	QCOMPARE(zero_object.value("adherencePercent").toDouble(), 0.0);
	QVERIFY(zero_object.value("adherencePercent").isDouble());
	// Метрика без данных остаётся null.
	QVERIFY(zero_object.value("avgRpe").isNull());
}
//-------------------------------------------------------------------------------------------------------------

QTEST_GUILESS_MAIN(AProgress_Calculator_Test)
#include "tst_Progress_Calculator.moc"
//-------------------------------------------------------------------------------------------------------------
