#include "Data/Database/Sql_Database.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Presentation/Program_VM.h"

#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

// Avm_Program: собирает дерево активной программы и отдаёт его QML через
// модели-списки и скалярные свойства. Проверяем связку «БД → роли».
class AProgram_VM_Test : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void withoutProgramExposesEmptyState();
	void loadsProgramTreeIntoModels();
	void exposesScalarProgramProperties();
	void daySelectionFiltersExercises();
	void reloadEmitsChanged();

private:
	void Seed_Exercise(const QString &in_id);
	void Seed_Program();
	QVariant Role_Of(AList_Model &in_model, int in_row, const QString &in_role);

	QTemporaryDir Directory;
	AsSql_Database Database;
	AProgram_Repository *Program_Repository = 0;
	ARule_Repository *Rule_Repository = 0;
};
//----------------------------------------------------------------------------
void AProgram_VM_Test::initTestCase()
{
	QVERIFY(Directory.isValid());
	QVERIFY(Database.Open(Directory.filePath("program_vm.sqlite3")));
	Program_Repository = new AProgram_Repository(&Database, this);
	Rule_Repository = new ARule_Repository(&Database, this);
	Seed_Exercise("ex_squat");
	Seed_Exercise("ex_bench");
	Seed_Program();
}
//----------------------------------------------------------------------------
QVariant AProgram_VM_Test::Role_Of(AList_Model &in_model, int in_row, const QString &in_role)
{
	for (int i = 0; i < in_model.Role_Count(); ++i)
	{
		if (in_model.Role_Name(Qt::UserRole + 1 + i) == in_role)
			return in_model.Data(in_row, Qt::UserRole + 1 + i);
	}

	return QVariant();
}
//----------------------------------------------------------------------------
void AProgram_VM_Test::Seed_Exercise(const QString &in_id)
{
	QSqlQuery query(Database.Sql_Connection());

	query.prepare("INSERT INTO exercises(id, title, muscle_group, equipment, is_custom, created_at) "
	              "VALUES(?, ?, ?, ?, 0, '2026-09-29T00:00:00Z')");
	query.addBindValue(in_id);
	query.addBindValue(QString("Упражнение %1").arg(in_id));
	query.addBindValue("legs");
	query.addBindValue("barbell");
	QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}
//----------------------------------------------------------------------------
void AProgram_VM_Test::Seed_Program()
{
	SProgram program;
	QVector<SGoal_Target> targets;
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> exercises;
	QVector<SWeek_Slot> week_slots;

	program.ID = "p1";
	program.User_Id = "u1";
	program.Title = "Сила 4 недели";
	program.Source_Kind = EProgram_Source_Kind::Scratch;
	program.Goal = EProgram_Goal::Strength;
	program.Schedule_Kind = ESchedule_Kind::Weekday;
	// Создание программы идёт из состояния draft (§3.1); перевод в active —
	// отдельный переход с записью actual_start_date.
	program.State = EProgram_State::Draft;
	program.Term.Start_Date = "2026-09-28";
	program.Term.Weeks_Total = 4;
	program.Actual_Start_Date = "2026-09-28";

	SGoal_Target goal;

	goal.ID = "g1";
	goal.Program_Id = "p1";
	goal.Metric = EGoal_Metric::WorkingWeightKg;
	goal.Exercise_Id = "ex_squat";
	goal.Start_Value = 100;
	goal.Has_Start_Value = true;
	goal.Target_Value = 110;
	goal.Direction = EGoal_Direction::Increase;
	targets.append(goal);

	SProgram_Day day;

	day.ID = "d1";
	day.Program_Id = "p1";
	day.Title = "Ноги";
	day.Kind = EDay_Kind::Workout;
	day.Order_Index = 0;
	days.append(day);

	SDay_Exercise exercise;

	exercise.ID = "de1";
	exercise.Program_Day_Id = "d1";
	exercise.Exercise_Id = "ex_squat";
	exercise.Order_Index = 0;
	exercise.Sets = 4;
	exercise.Reps_Min = 5;
	exercise.Reps_Max = 5;
	exercise.Weight_Mode = EWeight_Mode::Fixed;
	exercise.Target_Weight_Kg = 100;
	exercise.Has_Target_Weight_Kg = true;
	exercises.append(exercise);

	SWeek_Slot slot;

	slot.ID = "slot_1";
	slot.Program_Id = "p1";
	slot.Day_Of_Week = 1;
	slot.Kind = ESlot_Kind::Workout;
	slot.Program_Day_Id = "d1";
	week_slots.append(slot);

	QVERIFY2(Program_Repository->Insert_Program_Full(program, targets, days, exercises, week_slots),
	         qPrintable(Program_Repository->Get_Last_Error()));
	// Активация — отдельный переход: он же проставляет actual_start_date.
	QVERIFY2(Program_Repository->Change_State("p1", EProgram_State::Active),
	         qPrintable(Program_Repository->Get_Last_Error()));
}
//----------------------------------------------------------------------------
void AProgram_VM_Test::withoutProgramExposesEmptyState()
{
	Avm_Program vm(Program_Repository, Rule_Repository, this);

	QVERIFY(vm.Reload("nobody") == false);
	QVERIFY(vm.Has_Program() == false);
	QCOMPARE(vm.week_slots()->Row_Count(), 0);
	QCOMPARE(vm.program_days()->Row_Count(), 0);
	QVERIFY(vm.errorKey().isEmpty() == false);
}
//----------------------------------------------------------------------------
void AProgram_VM_Test::loadsProgramTreeIntoModels()
{
	Avm_Program vm(Program_Repository, Rule_Repository, this);

	QVERIFY(vm.Reload("u1"));
	QVERIFY(vm.Has_Program());
	QCOMPARE(vm.programId(), QString("p1"));
	QCOMPARE(vm.title(), QString("Сила 4 недели"));

	AList_Model *slot_model = vm.week_slots();
	AList_Model *days = vm.program_days();

	QCOMPARE(slot_model->Row_Count(), 1);
	QCOMPARE(days->Row_Count(), 1);
	QCOMPARE(Role_Of(*slot_model, 0, "id").toString(), QString("slot_1"));
	QCOMPARE(Role_Of(*slot_model, 0, "dayTitle").toString(), QString("Ноги"));
	QCOMPARE(Role_Of(*days, 0, "title").toString(), QString("Ноги"));

	// Предписания дня выбираются явно: пока день не выбран — список пуст.
	QCOMPARE(vm.day_exercises()->Row_Count(), 0);
	vm.Select_Day("d1");

	QCOMPARE(vm.day_exercises()->Row_Count(), 1);
	QCOMPARE(Role_Of(*vm.day_exercises(), 0, "title").toString(), QString("Упражнение ex_squat"));
	QCOMPARE(vm.selectedDayId(), QString("d1"));
}
//----------------------------------------------------------------------------
void AProgram_VM_Test::exposesScalarProgramProperties()
{
	Avm_Program vm(Program_Repository, Rule_Repository, this);

	QVERIFY(vm.Reload("u1"));
	QCOMPARE(vm.goal(), QString("strength"));
	QCOMPARE(vm.state(), QString("active"));
	QCOMPARE(vm.weeksTotal(), 4);
	// Ревизия 2: создание программы пишет ревизию 1, активация инкрементит.
	QCOMPARE(vm.revision(), 2);
	QCOMPARE(vm.goal_targets()->Row_Count(), 1);
	QCOMPARE(vm.rules()->Row_Count(), 0);
}
//----------------------------------------------------------------------------
void AProgram_VM_Test::daySelectionFiltersExercises()
{
	Avm_Program vm(Program_Repository, Rule_Repository, this);

	QVERIFY(vm.Reload("u1"));

	// Неизвестный день не оставляет прежние предписания в модели.
	vm.Select_Day("нет-такого-дня");

	QCOMPARE(vm.day_exercises()->Row_Count(), 0);
	QVERIFY(vm.selectedDayId().isEmpty());
}
//----------------------------------------------------------------------------
void AProgram_VM_Test::reloadEmitsChanged()
{
	Avm_Program vm(Program_Repository, Rule_Repository, this);
	QSignalSpy spy(&vm, &Avm_Program::changed);

	QVERIFY(vm.Reload("u1"));
	QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(AProgram_VM_Test)
#include "tst_Program_VM.moc"
