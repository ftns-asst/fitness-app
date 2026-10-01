#include "Data/Database/Sql_Database.h"
#include "Data/Program/Workout_Repository.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

// Репозиторий фактов: чтение сессий и подходов. Проверяем фильтры и то, что
// соло-сессия не попадает в факты программы (инвариант 18 §4).
class AWorkout_Repository_Test : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void loadsSessionsWithAllFields();
	void filtersSessionsByProgramAndDate();
	void loadsSetsForGivenSessions();
	void programFactsExcludeSoloSessions();
	void emptyDatabaseReturnsEmptyLists();

private:
	void Seed_Exercise();
	void Seed_Program();
	void Seed_Session(const QString &in_id, const QString &in_program_id, const QString &in_date,
	                  EWorkout_State in_state, const QString &in_deviation = QString());
	void Seed_Set(const QString &in_id, const QString &in_session_id, double in_weight, int in_reps, double in_rpe,
	              bool in_done, bool in_warmup);

	QTemporaryDir Directory;
	AsSql_Database Database;
	AWorkout_Repository *Repository = 0;
};
//----------------------------------------------------------------------------
void AWorkout_Repository_Test::initTestCase()
{
	QVERIFY(Directory.isValid());
	QVERIFY(Database.Open(Directory.filePath("workout_repository.sqlite3")));
	Repository = new AWorkout_Repository(&Database, this);
	Seed_Exercise();
	Seed_Program();
}
//----------------------------------------------------------------------------
void AWorkout_Repository_Test::Seed_Exercise()
{
	QSqlQuery query(Database.Sql_Connection());

	query.prepare("INSERT INTO exercises(id, title, muscle_group, equipment, is_custom, created_at) "
	              "VALUES('ex_squat', 'Присед', 'legs', 'barbell', 0, '2026-09-29T00:00:00Z')");
	QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}
//----------------------------------------------------------------------------
// Минимальная программа: workouts.program_id ссылается на programs(id), без
// реальной строки FK не пустит сессию (инвариант структуры, §4).
//----------------------------------------------------------------------------
void AWorkout_Repository_Test::Seed_Program()
{
	QSqlQuery query(Database.Sql_Connection());

	query.prepare("INSERT INTO programs(id, user_id, title, source_kind, goal, level, schedule_kind, state, revision, "
	              "start_date, weeks_total, term_mode, created_at, updated_at, sync_state) "
	              "VALUES('p1', 'u1', 'Сила', 'scratch', 'strength', 'beginner', 'weekday', 'active', 1, "
	              "'2026-09-28', 4, 'fixedWeeks', '2026-09-28T00:00:00Z', '2026-09-28T00:00:00Z', 'local')");
	QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}
//----------------------------------------------------------------------------
void AWorkout_Repository_Test::Seed_Session(const QString &in_id, const QString &in_program_id, const QString &in_date,
                                            EWorkout_State in_state, const QString &in_deviation)
{
	QSqlQuery query(Database.Sql_Connection());

	query.prepare("INSERT INTO workouts(id, user_id, program_id, date, started_at, completed_at, duration_seconds, "
	              "state, deviation, exercise_count, set_count, tonnage_kg, avg_rpe, program_revision, sync_state) "
	              "VALUES(?, 'u1', ?, ?, ?, ?, 3600, ?, ?, 1, 4, 500, 8, 3, 'local')");
	query.addBindValue(in_id);
	query.addBindValue(in_program_id.isEmpty() ? QVariant() : QVariant(in_program_id));
	query.addBindValue(in_date);
	query.addBindValue(in_date + "T18:00:00Z");
	query.addBindValue(in_date + "T19:00:00Z");
	query.addBindValue(To_String(in_state));
	query.addBindValue(in_deviation.isEmpty() ? QVariant() : QVariant(in_deviation));
	QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}
//----------------------------------------------------------------------------
void AWorkout_Repository_Test::Seed_Set(const QString &in_id, const QString &in_session_id, double in_weight,
                                        int in_reps, double in_rpe, bool in_done, bool in_warmup)
{
	QSqlQuery query(Database.Sql_Connection());

	query.prepare("INSERT INTO sets(id, session_id, exercise_id, set_number, is_warmup, weight_kg, reps, rpe, done, "
	              "target_weight_kg, target_reps) VALUES(?, ?, 'ex_squat', 1, ?, ?, ?, ?, ?, 100, 5)");
	query.addBindValue(in_id);
	query.addBindValue(in_session_id);
	query.addBindValue(in_warmup ? 1 : 0);
	query.addBindValue(in_weight);
	query.addBindValue(in_reps);
	if (in_rpe < 0)
		query.addBindValue(QVariant());
	else
		query.addBindValue(in_rpe);
	query.addBindValue(in_done ? 1 : 0);
	QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}
//----------------------------------------------------------------------------
void AWorkout_Repository_Test::loadsSessionsWithAllFields()
{
	QVector<SWorkout_Session> sessions;

	Seed_Session("s1", "p1", "2026-09-28", EWorkout_State::Completed, "asPlanned");
	Seed_Set("set1", "s1", 100, 5, 8, true, false);

	QVERIFY(Repository->Load_Sessions("u1", "p1", QString(), QString(), sessions));
	QCOMPARE(sessions.size(), 1);
	QCOMPARE(sessions.at(0).ID, QString("s1"));
	QCOMPARE(sessions.at(0).User_Id, QString("u1"));
	QCOMPARE(sessions.at(0).Program_Id, QString("p1"));
	QCOMPARE(sessions.at(0).Date, QString("2026-09-28"));
	QCOMPARE(sessions.at(0).Duration_Seconds, 3600);
	QCOMPARE(sessions.at(0).State, EWorkout_State::Completed);
	QCOMPARE(sessions.at(0).Has_Deviation, true);
	QCOMPARE(sessions.at(0).Program_Revision, 3);
	QCOMPARE(sessions.at(0).Tonnage_Kg, 500.0);
}
//----------------------------------------------------------------------------
void AWorkout_Repository_Test::filtersSessionsByProgramAndDate()
{
	QVector<SWorkout_Session> sessions;

	Seed_Session("s2", "p1", "2026-09-30", EWorkout_State::Completed);
	Seed_Session("s3", QString(), "2026-10-01", EWorkout_State::Completed);

	// Только программа p1 и только её диапазон дат.
	QVERIFY(Repository->Load_Sessions("u1", "p1", "2026-09-29", "2026-10-05", sessions));
	QCOMPARE(sessions.size(), 1);
	QCOMPARE(sessions.at(0).ID, QString("s2"));

	// Пустой program_id — все сессии пользователя, включая соло (§4.18).
	QVERIFY(Repository->Load_Sessions("u1", QString(), QString(), QString(), sessions));
	QVERIFY(sessions.size() >= 3);
}
//----------------------------------------------------------------------------
void AWorkout_Repository_Test::loadsSetsForGivenSessions()
{
	QVector<SSet_Log> sets;

	Seed_Session("s4", "p1", "2026-10-02", EWorkout_State::Completed);
	Seed_Set("set2", "s4", 105, 3, -1, true, false);
	Seed_Set("set3", "s4", 40, 10, -1, true, true);

	QVERIFY(Repository->Load_Sets(QVector<QString>{"s4"}, sets));
	QCOMPARE(sets.size(), 2);
	QCOMPARE(sets.at(0).ID, QString("set2"));
	QCOMPARE(sets.at(0).Session_Id, QString("s4"));
	QCOMPARE(sets.at(0).Exercise_Id, QString("ex_squat"));
	QCOMPARE(sets.at(0).Weight_Kg, 105.0);
	QCOMPARE(sets.at(0).Reps, 3);
	// RPE не введён — Has_Rpe = false, а не 0 (§3.8).
	QVERIFY(sets.at(0).Has_Rpe == false);
	QCOMPARE(sets.at(0).Is_Warmup, false);
	// Снимок предписания сохраняется: правка плана не переписывает факт.
	QVERIFY(sets.at(0).Has_Target_Weight_Kg);
	QCOMPARE(sets.at(0).Target_Weight_Kg, 100.0);
	QCOMPARE(sets.at(1).Is_Warmup, true);
}
//----------------------------------------------------------------------------
void AWorkout_Repository_Test::programFactsExcludeSoloSessions()
{
	QVector<SWorkout_Session> sessions;
	QVector<SSet_Log> sets;

	Seed_Session("s5", "p1", "2026-10-06", EWorkout_State::Completed);
	Seed_Session("s6", QString(), "2026-10-07", EWorkout_State::Completed);
	Seed_Set("set4", "s5", 100, 5, 8, true, false);
	Seed_Set("set5", "s6", 100, 5, 8, true, false);

	QVERIFY(Repository->Load_Program_Facts("u1", "p1", QString(), QString(), sessions, sets));

	// Тесты делят одну БД, поэтому считаем точно: в фактах программы есть
	// s5 и нет соло-сессии s6 (program_id NULL, §4 инвариант 18).
	QVERIFY(sessions.size() >= 1);

	bool has_program_session = false;
	bool has_solo_session = false;

	for (int i = 0; i < sessions.size(); ++i)
	{
		if (sessions.at(i).ID == "s5")
			has_program_session = true;

		if (sessions.at(i).ID == "s6")
			has_solo_session = true;
	}

	QVERIFY(has_program_session);
	QVERIFY(has_solo_session == false);

	// Подходы соло-сессии в факты программы не подмешиваются.
	for (int i = 0; i < sets.size(); ++i)
		QVERIFY(sets.at(i).Session_Id != "s6");
}
//----------------------------------------------------------------------------
void AWorkout_Repository_Test::emptyDatabaseReturnsEmptyLists()
{
	QVector<SWorkout_Session> sessions;
	QVector<SSet_Log> sets;

	Seed_Session("s7", "p1", "2026-11-01", EWorkout_State::Completed);

	QVERIFY(Repository->Load_Sessions("u1", "p1", "2027-01-01", "2027-01-31", sessions));
	QVERIFY(sessions.isEmpty());
	QVERIFY(Repository->Load_Sets(QVector<QString>{"нет-сессии"}, sets));
	QVERIFY(sets.isEmpty());
}

QTEST_GUILESS_MAIN(AWorkout_Repository_Test)
#include "tst_Workout_Repository.moc"
