#include "Domain/Strength_Calculator.h"

#include <QtTest>
#include <limits>

//-------------------------------------------------------------------------------------------------------------
class AStrength_Calculator_Test : public QObject
{
	Q_OBJECT
private slots:
	void legend_data();
	void legend();
	void summarizesOnlyValidWorkingSets();
	void missingAndZeroBaseline();
	void completedFactsAreOrderedAndSplitScoped();
	void indexExcludesStartAndMissing();
	void historicalRangeWinsOverEditedPlan();
};
//-------------------------------------------------------------------------------------------------------------
void AStrength_Calculator_Test::legend_data()
{
	QTest::addColumn<double>("weight");
	QTest::addColumn<double>("reps");
	QTest::addColumn<int>("tolerance");
	QTest::addColumn<QString>("status");
	QTest::addColumn<QString>("reason");
	QTest::newRow("weight-up-boundary") << 62.5 << 8.0 << 2 << "better" << "weightInTolerance";
	QTest::newRow("weight-up-outside") << 62.5 << 7.5 << 2 << "worse" << "repDrop";
	QTest::newRow("same-weight-more-reps") << 60.0 << 11.0 << 2 << "better" << "moreReps";
	QTest::newRow("unchanged") << 60.0 << 10.0 << 2 << "same" << "unchanged";
	QTest::newRow("above-range-priority") << 62.5 << 13.0 << 2 << "attention" << "aboveRange";
	QTest::newRow("lower-weight-more-reps") << 55.0 << 11.0 << 2 << "attention" << "lighterMoreReps";
	QTest::newRow("below-range-priority") << 62.5 << 5.0 << 20 << "worse" << "belowRange";
	QTest::newRow("same-weight-less-reps") << 60.0 << 9.0 << 2 << "worse" << "fewerReps";
	QTest::newRow("lower-weight-same-reps") << 55.0 << 10.0 << 2 << "worse" << "lessWeight";
	QTest::newRow("zero-tolerance") << 62.5 << 9.5 << 0 << "worse" << "repDrop";
	QTest::newRow("zero-tolerance-no-drop") << 62.5 << 10.0 << 0 << "better" << "weightInTolerance";
	QTest::newRow("max-tolerance") << 62.5 << 6.0 << 20 << "better" << "weightInTolerance";
}
//-------------------------------------------------------------------------------------------------------------
void AStrength_Calculator_Test::legend()
{
	QFETCH(double, weight);
	QFETCH(double, reps);
	QFETCH(int, tolerance);
	QFETCH(QString, status);
	QFETCH(QString, reason);
	SStrength_Workload previous;
	previous.Has_Data = true;
	previous.Weight_Kg = 60;
	previous.Average_Reps = 10;
	SStrength_Workload current = previous;
	current.Weight_Kg = weight;
	current.Average_Reps = reps;
	SExercise_Trend trend = AStrength_Calculator::Evaluate_Exercise_Trend(previous, current, 6, 12, tolerance);
	QCOMPARE(trend.Status, status);
	QCOMPARE(trend.Reason, reason);
	QVERIFY(trend.Is_Evaluated);
	QCOMPARE(trend.Delta_Kg, weight - 60);
}
//-------------------------------------------------------------------------------------------------------------
void AStrength_Calculator_Test::summarizesOnlyValidWorkingSets()
{
	SSet_Log set;
	set.Done = true;
	set.Weight_Kg = 60;
	set.Reps = 10;
	QVector<SSet_Log> sets;
	sets << set;
	set.Weight_Kg = 55;
	set.Reps = 8;
	sets << set;
	set.Is_Warmup = true;
	set.Weight_Kg = 100;
	sets << set;
	set.Is_Warmup = false;
	set.Done = false;
	sets << set;
	set.Done = true;
	set.Weight_Kg = std::numeric_limits<double>::quiet_NaN();
	sets << set;
	set.Weight_Kg = -1;
	sets << set;
	set.Weight_Kg = 80;
	set.Reps = 0;
	sets << set;
	SStrength_Workload result = AStrength_Calculator::Summarize(sets);
	QVERIFY(result.Has_Data);
	QCOMPARE(result.Weight_Kg, 60.0);
	QCOMPARE(result.Average_Reps, 9.0);
	QCOMPARE(result.Set_Count, 2);
}
//-------------------------------------------------------------------------------------------------------------
void AStrength_Calculator_Test::missingAndZeroBaseline()
{
	SStrength_Workload missing;
	SStrength_Workload current;
	current.Has_Data = true;
	current.Weight_Kg = 0;
	current.Average_Reps = 10;
	SExercise_Trend trend = AStrength_Calculator::Evaluate_Exercise_Trend(missing, current, 6, 12, 2);
	QCOMPARE(trend.Status, QString("start"));
	QVERIFY(!trend.Is_Evaluated);
	QVERIFY(!trend.Has_Delta);
	QCOMPARE(AStrength_Calculator::Evaluate_Exercise_Trend(current, missing, 6, 12, 2).Status, QString("noData"));
	SStrength_Workload next = current;
	next.Weight_Kg = 5;
	trend = AStrength_Calculator::Evaluate_Exercise_Trend(current, next, 6, 12, 2);
	QCOMPARE(trend.Delta_Kg, 5.0);
	QVERIFY(trend.Has_Delta);
	QVERIFY(!trend.Has_Delta_Percent);
	QCOMPARE(AStrength_Calculator::Evaluate_Exercise_Trend(current, next, 6, 12, -1).Status, QString("noData"));
	QCOMPARE(AStrength_Calculator::Evaluate_Exercise_Trend(current, next, 6, 12, 21).Status, QString("noData"));
}
//-------------------------------------------------------------------------------------------------------------
void AStrength_Calculator_Test::completedFactsAreOrderedAndSplitScoped()
{
	SProgram program;
	program.ID = "p";
	program.User_Id = "u";
	SProgram_Day upper;
	upper.ID = "upper";
	upper.Title = "Upper";
	SProgram_Day lower;
	lower.ID = "lower";
	SDay_Exercise prescription;
	prescription.ID = "de";
	prescription.Program_Day_Id = "upper";
	prescription.Exercise_Id = "squat";
	prescription.Reps_Min = 6;
	prescription.Reps_Max = 12;
	QVector<SWorkout_Session> sessions;
	QVector<SSet_Log> sets;
	for (int i = 3; i >= 0; --i)
	{
		SWorkout_Session session;
		session.ID = QString::number(i);
		session.User_Id = "u";
		session.Program_Id = "p";
		session.Program_Day_Id = i == 2 ? "lower" : "upper";
		session.Date = QString("2026-10-0%1").arg(i + 1);
		session.State = i == 3 ? EWorkout_State::Draft : EWorkout_State::Completed;
		sessions << session;
		SSet_Log set;
		set.Session_Id = session.ID;
		set.Exercise_Id = "squat";
		set.Done = true;
		set.Weight_Kg = 60 + 5 * i;
		set.Reps = 10;
		sets << set;
	}
	SStrength_Report report = AStrength_Calculator::Compute(program, {upper, lower}, {prescription}, sessions, sets);
	QCOMPARE(report.Exercises.size(), 2);
	QCOMPARE(report.History.size(), 3);
	QCOMPARE(report.History.at(0).Date, QString("2026-10-01"));
	QCOMPARE(report.Exercises.at(0).Current.Weight_Kg, 65.0);
	QCOMPARE(report.Exercises.at(0).Start.Weight_Kg, 60.0);
	QCOMPARE(report.Exercises.at(0).Previous.Weight_Kg, 60.0);
	QCOMPARE(report.Exercises.at(0).Index.Better, 1);
	QCOMPARE(report.Index.Evaluated, 1);
	QCOMPARE(report.Index.Percent, 100.0);
	QCOMPARE(report.Exercises.at(1).Trend.Status, QString("start"));
	// The caller's input remains untouched; repeated computation is identical.
	QCOMPARE(sessions.at(0).ID, QString("3"));
	QCOMPARE(AStrength_Calculator::Compute(program, {upper, lower}, {prescription}, sessions, sets).Index.Percent,
	         report.Index.Percent);
}
//-------------------------------------------------------------------------------------------------------------
void AStrength_Calculator_Test::indexExcludesStartAndMissing()
{
	SProgress_Index index;
	for (const QString &status : QStringList{"start", "noData", "better", "same", "attention", "worse", "better"})
		AStrength_Calculator::Accumulate(index, status);
	QCOMPARE(index.Better, 2);
	QCOMPARE(index.Worse, 1);
	QCOMPARE(index.Evaluated, 5);
	QCOMPARE(index.Percent, 20.0);
}
//-------------------------------------------------------------------------------------------------------------
void AStrength_Calculator_Test::historicalRangeWinsOverEditedPlan()
{
	SProgram program;
	program.ID = "p";
	program.User_Id = "u";
	SDay_Exercise prescription;
	prescription.Exercise_Id = "squat";
	prescription.Reps_Min = 1;
	prescription.Reps_Max = 5;
	QVector<SWorkout_Session> sessions;
	QVector<SSet_Log> sets;
	for (int i = 0; i < 2; ++i)
	{
		SWorkout_Session session;
		session.ID = QString::number(i);
		session.User_Id = "u";
		session.Program_Id = "p";
		session.Date = QString("2026-10-0%1").arg(i + 1);
		session.State = EWorkout_State::Completed;
		sessions << session;
		SSet_Log set;
		set.Session_Id = session.ID;
		set.Exercise_Id = "squat";
		set.Done = true;
		set.Weight_Kg = 60 + i * 5;
		set.Reps = 10;
		set.Has_Target_Reps_Range = true;
		set.Target_Reps_Min = 6;
		set.Target_Reps_Max = 12;
		sets << set;
	}
	SStrength_Report report = AStrength_Calculator::Compute(program, {}, {prescription}, sessions, sets);
	QCOMPARE(report.History.last().Trend.Status, QString("better"));
	QCOMPARE(report.History.last().Range_Source, QString("snapshot"));
	QCOMPARE(report.History.last().Reps_Max, 12);
}
//-------------------------------------------------------------------------------------------------------------
QTEST_GUILESS_MAIN(AStrength_Calculator_Test)
#include "tst_Strength_Calculator.moc"
