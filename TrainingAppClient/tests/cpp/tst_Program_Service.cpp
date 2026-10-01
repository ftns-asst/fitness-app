#include "Data/Database/Sql_Database.h"
#include "Data/Program/Program_Repository.h"
#include "Domain/Program_Service.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class AProgram_Service_Test : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void createFromScratchWritesOneAtomicRevision();
	void createFromSnapshotCopiesStructureWithNewIdentity();
	void snapshotCreationRollsBackOnInvalidReference();
	void stateTransitionsDelegateToRepository();
	void recommendLinearProgressionAfterSuccessfulSets();
	void recommendationHonoursDoubleProgressionAndRpe();
	void recommendationAppliesDeloadAndWeeklyPercent();
	void recommendationRejectsInvalidInputsAndManualRules();

private:
	void Seed_Exercise(const QString &in_id);
	void Make_Snapshot(SProgram &out_program, QVector<SProgram_Day> &out_days, QVector<SDay_Exercise> &out_exercises,
	                   QVector<SWeek_Slot> &out_slots);

	QTemporaryDir Directory;
	AsSql_Database Database;
	AProgram_Repository *Repository = 0;
	AsProgram_Service *Service = 0;
};

//----------------------------------------------------------------------------
void AProgram_Service_Test::initTestCase()
{
	QVERIFY(Directory.isValid());
	QVERIFY(Database.Open(Directory.filePath("program_service.sqlite3")));
	Repository = new AProgram_Repository(&Database, this);
	Service = new AsProgram_Service(Repository, this);
	Seed_Exercise("ex_squat");
}
//----------------------------------------------------------------------------
void AProgram_Service_Test::Seed_Exercise(const QString &in_id)
{
	QSqlQuery query(Database.Sql_Connection());

	query.prepare("INSERT OR IGNORE INTO exercises(id, title, muscle_group, equipment, is_custom, created_at) "
	              "VALUES(?, 'Присед', 'legs', 'barbell', 0, '2026-09-30T00:00:00Z')");
	query.addBindValue(in_id);
	QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}
//----------------------------------------------------------------------------
void AProgram_Service_Test::Make_Snapshot(SProgram &out_program, QVector<SProgram_Day> &out_days,
                                          QVector<SDay_Exercise> &out_exercises, QVector<SWeek_Slot> &out_slots)
{
	SProgram_Day day;
	SDay_Exercise exercise;
	SWeek_Slot slot;
	static int snapshot_index = 0;
	QString suffix;

	++snapshot_index;
	suffix = QString::number(snapshot_index);

	out_program = SProgram();
	out_program.ID = QString("template_%1").arg(suffix);
	out_program.User_Id = "template_owner";
	out_program.Title = "Базовая сила";
	out_program.Source_Kind = EProgram_Source_Kind::Template;
	out_program.Goal = EProgram_Goal::Strength;
	out_program.Level = EProgram_Level::Beginner;
	out_program.Schedule_Kind = ESchedule_Kind::Weekday;
	out_program.Term.Start_Date = "2026-09-30";
	out_program.Term.Weeks_Total = 8;
	out_program.Term.Mode = ETerm_Mode::FixedWeeks;

	day.ID = QString("template_day_%1").arg(suffix);
	day.Program_Id = out_program.ID;
	day.Title = "День A";
	day.Kind = EDay_Kind::Workout;
	day.Focus << "legs";
	day.Order_Index = 0;
	out_days = QVector<SProgram_Day>() << day;

	exercise.ID = QString("template_exercise_%1").arg(suffix);
	exercise.Program_Day_Id = day.ID;
	exercise.Exercise_Id = "ex_squat";
	exercise.Sets = 3;
	exercise.Reps_Min = 5;
	exercise.Reps_Max = 8;
	exercise.Weight_Mode = EWeight_Mode::Fixed;
	exercise.Target_Weight_Kg = 80;
	exercise.Has_Target_Weight_Kg = true;
	out_exercises = QVector<SDay_Exercise>() << exercise;

	for (int day_of_week = 1; day_of_week <= 7; ++day_of_week)
	{
		slot = SWeek_Slot();
		slot.ID = QString("template_slot_%1_%2").arg(suffix).arg(day_of_week);
		slot.Program_Id = out_program.ID;
		slot.Day_Of_Week = day_of_week;
		slot.Kind = day_of_week == 1 ? ESlot_Kind::Workout : ESlot_Kind::Rest;
		if (day_of_week == 1)
			slot.Program_Day_Id = day.ID;
		out_slots.append(slot);
	}
}
//----------------------------------------------------------------------------
void AProgram_Service_Test::createFromScratchWritesOneAtomicRevision()
{
	SProgram program;
	SProgram stored;
	SProgram created;
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> exercises;
	QVector<SWeek_Slot> week_slots;
	QVector<SGoal_Target> targets;
	SGoal_Target target;
	QVector<SGoal_Target> stored_targets;
	QVector<SProgram_Day> stored_days;
	QVector<SDay_Exercise> stored_exercises;
	QVector<SWeek_Slot> stored_slots;
	QSqlQuery query(Database.Sql_Connection());

	program.User_Id = "scratch_user";
	program.Title = "План с нуля";
	program.Goal = EProgram_Goal::Strength;
	program.Schedule_Kind = ESchedule_Kind::Weekday;
	program.Term.Start_Date = "2026-09-30";
	program.Term.Weeks_Total = 0;
	program.Term.Mode = ETerm_Mode::OpenEnded;
	Make_Snapshot(stored, days, exercises, week_slots);
	target.ID = "scratch_goal";
	target.Metric = EGoal_Metric::WorkingWeightKg;
	target.Exercise_Id = "ex_squat";
	target.Unit = EGoal_Unit::Kg;
	target.Target_Value = 100;
	target.Direction = EGoal_Direction::Increase;
	targets.append(target);
	program.ID.clear();
	program.Source_Kind = EProgram_Source_Kind::Scratch;

	QVERIFY2(Service->Create_From_Scratch(program, targets, days, exercises, week_slots, created),
	         qPrintable(Service->Get_Last_Error()));
	QVERIFY(created.ID.isEmpty() == false);
	QVERIFY(
	    Repository->Load_Program_Full(created.ID, stored, stored_targets, stored_days, stored_exercises, stored_slots));
	QCOMPARE(stored.User_Id, QString("scratch_user"));
	QCOMPARE(stored.Title, QString("План с нуля"));
	QCOMPARE(stored.State, EProgram_State::Draft);
	QCOMPARE(stored.Revision, 1);
	QCOMPARE(stored.Source_Kind, EProgram_Source_Kind::Scratch);
	QCOMPARE(stored_days.size(), 1);
	QCOMPARE(stored_slots.size(), 7);
	QVERIFY(query.prepare("SELECT COUNT(*) FROM program_revisions WHERE program_id = ?"));
	query.addBindValue(created.ID);
	QVERIFY(query.exec());
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 1);
}
//----------------------------------------------------------------------------
void AProgram_Service_Test::createFromSnapshotCopiesStructureWithNewIdentity()
{
	SProgram snapshot;
	SProgram request;
	SProgram created;
	SProgram stored;
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> exercises;
	QVector<SWeek_Slot> week_slots;
	QVector<SGoal_Target> targets;
	QVector<SGoal_Target> stored_targets;
	QVector<SProgram_Day> stored_days;
	QVector<SDay_Exercise> stored_exercises;
	QVector<SWeek_Slot> stored_slots;

	Make_Snapshot(snapshot, days, exercises, week_slots);
	QVERIFY(Repository->Insert_Program_Full(snapshot, targets, days, exercises, week_slots));
	request.User_Id = "copy_user";
	request.Title = "Моя копия";
	request.State = EProgram_State::Draft;
	QVERIFY2(Service->Create_From_Snapshot(request, snapshot, targets, days, exercises, week_slots, created),
	         qPrintable(Service->Get_Last_Error()));
	QVERIFY(created.ID.isEmpty() == false);
	QVERIFY(created.ID != snapshot.ID);
	QCOMPARE(created.Source_Kind, EProgram_Source_Kind::Copy);
	QCOMPARE(created.Source_Template_Id, snapshot.ID);
	QCOMPARE(created.User_Id, QString("copy_user"));
	QCOMPARE(created.Title, QString("Моя копия"));
	QVERIFY(
	    Repository->Load_Program_Full(created.ID, stored, stored_targets, stored_days, stored_exercises, stored_slots));
	QCOMPARE(stored_days.size(), 1);
	QCOMPARE(stored_exercises.size(), 1);
	QCOMPARE(stored_slots.size(), 7);
	QVERIFY(stored_days.at(0).ID != days.at(0).ID);
	QCOMPARE(stored_exercises.at(0).Program_Day_Id, stored_days.at(0).ID);
	QCOMPARE(stored_slots.at(0).Program_Day_Id, stored_days.at(0).ID);
	QCOMPARE(stored_exercises.at(0).Exercise_Id, QString("ex_squat"));
	QCOMPARE(stored_days.at(0).Source_Day_Id, days.at(0).Source_Day_Id);
	QCOMPARE(stored.Revision, 1);

	// Изменение собственного снимка не может задеть источник или его расписание.
	QVERIFY(Repository->Load_Program_Full(snapshot.ID, stored, stored_targets, stored_days, stored_exercises,
	                                      stored_slots));
	QCOMPARE(stored_days.at(0).ID, days.at(0).ID);
	QCOMPARE(stored_slots.at(0).Program_Day_Id, days.at(0).ID);
}
//----------------------------------------------------------------------------
void AProgram_Service_Test::snapshotCreationRollsBackOnInvalidReference()
{
	SProgram snapshot;
	SProgram request;
	SProgram created;
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> exercises;
	QVector<SWeek_Slot> week_slots;
	QVector<SGoal_Target> targets;
	QSqlQuery query(Database.Sql_Connection());

	Make_Snapshot(snapshot, days, exercises, week_slots);
	request.User_Id = "rollback_user";
	request.Title = "Не должно сохраниться";
	exercises[0].Exercise_Id = "missing_exercise";
	QVERIFY(Service->Create_From_Snapshot(request, snapshot, targets, days, exercises, week_slots, created) == false);
	QVERIFY(created.ID.isEmpty());
	QVERIFY(Service->Get_Last_Error().isEmpty() == false);
	query.prepare("SELECT COUNT(*) FROM programs WHERE user_id = ?");
	query.addBindValue(request.User_Id);
	QVERIFY(query.exec());
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 0);
}
//----------------------------------------------------------------------------
void AProgram_Service_Test::stateTransitionsDelegateToRepository()
{
	SProgram program;
	SProgram loaded;
	SProgram created;
	QVector<SGoal_Target> targets;
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> exercises;
	QVector<SWeek_Slot> week_slots;

	program.ID.clear();
	program.User_Id = "state_user";
	program.Title = "Состояния";
	program.Source_Kind = EProgram_Source_Kind::Scratch;
	QVERIFY(Service->Create_From_Scratch(program, targets, days, exercises, week_slots, created));
	QVERIFY(Service->Activate(created.ID));
	QVERIFY(Repository->Load_Program(created.ID, loaded));
	QCOMPARE(loaded.State, EProgram_State::Active);
	QVERIFY(loaded.Actual_Start_Date.isEmpty() == false);
	QVERIFY(Service->Pause(created.ID));
	QVERIFY(Service->Resume(created.ID));
	QVERIFY(Service->Complete(created.ID));
	QVERIFY(Service->Archive(created.ID));
	QVERIFY(Service->Activate(created.ID) == false);
}
//----------------------------------------------------------------------------
void AProgram_Service_Test::recommendLinearProgressionAfterSuccessfulSets()
{
	SDay_Exercise exercise;
	double recommendation = -1;

	exercise.Sets = 3;
	exercise.Reps_Max = 8;
	exercise.Has_Progression = true;
	exercise.Progression.Type = EProgression_Type::LinearWeight;
	exercise.Progression.Has_Increment_Kg = true;
	exercise.Progression.Increment_Kg = 2.5;
	exercise.Progression.Has_Max_Rpe = true;
	exercise.Progression.Max_Rpe = 8;

	QVERIFY(Service->Recommend_Weight(exercise, 80, 3, 8, 7, 1, recommendation));
	QCOMPARE(recommendation, 82.5);
	QVERIFY(Service->Recommend_Weight(exercise, 80, 2, 8, 7, 1, recommendation));
	QCOMPARE(recommendation, 80.0);
	QVERIFY(Service->Recommend_Weight(exercise, 80, 3, 8, 9, 1, recommendation));
	QCOMPARE(recommendation, 80.0);
}
//----------------------------------------------------------------------------
void AProgram_Service_Test::recommendationHonoursDoubleProgressionAndRpe()
{
	SDay_Exercise exercise;
	double recommendation = -1;

	exercise.Sets = 3;
	exercise.Reps_Max = 10;
	exercise.Has_Progression = true;
	exercise.Progression.Type = EProgression_Type::DoubleProgression;
	exercise.Progression.Increment_Kg = 2.5;
	exercise.Progression.Has_Increment_Kg = true;
	exercise.Progression.Trigger_Reps_Min = 10;
	exercise.Progression.Has_Trigger_Reps_Min = true;
	exercise.Progression.Max_Rpe = 8;
	exercise.Progression.Has_Max_Rpe = true;

	QVERIFY(Service->Recommend_Weight(exercise, 60, 3, 9, 7, 1, recommendation));
	QCOMPARE(recommendation, 60.0);
	QVERIFY(Service->Recommend_Weight(exercise, 60, 3, 10, 8, 1, recommendation));
	QCOMPARE(recommendation, 62.5);
	QVERIFY(Service->Recommend_Weight(exercise, 60, 3, 10, 8.5, 1, recommendation));
	QCOMPARE(recommendation, 60.0);
}
//----------------------------------------------------------------------------
void AProgram_Service_Test::recommendationAppliesDeloadAndWeeklyPercent()
{
	SDay_Exercise exercise;
	double recommendation = -1;

	exercise.Has_Progression = true;
	exercise.Progression.Type = EProgression_Type::LinearWeight;
	exercise.Progression.Increment_Kg = 2.5;
	exercise.Progression.Has_Increment_Kg = true;
	exercise.Progression.Deload_Every_Weeks = 4;
	exercise.Progression.Has_Deload_Every_Weeks = true;
	QVERIFY(Service->Recommend_Weight(exercise, 100, 0, 0, 0, 4, recommendation));
	QCOMPARE(recommendation, 60.0);

	exercise.Progression.Type = EProgression_Type::PercentPerWeek;
	QVERIFY(Service->Recommend_Weight(exercise, 100, 0, 0, 0, 3, recommendation));
	QCOMPARE(recommendation, 106.1208);
}
//----------------------------------------------------------------------------
void AProgram_Service_Test::recommendationRejectsInvalidInputsAndManualRules()
{
	SDay_Exercise exercise;
	double recommendation = 12;

	exercise.Has_Progression = true;
	exercise.Progression.Type = EProgression_Type::Manual;
	QVERIFY(Service->Recommend_Weight(exercise, 60, 3, 10, 7, 1, recommendation) == false);
	QVERIFY(Service->Recommend_Weight(exercise, -1, 3, 10, 7, 1, recommendation) == false);
	QVERIFY(Service->Recommend_Weight(exercise, 60, 3, 10, 7, 0, recommendation) == false);
}

QTEST_GUILESS_MAIN(AProgram_Service_Test)
#include "tst_Program_Service.moc"