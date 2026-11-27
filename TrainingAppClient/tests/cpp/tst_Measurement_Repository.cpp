#include "Data/Database/Sql_Database.h"
#include "Data/Exercises/Exercises_Repository.h"
#include "Data/Program/Measurement_Repository.h"
#include "Domain/Program_Model.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class AMeasurement_Repository_Test : public QObject
{
	Q_OBJECT

private slots:
	void saveAndMergeSameDay();
	void saveValidatesInput();
	void rangeAndLatest();
	void deleteMeasurement();
	void measurementsArePerUser();
	void allTenParametersRoundTrip();
	void newParametersMergeWithoutErasing();
	void newParametersMustBePositive();
	void latestDateOfEmptyHistoryIsEmpty();
	void muscleMeasurements_data();
	void muscleMeasurements();
	void programSelectionUsesAssignedExercisesAndPreservesHistory();
	void programSelectionClearsOutputOnDatabaseFailure();

private:
	QString Make_Database_Path();
	SBody_Measurement Make_Measurement(const QString &in_user_id, const QString &in_date);

	QTemporaryDir Directory;
	int Counter = 0;
};
//-------------------------------------------------------------------------------------------------------------
QString AMeasurement_Repository_Test::Make_Database_Path()
{
	++Counter;

	return Directory.filePath(QString("measurements_%1.sqlite3").arg(Counter));
}
//-------------------------------------------------------------------------------------------------------------
SBody_Measurement AMeasurement_Repository_Test::Make_Measurement(const QString &in_user_id, const QString &in_date)
{
	SBody_Measurement measurement;

	measurement.User_Id = in_user_id;
	measurement.Date = in_date;

	return measurement;
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Repository_Test::saveAndMergeSameDay()
{
	AsSql_Database database;
	AMeasurement_Repository repository(&database);
	SBody_Measurement measurement;
	SBody_Measurement loaded;
	QSqlQuery query;
	QString first_id;

	QVERIFY(database.Open(Make_Database_Path()));
	query = QSqlQuery(database.Sql_Connection());

	// Утренний вес.
	measurement = Make_Measurement("u1", "2026-09-29");
	measurement.Weight_Kg = 82.5;
	measurement.Has_Weight_Kg = true;
	QVERIFY(repository.Save_Measurement(measurement));
	QVERIFY(repository.Find_Measurement("u1", "2026-09-29", loaded));
	QVERIFY(!loaded.ID.isEmpty());
	first_id = loaded.ID;
	QCOMPARE(loaded.Weight_Kg, 82.5);
	QVERIFY(!loaded.Has_Waist_Cm);

	// Вечерние обхваты не затирают утренний вес (§3.8).
	measurement = Make_Measurement("u1", "2026-09-29");
	measurement.Waist_Cm = 84;
	measurement.Has_Waist_Cm = true;
	measurement.Chest_Cm = 104;
	measurement.Has_Chest_Cm = true;
	QVERIFY(repository.Save_Measurement(measurement));

	QVERIFY(repository.Find_Measurement("u1", "2026-09-29", loaded));
	QCOMPARE(loaded.ID, first_id);
	QCOMPARE(loaded.Weight_Kg, 82.5);
	QCOMPARE(loaded.Waist_Cm, 84.0);
	QCOMPARE(loaded.Chest_Cm, 104.0);
	QVERIFY(!loaded.Has_Hip_Cm);

	// Повторный вес того же дня обновляет значение, а не создаёт строку.
	measurement = Make_Measurement("u1", "2026-09-29");
	measurement.Weight_Kg = 82;
	measurement.Has_Weight_Kg = true;
	measurement.Thigh_Cm = 58;
	measurement.Has_Thigh_Cm = true;
	QVERIFY(repository.Save_Measurement(measurement));

	QVERIFY(repository.Find_Measurement("u1", "2026-09-29", loaded));
	QCOMPARE(loaded.ID, first_id);
	QCOMPARE(loaded.Weight_Kg, 82.0);
	QCOMPARE(loaded.Thigh_Cm, 58.0);
	QCOMPARE(loaded.Waist_Cm, 84.0);
	QVERIFY(query.exec("SELECT COUNT(*) FROM body_measurements WHERE user_id = 'u1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 1);
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Repository_Test::saveValidatesInput()
{
	AsSql_Database database;
	AMeasurement_Repository repository(&database);
	SBody_Measurement measurement;
	SBody_Measurement loaded;
	QVector<SBody_Measurement> empty_range;

	QVERIFY(database.Open(Make_Database_Path()));
	measurement = Make_Measurement("u1", "2026-09-29");
	QVERIFY(!repository.Save_Measurement(measurement));
	QVERIFY(!repository.Get_Last_Error().isEmpty());

	measurement = Make_Measurement(QString(), "2026-09-29");
	measurement.Weight_Kg = 80;
	measurement.Has_Weight_Kg = true;
	QVERIFY(!repository.Save_Measurement(measurement));

	measurement = Make_Measurement("u1", "29.09.2026");
	measurement.Weight_Kg = 80;
	measurement.Has_Weight_Kg = true;
	QVERIFY(!repository.Save_Measurement(measurement));
	measurement.Date = "2026-13-01";
	QVERIFY(!repository.Save_Measurement(measurement));
	measurement.Date = QString();
	QVERIFY(!repository.Save_Measurement(measurement));

	measurement = Make_Measurement("u1", "2026-09-29");
	measurement.Weight_Kg = 1000;
	measurement.Has_Weight_Kg = true;
	QVERIFY(!repository.Save_Measurement(measurement));
	measurement.Weight_Kg = 80;
	measurement.Waist_Cm = -1;
	measurement.Has_Waist_Cm = true;
	QVERIFY(!repository.Save_Measurement(measurement));

	AsSql_Database closed;
	AMeasurement_Repository closed_repository(&closed);
	measurement = Make_Measurement("u1", "2026-09-29");
	measurement.Weight_Kg = 80;
	measurement.Has_Weight_Kg = true;
	QVERIFY(!closed_repository.Save_Measurement(measurement));
	QVERIFY(!closed_repository.Find_Measurement("u1", "2026-09-29", loaded));
	QVERIFY(!closed_repository.Load_Range("u1", "2026-09-01", "2026-09-30", empty_range));
	QVERIFY(!closed_repository.Load_Latest("u1", loaded));
	QVERIFY(!closed_repository.Get_Last_Error().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Repository_Test::rangeAndLatest()
{
	AsSql_Database database;
	AMeasurement_Repository repository(&database);
	SBody_Measurement measurement;
	SBody_Measurement latest;
	QVector<SBody_Measurement> range;

	QVERIFY(database.Open(Make_Database_Path()));
	measurement = Make_Measurement("u1", "2026-09-14");
	measurement.Weight_Kg = 84;
	measurement.Has_Weight_Kg = true;
	QVERIFY(repository.Save_Measurement(measurement));
	measurement = Make_Measurement("u1", "2026-09-21");
	measurement.Weight_Kg = 83.5;
	measurement.Has_Weight_Kg = true;
	QVERIFY(repository.Save_Measurement(measurement));
	measurement = Make_Measurement("u1", "2026-09-28");
	measurement.Weight_Kg = 83;
	measurement.Has_Weight_Kg = true;
	measurement.Waist_Cm = 82;
	measurement.Has_Waist_Cm = true;
	QVERIFY(repository.Save_Measurement(measurement));

	QVERIFY(repository.Load_Range("u1", "2026-09-14", "2026-09-28", range));
	QCOMPARE(range.size(), 3);
	QCOMPARE(range.at(0).Date, QString("2026-09-14"));
	QCOMPARE(range.at(2).Date, QString("2026-09-28"));
	QVERIFY(repository.Load_Range("u1", "2026-09-21", "2026-09-21", range));
	QCOMPARE(range.size(), 1);
	QCOMPARE(range.at(0).Weight_Kg, 83.5);
	QVERIFY(!range.at(0).Has_Waist_Cm);
	QVERIFY(repository.Load_Range("u1", "2026-08-01", "2026-08-31", range));
	QVERIFY(range.isEmpty());
	QVERIFY(repository.Load_Latest("u1", latest));
	QCOMPARE(latest.Date, QString("2026-09-28"));
	QCOMPARE(latest.Weight_Kg, 83.0);
	QCOMPARE(latest.Waist_Cm, 82.0);
	QVERIFY(!repository.Load_Latest("u2", latest));
	QVERIFY(!repository.Get_Last_Error().isEmpty());
	QVERIFY(!repository.Find_Measurement("u2", "2026-09-28", latest));
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Repository_Test::deleteMeasurement()
{
	AsSql_Database database;
	AMeasurement_Repository repository(&database);
	SBody_Measurement measurement;
	SBody_Measurement loaded;
	QVector<SBody_Measurement> range;

	QVERIFY(database.Open(Make_Database_Path()));
	measurement = Make_Measurement("u1", "2026-09-29");
	measurement.Weight_Kg = 82;
	measurement.Has_Weight_Kg = true;
	QVERIFY(repository.Save_Measurement(measurement));
	QVERIFY(repository.Delete_Measurement("u1", "2026-09-29"));
	QVERIFY(!repository.Find_Measurement("u1", "2026-09-29", loaded));
	QVERIFY(repository.Load_Range("u1", "2026-09-01", "2026-09-30", range));
	QVERIFY(range.isEmpty());
	QVERIFY(!repository.Delete_Measurement("u1", "2026-09-29"));
	QVERIFY(!repository.Delete_Measurement("u2", "2026-09-29"));
	QVERIFY(!repository.Get_Last_Error().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Repository_Test::measurementsArePerUser()
{
	AsSql_Database database;
	AMeasurement_Repository repository(&database);
	SBody_Measurement measurement;
	SBody_Measurement loaded;
	QVector<SBody_Measurement> range;

	QVERIFY(database.Open(Make_Database_Path()));
	measurement = Make_Measurement("u1", "2026-09-29");
	measurement.Weight_Kg = 82;
	measurement.Has_Weight_Kg = true;
	QVERIFY(repository.Save_Measurement(measurement));
	measurement = Make_Measurement("u2", "2026-09-29");
	measurement.Weight_Kg = 95;
	measurement.Has_Weight_Kg = true;
	QVERIFY(repository.Save_Measurement(measurement));
	QVERIFY(repository.Find_Measurement("u1", "2026-09-29", loaded));
	QCOMPARE(loaded.Weight_Kg, 82.0);
	QVERIFY(repository.Find_Measurement("u2", "2026-09-29", loaded));
	QCOMPARE(loaded.Weight_Kg, 95.0);
	QVERIFY(repository.Load_Range("u1", "2026-09-01", "2026-09-30", range));
	QCOMPARE(range.size(), 1);
	QCOMPARE(range.at(0).User_Id, QString("u1"));
}
//-------------------------------------------------------------------------------------------------------------
//-------------------------------------------------------------------------------------------------------------
// Десять параметров книги-эталона (docs/09, FR-T5): вес и девять обхватов
// переживают запись и чтение, и ни один не подменяется соседним.
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Repository_Test::allTenParametersRoundTrip()
{
	AsSql_Database database;
	AMeasurement_Repository repository(&database);
	SBody_Measurement measurement = Make_Measurement("u_ten", "2026-10-01");
	SBody_Measurement loaded;
	double value;

	QVERIFY(database.Open(Make_Database_Path()));

	measurement.Weight_Kg = 89.0;
	measurement.Has_Weight_Kg = true;

	for (int i = 0; i < (int)EMeasurement_Key::Max; ++i)
		Set_Measurement_Value(measurement, (EMeasurement_Key)i, 30.0 + i);

	QVERIFY(repository.Save_Measurement(measurement));
	QVERIFY(repository.Find_Measurement("u_ten", "2026-10-01", loaded));
	QCOMPARE(loaded.Weight_Kg, 89.0);

	for (int i = 0; i < (int)EMeasurement_Key::Max; ++i)
	{
		QVERIFY2(Get_Measurement_Value(loaded, (EMeasurement_Key)i, value), qPrintable(To_String((EMeasurement_Key)i)));
		QCOMPARE(value, 30.0 + i);
	}

	QVERIFY(loaded.Has_Belly_Cm && loaded.Has_Shoulders_Cm && loaded.Has_Calves_Cm && loaded.Has_Neck_Cm);
	QCOMPARE(loaded.Neck_Cm, 30.0 + (int)EMeasurement_Key::Neck);
}
//-------------------------------------------------------------------------------------------------------------
// Частичный ввод (FR-T6): только талия, потом только шея — первое значение
// не обнуляется вторым, а не заданное остаётся «не измерял».
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Repository_Test::newParametersMergeWithoutErasing()
{
	AsSql_Database database;
	AMeasurement_Repository repository(&database);
	SBody_Measurement measurement = Make_Measurement("u_part", "2026-10-04");
	SBody_Measurement loaded;

	QVERIFY(database.Open(Make_Database_Path()));

	Set_Measurement_Value(measurement, EMeasurement_Key::Belly, 101.5);
	QVERIFY(repository.Save_Measurement(measurement));

	measurement = Make_Measurement("u_part", "2026-10-04");
	Set_Measurement_Value(measurement, EMeasurement_Key::Neck, 41.0);
	QVERIFY(repository.Save_Measurement(measurement));

	QVERIFY(repository.Find_Measurement("u_part", "2026-10-04", loaded));
	QVERIFY(loaded.Has_Belly_Cm);
	QCOMPARE(loaded.Belly_Cm, 101.5);
	QVERIFY(loaded.Has_Neck_Cm);
	QCOMPARE(loaded.Neck_Cm, 41.0);
	QVERIFY(!loaded.Has_Weight_Kg);
	QVERIFY(!loaded.Has_Shoulders_Cm);
	QVERIFY(!loaded.Has_Calves_Cm);
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Repository_Test::newParametersMustBePositive()
{
	AsSql_Database database;
	AMeasurement_Repository repository(&database);
	SBody_Measurement measurement = Make_Measurement("u_bad", "2026-10-04");

	QVERIFY(database.Open(Make_Database_Path()));

	Set_Measurement_Value(measurement, EMeasurement_Key::Calves, 0);
	QVERIFY(!repository.Save_Measurement(measurement));

	measurement = Make_Measurement("u_bad", "2026-10-04");
	Set_Measurement_Value(measurement, EMeasurement_Key::Shoulders, qQNaN());
	QVERIFY(!repository.Save_Measurement(measurement));
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Repository_Test::latestDateOfEmptyHistoryIsEmpty()
{
	AsSql_Database database;
	AMeasurement_Repository repository(&database);
	SBody_Measurement measurement = Make_Measurement("u_last", "2026-10-01");
	QString date = "stale";

	QVERIFY(database.Open(Make_Database_Path()));

	// Нет замеров — не ошибка: расписанию нужно именно «ещё не мерил».
	QVERIFY(repository.Latest_Measurement_Date("u_last", date));
	QVERIFY(date.isEmpty());

	measurement.Weight_Kg = 80;
	measurement.Has_Weight_Kg = true;
	QVERIFY(repository.Save_Measurement(measurement));
	measurement.Date = "2026-10-07";
	QVERIFY(repository.Save_Measurement(measurement));

	QVERIFY(repository.Latest_Measurement_Date("u_last", date));
	QCOMPARE(date, QString("2026-10-07"));
	QVERIFY(repository.Latest_Measurement_Date("somebody_else", date));
	QVERIFY(date.isEmpty());
}
//-------------------------------------------------------------------------------------------------------------

void AMeasurement_Repository_Test::muscleMeasurements_data()
{
	QTest::addColumn<QStringList>("muscles");
	QTest::addColumn<QStringList>("expected");
	QTest::newRow("bench") << QStringList({"chest", "front_delts", "triceps"})
	                       << QStringList({"chest", "arm", "shoulders"});
	QTest::newRow("legs") << QStringList({"quads", "hamstrings", "adductors", "abductors", "glutes", "calves"})
	                      << QStringList({"hip", "thigh", "calves"});
	QTest::newRow("back") << QStringList({"lats", "upper_back", "traps"}) << QStringList({"chest", "shoulders"});
	QTest::newRow("core") << QStringList({"abs", "obliques"}) << QStringList({"waist", "belly"});
	QTest::newRow("delts") << QStringList({"rear_delts", "side_delts"}) << QStringList({"shoulders"});
	QTest::newRow("duplicates") << QStringList({"biceps", "triceps", "biceps"}) << QStringList({"arm"});
	QTest::newRow("unmeasurable") << QStringList({"cardio", "forearms", "lower_back", "unknown", "fullBody"})
	                              << QStringList();
	QTest::newRow("empty") << QStringList() << QStringList();
	QTest::newRow("legacy-exercise-groups") << QStringList({"legs", "back"}) << QStringList({"chest", "thigh"});
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Repository_Test::muscleMeasurements()
{
	QFETCH(QStringList, muscles);
	QFETCH(QStringList, expected);
	QCOMPARE(Measurement_Keys_For_Muscles(muscles), expected);
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Repository_Test::programSelectionUsesAssignedExercisesAndPreservesHistory()
{
	AsSql_Database database;
	AMeasurement_Repository repository(&database);
	AExercises_Repository exercises(0, 0, &database);
	QStringList keys;
	SBody_Measurement saved = Make_Measurement("u_selection", "2026-10-01");
	SBody_Measurement loaded;

	QVERIFY(database.Open(Make_Database_Path()));
	QVERIFY(exercises.Ensure_Offline_Base());
	QSqlQuery query(database.Sql_Connection());
	QVERIFY2(query.exec(
	             "INSERT INTO "
	             "programs(id,user_id,title,source_kind,goal,level,schedule_kind,state,revision,created_at,updated_at) "
	             "VALUES('selected','u_selection','Legs','scratch','fitness','beginner','weekday','draft',1,'',''), "
	             "('other','u_other','Chest','scratch','fitness','beginner','weekday','draft',1,'','')"),
	         qPrintable(query.lastError().text()));
	QVERIFY(query.exec("INSERT INTO program_days(id,program_id,title,kind,focus,order_index) "
	                   "VALUES('selected-day','selected','Misleading chest title','workout','[\"push\"]',0), "
	                   "('other-day','other','Other','workout','',0)"));
	// Цель талии не должна добавлять обхват к программе ног.
	QVERIFY(query.exec("INSERT INTO goal_targets(id,program_id,metric,measurement_key,unit,target_value,direction) "
	                   "VALUES('unrelated-goal','selected','measurementCm','waist','cm',80,'decrease')"));
	QVERIFY(query.exec(
	    "INSERT INTO "
	    "program_day_exercises(id,program_day_id,exercise_id,order_index,sets,reps_min,reps_max,weight_mode) "
	    "VALUES('squat','selected-day','builtin-squat',0,3,8,10,'byFeel'), "
	    "('bench','other-day','builtin-bench-press',0,3,8,10,'byFeel')"));
	saved.Has_Weight_Kg = true;
	saved.Weight_Kg = 80;
	Set_Measurement_Value(saved, EMeasurement_Key::Neck, 38);
	QVERIFY(repository.Save_Measurement(saved));
	QVERIFY(repository.Load_Program_Measurement_Keys("selected", keys));
	QCOMPARE(keys, QStringList({"hip", "thigh"}));
	QVERIFY(query.exec(
	    "INSERT INTO "
	    "program_day_exercises(id,program_day_id,exercise_id,order_index,sets,reps_min,reps_max,weight_mode) "
	    "VALUES('assigned-bench','selected-day','builtin-bench-press',1,3,8,10,'byFeel'), "
	    "('duplicate-squat','selected-day','builtin-squat',2,3,8,10,'byFeel')"));
	QVERIFY(repository.Load_Program_Measurement_Keys("selected", keys));
	QCOMPARE(keys, QStringList({"chest", "hip", "arm", "thigh", "shoulders"}));
	QVERIFY(query.exec("DELETE FROM program_day_exercises WHERE id IN ('assigned-bench','duplicate-squat')"));
	// Все группы кэша важнее единственной группы FK-зеркала.
	QVERIFY(query.exec("UPDATE exercises SET muscle_group='chest' WHERE id='builtin-squat'"));
	QVERIFY(repository.Load_Program_Measurement_Keys("selected", keys));
	QCOMPARE(keys, QStringList({"hip", "thigh"}));
	// Явно пустой список кэша не должен возвращать группу из зеркала.
	QVERIFY(query.exec("UPDATE exercises_cache SET muscle_groups='' WHERE id='builtin-squat'"));
	QVERIFY(repository.Load_Program_Measurement_Keys("selected", keys));
	QVERIFY(keys.isEmpty());
	QVERIFY(query.exec("DELETE FROM exercises_cache WHERE id='builtin-squat'"));
	QVERIFY(query.exec("UPDATE exercises SET muscle_group='quads,glutes' WHERE id='builtin-squat'"));
	QVERIFY(repository.Load_Program_Measurement_Keys("selected", keys));
	QCOMPARE(keys, QStringList({"hip", "thigh"}));
	QVERIFY(query.exec("UPDATE program_day_exercises SET exercise_id='builtin-calf-raise' WHERE id='squat'"));
	QVERIFY(repository.Load_Program_Measurement_Keys("selected", keys));
	QCOMPARE(keys, QStringList({"calves"}));
	QVERIFY(query.exec("DELETE FROM program_day_exercises WHERE id='squat'"));
	QVERIFY(repository.Load_Program_Measurement_Keys("selected", keys));
	QVERIFY(keys.isEmpty());
	QVERIFY(repository.Load_Program_Measurement_Keys("", keys));
	QVERIFY(keys.isEmpty());
	QVERIFY(repository.Load_Program_Measurement_Keys("missing", keys));
	QVERIFY(keys.isEmpty());
	saved = Make_Measurement(saved.User_Id, saved.Date);
	saved.Has_Weight_Kg = true;
	saved.Weight_Kg = 79;
	QVERIFY(repository.Save_Measurement(saved));
	QVERIFY(repository.Find_Measurement(saved.User_Id, saved.Date, loaded));
	QCOMPARE(loaded.Weight_Kg, 79.0);
	QCOMPARE(loaded.Neck_Cm, 38.0);
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Repository_Test::programSelectionClearsOutputOnDatabaseFailure()
{
	AMeasurement_Repository repository(0);
	QStringList keys({"chest"});

	QVERIFY(!repository.Load_Program_Measurement_Keys("selected", keys));
	QVERIFY(keys.isEmpty());
	QVERIFY(!repository.Get_Last_Error().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
QTEST_MAIN(AMeasurement_Repository_Test)

#include "tst_Measurement_Repository.moc"
