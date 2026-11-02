#include "Data/Database/Sql_Database.h"
#include "Presentation/Diary_VM.h"
#include <QTemporaryDir>
#include <QtTest>
#include <limits>

using namespace Program_Model;

//-------------------------------------------------------------------------------------------------------------
class ADiary_VM_Test : public QObject
{
	Q_OBJECT
private slots:
	void init();
	void showsTermNormsAndMissingValues();
	void dailyIndicatorsComeFromSelectedProgram();
	void pastEditsPersistAndRefreshSummaries();
	void futureAndStaleSelectionCannotWrite();
	void rejectsForeignUsersRulesAndInvalidValues();
	void respectsApplicabilityAndExcludesFutureFacts();
	void scheduleWeightAndCompletedWorkout();
	void openEndedProgramHasNoFutureRows();
	void personalRecordsWorkWithoutProgram();

private:
	QTemporaryDir Directory;
	AsSql_Database Database;
};
//-------------------------------------------------------------------------------------------------------------
void ADiary_VM_Test::init()
{
	Database.Close();
	QVERIFY(Database.Open(Directory.filePath(QUuid::createUuid().toString() + ".sqlite3")));
	AProgram_Repository programs(&Database);
	ARule_Repository rules(&Database);
	SProgram program;
	program.ID = "p";
	program.User_Id = "u";
	program.Title = "Diary";
	program.Term.Start_Date = "2026-10-01";
	program.Term.Weeks_Total = 1;
	program.Actual_Start_Date = "2026-10-01";
	QVERIFY(programs.Insert_Program(program));
	SProgram_Rule rule;
	rule.ID = "protein";
	rule.Program_Id = "p";
	rule.Title = "Protein";
	rule.Code = "protein";
	rule.Category = ERule_Category::Nutrition;
	rule.Metric = ERule_Metric::ProteinGrams;
	rule.Has_Target_Min = true;
	rule.Target_Min = 180;
	rule.Display_Unit = ERule_Display_Unit::G;
	rule.Target_Unit = ERule_Display_Unit::G;
	QVERIFY(rules.Replace_Rules("p", QVector<SProgram_Rule>() << rule));
}
//-------------------------------------------------------------------------------------------------------------
void ADiary_VM_Test::showsTermNormsAndMissingValues()
{
	AProgram_Repository programs(&Database);
	ARule_Repository rules(&Database);
	AWorkout_Repository workouts(&Database);
	AMeasurement_Repository measurements(&Database);
	ADay_Note_Repository notes(&Database);
	AStats_Repository stats(&Database);
	Avm_Diary vm(&programs, &rules, &workouts, &measurements, &notes, &stats);
	vm.setToday("2026-10-03");
	QVERIFY(vm.reload("u", "p"));
	QCOMPARE(vm.days()->Row_Count(), 7);
	QCOMPARE(vm.days()->get(6).value("date").toString(), QString("2026-10-07"));
	QCOMPARE(vm.norms()->get(0).value("target").toString(), QString::fromUtf8("≥ 180"));
	QVERIFY(vm.days()->get(0).value("weight").isNull());
	QVERIFY(vm.summaries()->get(0).value("average").isNull());
	QCOMPARE(vm.summaries()->get(0).value("daysOff").toInt(), 0);
	QVERIFY(vm.openDay("2026-10-02"));
	QCOMPARE(vm.selectedValues().at(0).toMap().value("verdict").toString(), QString("unknown"));
	QVERIFY(vm.selectedWeight().isNull());
	// Нет программы — дневник работает как личные записи: окно по умолчанию
	// завершается сегодня, правил нет.
	QVERIFY(vm.reload("nobody"));
	QVERIFY(!vm.Has_Program());
	QCOMPARE(vm.days()->Row_Count(), 12 * 7);
	QCOMPARE(vm.days()->get(vm.days()->Row_Count() - 1).value("date").toString(), QString("2026-10-03"));
	QCOMPARE(vm.norms()->Row_Count(), 0);
	QVERIFY(vm.selectedDate().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void ADiary_VM_Test::pastEditsPersistAndRefreshSummaries()
{
	AProgram_Repository programs(&Database);
	ARule_Repository rules(&Database);
	AWorkout_Repository workouts(&Database);
	AMeasurement_Repository measurements(&Database);
	ADay_Note_Repository notes(&Database);
	AStats_Repository stats(&Database);
	Avm_Diary vm(&programs, &rules, &workouts, &measurements, &notes, &stats);
	vm.setToday("2026-10-03");
	QVERIFY(vm.reload("u", "p"));
	QVERIFY(vm.openDay("2026-10-01"));
	QVERIFY(stats.Save_Program_Stats("p", 1, "{}"));
	QVERIFY(stats.Save_Lifetime_Stats("u", "{}"));
	QVERIFY(vm.saveRuleValue("protein", 200));
	QVERIFY(vm.saveNote("First line\nSecond line"));
	QVERIFY(vm.saveWeight(90.0));
	QVERIFY(vm.openDay("2026-10-02"));
	QVERIFY(vm.saveRuleValue("protein", 100));
	QCOMPARE(vm.summaries()->get(0).value("average").toDouble(), 150.0);
	QCOMPARE(vm.summaries()->get(0).value("daysInNorm").toInt(), 1);
	QCOMPARE(vm.summaries()->get(0).value("daysOff").toInt(), 1);
	QVERIFY(vm.saveRuleValue("protein", 180));
	QCOMPARE(vm.summaries()->get(0).value("average").toDouble(), 190.0);
	QCOMPARE(vm.summaries()->get(0).value("daysInNorm").toInt(), 2);
	SStats_Cache_Entry cache;
	QVERIFY(!stats.Load_Program_Stats("p", cache));
	QVERIFY(!stats.Load_Lifetime_Stats("u", cache));
	QVERIFY(vm.reload("u", "p"));
	QVERIFY(vm.openDay("2026-10-01"));
	QCOMPARE(vm.selectedNote(), QString("First line\nSecond line"));
	QCOMPARE(vm.selectedWeight().toDouble(), 90.0);
	QVERIFY(vm.saveRuleValue("protein", QVariant()));
	QCOMPARE(vm.summaries()->get(0).value("average").toDouble(), 180.0);
	QVERIFY(vm.saveNote(""));
	QCOMPARE(vm.selectedNote(), QString());
}
//-------------------------------------------------------------------------------------------------------------
void ADiary_VM_Test::futureAndStaleSelectionCannotWrite()
{
	AProgram_Repository programs(&Database);
	ARule_Repository rules(&Database);
	AWorkout_Repository workouts(&Database);
	AMeasurement_Repository measurements(&Database);
	ADay_Note_Repository notes(&Database);
	AStats_Repository stats(&Database);
	Avm_Diary vm(&programs, &rules, &workouts, &measurements, &notes, &stats);
	vm.setToday("2026-10-03");
	QVERIFY(vm.reload("u", "p"));
	QVERIFY(!vm.openDay("2026-10-04"));
	QVERIFY(!vm.saveNote("future"));
	QVERIFY(!vm.saveWeight(90));
	QVERIFY(!vm.saveRuleValue("protein", 180));
	QVERIFY(vm.openDay("2026-10-03"));
	vm.setToday("2026-10-02");
	QVERIFY(!vm.saveNote("stale"));
	QVERIFY(vm.reload("u", "p"));
	QVERIFY(!vm.saveNote("no selection"));
	QVERIFY(!vm.openDay("2026-09-30"));
	QVERIFY(!vm.saveWeight(80));
}
//-------------------------------------------------------------------------------------------------------------
void ADiary_VM_Test::rejectsForeignUsersRulesAndInvalidValues()
{
	AProgram_Repository programs(&Database);
	ARule_Repository rules(&Database);
	AWorkout_Repository workouts(&Database);
	AMeasurement_Repository measurements(&Database);
	ADay_Note_Repository notes(&Database);
	AStats_Repository stats(&Database);
	Avm_Diary vm(&programs, &rules, &workouts, &measurements, &notes, &stats);
	vm.setToday("2026-10-03");
	QVERIFY(!vm.reload("other", "p"));
	QVERIFY(vm.reload("u", "p"));
	QVERIFY(vm.openDay("2026-10-02"));
	QVERIFY(!vm.saveRuleValue("foreign", 180));
	QVERIFY(!vm.saveRuleValue("protein", "bad"));
	QVERIFY(!vm.saveRuleValue("protein", -1));
	QVERIFY(!vm.saveRuleValue("protein", std::numeric_limits<double>::infinity()));
	QVERIFY(!vm.saveWeight(0));
	QVERIFY(!vm.saveWeight("bad"));
	QVERIFY(!vm.saveWeight(std::numeric_limits<double>::quiet_NaN()));
	QVERIFY(!vm.errorKey().isEmpty());
	QVERIFY(vm.saveWeight(88));
	QVERIFY(vm.errorKey().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void ADiary_VM_Test::respectsApplicabilityAndExcludesFutureFacts()
{
	AProgram_Repository programs(&Database);
	ARule_Repository rules(&Database);
	AWorkout_Repository workouts(&Database);
	AMeasurement_Repository measurements(&Database);
	ADay_Note_Repository notes(&Database);
	AStats_Repository stats(&Database);
	QVector<SProgram_Rule> loaded;
	QVERIFY(rules.Load_Rules("p", loaded));
	loaded[0].Active_From = "2026-10-02";
	loaded[0].Active_To = "2026-10-05";
	loaded[0].Applies_To = ERule_Applies_To::RestDays;
	QVERIFY(rules.Replace_Rules("p", loaded));
	SRule_Checkin checkin;
	checkin.Rule_Id = "protein";
	checkin.Program_Id = "p";
	checkin.Date = "2026-10-04";
	checkin.Has_Value = true;
	checkin.Value = 999;
	checkin.Unit = "g";
	checkin.State = ECheckin_State::Done;
	QVERIFY(rules.Save_Checkin(checkin));
	Avm_Diary vm(&programs, &rules, &workouts, &measurements, &notes, &stats);
	vm.setToday("2026-10-03");
	QVERIFY(vm.reload("u", "p"));
	QVERIFY(vm.summaries()->get(0).value("average").isNull());
	QVERIFY(vm.openDay("2026-10-01"));
	QVERIFY(!vm.saveRuleValue("protein", 200));
	QVERIFY(vm.openDay("2026-10-02"));
	QVERIFY(vm.saveRuleValue("protein", 0));
	QCOMPARE(vm.summaries()->get(0).value("average").toDouble(), 0.0);
	QCOMPARE(vm.summaries()->get(0).value("daysOff").toInt(), 1);
}
//-------------------------------------------------------------------------------------------------------------
void ADiary_VM_Test::scheduleWeightAndCompletedWorkout()
{
	AProgram_Repository programs(&Database);
	ARule_Repository rules(&Database);
	AWorkout_Repository workouts(&Database);
	AMeasurement_Repository measurements(&Database);
	ADay_Note_Repository notes(&Database);
	AStats_Repository stats(&Database);
	SProgram_Day day;
	day.ID = "upper";
	day.Program_Id = "p";
	day.Title = "Upper";
	SWeek_Slot slot;
	slot.ID = "thursday";
	slot.Program_Id = "p";
	slot.Day_Of_Week = 4;
	slot.Kind = ESlot_Kind::Workout;
	slot.Program_Day_Id = "upper";
	QVERIFY(programs.Replace_Structure("p", QVector<SProgram_Day>() << day, {}, QVector<SWeek_Slot>() << slot,
	                                   ERevision_Reason::Days, "Diary schedule"));
	SBody_Measurement measurement;
	measurement.User_Id = "u";
	measurement.Date = "2026-10-01";
	measurement.Has_Weight_Kg = true;
	measurement.Weight_Kg = 90;
	measurement.Has_Waist_Cm = true;
	measurement.Waist_Cm = 80;
	QVERIFY(measurements.Save_Measurement(measurement));
	SWorkout_Session session;
	session.ID = "workout";
	session.User_Id = "u";
	session.Program_Id = "p";
	session.Date = "2026-10-01";
	session.State = EWorkout_State::Completed;
	QVERIFY(workouts.Insert_Session(session));
	Avm_Diary vm(&programs, &rules, &workouts, &measurements, &notes, &stats);
	vm.setToday("2026-10-03");
	QVERIFY(vm.reload("u", "p"));
	QCOMPARE(vm.days()->get(0).value("dayTitle").toString(), QString("Upper"));
	QVERIFY(vm.days()->get(0).value("workoutDone").toBool());
	QCOMPARE(vm.days()->get(2).value("average7").toDouble(), 90.0);
	QVERIFY(vm.openDay("2026-10-01"));
	QVERIFY(vm.saveWeight(89));
	QVERIFY(measurements.Find_Measurement("u", "2026-10-01", measurement));
	QCOMPARE(measurement.Waist_Cm, 80.0);
}
//-------------------------------------------------------------------------------------------------------------
void ADiary_VM_Test::openEndedProgramHasNoFutureRows()
{
	AProgram_Repository programs(&Database);
	ARule_Repository rules(&Database);
	AWorkout_Repository workouts(&Database);
	AMeasurement_Repository measurements(&Database);
	ADay_Note_Repository notes(&Database);
	AStats_Repository stats(&Database);
	STerm term;
	term.Start_Date = "2026-10-01";
	term.Mode = ETerm_Mode::OpenEnded;
	QVERIFY(programs.Change_Term("p", term));
	Avm_Diary vm(&programs, &rules, &workouts, &measurements, &notes, &stats);
	vm.setToday("2026-10-03");
	QVERIFY(vm.reload("u", "p"));
	QCOMPARE(vm.days()->Row_Count(), 3);
	vm.setToday("2026-09-30");
	QVERIFY(vm.reload("u", "p"));
	QCOMPARE(vm.days()->Row_Count(), 0);
}
//-------------------------------------------------------------------------------------------------------------
// Без программы дневник не пустой: вес и заметки принадлежат пользователю и дате.
void ADiary_VM_Test::personalRecordsWorkWithoutProgram()
{
	AProgram_Repository programs(&Database);
	ARule_Repository rules(&Database);
	AWorkout_Repository workouts(&Database);
	AMeasurement_Repository measurements(&Database);
	ADay_Note_Repository notes(&Database);
	AStats_Repository stats(&Database);
	Avm_Diary vm(&programs, &rules, &workouts, &measurements, &notes, &stats);
	vm.setToday("2026-10-03");
	QVERIFY(vm.reload("solo"));
	QVERIFY(!vm.Has_Program());
	QCOMPARE(vm.norms()->Row_Count(), 0);
	QVERIFY(vm.openDay("2026-10-01"));
	QVERIFY(vm.saveWeight(82.5));
	QVERIFY(vm.saveNote("вес без программы"));
	QVERIFY(vm.reload("solo"));
	QVERIFY(vm.openDay("2026-10-01"));
	QCOMPARE(vm.selectedWeight().toDouble(), 82.5);
	QCOMPARE(vm.selectedNote(), QString("вес без программы"));
	// Правила в таком режиме недоступны: их неоткуда взять.
	QVERIFY(!vm.saveRuleValue("protein", 200));
	QCOMPARE(vm.errorKey(), QString("diary.unknown_rule"));
}
//-------------------------------------------------------------------------------------------------------------
void ADiary_VM_Test::dailyIndicatorsComeFromSelectedProgram()
{
	AProgram_Repository programs(&Database);
	ARule_Repository rules(&Database);
	AWorkout_Repository workouts(&Database);
	AMeasurement_Repository measurements(&Database);
	ADay_Note_Repository notes(&Database);
	AStats_Repository stats(&Database);
	QVector<SProgram_Rule> loaded;
	QVERIFY(rules.Load_Rules("p", loaded));
	SProgram_Rule calories = loaded.first();
	calories.ID = "calories";
	calories.Code = "calories";
	calories.Title = "Calories";
	calories.Metric = ERule_Metric::CaloriesKcal;
	calories.Display_Unit = ERule_Display_Unit::Kcal;
	calories.Target_Unit = ERule_Display_Unit::Kcal;
	loaded.append(calories);
	QVERIFY(rules.Replace_Rules("p", loaded));
	Avm_Diary vm(&programs, &rules, &workouts, &measurements, &notes, &stats);
	vm.setToday("2026-10-03");
	QVERIFY(vm.reload("u", "p"));
	QCOMPARE(vm.programTitle(), QString("Diary"));
	QVERIFY(vm.openDay("2026-10-02"));
	QCOMPARE(vm.selectedValues().size(), 2);
	QStringList ids;
	for (const QVariant &cell : vm.selectedValues())
		ids.append(cell.toMap().value("ruleId").toString());
	QVERIFY(ids.contains("calories"));
	QVERIFY(ids.contains("protein"));
	QVERIFY(vm.reload("nobody"));
	QVERIFY(vm.programTitle().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
QTEST_MAIN(ADiary_VM_Test)
#include "tst_Diary_VM.moc"
