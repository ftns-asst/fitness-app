#include "Domain/Program_Model.h"
#include "Domain/Rule_Engine.h"

#include <QtTest>

class ARule_Engine_Test : public QObject
{
	Q_OBJECT

private slots:
	void evaluateValueHonoursTargetRange();
	void evaluateCheckinPreservesExplicitStates();
	void computedRpeCheckinUsesWorkingSets();
	void computedProgressionCheckinCountsTargetReps();
	void computedRestCheckinUsesEarlierSession();
	void buildComputedCheckinRejectsManualRules();
	void ruleStatsAggregateStatesAndAdherence();
	void overallAdherenceAggregatesRules();
	void ruleActiveWindow();

	// — Рекомендация в движке —
	// У рекомендации нет цели, поэтому Evaluate_Value обязан сказать об этом
	// прямо, а не вернуть «готово» для любого числа. И её отметки не должны
	// портить adherence: пользователь отмечал совет, а процент выполнения
	// программы от этого меняться не должен.
	void adviceRuleHasNoVerdictByValue();
	void adviceCheckinsDoNotAffectAdherence();

private:
	SProgram_Rule Make_Rule(const QString &in_id);
	SProgram_Rule Make_Advice_Rule(const QString &in_id, const QString &in_text);
	SWorkout_Session Make_Session(const QString &in_id, const QString &in_started_at);
	SSet_Log Make_Set(const QString &in_session_id, int in_number, double in_rpe, bool in_warmup, bool in_done);
	SRule_Checkin Make_Checkin(const QString &in_rule_id, const QString &in_date, double in_value, bool in_has_value);
};

//-------------------------------------------------------------------------------------------------------------
SProgram_Rule ARule_Engine_Test::Make_Rule(const QString &in_id)
{
	SProgram_Rule rule;

	rule.ID = in_id;
	rule.Program_Id = "p1";
	rule.Category = ERule_Category::Training;
	rule.Code = "custom";
	rule.Title = "Правило";
	rule.Metric = ERule_Metric::Custom;
	rule.Periodicity = ERule_Periodicity::PerDay;
	rule.Applies_To = ERule_Applies_To::AllDays;
	rule.Check_Source = ERule_Check_Source::SelfReport;
	rule.Enforcement = ERule_Enforcement::Soft;
	rule.Display_Unit = ERule_Display_Unit::Count;

	return rule;
}
//-------------------------------------------------------------------------------------------------------------
// Рекомендация отличается от обычного правила по сути, а не по значению:
// цели у неё нет, метрика custom, а текст лежит в Note. Заводится отдельно от
// Make_Rule, чтобы разница читалась, а не собиралась в тесте вручную.
SProgram_Rule ARule_Engine_Test::Make_Advice_Rule(const QString &in_id, const QString &in_text)
{
	SProgram_Rule rule = Make_Rule(in_id);

	rule.Category = ERule_Category::Other;
	rule.Metric = ERule_Metric::Custom;
	rule.Target_Unit = ERule_Display_Unit::Units;
	rule.Display_Unit = ERule_Display_Unit::Units;
	rule.Note = in_text;

	return rule;
}
//-------------------------------------------------------------------------------------------------------------
SWorkout_Session ARule_Engine_Test::Make_Session(const QString &in_id, const QString &in_started_at)
{
	SWorkout_Session session;

	session.ID = in_id;
	session.User_Id = "u1";
	session.Program_Id = "p1";
	session.Date = in_started_at.left(10);
	session.Started_At = in_started_at;
	session.Completed_At = in_started_at;
	session.State = EWorkout_State::Completed;

	return session;
}
//-------------------------------------------------------------------------------------------------------------
SSet_Log ARule_Engine_Test::Make_Set(const QString &in_session_id, int in_number, double in_rpe, bool in_warmup,
                                     bool in_done)
{
	SSet_Log set;

	set.ID = QString("set_%1_%2").arg(in_session_id).arg(in_number);
	set.Session_Id = in_session_id;
	set.Set_Number = in_number;
	set.Weight_Kg = 80;
	set.Reps = 5;
	set.Rpe = in_rpe;
	set.Has_Rpe = in_rpe >= 0;
	set.Is_Warmup = in_warmup;
	set.Done = in_done;

	return set;
}
//-------------------------------------------------------------------------------------------------------------
SRule_Checkin ARule_Engine_Test::Make_Checkin(const QString &in_rule_id, const QString &in_date, double in_value,
                                              bool in_has_value)
{
	SRule_Checkin checkin;

	checkin.ID = QString("c_%1_%2").arg(in_rule_id).arg(in_date);
	checkin.Rule_Id = in_rule_id;
	checkin.Program_Id = "p1";
	checkin.Date = in_date;
	checkin.Value = in_value;
	checkin.Has_Value = in_has_value;
	checkin.Unit = "g";

	return checkin;
}
//-------------------------------------------------------------------------------------------------------------
void ARule_Engine_Test::evaluateValueHonoursTargetRange()
{
	AsRule_Engine engine;
	SProgram_Rule rule = Make_Rule("r1");
	ECheckin_State state = ECheckin_State::Unknown;

	rule.Metric = ERule_Metric::SleepHours;
	rule.Target_Min = 7;
	rule.Has_Target_Min = true;
	rule.Target_Max = 9;
	rule.Has_Target_Max = true;

	QVERIFY(engine.Evaluate_Value(rule, 8, state));
	QCOMPARE(state, ECheckin_State::Done);
	QVERIFY(engine.Evaluate_Value(rule, 7, state));
	QCOMPARE(state, ECheckin_State::Done);
	QVERIFY(engine.Evaluate_Value(rule, 6.5, state));
	QCOMPARE(state, ECheckin_State::Failed);
	QVERIFY(engine.Evaluate_Value(rule, 9.5, state));
	QCOMPARE(state, ECheckin_State::Failed);

	rule.Has_Target_Max = false;
	QVERIFY(engine.Evaluate_Value(rule, 7.5, state));
	QCOMPARE(state, ECheckin_State::Done);
	QVERIFY(engine.Evaluate_Value(rule, 6.9, state));
	QCOMPARE(state, ECheckin_State::Failed);

	rule.Has_Target_Min = false;
	rule.Target_Max = 8;
	rule.Has_Target_Max = true;
	QVERIFY(engine.Evaluate_Value(rule, 8, state));
	QCOMPARE(state, ECheckin_State::Done);
	QVERIFY(engine.Evaluate_Value(rule, 8.5, state));
	QCOMPARE(state, ECheckin_State::Failed);

	rule.Has_Target_Max = false;
	QVERIFY(!engine.Evaluate_Value(rule, 8, state));
	QVERIFY(!engine.Get_Last_Error().isEmpty());

	rule.Target_Exact = 78;
	rule.Has_Target_Exact = true;
	QVERIFY(engine.Evaluate_Value(rule, 78, state));
	QCOMPARE(state, ECheckin_State::Done);
	QVERIFY(engine.Evaluate_Value(rule, 78.5, state));
	QCOMPARE(state, ECheckin_State::Done);
	QVERIFY(engine.Evaluate_Value(rule, 79, state));
	QCOMPARE(state, ECheckin_State::Failed);

	rule.Has_Target_Exact = false;
	rule.Target_Min = 9;
	rule.Has_Target_Min = true;
	rule.Target_Max = 8;
	rule.Has_Target_Max = true;
	QVERIFY(!engine.Evaluate_Value(rule, 8.5, state));
}
//-------------------------------------------------------------------------------------------------------------
void ARule_Engine_Test::evaluateCheckinPreservesExplicitStates()
{
	AsRule_Engine engine;
	SProgram_Rule rule = Make_Rule("r1");
	SRule_Checkin checkin;
	ECheckin_State state = ECheckin_State::Unknown;

	rule.Metric = ERule_Metric::ProteinGrams;
	rule.Target_Min = 145;
	rule.Has_Target_Min = true;

	checkin.Rule_Id = "r1";
	checkin.State = ECheckin_State::Skipped;
	QVERIFY(engine.Evaluate_Checkin(rule, checkin, state));
	QCOMPARE(state, ECheckin_State::Skipped);

	checkin.State = ECheckin_State::Partial;
	QVERIFY(engine.Evaluate_Checkin(rule, checkin, state));
	QCOMPARE(state, ECheckin_State::Partial);

	checkin.State = ECheckin_State::Unknown;
	QVERIFY(engine.Evaluate_Checkin(rule, checkin, state));
	QCOMPARE(state, ECheckin_State::Unknown);

	checkin.Value = 150;
	checkin.Has_Value = true;
	QVERIFY(engine.Evaluate_Checkin(rule, checkin, state));
	QCOMPARE(state, ECheckin_State::Done);

	checkin.Value = 100;
	QVERIFY(engine.Evaluate_Checkin(rule, checkin, state));
	QCOMPARE(state, ECheckin_State::Failed);

	SRule_Checkin marked;
	marked.Rule_Id = "r1";
	marked.State = ECheckin_State::Failed;
	QVERIFY(engine.Evaluate_Checkin(rule, marked, state));
	QCOMPARE(state, ECheckin_State::Failed);
}
//-------------------------------------------------------------------------------------------------------------
void ARule_Engine_Test::computedRpeCheckinUsesWorkingSets()
{
	AsRule_Engine engine;
	SProgram_Rule rule = Make_Rule("rpe");
	SWorkout_Session session = Make_Session("s1", "2026-10-01T18:00:00Z");
	QVector<SSet_Log> sets;
	SRule_Checkin checkin;

	rule.Metric = ERule_Metric::MaxRpe;
	rule.Periodicity = ERule_Periodicity::PerWorkout;
	rule.Check_Source = ERule_Check_Source::ComputedFromSets;
	rule.Display_Unit = ERule_Display_Unit::Rpe;
	rule.Target_Max = 8;
	rule.Has_Target_Max = true;

	sets.append(Make_Set("s1", 1, 7.5, false, true));
	sets.append(Make_Set("s1", 2, 8.5, false, true));
	sets.append(Make_Set("s1", 3, 10, true, true));
	sets.append(Make_Set("s1", 4, 9, false, false));

	QVERIFY(engine.Build_Computed_Checkin(rule, session, sets, QVector<SWorkout_Session>(), checkin));
	QCOMPARE(checkin.Rule_Id, QString("rpe"));
	QCOMPARE(checkin.Program_Id, QString("p1"));
	QCOMPARE(checkin.Date, QString("2026-10-01"));
	QCOMPARE(checkin.Unit, QString("rpe"));
	QCOMPARE(checkin.Source, ECheckin_Source::Computed);
	QCOMPARE(checkin.Session_Id, QString("s1"));
	QVERIFY(checkin.Has_Value);
	QCOMPARE(checkin.Value, 8.5);
	QCOMPARE(checkin.State, ECheckin_State::Failed);

	rule.Target_Max = 9;
	QVERIFY(engine.Build_Computed_Checkin(rule, session, sets, QVector<SWorkout_Session>(), checkin));
	QCOMPARE(checkin.State, ECheckin_State::Done);

	QVector<SSet_Log> no_rpe;
	no_rpe.append(Make_Set("s1", 1, -1, false, true));
	QVERIFY(engine.Build_Computed_Checkin(rule, session, no_rpe, QVector<SWorkout_Session>(), checkin));
	QVERIFY(!checkin.Has_Value);
	QCOMPARE(checkin.State, ECheckin_State::Unknown);
}
//-------------------------------------------------------------------------------------------------------------
void ARule_Engine_Test::computedProgressionCheckinCountsTargetReps()
{
	AsRule_Engine engine;
	SProgram_Rule rule = Make_Rule("prog");
	SWorkout_Session session = Make_Session("s1", "2026-10-01T18:00:00Z");
	QVector<SSet_Log> sets;
	SSet_Log set;
	SRule_Checkin checkin;

	rule.Metric = ERule_Metric::Custom;
	rule.Periodicity = ERule_Periodicity::PerWorkout;
	rule.Check_Source = ERule_Check_Source::ComputedFromSets;
	rule.Display_Unit = ERule_Display_Unit::Count;
	rule.Target_Min = 100;
	rule.Has_Target_Min = true;

	set = Make_Set("s1", 1, -1, false, true);
	set.Target_Reps = 5;
	set.Has_Target_Reps = true;
	sets.append(set);

	set = Make_Set("s1", 2, -1, false, true);
	set.Reps = 4;
	set.Target_Reps = 5;
	set.Has_Target_Reps = true;
	sets.append(set);

	set = Make_Set("s1", 3, -1, false, false);
	set.Target_Reps = 5;
	set.Has_Target_Reps = true;
	sets.append(set);

	set = Make_Set("s1", 4, -1, true, true);
	set.Target_Reps = 5;
	set.Has_Target_Reps = true;
	sets.append(set);

	QVERIFY(engine.Build_Computed_Checkin(rule, session, sets, QVector<SWorkout_Session>(), checkin));
	QVERIFY(checkin.Has_Value);
	QCOMPARE(checkin.Value, 100.0 / 3.0);
	QCOMPARE(checkin.State, ECheckin_State::Failed);

	sets[1].Reps = 5;
	sets[2].Done = true;
	QVERIFY(engine.Build_Computed_Checkin(rule, session, sets, QVector<SWorkout_Session>(), checkin));
	QCOMPARE(checkin.Value, 100.0);
	QCOMPARE(checkin.State, ECheckin_State::Done);

	QVector<SSet_Log> no_target;
	no_target.append(Make_Set("s1", 5, -1, false, true));
	QVERIFY(engine.Build_Computed_Checkin(rule, session, no_target, QVector<SWorkout_Session>(), checkin));
	QVERIFY(!checkin.Has_Value);
	QCOMPARE(checkin.State, ECheckin_State::Unknown);
}
//-------------------------------------------------------------------------------------------------------------
void ARule_Engine_Test::computedRestCheckinUsesEarlierSession()
{
	AsRule_Engine engine;
	SProgram_Rule rule = Make_Rule("rest");
	SWorkout_Session session = Make_Session("s1", "2026-10-02T18:00:00Z");
	QVector<SWorkout_Session> earlier;
	SRule_Checkin checkin;

	rule.Metric = ERule_Metric::RestHoursBetweenSessions;
	rule.Periodicity = ERule_Periodicity::PerDay;
	rule.Check_Source = ERule_Check_Source::ComputedFromSessions;
	rule.Display_Unit = ERule_Display_Unit::Hours;
	rule.Target_Min = 24;
	rule.Has_Target_Min = true;

	earlier.append(Make_Session("s0", "2026-10-01T12:00:00Z"));

	QVERIFY(engine.Build_Computed_Checkin(rule, session, QVector<SSet_Log>(), earlier, checkin));
	QCOMPARE(checkin.Unit, QString("hours"));
	QVERIFY(checkin.Has_Value);
	QCOMPARE(checkin.Value, 30.0);
	QCOMPARE(checkin.State, ECheckin_State::Done);

	earlier[0] = Make_Session("s0", "2026-10-02T10:00:00Z");
	QVERIFY(engine.Build_Computed_Checkin(rule, session, QVector<SSet_Log>(), earlier, checkin));
	QCOMPARE(checkin.Value, 8.0);
	QCOMPARE(checkin.State, ECheckin_State::Failed);

	QVERIFY(engine.Build_Computed_Checkin(rule, session, QVector<SSet_Log>(), QVector<SWorkout_Session>(), checkin));
	QVERIFY(!checkin.Has_Value);
	QCOMPARE(checkin.State, ECheckin_State::Unknown);
}
//-------------------------------------------------------------------------------------------------------------
void ARule_Engine_Test::buildComputedCheckinRejectsManualRules()
{
	AsRule_Engine engine;
	SProgram_Rule rule = Make_Rule("manual");
	SWorkout_Session session = Make_Session("s1", "2026-10-01T18:00:00Z");
	SRule_Checkin checkin;

	QVERIFY(engine.Build_Computed_Checkin(rule, session, QVector<SSet_Log>(), QVector<SWorkout_Session>(), checkin) ==
	        false);
	QVERIFY(!engine.Get_Last_Error().isEmpty());

	rule.Check_Source = ERule_Check_Source::ComputedFromSets;
	rule.Metric = ERule_Metric::MaxRpe;
	rule.Target_Max = 8;
	rule.Has_Target_Max = true;

	SWorkout_Session incomplete;
	incomplete.ID = "s2";
	QVERIFY(
	    !engine.Build_Computed_Checkin(rule, incomplete, QVector<SSet_Log>(), QVector<SWorkout_Session>(), checkin));

	rule.Metric = ERule_Metric::ProteinGrams;
	QVERIFY(engine.Build_Computed_Checkin(rule, session, QVector<SSet_Log>(), QVector<SWorkout_Session>(), checkin) ==
	        false);
}
//-------------------------------------------------------------------------------------------------------------
void ARule_Engine_Test::ruleStatsAggregateStatesAndAdherence()
{
	AsRule_Engine engine;
	SProgram_Rule rule = Make_Rule("r1");
	SProgram_Rule no_id;
	QVector<SRule_Checkin> checkins;
	SRule_Stats stats;

	rule.Metric = ERule_Metric::ProteinGrams;
	rule.Target_Min = 145;
	rule.Has_Target_Min = true;

	checkins.append(Make_Checkin("r1", "2026-10-01", 150, true));
	checkins.append(Make_Checkin("r1", "2026-10-02", 160, true));
	checkins.append(Make_Checkin("r1", "2026-10-03", 100, true));
	checkins.append(Make_Checkin("r1", "2026-10-04", 0, false));
	checkins.append(Make_Checkin("other", "2026-10-05", 150, true));

	QVERIFY(engine.Compute_Rule_Stats(rule, checkins, stats));
	QCOMPARE(stats.Done_Count, 2);
	QCOMPARE(stats.Failed_Count, 1);
	QCOMPARE(stats.Skipped_Count, 0);
	QCOMPARE(stats.Unknown_Count, 1);
	QCOMPARE(stats.Evaluated_Count, 3);
	QVERIFY(stats.Has_Average_Value);
	QCOMPARE(stats.Average_Value, 410.0 / 3.0);
	QVERIFY(stats.Has_Adherence_Percent);
	QCOMPARE(stats.Adherence_Percent, 200.0 / 3.0);

	checkins.clear();
	QVERIFY(engine.Compute_Rule_Stats(rule, checkins, stats));
	QCOMPARE(stats.Evaluated_Count, 0);
	QVERIFY(!stats.Has_Adherence_Percent);
	QVERIFY(!stats.Has_Average_Value);

	QVERIFY(!engine.Compute_Rule_Stats(no_id, checkins, stats));
}
//-------------------------------------------------------------------------------------------------------------
void ARule_Engine_Test::overallAdherenceAggregatesRules()
{
	AsRule_Engine engine;
	SProgram_Rule protein = Make_Rule("r1");
	SProgram_Rule rpe = Make_Rule("r2");
	QVector<SProgram_Rule> rules;
	QVector<SRule_Checkin> checkins;
	SRule_Stats stats;

	protein.Metric = ERule_Metric::ProteinGrams;
	protein.Target_Min = 145;
	protein.Has_Target_Min = true;
	rpe.Metric = ERule_Metric::MaxRpe;
	rpe.Target_Max = 8;
	rpe.Has_Target_Max = true;

	rules.append(protein);
	rules.append(rpe);
	checkins.append(Make_Checkin("r1", "2026-10-01", 150, true));
	checkins.append(Make_Checkin("r1", "2026-10-02", 100, true));
	checkins.append(Make_Checkin("r2", "2026-10-03", 7, true));

	QVERIFY(engine.Compute_Overall_Stats(rules, checkins, stats));
	QCOMPARE(stats.Done_Count, 2);
	QCOMPARE(stats.Failed_Count, 1);
	QCOMPARE(stats.Evaluated_Count, 3);
	QCOMPARE(stats.Adherence_Percent, 200.0 / 3.0);

	QVector<SProgram_Rule> empty;
	QVERIFY(!engine.Compute_Overall_Stats(empty, checkins, stats));
}
//-------------------------------------------------------------------------------------------------------------
void ARule_Engine_Test::ruleActiveWindow()
{
	AsRule_Engine engine;
	SProgram_Rule rule = Make_Rule("r1");

	QVERIFY(engine.Is_Rule_Active(rule, "2026-10-05"));

	rule.Active_From = "2026-10-01";
	rule.Active_To = "2026-10-07";
	QVERIFY(engine.Is_Rule_Active(rule, "2026-10-01"));
	QVERIFY(engine.Is_Rule_Active(rule, "2026-10-05"));
	QVERIFY(engine.Is_Rule_Active(rule, "2026-10-07"));
	QVERIFY(!engine.Is_Rule_Active(rule, "2026-09-30"));
	QVERIFY(!engine.Is_Rule_Active(rule, "2026-10-08"));

	QVERIFY(!engine.Is_Rule_Active(rule, "not-a-date"));
	QVERIFY(!engine.Get_Last_Error().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
// Рекомендация — не измеримое правило. Возвращать для неё «выполнено» на
// любое число значило бы молча подставить вердикт вместо отказа: вызывающий
// код решил бы, что цель достигнута, хотя цели нет.
void ARule_Engine_Test::adviceRuleHasNoVerdictByValue()
{
	AsRule_Engine engine;
	SProgram_Rule advice = Make_Advice_Rule("adv", "Пей воду натощак");
	ECheckin_State state = ECheckin_State::Failed;

	QVERIFY(!engine.Evaluate_Value(advice, 8, state));
	QVERIFY(!engine.Get_Last_Error().isEmpty());

	// С числовой целью то же правило измеримо и вердикт выдаёт — значит отказ
	// выше вызван отсутствием цели, а не самой категорией.
	advice.Target_Min = 7;
	advice.Has_Target_Min = true;
	QVERIFY(engine.Evaluate_Value(advice, 8, state));
	QCOMPARE(state, ECheckin_State::Done);
}
//-------------------------------------------------------------------------------------------------------------
// Отметка рекомендации — это «прочитал совет», а не результат измерения.
// Поэтому она не должна ни попадать в adherence, ни портить процент по
// остальным правилам: пользователь отметил текст, и цифра выполнения
// изменилась бы на глазах у человека, который ничего не забыл.
void ARule_Engine_Test::adviceCheckinsDoNotAffectAdherence()
{
	AsRule_Engine engine;
	SProgram_Rule protein = Make_Rule("r1");
	SProgram_Rule advice = Make_Advice_Rule("adv", "Ложись спать вовремя");
	QVector<SProgram_Rule> rules;
	QVector<SRule_Checkin> checkins;
	SRule_Stats stats;

	protein.Metric = ERule_Metric::ProteinGrams;
	protein.Target_Min = 145;
	protein.Has_Target_Min = true;

	rules.append(protein);
	rules.append(advice);

	// Отметка рекомендации без значения — обычный случай: что именно
	// пользователь отметил, измерить нечем.
	checkins.append(Make_Checkin("r1", "2026-10-01", 150, true));
	SRule_Checkin advice_mark = Make_Checkin("adv", "2026-10-01", 0, false);
	advice_mark.State = ECheckin_State::Done;
	checkins.append(advice_mark);

	QVERIFY(engine.Compute_Overall_Stats(rules, checkins, stats));
	QCOMPARE(stats.Evaluated_Count, 1);
	QCOMPARE(stats.Done_Count, 1);
	QCOMPARE(stats.Adherence_Percent, 100.0);

	// Отметка «не выполнено» у рекомендации тоже не должна превращаться в
	// провал программы.
	checkins[1].State = ECheckin_State::Failed;
	QVERIFY(engine.Compute_Overall_Stats(rules, checkins, stats));
	QCOMPARE(stats.Failed_Count, 0);
	QCOMPARE(stats.Adherence_Percent, 100.0);

	// Собственная сводка рекомендации пуста: оценивать нечего.
	QVERIFY(engine.Compute_Rule_Stats(advice, checkins, stats));
	QCOMPARE(stats.Evaluated_Count, 0);
	QVERIFY(!stats.Has_Adherence_Percent);

	// Прогресс сам по себе не восстанавливается: у рекомендации нет ни цели,
	// ни чисел, из которых считается среднее.
	QVERIFY(!stats.Has_Average_Value);
}

QTEST_GUILESS_MAIN(ARule_Engine_Test)
#include "tst_Rule_Engine.moc"
