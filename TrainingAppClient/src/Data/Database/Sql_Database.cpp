#include "Data/Database/Sql_Database.h"

#include <QDir>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QStringList>
#include <QUuid>

Q_LOGGING_CATEGORY(SQL_Log, "trainingapp.sqlite")

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

	// Несколько desktop smoke-процессов могут одновременно открывать
	// общий application cache. Даём SQLite дождаться короткой миграционной
	// блокировки.
	if (Execute_SQL("PRAGMA busy_timeout = 5000") == false || Execute_SQL("PRAGMA foreign_keys = ON") == false ||
	    Migrate() == false)
	{
		Close();
		return false;
	}

	qCInfo(SQL_Log).noquote() << "opened" << Database_File_Path << "schema" << Current_Version;

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

	if (version < 2 && (Create_Schema_V2() == false || Write_Schema_Version(2) == false))
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
	// v2 — модель глобальной программы (срез W, docs/02-program-model.md §8).
	return 2;
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
// Схема v2 — модель глобальной программы (срез W, docs/02-program-model.md §8):
// программы, цели, расписание, дни и предписания, правила и
// отметки, ревизии, факты (workouts/sets), замеры тела, кэш статистики и
// резерв питания. Каталог plans/plan_days/day_exercises и seed — миграция v3 (фаза
// B).
//----------------------------------------------------------------------------
bool AsSql_Database::Create_Schema_V2()
{
	const QStringList statements = {
	    "CREATE TABLE IF NOT EXISTS exercises (id TEXT PRIMARY KEY NOT NULL, title TEXT NOT NULL, subtitle TEXT, "
	    "muscle_group TEXT NOT NULL, equipment TEXT NOT NULL, is_custom INTEGER NOT NULL DEFAULT 0 "
	    "CHECK (is_custom IN (0, 1)), description TEXT, created_at TEXT NOT NULL)",

	    "CREATE TABLE IF NOT EXISTS programs (id TEXT PRIMARY KEY NOT NULL, user_id TEXT NOT NULL, title TEXT NOT "
	    "NULL, "
	    "source_kind TEXT NOT NULL CHECK (source_kind IN ('template', 'copy', 'scratch', 'matched')), "
	    "source_template_id TEXT, "
	    "goal TEXT NOT NULL CHECK (goal IN ('strength', 'muscle', 'fitness', 'fatLoss', 'endurance')), "
	    "level TEXT NOT NULL CHECK (level IN ('beginner', 'intermediate', 'advanced')), "
	    "schedule_kind TEXT NOT NULL CHECK (schedule_kind IN ('weekday', 'rotation')), "
	    "state TEXT NOT NULL CHECK (state IN ('draft', 'active', 'paused', 'completed', 'cancelled', 'archived')), "
	    "revision INTEGER NOT NULL CHECK (revision >= 1), start_date TEXT, actual_start_date TEXT, "
	    "weeks_total INTEGER NOT NULL DEFAULT 0 CHECK (weeks_total >= 0 AND weeks_total <= 52), "
	    "extended_weeks INTEGER NOT NULL DEFAULT 0 CHECK (extended_weeks >= 0), "
	    "term_mode TEXT NOT NULL DEFAULT 'fixedWeeks' CHECK (term_mode IN ('fixedWeeks', 'openEnded')), "
	    "end_date_generated TEXT, created_at TEXT NOT NULL, updated_at TEXT NOT NULL, "
	    "sync_state TEXT NOT NULL DEFAULT 'local' CHECK (sync_state IN ('synced', 'local', 'syncing', 'offline')), "
	    "note TEXT)",

	    "CREATE TABLE IF NOT EXISTS goal_targets (id TEXT PRIMARY KEY NOT NULL, "
	    "program_id TEXT NOT NULL REFERENCES programs(id) ON DELETE CASCADE, "
	    "metric TEXT NOT NULL CHECK (metric IN ('bodyWeightKg', 'workingWeightKg', 'bodyFatPercent', 'measurementCm', "
	    "'workoutsPerWeek', 'tonnagePerWeekKg', 'repsAtWeight')), "
	    "exercise_id TEXT REFERENCES exercises(id) ON DELETE SET NULL, "
	    "measurement_key TEXT CHECK (measurement_key IS NULL OR measurement_key IN ('chest', 'waist', 'hip', 'arm', "
	    "'thigh')), unit TEXT NOT NULL CHECK (unit IN ('kg', 'percent', 'cm', 'count', 'timesPerWeek')), start_value "
	    "REAL, "
	    "target_value REAL NOT NULL, target_weight_kg REAL, target_reps INTEGER, "
	    "direction TEXT NOT NULL CHECK (direction IN ('increase', 'decrease', 'maintain')), target_date TEXT, "
	    "state TEXT NOT NULL DEFAULT 'active' CHECK (state IN ('active', 'achieved', 'dropped')))",

	    "CREATE TABLE IF NOT EXISTS program_days (id TEXT PRIMARY KEY NOT NULL, "
	    "program_id TEXT NOT NULL REFERENCES programs(id) ON DELETE CASCADE, title TEXT NOT NULL, "
	    "kind TEXT NOT NULL CHECK (kind IN ('workout', 'rest', 'cardio', 'mobility')), focus TEXT, "
	    "estimated_minutes INTEGER NOT NULL DEFAULT 0 CHECK (estimated_minutes >= 0), "
	    "order_index INTEGER NOT NULL CHECK (order_index >= 0), source_day_id TEXT, note TEXT)",

	    "CREATE TABLE IF NOT EXISTS program_schedule (id TEXT PRIMARY KEY NOT NULL, "
	    "program_id TEXT NOT NULL REFERENCES programs(id) ON DELETE CASCADE, "
	    "day_of_week INTEGER CHECK (day_of_week IS NULL OR day_of_week BETWEEN 1 AND 7), "
	    "rotation_index INTEGER CHECK (rotation_index IS NULL OR rotation_index >= 1), "
	    "program_day_id TEXT REFERENCES program_days(id) ON DELETE SET NULL, "
	    "kind TEXT NOT NULL CHECK (kind IN ('workout', 'rest', 'optional', 'cardio', 'mobility')), "
	    "is_anchor INTEGER NOT NULL DEFAULT 0 CHECK (is_anchor IN (0, 1)), note TEXT, "
	    "UNIQUE (program_id, day_of_week), UNIQUE (program_id, rotation_index))",

	    "CREATE TABLE IF NOT EXISTS program_day_exercises (id TEXT PRIMARY KEY NOT NULL, "
	    "program_day_id TEXT NOT NULL REFERENCES program_days(id) ON DELETE CASCADE, "
	    "exercise_id TEXT NOT NULL REFERENCES exercises(id), order_index INTEGER NOT NULL CHECK (order_index >= 0), "
	    "sets INTEGER NOT NULL CHECK (sets BETWEEN 1 AND 20), reps_min INTEGER NOT NULL CHECK (reps_min >= 0), "
	    "reps_max INTEGER NOT NULL CHECK (reps_max >= 0), "
	    "weight_mode TEXT NOT NULL CHECK (weight_mode IN ('fixed', 'percentOf1RM', 'byFeel', 'fromLastSession', "
	    "'bodyWeight')), target_weight_kg REAL CHECK (target_weight_kg IS NULL OR target_weight_kg >= 0), "
	    "percent_of_1rm REAL CHECK (percent_of_1rm IS NULL OR (percent_of_1rm > 0 AND percent_of_1rm <= 100)), "
	    "rpe_target REAL CHECK (rpe_target IS NULL OR (rpe_target >= 5 AND rpe_target <= 10)), "
	    "rest_seconds INTEGER NOT NULL DEFAULT 0 CHECK (rest_seconds >= 0), tempo TEXT, superset_group TEXT, "
	    "warmup_sets INTEGER NOT NULL DEFAULT 0 CHECK (warmup_sets >= 0), progression_json TEXT, substitution_ids "
	    "TEXT, "
	    "is_optional INTEGER NOT NULL DEFAULT 0 CHECK (is_optional IN (0, 1)), note TEXT, CHECK (reps_max >= "
	    "reps_min))",

	    "CREATE TABLE IF NOT EXISTS program_rules (id TEXT PRIMARY KEY NOT NULL, "
	    "program_id TEXT NOT NULL REFERENCES programs(id) ON DELETE CASCADE, "
	    "category TEXT NOT NULL CHECK (category IN ('nutrition', 'training', 'recovery', 'lifestyle', 'technique')), "
	    "code TEXT NOT NULL, title TEXT NOT NULL, "
	    "metric TEXT NOT NULL CHECK (metric IN ('proteinGrams', 'caloriesKcal', 'waterMl', 'sleepHours', 'stepCount', "
	    "'bodyWeightKg', 'maxRpe', 'restHoursBetweenSessions', 'alcoholUnits', 'custom')), "
	    "periodicity TEXT NOT NULL CHECK (periodicity IN ('perDay', 'perWeek', 'perWorkout', 'once')), "
	    "target_min REAL, target_max REAL, target_exact REAL, "
	    "target_unit TEXT NOT NULL CHECK (target_unit IN ('g', 'gPerKg', 'kcal', 'ml', 'hours', 'count', 'rpe', "
	    "'units')), "
	    "applies_to TEXT NOT NULL DEFAULT 'allDays' CHECK (applies_to IN ('allDays', 'trainingDays', 'restDays', "
	    "'customDays')), applies_days TEXT, "
	    "check_source TEXT NOT NULL CHECK (check_source IN ('selfReport', 'computedFromSets', 'computedFromProfile', "
	    "'computedFromSessions', 'computedFromLog', 'mixed')), "
	    "enforcement TEXT NOT NULL DEFAULT 'soft' CHECK (enforcement IN ('soft', 'hard')), "
	    "active_from TEXT, active_to TEXT, "
	    "display_unit TEXT NOT NULL CHECK (display_unit IN ('g', 'gPerKg', 'kcal', 'ml', 'hours', 'count', 'rpe', "
	    "'units')), "
	    "note TEXT, CHECK (target_min IS NOT NULL OR target_max IS NOT NULL OR target_exact IS NOT NULL))",

	    "CREATE TABLE IF NOT EXISTS rule_checkins (id TEXT PRIMARY KEY NOT NULL, "
	    "rule_id TEXT NOT NULL REFERENCES program_rules(id) ON DELETE CASCADE, "
	    "program_id TEXT NOT NULL REFERENCES programs(id) ON DELETE CASCADE, date TEXT NOT NULL, value REAL, "
	    "unit TEXT NOT NULL, state TEXT NOT NULL CHECK (state IN ('done', 'failed', 'skipped', 'partial', 'unknown')), "
	    "source TEXT NOT NULL CHECK (source IN ('manual', 'computed', 'imported')), "
	    "session_id TEXT REFERENCES workouts(id) ON DELETE SET NULL, meal_log_id TEXT, note TEXT, "
	    "created_at TEXT NOT NULL, updated_at TEXT NOT NULL, UNIQUE (rule_id, date))",

	    "CREATE TABLE IF NOT EXISTS program_revisions (id TEXT PRIMARY KEY NOT NULL, "
	    "program_id TEXT NOT NULL REFERENCES programs(id) ON DELETE CASCADE, "
	    "revision INTEGER NOT NULL CHECK (revision >= 1), changed_at TEXT NOT NULL, "
	    "reason TEXT NOT NULL CHECK (reason IN ('goal', 'term', 'schedule', 'days', 'rules', 'exercisePlan', "
	    "'state')), "
	    "summary TEXT NOT NULL DEFAULT '', snapshot_json TEXT, UNIQUE (program_id, revision))",

	    "CREATE TABLE IF NOT EXISTS workouts (id TEXT PRIMARY KEY NOT NULL, user_id TEXT NOT NULL, "
	    "program_id TEXT REFERENCES programs(id) ON DELETE SET NULL, "
	    "program_day_id TEXT REFERENCES program_days(id) ON DELETE SET NULL, "
	    "planned_date TEXT, planned_week_index INTEGER, date TEXT NOT NULL, started_at TEXT, completed_at TEXT, "
	    "duration_seconds INTEGER, state TEXT NOT NULL CHECK (state IN ('draft', 'completed', 'cancelled')), "
	    "deviation TEXT CHECK (deviation IS NULL OR deviation IN ('asPlanned', 'partial', 'substituted', 'extraDay', "
	    "'movedDay')), exercise_count INTEGER NOT NULL DEFAULT 0 CHECK (exercise_count >= 0), "
	    "set_count INTEGER NOT NULL DEFAULT 0 CHECK (set_count >= 0), "
	    "tonnage_kg REAL NOT NULL DEFAULT 0 CHECK (tonnage_kg >= 0), avg_rpe REAL, program_revision INTEGER, "
	    "correction_of_session_id TEXT, note TEXT, "
	    "sync_state TEXT NOT NULL DEFAULT 'local' CHECK (sync_state IN ('synced', 'local', 'syncing', 'offline')))",

	    "CREATE TABLE IF NOT EXISTS sets (id TEXT PRIMARY KEY NOT NULL, "
	    "session_id TEXT NOT NULL REFERENCES workouts(id) ON DELETE CASCADE, "
	    "day_exercise_id TEXT REFERENCES program_day_exercises(id) ON DELETE SET NULL, "
	    "exercise_id TEXT NOT NULL REFERENCES exercises(id), set_number INTEGER NOT NULL CHECK (set_number >= 1), "
	    "is_warmup INTEGER NOT NULL DEFAULT 0 CHECK (is_warmup IN (0, 1)), "
	    "weight_kg REAL NOT NULL DEFAULT 0 CHECK (weight_kg >= 0), reps INTEGER NOT NULL DEFAULT 0 CHECK (reps >= 0), "
	    "rpe REAL CHECK (rpe IS NULL OR (rpe >= 0 AND rpe <= 10)), target_weight_kg REAL, target_reps INTEGER, "
	    "done INTEGER NOT NULL DEFAULT 0 CHECK (done IN (0, 1)), note TEXT)",

	    "CREATE TABLE IF NOT EXISTS body_measurements (id TEXT PRIMARY KEY NOT NULL, user_id TEXT NOT NULL, "
	    "date TEXT NOT NULL, weight_kg REAL CHECK (weight_kg IS NULL OR (weight_kg > 0 AND weight_kg < 1000)), "
	    "chest_cm REAL, waist_cm REAL, hip_cm REAL, arm_cm REAL, thigh_cm REAL, UNIQUE (user_id, date))",

	    "CREATE TABLE IF NOT EXISTS nutrition_entries (id TEXT PRIMARY KEY NOT NULL, user_id TEXT NOT NULL, "
	    "date TEXT NOT NULL, meal_type TEXT NOT NULL CHECK (meal_type IN ('breakfast', 'lunch', 'dinner', 'snack')), "
	    "food_id TEXT, custom_title TEXT, portion_grams REAL CHECK (portion_grams IS NULL OR portion_grams >= 0), "
	    "protein_g REAL NOT NULL DEFAULT 0, fat_g REAL NOT NULL DEFAULT 0, carbs_g REAL NOT NULL DEFAULT 0, "
	    "calories_kcal REAL NOT NULL DEFAULT 0, source TEXT NOT NULL DEFAULT 'manual' "
	    "CHECK (source IN ('manual', 'barcode', 'preset')), "
	    "sync_state TEXT NOT NULL DEFAULT 'local' CHECK (sync_state IN ('synced', 'local', 'syncing', 'offline')))",

	    "CREATE TABLE IF NOT EXISTS nutrition_day_totals (user_id TEXT NOT NULL, date TEXT NOT NULL, "
	    "protein_g REAL NOT NULL DEFAULT 0, fat_g REAL NOT NULL DEFAULT 0, carbs_g REAL NOT NULL DEFAULT 0, "
	    "calories_kcal REAL NOT NULL DEFAULT 0, entries_count INTEGER NOT NULL DEFAULT 0, "
	    "PRIMARY KEY (user_id, date))",

	    "CREATE TABLE IF NOT EXISTS program_stats (program_id TEXT PRIMARY KEY NOT NULL "
	    "REFERENCES programs(id) ON DELETE CASCADE, computed_at TEXT NOT NULL, program_revision INTEGER NOT NULL, "
	    "payload_json TEXT NOT NULL)",

	    "CREATE TABLE IF NOT EXISTS lifetime_stats (user_id TEXT PRIMARY KEY NOT NULL, computed_at TEXT NOT NULL, "
	    "payload_json TEXT NOT NULL)",

	    // Инвариант 1 (docs/02-program-model.md §4): одна активная
	    // программа.
	    "CREATE UNIQUE INDEX IF NOT EXISTS programs_one_active_per_user ON programs(user_id) WHERE state = 'active'",
	    "CREATE INDEX IF NOT EXISTS goal_targets_program_idx ON goal_targets(program_id)",
	    "CREATE INDEX IF NOT EXISTS program_days_program_idx ON program_days(program_id, order_index)",
	    "CREATE INDEX IF NOT EXISTS program_schedule_program_dow_idx ON program_schedule(program_id, day_of_week)",
	    "CREATE INDEX IF NOT EXISTS program_day_exercises_day_idx ON program_day_exercises(program_day_id, "
	    "order_index)",
	    "CREATE INDEX IF NOT EXISTS program_rules_program_idx ON program_rules(program_id)",
	    "CREATE INDEX IF NOT EXISTS rule_checkins_rule_date_idx ON rule_checkins(rule_id, date)",
	    "CREATE INDEX IF NOT EXISTS program_revisions_program_idx ON program_revisions(program_id, revision)",
	    "CREATE INDEX IF NOT EXISTS workouts_program_date_idx ON workouts(program_id, date)",
	    "CREATE INDEX IF NOT EXISTS sets_session_idx ON sets(session_id)",
	    "CREATE INDEX IF NOT EXISTS body_measurements_user_date_idx ON body_measurements(user_id, date)",
	    "CREATE INDEX IF NOT EXISTS nutrition_entries_user_date_idx ON nutrition_entries(user_id, date)"};

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
