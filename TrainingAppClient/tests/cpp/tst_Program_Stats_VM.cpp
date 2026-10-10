#include "Data/Database/Sql_Database.h"
#include "Data/Program/Measurement_Repository.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Data/Program/Stats_Repository.h"
#include "Data/Program/Workout_Repository.h"
#include "Presentation/Program_Stats_VM.h"

#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

// Avm_Program_Stats: KPI программы по фактам + запись кэша (§5.1, §5.4).
class AProgram_Stats_VM_Test : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void aggregatesFactsIntoKpi();
	void writesCacheOnRefresh();
	void refreshWithoutProgramFails();
	void refreshEmitsChanged();

private:
	void Seed_Exercise();
	void Seed_Program();
	void Seed_Session(const QString &in_id, const QString &in_date);
	void Seed_Set(const QString &in_id, const QString &in_session_id, double in_weight, int in_reps, double in_rpe);
	// Значение роли по её имени: тест не должен знать числовых значений ролей.
	QVariant Row_By_Role(const AList_Model *in_model, int in_row, const QString &in_role) const;

	QTemporaryDir Directory;
	AsSql_Database Database;
	AProgram_Repository *Program_Repository = 0;
	AWorkout_Repository *Workout_Repository = 0;
	ARule_Repository *Rule_Repository = 0;
	AMeasurement_Repository *Measurement_Repository = 0;
	AStats_Repository *Stats_Repository = 0;
};
//----------------------------------------------------------------------------
void AProgram_Stats_VM_Test::initTestCase()
{
	QVERIFY(Directory.isValid());
	QVERIFY(Database.Open(Directory.filePath("program_stats_vm.sqlite3")));
	Program_Repository = new AProgram_Repository(&Database, this);
	Workout_Repository = new AWorkout_Repository(&Database, this);
	Rule_Repository = new ARule_Repository(&Database, this);
	Measurement_Repository = new AMeasurement_Repository(&Database, this);
	Stats_Repository = new AStats_Repository(&Database, this);
	Seed_Exercise();
	Seed_Program();
}
//----------------------------------------------------------------------------
QVariant AProgram_Stats_VM_Test::Row_By_Role(const AList_Model *in_model, int in_row, const QString &in_role) const
{
	// Значения ролей Qt начинаются с Qt::UserRole + 1, поэтому перебираются
	// ключи хеша, а не числа подряд.
	const QHash<int, QByteArray> role_names = in_model->roleNames();

	for (int role = Qt::UserRole + 1; role <= Qt::UserRole + role_names.size(); ++role)
	{
		if (role_names.value(role) == in_role)
			return in_model->Data(in_row, role);
	}

	return QVariant();
}
//----------------------------------------------------------------------------
void AProgram_Stats_VM_Test::Seed_Exercise()
{
	QSqlQuery query(Database.Sql_Connection());

	// Каталог приходит из AsSql_Database::Seed_Builtin_Exercises при Open(),
	// поэтому фикстура идемпотентна: тот же id уже есть в базе.
	query.prepare("INSERT OR IGNORE INTO exercises(id, title, muscle_group, equipment, is_custom, created_at) "
	              "VALUES('ex_squat', 'Присед', 'legs', 'barbell', 0, '2026-09-28T00:00:00Z')");
	QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}
//----------------------------------------------------------------------------
void AProgram_Stats_VM_Test::Seed_Program()
{
	SProgram program;
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> exercises;
	QVector<SWeek_Slot> week_slots;

	program.ID = "p1";
	program.User_Id = "u1";
	program.Title = "Сила 4 недели";
	program.Source_Kind = EProgram_Source_Kind::Scratch;
	program.Goal = EProgram_Goal::Strength;
	program.Schedule_Kind = ESchedule_Kind::Weekday;
	program.State = EProgram_State::Draft;
	program.Term.Start_Date = "2026-09-28";
	program.Term.Weeks_Total = 2;

	SDay_Exercise exercise;

	exercise.ID = "de1";
	exercise.Program_Day_Id = "d1";
	exercise.Exercise_Id = "ex_squat";
	exercise.Sets = 2;
	exercise.Reps_Min = 5;
	exercise.Reps_Max = 5;
	exercise.Weight_Mode = EWeight_Mode::Fixed;
	exercises.append(exercise);

	// Каждый рабочий слот обязан ссылаться на свой день плана (§4), поэтому
	// дней семь, а не один: иначе расписание отвергается.
	for (int day_of_week = 1; day_of_week <= 7; ++day_of_week)
	{
		SProgram_Day slot_day;

		slot_day.ID = QString("d%1").arg(day_of_week);
		slot_day.Program_Id = "p1";
		slot_day.Title = QString("День %1").arg(day_of_week);
		slot_day.Kind = EDay_Kind::Workout;
		slot_day.Order_Index = day_of_week - 1;
		days.append(slot_day);

		SWeek_Slot slot;

		slot.ID = QString("slot_%1").arg(day_of_week);
		slot.Program_Id = "p1";
		slot.Day_Of_Week = day_of_week;
		slot.Kind = ESlot_Kind::Workout;
		slot.Program_Day_Id = slot_day.ID;
		week_slots.append(slot);
	}

	QVERIFY2(Program_Repository->Insert_Program_Full(program, QVector<SGoal_Target>(), days, exercises, week_slots),
	         qPrintable(Program_Repository->Get_Last_Error()));
}
//----------------------------------------------------------------------------
void AProgram_Stats_VM_Test::Seed_Session(const QString &in_id, const QString &in_date)
{
	QSqlQuery query(Database.Sql_Connection());

	query.prepare("INSERT INTO workouts(id, user_id, program_id, date, started_at, completed_at, duration_seconds, "
	              "state, exercise_count, set_count, tonnage_kg, sync_state) "
	              "VALUES(?, 'u1', 'p1', ?, ?, ?, 3600, 'completed', 1, 2, 1000, 'local')");
	query.addBindValue(in_id);
	query.addBindValue(in_date);
	query.addBindValue(in_date + "T18:00:00Z");
	query.addBindValue(in_date + "T19:00:00Z");
	QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}
//----------------------------------------------------------------------------
void AProgram_Stats_VM_Test::Seed_Set(const QString &in_id, const QString &in_session_id, double in_weight, int in_reps,
                                      double in_rpe)
{
	QSqlQuery query(Database.Sql_Connection());

	query.prepare("INSERT INTO sets(id, session_id, exercise_id, set_number, is_warmup, weight_kg, reps, rpe, done, "
	              "target_weight_kg, target_reps) VALUES(?, ?, 'ex_squat', 1, 0, ?, ?, ?, 1, 100, 5)");
	query.addBindValue(in_id);
	query.addBindValue(in_session_id);
	query.addBindValue(in_weight);
	query.addBindValue(in_reps);
	query.addBindValue(in_rpe);
	QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}
//----------------------------------------------------------------------------
void AProgram_Stats_VM_Test::aggregatesFactsIntoKpi()
{
	Avm_Program_Stats vm(Program_Repository, Workout_Repository, Rule_Repository, Measurement_Repository,
	                     Stats_Repository, this);
	Seed_Session("s1", "2026-09-28");
	Seed_Set("set1", "s1", 100, 5, 8);

	vm.Set_Today("2026-10-05");

	QVERIFY(vm.Refresh("p1"));
	QVERIFY(vm.Has_Stats());
	QCOMPARE(vm.completedSessions(), 1);
	// План: 7 слотов в неделю × 2 недели окна.
	QCOMPARE(vm.plannedSessions(), 14);
	QVERIFY(vm.Has_Adherence_Percent());
	QCOMPARE(vm.tonnageKg(), 500.0);
	QCOMPARE(vm.setsCount(), 1);
	QCOMPARE(vm.repsCount(), 5);
	QCOMPARE(vm.durationMinutes(), 60.0);
	QCOMPARE(vm.dataRangeFrom(), QString("2026-09-28"));
	QVERIFY(vm.errorKey().isEmpty());

	// График тоннажа строится по недельным срезам, и точки берутся из роли
	// `tonnageKg`: без неё карточка графика остаётся пустой, хотя KPI посчитан.
	// Недели окна: тоннаж первой — из тренировки, вторая пустая.
	QCOMPARE(vm.weeks()->Row_Count(), 2);
	QCOMPARE(Row_By_Role(vm.weeks(), 0, "tonnageKg").toDouble(), 500.0);
	QCOMPARE(Row_By_Role(vm.weeks(), 1, "tonnageKg").toDouble(), 0.0);
}
//----------------------------------------------------------------------------
void AProgram_Stats_VM_Test::writesCacheOnRefresh()
{
	Avm_Program_Stats vm(Program_Repository, Workout_Repository, Rule_Repository, Measurement_Repository,
	                     Stats_Repository, this);
	SStats_Cache_Entry entry;

	vm.Set_Today("2026-10-05");
	QVERIFY(vm.Refresh("p1"));

	// Кэш пишется после пересчёта и помечает ревизию структуры (§5.4).
	QVERIFY(Stats_Repository->Load_Program_Stats("p1", entry));
	QCOMPARE(entry.Owner_Id, QString("p1"));
	QVERIFY(entry.Computed_At.isEmpty() == false);
	QVERIFY(entry.Payload_Json.contains("programId"));
}
//----------------------------------------------------------------------------
void AProgram_Stats_VM_Test::refreshWithoutProgramFails()
{
	Avm_Program_Stats vm(Program_Repository, Workout_Repository, Rule_Repository, Measurement_Repository,
	                     Stats_Repository, this);

	QVERIFY(vm.Refresh("нет-программы") == false);
	QVERIFY(vm.Has_Stats() == false);
	QVERIFY(vm.errorKey().isEmpty() == false);
	QCOMPARE(vm.weeks()->Row_Count(), 0);
}
//----------------------------------------------------------------------------
void AProgram_Stats_VM_Test::refreshEmitsChanged()
{
	Avm_Program_Stats vm(Program_Repository, Workout_Repository, Rule_Repository, Measurement_Repository,
	                     Stats_Repository, this);
	QSignalSpy spy(&vm, &Avm_Program_Stats::changed);

	vm.Set_Today("2026-10-05");
	QVERIFY(vm.Refresh("p1"));
	QCOMPARE(spy.count(), 1);
}

QTEST_GUILESS_MAIN(AProgram_Stats_VM_Test)
#include "tst_Program_Stats_VM.moc"
