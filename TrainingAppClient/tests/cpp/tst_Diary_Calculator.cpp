#include "Domain/Diary_Calculator.h"
#include <QtTest>

//-------------------------------------------------------------------------------------------------------------
class ADiary_Calculator_Test : public QObject
{
	Q_OBJECT
private slots:
	void applicabilityUsesDatesAndPlanKinds();
	void sparseSummariesUseEngineVerdicts();
	void rotationAndActualStart();
	void openEndedStopsAtToday();
	void invalidTermClearsOutputs();
};
//-------------------------------------------------------------------------------------------------------------
void ADiary_Calculator_Test::applicabilityUsesDatesAndPlanKinds()
{
	SProgram_Rule rule;
	SDay_Log day;
	rule.Has_Target_Max = true;
	rule.Target_Max = 1300;
	rule.Active_From = "2026-10-02";
	rule.Active_To = "2026-10-04";
	day.Date = "2026-10-01";
	QVERIFY(!ADiary_Calculator::Applies(rule, day));
	day.Date = "2026-10-02";
	QVERIFY(ADiary_Calculator::Applies(rule, day));
	rule.Applies_To = ERule_Applies_To::TrainingDays;
	QVERIFY(!ADiary_Calculator::Applies(rule, day));
	day.Is_Training = true;
	QVERIFY(ADiary_Calculator::Applies(rule, day));
	rule.Applies_To = ERule_Applies_To::RestDays;
	QVERIFY(!ADiary_Calculator::Applies(rule, day));
	rule.Applies_To = ERule_Applies_To::CustomDays;
	rule.Applies_Days.append("upper");
	day.Day_Id = "lower";
	QVERIFY(!ADiary_Calculator::Applies(rule, day));
	day.Day_Id = "upper";
	QVERIFY(ADiary_Calculator::Applies(rule, day));
	day.Date = "2026-10-04";
	QVERIFY(ADiary_Calculator::Applies(rule, day));
	day.Date = "2026-10-05";
	QVERIFY(!ADiary_Calculator::Applies(rule, day));
	day.Date = "2026-10-03";
	rule.Periodicity = ERule_Periodicity::PerWeek;
	QVERIFY(!ADiary_Calculator::Applies(rule, day));
}
//-------------------------------------------------------------------------------------------------------------
void ADiary_Calculator_Test::sparseSummariesUseEngineVerdicts()
{
	SProgram program;
	SProgram_Rule rule;
	QVector<SRule_Checkin> checkins;
	QVector<SDay_Log> days;
	QVector<SDiary_Summary> summaries;
	program.ID = "p";
	program.User_Id = "u";
	program.Term.Start_Date = "2026-10-01";
	program.Term.Weeks_Total = 1;
	rule.ID = "kcal";
	rule.Program_Id = "p";
	rule.Has_Target_Max = true;
	rule.Target_Max = 1300;
	for (int i = 0; i < 5; ++i)
	{
		SRule_Checkin checkin;
		checkin.Rule_Id = "kcal";
		checkin.Program_Id = "p";
		checkin.Date = QDate(2026, 10, 1).addDays(i).toString(Qt::ISODate);
		checkin.Has_Value = i != 2;
		checkin.Value = i == 0 ? 0 : 1400;
		// Сохранённый done не подменяет оценку фактического значения движком.
		checkin.State = ECheckin_State::Done;
		checkins.append(checkin);
	}
	QVERIFY(ADiary_Calculator::Build(program, {}, {}, QVector<SProgram_Rule>() << rule, checkins, {}, {}, {},
	                                 QDate(2026, 10, 3), days, summaries));
	QCOMPARE(summaries.at(0).Average, 700.0);
	QCOMPARE(summaries.at(0).Days_In_Norm, 1);
	QCOMPARE(summaries.at(0).Days_Off, 1);
	QVERIFY(!days.at(2).Values.at(0).Has_Value);
	QCOMPARE(days.at(2).Values.at(0).Verdict, ECheckin_State::Unknown);
	QVERIFY(!days.at(3).Values.at(0).Has_Value);
	QCOMPARE(days.at(1).Values.at(0).Verdict, ECheckin_State::Failed);
}
//-------------------------------------------------------------------------------------------------------------
void ADiary_Calculator_Test::rotationAndActualStart()
{
	SProgram program;
	SProgram_Day plan;
	SWeek_Slot workout;
	SWeek_Slot rest;
	QVector<SDay_Log> days;
	QVector<SDiary_Summary> summaries;
	program.Term.Start_Date = "2026-09-01";
	program.Actual_Start_Date = "2026-10-01";
	program.Term.Weeks_Total = 1;
	program.Schedule_Kind = ESchedule_Kind::Rotation;
	plan.ID = "upper";
	plan.Title = "Upper";
	workout.Rotation_Index = 1;
	workout.Program_Day_Id = plan.ID;
	workout.Kind = ESlot_Kind::Workout;
	rest.Rotation_Index = 2;
	QVERIFY(ADiary_Calculator::Build(program, QVector<SProgram_Day>() << plan, QVector<SWeek_Slot>() << workout << rest,
	                                 {}, {}, {}, {}, {}, QDate(2026, 10, 3), days, summaries));
	QCOMPARE(days.size(), 7);
	QCOMPARE(days.at(0).Date, QString("2026-10-01"));
	QCOMPARE(days.at(6).Date, QString("2026-10-07"));
	QCOMPARE(days.at(0).Day_Title, QString("Upper"));
	QVERIFY(!days.at(1).Is_Training);
	QCOMPARE(days.at(2).Day_Title, QString("Upper"));
}
//-------------------------------------------------------------------------------------------------------------
void ADiary_Calculator_Test::openEndedStopsAtToday()
{
	SProgram program;
	QVector<SDay_Log> days;
	QVector<SDiary_Summary> summaries;
	program.Term.Start_Date = "2026-10-01";
	program.Term.Mode = ETerm_Mode::OpenEnded;
	QVERIFY(ADiary_Calculator::Build(program, {}, {}, {}, {}, {}, {}, {}, QDate(2026, 10, 3), days, summaries));
	QCOMPARE(days.size(), 3);
	QVERIFY(days.last().Editable);
	program.Term.Start_Date = "2026-10-04";
	QVERIFY(ADiary_Calculator::Build(program, {}, {}, {}, {}, {}, {}, {}, QDate(2026, 10, 3), days, summaries));
	QVERIFY(days.isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void ADiary_Calculator_Test::invalidTermClearsOutputs()
{
	SProgram program;
	QVector<SDay_Log> days;
	QVector<SDiary_Summary> summaries;
	program.Term.Start_Date = "2026-10-01";
	program.Term.Weeks_Total = 53;
	days.append(SDay_Log());
	summaries.append(SDiary_Summary());
	QVERIFY(!ADiary_Calculator::Build(program, {}, {}, {}, {}, {}, {}, {}, QDate(2026, 10, 3), days, summaries));
	QVERIFY(days.isEmpty());
	QVERIFY(summaries.isEmpty());
	program.Term.Weeks_Total = 0;
	QVERIFY(!ADiary_Calculator::Build(program, {}, {}, {}, {}, {}, {}, {}, QDate(2026, 10, 3), days, summaries));
}
//-------------------------------------------------------------------------------------------------------------
QTEST_MAIN(ADiary_Calculator_Test)
#include "tst_Diary_Calculator.moc"
