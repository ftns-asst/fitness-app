#include "Data/Database/Sql_Database.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Stats_Repository.h"
#include "Domain/Program_Model.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class AStats_Repository_Test : public QObject
{
	Q_OBJECT

private slots:
	void programStatsRoundTrip();
	void programStatsValidateAndUpsert();
	void lifetimeStatsRoundTrip();
	void invalidationScope();
	void closedDatabaseFails();

private:
	QString Make_Database_Path();
	SProgram Make_Program(const QString &in_id, const QString &in_user_id);
	void Seed_Program(AProgram_Repository &in_repository, const QString &in_id, const QString &in_user_id);

	QTemporaryDir Directory;
	int Counter = 0;
};

//----------------------------------------------------------------------------
QString AStats_Repository_Test::Make_Database_Path()
{
	++Counter;

	return Directory.filePath(QString("stats_%1.sqlite3").arg(Counter));
}
//----------------------------------------------------------------------------
SProgram AStats_Repository_Test::Make_Program(const QString &in_id, const QString &in_user_id)
{
	SProgram program;

	program.ID = in_id;
	program.User_Id = in_user_id;
	program.Title = QString("Программа %1").arg(in_id);
	program.Source_Kind = EProgram_Source_Kind::Scratch;
	program.Goal = EProgram_Goal::Strength;
	program.Level = EProgram_Level::Beginner;
	program.Schedule_Kind = ESchedule_Kind::Weekday;
	program.State = EProgram_State::Draft;
	program.Revision = 1;
	program.Term.Start_Date = "2026-09-29";
	program.Term.Weeks_Total = 12;
	program.Term.Mode = ETerm_Mode::FixedWeeks;

	return program;
}
//----------------------------------------------------------------------------
void AStats_Repository_Test::Seed_Program(AProgram_Repository &in_repository, const QString &in_id,
                                          const QString &in_user_id)
{
	QVERIFY(in_repository.Insert_Program(Make_Program(in_id, in_user_id)));
}
//----------------------------------------------------------------------------
void AStats_Repository_Test::programStatsRoundTrip()
{
	AsSql_Database database;
	AProgram_Repository program_repository(&database);
	AStats_Repository repository(&database);
	SStats_Cache_Entry entry;
	bool is_fresh = true;

	QVERIFY(database.Open(Make_Database_Path()));
	Seed_Program(program_repository, "p1", "u1");

	// До пересчёта строки нет: «нечего считать» — не ошибка для читателя.
	QVERIFY(repository.Load_Program_Stats("p1", entry) == false);
	QVERIFY(repository.Get_Last_Error().isEmpty() == false);
	QVERIFY(repository.Is_Program_Stats_Fresh("p1", 1, is_fresh));
	QVERIFY(is_fresh == false);

	QVERIFY(repository.Save_Program_Stats("p1", 1,
	                                      "{\"plannedSessions\":36,\"completedSessions\":30,"
	                                      "\"adherencePercent\":83,\"tonnageKg\":125400.5}"));
	QVERIFY(repository.Load_Program_Stats("p1", entry));
	QCOMPARE(entry.Owner_Id, QString("p1"));
	QCOMPARE(entry.Program_Revision, 1);
	QVERIFY(entry.Computed_At.isEmpty() == false);
	QVERIFY(entry.Payload_Json.contains("adherencePercent"));

	// Свежесть привязана к ревизии структуры программы (§5.4).
	QVERIFY(repository.Is_Program_Stats_Fresh("p1", 1, is_fresh));
	QVERIFY(is_fresh);
	QVERIFY(repository.Is_Program_Stats_Fresh("p1", 2, is_fresh));
	QVERIFY(is_fresh == false);
}
//----------------------------------------------------------------------------
void AStats_Repository_Test::programStatsValidateAndUpsert()
{
	AsSql_Database database;
	AProgram_Repository program_repository(&database);
	AStats_Repository repository(&database);
	SStats_Cache_Entry entry;
	QSqlQuery query;

	QVERIFY(database.Open(Make_Database_Path()));
	Seed_Program(program_repository, "p1", "u1");
	query = QSqlQuery(database.Sql_Connection());

	QVERIFY(repository.Save_Program_Stats("p1", 1, "{\"tonnageKg\":100}"));
	QVERIFY(repository.Save_Program_Stats("p1", 2, "{\"tonnageKg\":200}"));

	QVERIFY(repository.Load_Program_Stats("p1", entry));
	QCOMPARE(entry.Program_Revision, 2);
	QCOMPARE(entry.Payload_Json, QString("{\"tonnageKg\":200}"));
	QVERIFY(query.exec("SELECT COUNT(*) FROM program_stats WHERE program_id = 'p1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 1);

	// Нагрузка обязана быть корректным JSON-объектом (§5.4).
	QVERIFY(repository.Save_Program_Stats("p1", 3, QString()) == false);
	QVERIFY(repository.Save_Program_Stats("p1", 3, "{oops}") == false);
	QVERIFY(repository.Save_Program_Stats("p1", 3, "[1, 2]") == false);
	QVERIFY(repository.Get_Last_Error().isEmpty() == false);

	// Ревизия нужна для инвалидации, поэтому она обязательна.
	QVERIFY(repository.Save_Program_Stats("p1", 0, "{\"tonnageKg\":300}") == false);
	QVERIFY(repository.Save_Program_Stats(QString(), 1, "{\"tonnageKg\":300}") == false);

	// FK: статистику нельзя завести для несуществующей программы.
	QVERIFY(repository.Save_Program_Stats("missing", 1, "{\"tonnageKg\":300}") == false);

	QVERIFY(repository.Load_Program_Stats("p1", entry));
	QCOMPARE(entry.Program_Revision, 2);
}
//----------------------------------------------------------------------------
void AStats_Repository_Test::lifetimeStatsRoundTrip()
{
	AsSql_Database database;
	AStats_Repository repository(&database);
	SStats_Cache_Entry entry;
	QSqlQuery query;

	QVERIFY(database.Open(Make_Database_Path()));
	query = QSqlQuery(database.Sql_Connection());

	QVERIFY(repository.Load_Lifetime_Stats("u1", entry) == false);
	QVERIFY(repository.Get_Last_Error().isEmpty() == false);

	QVERIFY(repository.Save_Lifetime_Stats("u1", "{\"totalSessions\":48,\"totalTonnageKg\":210000}"));
	QVERIFY(repository.Load_Lifetime_Stats("u1", entry));
	QCOMPARE(entry.Owner_Id, QString("u1"));
	QCOMPARE(entry.Program_Revision, 0);
	QVERIFY(entry.Computed_At.isEmpty() == false);
	QCOMPARE(entry.Payload_Json, QString("{\"totalSessions\":48,\"totalTonnageKg\":210000}"));

	QVERIFY(repository.Save_Lifetime_Stats("u1", "{\"totalSessions\":49,\"totalTonnageKg\":214000}"));
	QVERIFY(repository.Load_Lifetime_Stats("u1", entry));
	QCOMPARE(entry.Payload_Json, QString("{\"totalSessions\":49,\"totalTonnageKg\":214000}"));
	QVERIFY(query.exec("SELECT COUNT(*) FROM lifetime_stats WHERE user_id = 'u1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 1);

	QVERIFY(repository.Save_Lifetime_Stats("u1", "[]") == false);
	QVERIFY(repository.Save_Lifetime_Stats(QString(), "{}") == false);
}
//----------------------------------------------------------------------------
void AStats_Repository_Test::invalidationScope()
{
	AsSql_Database database;
	AProgram_Repository program_repository(&database);
	AStats_Repository repository(&database);
	SStats_Cache_Entry entry;

	QVERIFY(database.Open(Make_Database_Path()));
	Seed_Program(program_repository, "p1", "u1");
	Seed_Program(program_repository, "p2", "u1");
	Seed_Program(program_repository, "p3", "u2");

	QVERIFY(repository.Save_Program_Stats("p1", 1, "{\"tonnageKg\":100}"));
	QVERIFY(repository.Save_Program_Stats("p2", 1, "{\"tonnageKg\":200}"));
	QVERIFY(repository.Save_Program_Stats("p3", 1, "{\"tonnageKg\":300}"));
	QVERIFY(repository.Save_Lifetime_Stats("u1", "{\"totalSessions\":10}"));
	QVERIFY(repository.Save_Lifetime_Stats("u2", "{\"totalSessions\":20}"));

	// Точечная инвалидация одной программы не трогает остальные.
	QVERIFY(repository.Invalidate_Program_Stats("p1"));
	QVERIFY(repository.Load_Program_Stats("p1", entry) == false);
	QVERIFY(repository.Load_Program_Stats("p2", entry));
	QVERIFY(repository.Load_Program_Stats("p3", entry));

	QVERIFY(repository.Save_Program_Stats("p1", 1, "{\"tonnageKg\":100}"));

	// Инвалидация общей статистики не трогает кэш программ.
	QVERIFY(repository.Invalidate_Lifetime_Stats("u1"));
	QVERIFY(repository.Load_Lifetime_Stats("u1", entry) == false);
	QVERIFY(repository.Load_Lifetime_Stats("u2", entry));
	QVERIFY(repository.Load_Program_Stats("p1", entry));

	// Инвалидация по пользователю — программы пользователя + его общая
	// статистика одной транзакцией (запись факта, завершение программы).
	QVERIFY(repository.Save_Lifetime_Stats("u1", "{\"totalSessions\":11}"));
	QVERIFY(repository.Invalidate_User_Stats("u1"));
	QVERIFY(repository.Load_Program_Stats("p1", entry) == false);
	QVERIFY(repository.Load_Program_Stats("p2", entry) == false);
	QVERIFY(repository.Load_Lifetime_Stats("u1", entry) == false);

	QVERIFY(repository.Load_Program_Stats("p3", entry));
	QVERIFY(repository.Load_Lifetime_Stats("u2", entry));
}
//----------------------------------------------------------------------------
void AStats_Repository_Test::closedDatabaseFails()
{
	AsSql_Database closed;
	AStats_Repository repository(&closed);
	SStats_Cache_Entry entry;
	bool is_fresh = true;

	QVERIFY(repository.Save_Program_Stats("p1", 1, "{}") == false);
	QVERIFY(repository.Save_Lifetime_Stats("u1", "{}") == false);
	QVERIFY(repository.Load_Program_Stats("p1", entry) == false);
	QVERIFY(repository.Load_Lifetime_Stats("u1", entry) == false);
	QVERIFY(repository.Is_Program_Stats_Fresh("p1", 1, is_fresh) == false);
	QVERIFY(repository.Invalidate_Program_Stats("p1") == false);
	QVERIFY(repository.Invalidate_Lifetime_Stats("u1") == false);
	QVERIFY(repository.Invalidate_User_Stats("u1") == false);
	QVERIFY(repository.Get_Last_Error().isEmpty() == false);
}
//----------------------------------------------------------------------------

QTEST_MAIN(AStats_Repository_Test)

#include "tst_Stats_Repository.moc"
//----------------------------------------------------------------------------
