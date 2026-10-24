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
	void createsCurrentSchema();
	void migrationIsIdempotent();
	void seedsBuiltinExerciseCatalog();
	void migratesV1DatabaseToCurrent();
	void enforcesProgramSchemaConstraints();
	void storesBackendAlignedCache();
	void enforcesConstraintsAndForeignKeys();
	void rejectsNewerSchema();
	void reportsOpenError();

	// — Правило-рекомендация: категория «Другое» снимает требование цели —
	// Текстовое правило обязано приниматься схемой, измеримое без цели — по-прежнему
	// отвергаться, а пустое (ни цели, ни текста, ни заголовка) не проходить.
	// Иначе «Другое» не отличить от мусора.
	void acceptsAdviceRuleWithoutTarget();
	void keepsMeasurableRuleTargetRequired();
	void rejectsRuleWithoutTargetNorText();
	void migratesWithExistingRules();
	// Миграция v4 → v5 (спринт S7) добавляет столбцы в profile_local. Проверяется
	// главное: накопленные данные локального профиля переживают миграцию.
	void migratesProfileLocalAddingColumns();
	void migratesPartialDatabaseWithoutProfileLocal();

	// — Единица «кг» (миграция v4) —
	// Пока в CHECK display_unit нет килограммов, метрика «вес тела» недоступна
	// в форме своего правила. Миграция обязана и расширить CHECK, и не потерять
	// накопленные правила — иначе она стоила бы пользователю всей программы.
	void acceptsKilogramUnit();
	void keepsRulesWhenAddingKilogramUnit();
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
// Минимальная цепочка, нужная для вставки правила: программа с активным
// состоянием. Повторялась бы в каждом тесте про правила, поэтому вынесена:
// тест про правило не должен знать, чем заполняется programs.
static void Seed_Program_Fixture(const QSqlDatabase &in_database, QSqlQuery &in_query)
{
	in_query = QSqlQuery(in_database);
	QVERIFY(in_query.exec("INSERT INTO programs(id, user_id, title, source_kind, goal, level, schedule_kind, state, "
	                      "revision, weeks_total, term_mode, created_at, updated_at, sync_state) "
	                      "VALUES('p1', 'u1', 'Сила 8 недель', 'scratch', 'strength', 'beginner', 'weekday', "
	                      "'active', 1, 8, 'fixedWeeks', '2026-10-01T00:00:00Z', '2026-10-01T00:00:00Z', "
	                      "'local')"));
}
//----------------------------------------------------------------------------
void ASql_Database_Test::createsCurrentSchema()
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
	QCOMPARE(database.Get_Schema_Version(), AsSql_Database::Get_Current_Schema_Version());
	QCOMPARE(database.Get_Last_Error(), QString());

	inspection = Open_Inspection_Database(path, "inspect_schema");
	QVERIFY2(inspection.isOpen(), qPrintable(inspection.lastError().text()));
	query = QSqlQuery(inspection);
	QVERIFY(query.exec("SELECT name FROM sqlite_master WHERE type = 'table' ORDER BY name"));

	while (query.next())
		tables.append(query.value(0).toString());

	// Schema v1 остаётся: auth-кэш и outbox.
	for (const QString &table :
	     {"auth_tokens", "profile_cache", "profile_local", "schema_version", "sync_outbox", "user_cache"})
		QVERIFY2(tables.contains(table), qPrintable(QString("Нет таблицы %1").arg(table)));

	// v2+ — модель программы (`02-program-model.md` §8).
	for (const QString &table :
	     {"body_measurements", "exercises", "goal_targets", "lifetime_stats", "nutrition_day_totals",
	      "nutrition_entries", "program_day_exercises", "program_days", "program_revisions", "program_rules",
	      "program_schedule", "program_stats", "programs", "rule_checkins", "sets", "workouts"})
		QVERIFY2(tables.contains(table), qPrintable(QString("Нет таблицы %1").arg(table)));

	// Каталог планов из issue 22 в срез W не входит (миграция v3).
	QVERIFY(tables.contains("plans") == false);
	QVERIFY(tables.contains("plan_days") == false);
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
	QCOMPARE(database.Get_Schema_Version(), AsSql_Database::Get_Current_Schema_Version());
}
//----------------------------------------------------------------------------
// Каталог упражнений: редактор дня пишет предписание с жёсткими
// идентификаторами (ProgramEditPage/ProgramDayPage: ex_squat, ex_bench, ex_row,
// ex_deadlift), поэтому строки обязаны существовать сразу после Open(). На
// пустом каталоге FOREIGN KEY program_day_exercises.exercise_id → exercises.id
// отвергал вставку, и создание программы падало без сообщения на экране.
void ASql_Database_Test::seedsBuiltinExerciseCatalog()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QSqlDatabase inspection;
	QSqlQuery query;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));

	inspection = Open_Inspection_Database(path, "inspect_catalog");
	QVERIFY(inspection.isOpen());
	query = QSqlQuery(inspection);
	QVERIFY(query.exec("PRAGMA foreign_keys = ON"));

	for (const QString &exercise_id : {"ex_squat", "ex_bench", "ex_row", "ex_deadlift"})
	{
		QVERIFY2(query.exec(QString("SELECT COUNT(*) FROM exercises WHERE id = '%1'").arg(exercise_id)),
		         qPrintable(query.lastError().text()));
		QVERIFY(query.next());
		QCOMPARE(query.value(0).toInt(), 1);
	}

	// Пользовательский каталог сид не переписывает: своё упражнение остаётся
	// помеченным is_custom = 1, а повторная миграция не добавляет дублей.
	QVERIFY2(query.exec("INSERT INTO exercises(id, title, muscle_group, equipment, is_custom, created_at) "
	                    "VALUES('ex_custom', 'Своё', 'legs', 'none', 1, '2026-09-29T00:00:00Z')"),
	         qPrintable(query.lastError().text()));
	QVERIFY(database.Migrate());
	QVERIFY(query.exec("SELECT COUNT(*) FROM exercises"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 5);
	QVERIFY(query.exec("SELECT is_custom FROM exercises WHERE id = 'ex_custom'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 1);

	// База, созданная до появления каталога, лечится при следующем открытии:
	// именно этот случай ломал создание программы у уже установленного клиента.
	QVERIFY(query.exec("DELETE FROM exercises"));
	database.Close();
	QVERIFY(database.Open(path));
	QVERIFY(query.exec("SELECT COUNT(*) FROM exercises"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 4);
	Close_Inspection_Database(inspection);
}
//----------------------------------------------------------------------------
void ASql_Database_Test::migratesProfileLocalAddingColumns()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QSqlDatabase inspection;
	QSqlQuery query;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("macro_profile.sqlite3");
	QVERIFY(database.Open(path));
	QCOMPARE(database.Get_Schema_Version(), AsSql_Database::Get_Current_Schema_Version());

	inspection = Open_Inspection_Database(path, "inspect_macro_profile");
	QVERIFY(inspection.isOpen());
	query = QSqlQuery(inspection);

	// Локальный профиль заполняется до миграции: именно его сохранность и
	// проверяется. Пустая таблица прошла бы любую миграцию незаметно.
	QVERIFY2(query.exec("INSERT INTO profile_local(user_id, level, goal, workouts_per_week, updated_at) "
	                    "VALUES('u1', 'beginner', 'strength', 3, '2026-09-20T10:00:00Z')"),
	         qPrintable(query.lastError().text()));

	// Повторная миграция на уже актуальной базе обязана быть безопасна: v5
	// добавляет столбцы, а повторный ALTER упал бы.
	QVERIFY(database.Migrate());
	// Запрос сбрасывается до закрытия соединения: живой QSqlQuery держит его
	// открытым, и SQLite ругается «connection is still in use» — причём не
	// здесь, а в следующем тесте, на другом файле.
	query = QSqlQuery();
	Close_Inspection_Database(inspection);
	database.Close();
	QVERIFY(database.Open(path));
	QCOMPARE(database.Get_Schema_Version(), AsSql_Database::Get_Current_Schema_Version());

	inspection = Open_Inspection_Database(path, "inspect_macro_profile_after");
	query = QSqlQuery(inspection);

	// Новые столбцы существуют и по умолчанию пусты: у старых записей нет ни даты
	// рождения, ни активности, и выдумывать их при миграции нельзя.
	QVERIFY2(query.exec("SELECT birth_date, activity_level FROM profile_local WHERE user_id = 'u1'"),
	         qPrintable(query.lastError().text()));
	QVERIFY(query.next());
	QVERIFY(query.value(0).isNull());
	QVERIFY(query.value(1).isNull());

	// Старые поля целы — именно это ломается при пересоздании таблицы.
	QVERIFY(query.exec("SELECT level, goal, workouts_per_week FROM profile_local WHERE user_id = 'u1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toString(), QString("beginner"));
	QCOMPARE(query.value(1).toString(), QString("strength"));
	QCOMPARE(query.value(2).toInt(), 3);

	// CHECK ограничения новых столбцов работают: уровень вне перечня —
	// испорченные данные, и пропускать их нельзя.
	QVERIFY(!query.exec("UPDATE profile_local SET activity_level = 'сверхчеловек' WHERE user_id = 'u1'"));
	QVERIFY(query.exec("UPDATE profile_local SET activity_level = 'moderate' WHERE user_id = 'u1'"));
	query = QSqlQuery();
	Close_Inspection_Database(inspection);
}
//----------------------------------------------------------------------------
// Та же база, что у установленного клиента: создана релизом v1, где таблица
// profile_local могла не появиться из-за обрыва записи. Миграция обязана её
// вылечить, а не упасть на ALTER отсутствующей таблицы.
//----------------------------------------------------------------------------
void ASql_Database_Test::migratesPartialDatabaseWithoutProfileLocal()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QSqlDatabase inspection;
	QSqlQuery query;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("partial_v1.sqlite3");

	inspection = Open_Inspection_Database(path, "prepare_partial_v1");
	QVERIFY(inspection.isOpen());
	query = QSqlQuery(inspection);
	// Только таблица версии и пользовательский кэш — ровно то, что успело
	// записаться до сбоя.
	QVERIFY(query.exec("CREATE TABLE schema_version(id INTEGER PRIMARY KEY CHECK (id = 1), version INTEGER NOT NULL)"));
	QVERIFY(query.exec("CREATE TABLE user_cache(id TEXT PRIMARY KEY NOT NULL, email TEXT NOT NULL UNIQUE, "
	                   "display_name TEXT NOT NULL, created_at TEXT NOT NULL, updated_at TEXT)"));
	QVERIFY(query.exec("INSERT INTO user_cache(id, email, display_name, created_at) "
	                   "VALUES('u1', 'a@example.com', 'somename', '2026-09-20T00:00:00Z')"));
	QVERIFY(query.exec("INSERT INTO schema_version(id, version) VALUES(1, 1)"));
	query = QSqlQuery();
	Close_Inspection_Database(inspection);

	QVERIFY2(database.Open(path), qPrintable(database.Get_Last_Error()));
	QCOMPARE(database.Get_Schema_Version(), AsSql_Database::Get_Current_Schema_Version());
	database.Close();

	inspection = Open_Inspection_Database(path, "inspect_partial_after");
	query = QSqlQuery(inspection);
	// Таблица появилась, и в ней работают новые столбцы.
	QVERIFY2(query.exec("SELECT birth_date, activity_level FROM profile_local"), qPrintable(query.lastError().text()));
	// Данные пользовательского кэша пережили.
	QVERIFY(query.exec("SELECT display_name FROM user_cache WHERE id = 'u1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toString(), QString("somename"));
	query = QSqlQuery();
	Close_Inspection_Database(inspection);
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
// Правило-рекомендация: пользователь пишет совет текстом, без измеримой
// цели (категория «Другое»). До миграции v3 таблица program_rules требовала
// одну из целей CHECK-ограничением, и такой отказ доходил до пользователя
// голым «FOREIGN KEY constraint failed».
void ASql_Database_Test::acceptsAdviceRuleWithoutTarget()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QSqlQuery query;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("advice.sqlite3");
	QVERIFY(database.Open(path));
	Seed_Program_Fixture(database.Sql_Connection(), query);

	// Текстовое правило без цели принимается: метрика custom, единица
	// отсутствует у пользователя, цель заменена текстом в note.
	QVERIFY2(query.exec("INSERT INTO program_rules(id, program_id, category, code, title, metric, periodicity, "
	                    "target_unit, applies_to, check_source, display_unit, note) "
	                    "VALUES('r1', 'p1', 'other', 'custom', 'Пей воду натощак', 'custom', 'perDay', "
	                    "'units', 'allDays', 'selfReport', 'units', 'Пей воду натощак каждое утро')"),
	         qPrintable(query.lastError().text()));

	// Текст читается обратно — потерять его при вставке нельзя.
	QVERIFY(query.exec("SELECT note FROM program_rules WHERE id = 'r1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toString(), QString("Пей воду натощак каждое утро"));
}
//----------------------------------------------------------------------------
// Ослабление CHECK не должно открыть лазейку: измеримое правило без цели
// по-прежнему бессмысленно и должно отвергаться.
void ASql_Database_Test::keepsMeasurableRuleTargetRequired()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QSqlQuery query;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("measurable.sqlite3");
	QVERIFY(database.Open(path));
	Seed_Program_Fixture(database.Sql_Connection(), query);

	QVERIFY2(query.exec("SELECT sql FROM sqlite_master WHERE name='program_rules'") && query.next(),
	         qPrintable(query.lastError().text()));
	QVERIFY2(query.value(0).toString().contains("category = 'other'"),
	         qPrintable(query.value(0).toString().right(220)));

	// С целью — принимается.
	QVERIFY2(query.exec("INSERT INTO program_rules(id, program_id, category, code, title, metric, periodicity, "
	                    "target_min, target_unit, applies_to, check_source, display_unit, note) "
	                    "VALUES('r2', 'p1', 'nutrition', 'protein', 'Белок', 'proteinGrams', 'perDay', "
	                    "120.0, 'g', 'allDays', 'selfReport', 'g', 'Белок')"),
	         qPrintable(query.lastError().text()));
}
//----------------------------------------------------------------------------
// Защита от бессмысленной записи: ни цели, ни текста. Именно этот случай
// закрывает «Другое» — иначе можно было бы завести пустое правило.
void ASql_Database_Test::rejectsRuleWithoutTargetNorText()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QSqlQuery query;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("empty.sqlite3");
	QVERIFY(database.Open(path));
	Seed_Program_Fixture(database.Sql_Connection(), query);

	QVERIFY(query.exec("INSERT INTO program_rules(id, program_id, category, code, title, metric, periodicity, "
	                   "target_unit, applies_to, check_source, display_unit, note) "
	                   "VALUES('r3', 'p1', 'other', 'custom', 'Совет', 'custom', 'perDay', "
	                   "'units', 'allDays', 'selfReport', 'units', NULL)") == false);

	// Пустой текст — тоже мусор: иначе «Другое» пропускает пустую строку.
	QVERIFY(query.exec("INSERT INTO program_rules(id, program_id, category, code, title, metric, periodicity, "
	                   "target_unit, applies_to, check_source, display_unit, note) "
	                   "VALUES('r4', 'p1', 'other', 'custom', 'Совет', 'custom', 'perDay', "
	                   "'units', 'allDays', 'selfReport', 'units', '   ')") == false);
}
//----------------------------------------------------------------------------
// Миграция v2 → v3 не теряет накопленные правила: ослабление CHECK требует
// пересоздать таблицу, и при этом легко потерять строки.
void ASql_Database_Test::migratesWithExistingRules()
{
	QTemporaryDir directory;
	AsSql_Database database;
	AsSql_Database builder;
	QSqlQuery query;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("v2.sqlite3");

	// База, выпущенная релизом со схемой v2.
	QVERIFY(builder.Open(path));
	Seed_Program_Fixture(builder.Sql_Connection(), query);
	QVERIFY2(query.exec("INSERT INTO program_rules(id, program_id, category, code, title, metric, periodicity, "
	                    "target_min, target_unit, applies_to, check_source, display_unit, note) "
	                    "VALUES('old1', 'p1', 'nutrition', 'protein', 'Белок', 'proteinGrams', 'perDay', "
	                    "120.0, 'g', 'allDays', 'selfReport', 'g', 'Белок')"),
	         qPrintable(query.lastError().text()));
	builder.Close();

	QVERIFY(database.Open(path));
	QCOMPARE(database.Get_Schema_Version(), AsSql_Database::Get_Current_Schema_Version());

	// Правило, созданное до миграции, уцелело.
	// Запрос должен идти по соединению открытой базы: после builder.Close()
	// объект query остаётся привязан к закрытому соединению, и SELECT
	// возвращает false без внятной причины.
	query = QSqlQuery(database.Sql_Connection());

	QVERIFY(query.exec("SELECT COUNT(*) FROM program_rules WHERE id = 'old1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 1);

	// Миграция повторяемая: повторный прогон не падает и не дублирует.
	QVERIFY(database.Migrate());
	QVERIFY(query.exec("SELECT COUNT(*) FROM program_rules"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 1);
}
//----------------------------------------------------------------------------
// Килограммы в CHECK display_unit: без них правило «вес не выше 80 кг» не
// сохранилось бы, и метрика веса оставалась бы недоступна в форме.
void ASql_Database_Test::acceptsKilogramUnit()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QSqlQuery query;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("kg.sqlite3");
	QVERIFY(database.Open(path));
	Seed_Program_Fixture(database.Sql_Connection(), query);

	QVERIFY2(query.exec("INSERT INTO program_rules(id, program_id, category, code, title, metric, periodicity, "
	                    "target_min, target_unit, applies_to, check_source, display_unit, note) "
	                    "VALUES('w1', 'p1', 'recovery', 'bodyWeightControl', 'Вес тела', 'bodyWeightKg', 'perWeek', "
	                    "80.0, 'kg', 'allDays', 'selfReport', 'kg', 'Раз в неделю')"),
	         qPrintable(query.lastError().text()));

	// Единица читается обратно: потерять её — значит показать «80 g».
	QVERIFY(query.exec("SELECT display_unit FROM program_rules WHERE id = 'w1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toString(), QString("kg"));
}
//----------------------------------------------------------------------------
// Добавление единицы требует пересоздать таблицу (CHECK не ALTER'ится), а
// пересоздание без переноса строк стоило бы пользователю всех правил. Это тот
// же приём, что в v2→v3, и он обязан быть проверен на данных.
void ASql_Database_Test::keepsRulesWhenAddingKilogramUnit()
{
	QTemporaryDir directory;
	AsSql_Database database;
	AsSql_Database builder;
	QSqlQuery query;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("v3.sqlite3");

	// База, выпущенная релизом со схемой v3: правило в граммах.
	QVERIFY(builder.Open(path));
	Seed_Program_Fixture(builder.Sql_Connection(), query);
	QVERIFY2(query.exec("INSERT INTO program_rules(id, program_id, category, code, title, metric, periodicity, "
	                    "target_min, target_unit, applies_to, check_source, display_unit, note) "
	                    "VALUES('old1', 'p1', 'nutrition', 'protein', 'Белок', 'proteinGrams', 'perDay', "
	                    "140.0, 'g', 'allDays', 'selfReport', 'g', 'Белок')"),
	         qPrintable(query.lastError().text()));
	builder.Close();

	QVERIFY(database.Open(path));
	QCOMPARE(database.Get_Schema_Version(), AsSql_Database::Get_Current_Schema_Version());

	// Запрос идёт по соединению открытой базы: после builder.Close() объект
	// query остаётся привязан к закрытому соединению, и SELECT возвращает
	// false без внятной причины.
	query = QSqlQuery(database.Sql_Connection());

	// Правило, созданное до миграции, уцелело вместе со своей единицей.
	QVERIFY(query.exec("SELECT display_unit FROM program_rules WHERE id = 'old1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toString(), QString("g"));

	// И килограммы после миграции принимаются.
	QVERIFY2(query.exec("INSERT INTO program_rules(id, program_id, category, code, title, metric, periodicity, "
	                    "target_min, target_unit, applies_to, check_source, display_unit, note) "
	                    "VALUES('new1', 'p1', 'recovery', 'bodyWeightControl', 'Вес тела', 'bodyWeightKg', 'perWeek', "
	                    "80.0, 'kg', 'allDays', 'selfReport', 'kg', 'Раз в неделю')"),
	         qPrintable(query.lastError().text()));

	QVERIFY(query.exec("SELECT COUNT(*) FROM program_rules"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 2);
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
void ASql_Database_Test::migratesV1DatabaseToCurrent()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QSqlDatabase inspection;
	QSqlQuery query;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("v1.sqlite3");

	// База, созданная релизом с schema v1: версия 1 и данные auth-кэша.
	inspection = Open_Inspection_Database(path, "prepare_v1");
	QVERIFY(inspection.isOpen());
	query = QSqlQuery(inspection);
	QVERIFY(query.exec("CREATE TABLE schema_version(id INTEGER PRIMARY KEY CHECK (id = 1), version INTEGER NOT NULL)"));
	QVERIFY(query.exec("CREATE TABLE user_cache(id TEXT PRIMARY KEY NOT NULL, email TEXT NOT NULL UNIQUE, "
	                   "display_name TEXT NOT NULL, created_at TEXT NOT NULL, updated_at TEXT)"));
	QVERIFY(query.exec("INSERT INTO user_cache(id, email, display_name, created_at) "
	                   "VALUES('u1', 'a@example.com', 'somename', '2026-09-20T10:00:00Z')"));
	QVERIFY(query.exec("INSERT INTO schema_version(id, version) VALUES(1, 1)"));
	Close_Inspection_Database(inspection);

	QVERIFY(database.Open(path));
	QCOMPARE(database.Get_Schema_Version(), AsSql_Database::Get_Current_Schema_Version());

	// Данные v1 пережили миграцию.
	query = QSqlQuery(database.Sql_Connection());
	QVERIFY(query.exec("SELECT display_name FROM user_cache WHERE id = 'u1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toString(), QString("somename"));

	// Таблицы программы доступны и пусты.
	QVERIFY(query.exec("SELECT COUNT(*) FROM programs"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 0);
	QVERIFY(query.exec("SELECT COUNT(*) FROM workouts"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 0);
}
//----------------------------------------------------------------------------
void ASql_Database_Test::enforcesProgramSchemaConstraints()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QSqlQuery query;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("program.sqlite3");
	QVERIFY(database.Open(path));
	query = QSqlQuery(database.Sql_Connection());

	// Допустимая цепочка: упражнение → программа → день →
	// предписание.
	QVERIFY2(query.exec("INSERT INTO exercises(id, title, muscle_group, equipment, is_custom, created_at) "
	                    "VALUES('ex1', 'Присед', 'legs', 'barbell', 0, '2026-09-29T00:00:00Z')"),
	         qPrintable(query.lastError().text()));
	QVERIFY2(
	    query.exec("INSERT INTO programs(id, user_id, title, source_kind, goal, level, schedule_kind, state, revision, "
	               "start_date, weeks_total, term_mode, created_at, updated_at, sync_state) "
	               "VALUES('p1', 'u1', 'Сила 12 недель', 'scratch', 'strength', 'beginner', 'weekday', 'active', 1, "
	               "'2026-09-29', 12, 'fixedWeeks', '2026-09-29T00:00:00Z', '2026-09-29T00:00:00Z', 'local')"),
	    qPrintable(query.lastError().text()));
	QVERIFY2(query.exec("INSERT INTO program_days(id, program_id, title, kind, focus, estimated_minutes, order_index) "
	                    "VALUES('d1', 'p1', 'День A', 'workout', '[\"legs\"]', 60, 0)"),
	         qPrintable(query.lastError().text()));
	QVERIFY2(
	    query.exec("INSERT INTO program_day_exercises(id, program_day_id, exercise_id, order_index, sets, reps_min, "
	               "reps_max, weight_mode, target_weight_kg, rest_seconds) "
	               "VALUES('dx1', 'd1', 'ex1', 0, 4, 8, 12, 'fixed', 60.0, 120)"),
	    qPrintable(query.lastError().text()));

	// Инвариант 1 (`02-program-model.md` §4): одна активная программа
	// на пользователя.
	QVERIFY(
	    query.exec("INSERT INTO programs(id, user_id, title, source_kind, goal, level, schedule_kind, state, revision, "
	               "weeks_total, term_mode, created_at, updated_at, sync_state) "
	               "VALUES('p2', 'u1', 'Вторая', 'scratch', 'muscle', 'beginner', 'weekday', 'active', 1, 0, "
	               "'openEnded', "
	               "'2026-09-29T00:00:00Z', '2026-09-29T00:00:00Z', 'local')") == false);
	QVERIFY(query.exec(
	    "INSERT INTO programs(id, user_id, title, source_kind, goal, level, schedule_kind, state, revision, "
	    "weeks_total, term_mode, created_at, updated_at, sync_state) "
	    "VALUES('p2', 'u1', 'Вторая', 'scratch', 'muscle', 'beginner', 'weekday', 'paused', 1, 0, 'openEnded', "
	    "'2026-09-29T00:00:00Z', '2026-09-29T00:00:00Z', 'local')"));

	// Слоты недели: уникальность (program, day_of_week), границы ISO-дня.
	QVERIFY2(
	    query.exec("INSERT INTO program_schedule(id, program_id, day_of_week, rotation_index, program_day_id, kind, "
	               "is_anchor) VALUES('s1', 'p1', 1, NULL, 'd1', 'workout', 1)"),
	    qPrintable(query.lastError().text()));
	QVERIFY(
	    query.exec("INSERT INTO program_schedule(id, program_id, day_of_week, kind) VALUES('s2', 'p1', 1, 'rest')") ==
	    false);
	QVERIFY(
	    query.exec("INSERT INTO program_schedule(id, program_id, day_of_week, kind) VALUES('s3', 'p1', 0, 'rest')") ==
	    false);
	QVERIFY(
	    query.exec("INSERT INTO program_schedule(id, program_id, day_of_week, kind) VALUES('s4', 'p1', 8, 'rest')") ==
	    false);

	// Правила: требуется хотя бы одно целевое значение; enum-поля
	// ограничены.
	QVERIFY2(
	    query.exec("INSERT INTO program_rules(id, program_id, category, code, title, metric, periodicity, target_min, "
	               "target_unit, applies_to, check_source, enforcement, display_unit) "
	               "VALUES('r1', 'p1', 'nutrition', 'protein', 'Белок 1,8 г/кг', 'proteinGrams', 'perDay', 130, 'g', "
	               "'allDays', 'selfReport', 'soft', 'g')"),
	    qPrintable(query.lastError().text()));
	QVERIFY(
	    query.exec("INSERT INTO program_rules(id, program_id, category, code, title, metric, periodicity, target_unit, "
	               "applies_to, check_source, enforcement, display_unit) "
	               "VALUES('r2', 'p1', 'nutrition', 'calories', 'Ккал', 'caloriesKcal', 'perDay', 'kcal', 'allDays', "
	               "'selfReport', 'soft', 'kcal')") == false);
	QVERIFY(query.exec(
	            "INSERT INTO program_rules(id, program_id, category, code, title, metric, periodicity, target_min, "
	            "target_unit, applies_to, check_source, enforcement, display_unit) "
	            "VALUES('r3', 'p1', 'nutrition', 'x', 'X', 'wrongMetric', 'perDay', 1, 'g', 'allDays', 'selfReport', "
	            "'soft', 'g')") == false);

	// Отметки: одна на пару (rule, date).
	QVERIFY2(
	    query.exec("INSERT INTO rule_checkins(id, rule_id, program_id, date, value, unit, state, source, created_at, "
	               "updated_at) VALUES('c1', 'r1', 'p1', '2026-09-29', 140, 'g', 'done', 'manual', "
	               "'2026-09-29T20:00:00Z', '2026-09-29T20:00:00Z')"),
	    qPrintable(query.lastError().text()));
	QVERIFY(
	    query.exec("INSERT INTO rule_checkins(id, rule_id, program_id, date, value, unit, state, source, created_at, "
	               "updated_at) VALUES('c2', 'r1', 'p1', '2026-09-29', 120, 'g', 'failed', 'manual', "
	               "'2026-09-29T21:00:00Z', '2026-09-29T21:00:00Z')") == false);

	// Ревизии: уникальность (program, revision).
	QVERIFY2(query.exec("INSERT INTO program_revisions(id, program_id, revision, changed_at, reason, summary) "
	                    "VALUES('rev1', 'p1', 1, '2026-09-29T00:00:00Z', 'state', 'Программа создана')"),
	         qPrintable(query.lastError().text()));
	QVERIFY(query.exec("INSERT INTO program_revisions(id, program_id, revision, changed_at, reason, summary) "
	                   "VALUES('rev2', 'p1', 1, '2026-09-29T01:00:00Z', 'days', 'Дубликат ревизии')") == false);

	// Сессия и подходы: enum состояния, FK, CHECK номеров подходов.
	QVERIFY2(
	    query.exec("INSERT INTO workouts(id, user_id, program_id, program_day_id, planned_date, planned_week_index, "
	               "date, started_at, completed_at, duration_seconds, state, deviation, exercise_count, set_count, "
	               "tonnage_kg, avg_rpe, program_revision, sync_state) "
	               "VALUES('w1', 'u1', 'p1', 'd1', '2026-09-29', 1, '2026-09-29', '2026-09-29T18:00:00Z', "
	               "'2026-09-29T19:00:00Z', 3600, 'completed', 'asPlanned', 1, 1, 480, 8.0, 1, 'local')"),
	    qPrintable(query.lastError().text()));
	QVERIFY(
	    query.exec("INSERT INTO workouts(id, user_id, date, state) VALUES('w2', 'u1', '2026-09-29', 'wrongState')") ==
	    false);
	QVERIFY2(
	    query.exec("INSERT INTO sets(id, session_id, day_exercise_id, exercise_id, set_number, is_warmup, weight_kg, "
	               "reps, rpe, target_weight_kg, target_reps, done) "
	               "VALUES('st1', 'w1', 'dx1', 'ex1', 1, 0, 60, 8, 8, 60, 8, 1)"),
	    qPrintable(query.lastError().text()));
	QVERIFY(query.exec("INSERT INTO sets(id, session_id, exercise_id, set_number, weight_kg, reps, done) "
	                   "VALUES('st2', 'missing', 'ex1', 1, 60, 8, 1)") == false);
	QVERIFY(query.exec("INSERT INTO sets(id, session_id, exercise_id, set_number, weight_kg, reps, done) "
	                   "VALUES('st3', 'w1', 'ex1', 0, 60, 8, 1)") == false);

	// Замеры тела: одна запись на (user, date).
	QVERIFY2(query.exec(
	             "INSERT INTO body_measurements(id, user_id, date, weight_kg) VALUES('m1', 'u1', '2026-09-29', 80.5)"),
	         qPrintable(query.lastError().text()));
	QVERIFY(query.exec(
	            "INSERT INTO body_measurements(id, user_id, date, weight_kg) VALUES('m2', 'u1', '2026-09-29', 80.0)") ==
	        false);

	// Резерв питания: enum meal_type ограничен (§3.10).
	QVERIFY(query.exec(
	            "INSERT INTO nutrition_entries(id, user_id, date, meal_type, custom_title, portion_grams, protein_g, "
	            "fat_g, carbs_g, calories_kcal) VALUES('n1', 'u1', '2026-09-29', 'supper', 'x', 100, 1, 1, 1, 1)") ==
	        false);

	// Каскад: удаление программы уносит структуру и отметки, факты
	// остаются.
	QVERIFY(query.exec("DELETE FROM programs WHERE id = 'p1'"));
	QVERIFY(query.exec("SELECT COUNT(*) FROM program_days"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 0);
	QVERIFY(query.exec("SELECT COUNT(*) FROM rule_checkins"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 0);
	QVERIFY(query.exec("SELECT COUNT(*) FROM workouts"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 1);
}

//----------------------------------------------------------------------------

QTEST_MAIN(ASql_Database_Test)

#include "tst_Sql_Database.moc"
