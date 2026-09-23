#include "Data/Database/Sql_Database.h"

#include <QDir>
#include <QFileInfo>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>

//----------------------------------------------------------------------------
AsSql_Database::AsSql_Database(QObject *in_parent) : QObject(in_parent)
{
	Connection_Name_Text = QString("training_app_%1").arg(QUuid::createUuid().toString(QUuid::WithoutBraces));
}
//----------------------------------------------------------------------------
AsSql_Database::~AsSql_Database()
{
	Close();
}
//----------------------------------------------------------------------------
bool AsSql_Database::Open(const QString &in_path)
{
	QString path;
	QSqlDatabase database;

	Close();
	Last_Error_Text.clear();
	Current_Version = 0;
	path = in_path.trimmed().isEmpty() ? Get_Default_Database_Path() : in_path;

	if (path != ":memory:" && Ensure_Directory(path) == false)
		return false;

	database = QSqlDatabase::addDatabase("QSQLITE", Connection_Name_Text);
	database.setDatabaseName(path);
	Database_Handle = new QSqlDatabase(database);
	Database_File_Path = path;

	if (Database_Handle->open() == false)
	{
		Set_Last_Error(QString("Не удалось открыть SQLite: %1").arg(Database_Handle->lastError().text()));
		Close();
		return false;
	}

	// Несколько desktop smoke-процессов могут одновременно открывать общий
	// application cache. Даём SQLite дождаться короткой миграционной блокировки.
	if (Execute_SQL("PRAGMA busy_timeout = 5000") == false || Execute_SQL("PRAGMA foreign_keys = ON") == false ||
	    Migrate() == false)
	{
		Close();
		return false;
	}

	return true;
}
//----------------------------------------------------------------------------
void AsSql_Database::Close()
{
	QString connection_name;

	connection_name = Connection_Name_Text;

	if (Database_Handle != 0)
	{
		Database_Handle->close();
		delete Database_Handle;
		Database_Handle = 0;
	}

	if (QSqlDatabase::contains(connection_name))
		QSqlDatabase::removeDatabase(connection_name);

	Database_File_Path.clear();
	Current_Version = 0;
}
//----------------------------------------------------------------------------
bool AsSql_Database::Migrate()
{
	int version;

	if (Is_Open() == false)
		return Set_Last_Error("SQLite не открыта");

	if (Database_Handle->transaction() == false)
		return Set_Last_Error(QString("Не удалось начать миграцию: %1").arg(Database_Handle->lastError().text()));

	if (Execute_SQL("CREATE TABLE IF NOT EXISTS schema_version ("
	                "id INTEGER PRIMARY KEY CHECK (id = 1), "
	                "version INTEGER NOT NULL CHECK (version >= 0))") == false ||
	    Read_Schema_Version(version) == false)
	{
		Database_Handle->rollback();

		return false;
	}

	if (version > Get_Current_Schema_Version())
	{
		Database_Handle->rollback();

		return Set_Last_Error(
		    QString("Версия SQLite %1 новее поддерживаемой %2").arg(version).arg(Get_Current_Schema_Version()));
	}

	if (version < 1 && (Create_Schema_V1() == false || Write_Schema_Version(1) == false))
	{
		Database_Handle->rollback();

		return false;
	}

	if (Database_Handle->commit() == false)
		return Set_Last_Error(QString("Не удалось завершить миграцию: %1").arg(Database_Handle->lastError().text()));

	Current_Version = Get_Current_Schema_Version();
	Last_Error_Text.clear();

	return true;
}
//----------------------------------------------------------------------------
bool AsSql_Database::Is_Open() const
{
	return Database_Handle != 0 && Database_Handle->isOpen();
}
//----------------------------------------------------------------------------
QString AsSql_Database::Get_Database_Path() const
{
	return Database_File_Path;
}
//----------------------------------------------------------------------------
int AsSql_Database::Get_Schema_Version() const
{
	return Current_Version;
}
//----------------------------------------------------------------------------
QString AsSql_Database::Get_Last_Error() const
{
	return Last_Error_Text;
}
//----------------------------------------------------------------------------
QString AsSql_Database::Get_Default_Database_Path()
{
	return QDir(QStandardPaths::writableLocation(QStandardPaths::AppDataLocation)).filePath("training-app.sqlite3");
}
//----------------------------------------------------------------------------
int AsSql_Database::Get_Current_Schema_Version()
{
	return 1;
}
//----------------------------------------------------------------------------
bool AsSql_Database::Ensure_Directory(const QString &in_path)
{
	QFileInfo file_info(in_path);
	QDir directory;

	directory = file_info.absoluteDir();
	if (directory.exists() || directory.mkpath(".") || directory.exists())
		return true;

	return Set_Last_Error(QString("Не удалось создать каталог SQLite: %1").arg(directory.absolutePath()));
}
//----------------------------------------------------------------------------
bool AsSql_Database::Set_Last_Error(const QString &in_error)
{
	Last_Error_Text = in_error;
	qWarning("SQLite: %s", qUtf8Printable(Last_Error_Text));
	return false;
}
//----------------------------------------------------------------------------
bool AsSql_Database::Execute_SQL(const QString &in_sql)
{
	QSqlQuery query(*Database_Handle);

	if (query.exec(in_sql))
		return true;

	return Set_Last_Error(QString("Ошибка SQLite: %1").arg(query.lastError().text()));
}
//----------------------------------------------------------------------------
bool AsSql_Database::Create_Schema_V1()
{
	const QStringList statements = {
	    "CREATE TABLE IF NOT EXISTS user_cache (id TEXT PRIMARY KEY NOT NULL, email TEXT NOT NULL UNIQUE, "
	    "display_name TEXT NOT NULL, created_at TEXT NOT NULL, updated_at TEXT)",
	    "CREATE TABLE IF NOT EXISTS profile_cache (user_id TEXT PRIMARY KEY NOT NULL REFERENCES user_cache(id) "
	    "ON DELETE CASCADE, age INTEGER CHECK (age IS NULL OR age BETWEEN 0 AND 150), gender TEXT, "
	    "height_cm INTEGER CHECK (height_cm IS NULL OR height_cm BETWEEN 0 AND 300), "
	    "weight_kg REAL CHECK (weight_kg IS NULL OR weight_kg BETWEEN 0 AND 999.999), updated_at TEXT)",
	    "CREATE TABLE IF NOT EXISTS auth_tokens (id INTEGER PRIMARY KEY CHECK (id = 1), user_id TEXT NOT NULL, "
	    "access_token TEXT NOT NULL, refresh_token TEXT NOT NULL, access_expires_at TEXT, refresh_expires_at TEXT, "
	    "updated_at TEXT NOT NULL)",
	    "CREATE TABLE IF NOT EXISTS profile_local (user_id TEXT PRIMARY KEY NOT NULL, level TEXT, goal TEXT, "
	    "workouts_per_week INTEGER CHECK (workouts_per_week IS NULL OR workouts_per_week BETWEEN 0 AND 14), "
	    "updated_at TEXT NOT NULL)",
	    "CREATE TABLE IF NOT EXISTS sync_outbox (id INTEGER PRIMARY KEY AUTOINCREMENT, entity_type TEXT NOT NULL, "
	    "entity_id TEXT NOT NULL, operation TEXT NOT NULL, payload_json TEXT NOT NULL, created_at TEXT NOT NULL, "
	    "attempt_count INTEGER NOT NULL DEFAULT 0 CHECK (attempt_count >= 0), next_attempt_at TEXT, last_error "
	    "TEXT)",
	    "CREATE INDEX IF NOT EXISTS sync_outbox_next_attempt_idx ON sync_outbox(next_attempt_at, id)"};

	for (int i = 0; i < statements.size(); ++i)
	{
		if (Execute_SQL(statements.at(i)) == false)
			return false;
	}

	return true;
}
//----------------------------------------------------------------------------
bool AsSql_Database::Read_Schema_Version(int &out_version)
{
	QSqlQuery query(*Database_Handle);

	out_version = 0;
	if (query.exec("SELECT version FROM schema_version WHERE id = 1") == false)
		return Set_Last_Error(QString("Не удалось прочитать версию SQLite: %1").arg(query.lastError().text()));

	if (query.next())
		out_version = query.value(0).toInt();

	return true;
}
//----------------------------------------------------------------------------
bool AsSql_Database::Write_Schema_Version(int in_version)
{
	QSqlQuery query(*Database_Handle);

	query.prepare("INSERT INTO schema_version(id, version) VALUES(1, ?) "
	              "ON CONFLICT(id) DO UPDATE SET version = excluded.version");
	query.addBindValue(in_version);
	if (query.exec())
		return true;

	return Set_Last_Error(QString("Не удалось записать версию SQLite: %1").arg(query.lastError().text()));
}
//----------------------------------------------------------------------------
QString AsSql_Database::Connection_Name() const
{
	return Connection_Name_Text;
}
//----------------------------------------------------------------------------
QSqlDatabase AsSql_Database::Sql_Connection() const
{
	if (Database_Handle == 0)
		return QSqlDatabase();

	return *Database_Handle;
}
//----------------------------------------------------------------------------
