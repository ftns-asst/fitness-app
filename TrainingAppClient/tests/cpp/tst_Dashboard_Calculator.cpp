#include "Domain/Dashboard_Calculator.h"

#include <QtTest>

//-------------------------------------------------------------------------------------------------------------
class ADashboard_Calculator_Test : public QObject
{
	Q_OBJECT

private slots:
	void calendarClock();
	void recentWeightAverageExcludesOldAndForeignReadings();
	void pausedClock();
	void bodyGoal();
	void completionProgress();
};
//-------------------------------------------------------------------------------------------------------------
void ADashboard_Calculator_Test::recentWeightAverageExcludesOldAndForeignReadings()
{
	SProgram program;
	program.User_Id = "u";
	program.Term.Start_Date = "2026-10-01";
	SBody_Measurement old;
	old.User_Id = "u";
	old.Date = "2026-11-05";
	old.Has_Weight_Kg = true;
	old.Weight_Kg = 100;
	SBody_Measurement first(old);
	first.Date = "2026-11-06";
	first.Weight_Kg = 84;
	SBody_Measurement last(first);
	last.Date = "2026-11-12";
	last.Weight_Kg = 82;
	SBody_Measurement foreign(last);
	foreign.User_Id = "other";
	foreign.Weight_Kg = 130;
	QVariantMap result = ADashboard_Calculator::Compute(program, {}, {last, old, foreign, first}, "2026-11-12");
	QCOMPARE(result.value("average7Kg").toDouble(), 83.0);
	QCOMPARE(result.value("weightReadings7d").toInt(), 2);
	QCOMPARE(result.value("currentWeightDate").toString(), QString("2026-11-12"));
	result = ADashboard_Calculator::Compute(program, {}, {old}, "2026-11-12");
	QVERIFY(result.value("average7Kg").isNull());
	QCOMPARE(result.value("currentWeightKg").toDouble(), 100.0);
}
//-------------------------------------------------------------------------------------------------------------
void ADashboard_Calculator_Test::calendarClock()
{
	SProgram program;
	program.Term.Start_Date = "2026-10-01";
	program.Actual_Start_Date = "2026-10-03";
	program.Term.Weeks_Total = 4;
	program.Term.Extended_Weeks = 1;
	QVariantMap result = ADashboard_Calculator::Compute(program, {}, {}, "2026-10-05");
	QCOMPARE(result.value("startDate").toString(), QString("2026-10-03"));
	QCOMPARE(result.value("currentDay").toInt(), 3);
	QCOMPARE(result.value("totalDays").toInt(), 28);
	QCOMPARE(result.value("daysLeft").toInt(), 25);
	result = ADashboard_Calculator::Compute(program, {}, {}, "2026-10-02");
	QCOMPARE(result.value("currentDay").toInt(), 0);
	QCOMPARE(result.value("daysLeft").toInt(), 28);
	result = ADashboard_Calculator::Compute(program, {}, {}, "2027-01-01");
	QCOMPARE(result.value("currentDay").toInt(), 28);
	QCOMPARE(result.value("daysLeft").toInt(), 0);
	program.Term.Mode = ETerm_Mode::OpenEnded;
	result = ADashboard_Calculator::Compute(program, {}, {}, "2026-10-05");
	QCOMPARE(result.value("currentDay").toInt(), 3);
	QVERIFY(result.value("totalDays").isNull());
	QVERIFY(result.value("daysLeft").isNull());
}
//-------------------------------------------------------------------------------------------------------------
void ADashboard_Calculator_Test::pausedClock()
{
	SProgram program;
	program.Term.Start_Date = "2026-10-01";
	program.Term.Weeks_Total = 4;
	program.State = EProgram_State::Paused;
	program.Updated_At = "2026-10-05T12:00:00Z";
	QVariantMap first = ADashboard_Calculator::Compute(program, {}, {}, "2026-10-10");
	QVariantMap later = ADashboard_Calculator::Compute(program, {}, {}, "2026-10-20");
	QCOMPARE(first.value("currentDay").toInt(), 5);
	QCOMPARE(later.value("currentDay"), first.value("currentDay"));
	program.State = EProgram_State::Active;
	later = ADashboard_Calculator::Compute(program, {}, {}, "2026-10-20", 15);
	QCOMPARE(later.value("currentDay").toInt(), 5);
	program.State = EProgram_State::Paused;
	later = ADashboard_Calculator::Compute(program, {}, {}, "2026-10-25", 15, "2026-10-22");
	QCOMPARE(later.value("currentDay").toInt(), 7);
}
//-------------------------------------------------------------------------------------------------------------
void ADashboard_Calculator_Test::bodyGoal()
{
	SProgram program;
	program.User_Id = "u1";
	program.Term.Start_Date = "2026-10-01";
	QVariantMap result = ADashboard_Calculator::Compute(program, {}, {}, "2026-10-05");
	QVERIFY(result.value("currentWeightKg").isNull());
	QVERIFY(result.value("startWeightKg").isNull());
	QVERIFY(result.value("weightDeltaKg").isNull());
	QVERIFY(result.value("remainingKg").isNull());
	SGoal_Target target;
	target.Metric = EGoal_Metric::BodyWeightKg;
	target.Has_Start_Value = true;
	target.Start_Value = 80;
	target.Target_Value = 75;
	target.Direction = EGoal_Direction::Decrease;
	SBody_Measurement current;
	current.User_Id = "u1";
	current.Date = "2026-10-03";
	current.Has_Weight_Kg = true;
	current.Weight_Kg = 82;
	current.Has_Waist_Cm = true;
	current.Waist_Cm = 0;
	SBody_Measurement future(current);
	future.Date = "2026-10-10";
	future.Weight_Kg = 70;
	SBody_Measurement other(future);
	other.User_Id = "other";
	other.Date = "2026-10-04";
	result = ADashboard_Calculator::Compute(program, {target}, {future, other, current}, "2026-10-05");
	QCOMPARE(result.value("currentWeightKg").toDouble(), 82.0);
	QCOMPARE(result.value("weightDeltaKg").toDouble(), 2.0);
	QCOMPARE(result.value("weightDeltaPercent").toDouble(), 2.5);
	QCOMPARE(result.value("remainingKg").toDouble(), 7.0);
	QCOMPARE(result.value("favorable").toBool(), false);
	QCOMPARE(result.value("waistCm").toDouble(), 0.0);
	QVERIFY(!result.value("waistCm").isNull());
	QVERIFY(result.value("bellyCm").isNull());
	target.Direction = EGoal_Direction::Increase;
	target.Target_Value = 85;
	result = ADashboard_Calculator::Compute(program, {target}, {current}, "2026-10-05");
	QCOMPARE(result.value("remainingKg").toDouble(), 3.0);
	QCOMPARE(result.value("favorable").toBool(), true);
	target.Direction = EGoal_Direction::Maintain;
	target.Target_Value = 80;
	result = ADashboard_Calculator::Compute(program, {target}, {current}, "2026-10-05");
	QCOMPARE(result.value("remainingKg").toDouble(), 2.0);
	QCOMPARE(result.value("favorable").toBool(), false);
	target.Start_Value = 0;
	result = ADashboard_Calculator::Compute(program, {target}, {current}, "2026-10-05");
	QVERIFY(result.value("weightDeltaPercent").isNull());
}
//-------------------------------------------------------------------------------------------------------------
void ADashboard_Calculator_Test::completionProgress()
{
	QCOMPARE(ADashboard_Calculator::Completion_Progress(0.5, 2, 4).toDouble(), 0.5);
	QCOMPARE(ADashboard_Calculator::Completion_Progress(0.25, 8, 4).toDouble(), 0.625);
	QCOMPARE(ADashboard_Calculator::Completion_Progress(0.5, 0, 0).toDouble(), 0.5);
	QCOMPARE(ADashboard_Calculator::Completion_Progress(QVariant(), 1, 4).toDouble(), 0.25);
	QVERIFY(ADashboard_Calculator::Completion_Progress(QVariant(), 0, 0).isNull());
}
//-------------------------------------------------------------------------------------------------------------
QTEST_GUILESS_MAIN(ADashboard_Calculator_Test)
#include "tst_Dashboard_Calculator.moc"
