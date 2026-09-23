#include "Data/Database/Sql_Database.h"

#include <QFile>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class ASql_Database_Test : public QObject
{
	Q_OBJECT

private slots:
	void createsSchemaV1();
	void migrationIsIdempotent();
	void storesBackendAlignedCache();
	void enforcesConstraintsAndForeignKeys();
	void rejectsNewerSchema();
	void reportsOpenError();
};

//----------------------------------------------------------------------------
static QSqlDatabase Open_Inspection_Database(const QString &in_path, const QString &in_name)
{
	QSqlDatabase database;

	database = QSqlDatabase::addDatabase("QSQLITE", in_name);
	database.setDatabaseName(in_path);
	database.open();
	return database;
}
//----------------------------------------------------------------------------
static void Close_Inspection_Database(QSqlDatabase &in_database)
{
	QString name;

	name = in_database.connectionName();
	in_database.close();
	in_database = QSqlDatabase();
	QSqlDatabase::removeDatabase(name);
}
//----------------------------------------------------------------------------
void ASql_Database_Test::createsSchemaV1()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QSqlDatabase inspection;
	QSqlQuery query;
	QStringList tables;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(database.Is_Open());
	QCOMPARE(database.Get_Database_Path(), path);
	QCOMPARE(database.Get_Schema_Version(), 1);
	QCOMPARE(database.Get_Last_Error(), QString());

	inspection = Open_Inspection_Database(path, "inspect_schema");
	QVERIFY2(inspection.isOpen(), qPrintable(inspection.lastError().text()));
	query = QSqlQuery(inspection);
	QVERIFY(query.exec("SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name"));
	while (query.next())
		tables.append(query.value(0).toString());

	for (const QString &table :
	     {"auth_tokens", "profile_cache", "profile_local", "schema_version", "sync_outbox", "user_cache"})
		QVERIFY2(tables.contains(table), qPrintable(QString("Нет таблицы %1").arg(table)));
	QVERIFY(tables.contains("users") == false);
	QVERIFY(tables.contains("tokens") == false);
	Close_Inspection_Database(inspection);
}
//----------------------------------------------------------------------------
void ASql_Database_Test::migrationIsIdempotent()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(database.Migrate());
	QVERIFY(database.Migrate());
	QCOMPARE(database.Get_Schema_Version(), AsSql_Database::Get_Current_Schema_Version());

	database.Close();
	QVERIFY(database.Open(path));
	QCOMPARE(database.Get_Schema_Version(), 1);
}
//----------------------------------------------------------------------------
void ASql_Database_Test::storesBackendAlignedCache()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QSqlDatabase inspection;
	QSqlQuery query;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	inspection = Open_Inspection_Database(path, "inspect_data");
	QVERIFY(inspection.isOpen());
	query = QSqlQuery(inspection);
	QVERIFY(query.exec("PRAGMA foreign_keys = ON"));
	query.prepare("INSERT INTO user_cache(id, email, display_name, created_at, updated_at) VALUES(?, ?, ?, ?, ?)");
	query.addBindValue("2e8cbeec-3528-45bc-908b-cdf76944d9c3");
	query.addBindValue("test@gmail.com");
	query.addBindValue("somename");
	query.addBindValue("2026-09-20T10:00:00Z");
	query.addBindValue("2026-09-20T10:00:00Z");
	QVERIFY2(query.exec(), qPrintable(query.lastError().text()));

	query.prepare("INSERT INTO profile_cache(user_id, age, gender, height_cm, weight_kg, updated_at) "
	              "VALUES(?, ?, ?, ?, ?, ?)");
	query.addBindValue("2e8cbeec-3528-45bc-908b-cdf76944d9c3");
	query.addBindValue(31);
	query.addBindValue("male");
	query.addBindValue(179);
	query.addBindValue(74.125);
	query.addBindValue("2026-09-20T10:00:00Z");
	QVERIFY2(query.exec(), qPrintable(query.lastError().text()));

	QVERIFY(query.exec("SELECT display_name, height_cm, weight_kg FROM user_cache "
	                   "JOIN profile_cache ON profile_cache.user_id = user_cache.id"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toString(), QString("somename"));
	QCOMPARE(query.value(1).toInt(), 179);
	QCOMPARE(query.value(2).toDouble(), 74.125);
	Close_Inspection_Database(inspection);
}
//----------------------------------------------------------------------------
void ASql_Database_Test::enforcesConstraintsAndForeignKeys()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QSqlDatabase inspection;
	QSqlQuery query;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	inspection = Open_Inspection_Database(path, "inspect_constraints");
	QVERIFY(inspection.isOpen());
	query = QSqlQuery(inspection);
	QVERIFY(query.exec("PRAGMA foreign_keys = ON"));
	QVERIFY(query.exec("INSERT INTO profile_cache(user_id, age) VALUES('missing', 31)") == false);
	QVERIFY(query.exec("INSERT INTO user_cache(id, email, display_name, created_at) "
	                   "VALUES('u1', 'a@example.com', 'A', '2026-09-20T10:00:00Z')"));
	QVERIFY(query.exec("INSERT INTO user_cache(id, email, display_name, created_at) "
	                   "VALUES('u2', 'a@example.com', 'B', '2026-09-20T10:00:00Z')") == false);
	QVERIFY(query.exec("INSERT INTO profile_cache(user_id, age, weight_kg) VALUES('u1', 151, 70)") == false);
	QVERIFY(query.exec("INSERT INTO profile_cache(user_id, age, weight_kg) VALUES('u1', 31, 1000)") == false);
	Close_Inspection_Database(inspection);
}
//----------------------------------------------------------------------------
void ASql_Database_Test::rejectsNewerSchema()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QSqlDatabase inspection;
	QSqlQuery query;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("future.sqlite3");
	inspection = Open_Inspection_Database(path, "prepare_future");
	QVERIFY(inspection.isOpen());
	query = QSqlQuery(inspection);
	QVERIFY(query.exec("CREATE TABLE schema_version(id INTEGER PRIMARY KEY, version INTEGER NOT NULL)"));
	QVERIFY(query.exec("INSERT INTO schema_version(id, version) VALUES(1, 999)"));
	Close_Inspection_Database(inspection);

	QVERIFY(database.Open(path) == false);
	QVERIFY(database.Is_Open() == false);
	QVERIFY(database.Get_Last_Error().contains("новее поддерживаемой"));
}
//----------------------------------------------------------------------------
void ASql_Database_Test::reportsOpenError()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("existing-file");
	QFile file(path);
	QVERIFY(file.open(QIODevice::WriteOnly));
	file.write("not-a-directory");
	file.close();

	QVERIFY(database.Open(path + "/cache.sqlite3") == false);
	QVERIFY(database.Is_Open() == false);
	QVERIFY(database.Get_Last_Error().isEmpty() == false);
}
//----------------------------------------------------------------------------

QTEST_MAIN(ASql_Database_Test)

#include "tst_Sql_Database.moc"
