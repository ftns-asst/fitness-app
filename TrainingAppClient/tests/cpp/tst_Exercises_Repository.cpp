#include "Data/Auth/Auth_Session.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Exercises/Exercises_Api.h"
#include "Data/Exercises/Exercises_Repository.h"
#include "Data/Network/Http_Client.h"
#include "Support/Test_Http_Server.h"

#include <QDateTime>
#include <QDir>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QTemporaryFile>
#include <QtTest>

// Спринт U2 (docs/08-exercises-sprint-plan.md §4): кэш упражнений и
// справочников с офлайн-фолбэком. Сеть — источник истины, кэш — выживание
// офлайна: успех переписывает upsert'ом, transport-сбой отдаёт кэш с
// маркером from_cache вместо ошибки. Миграция v7 создаёт таблицы кэша.
class AExercises_Repository_Test : public QObject
{
	Q_OBJECT

private slots:
	void migrationCreatesCacheTables();
	void listUpsertsIntoCache();
	void ownExercisesSurviveBasicRefresh();
	void offlineListFallsBackToCache();
	void catalogsFallBackToCachedValues();
	// Первый запуск без сети: пустой кэш засевается базовым каталогом,
	// иначе экран дня открывался «не из чего выбрать».
	void offlineEmptyCacheSeedsBaseCatalog();
	// Засев только в пустой кэш: серверный каталог не разбавляется примитивами.
	void offlineAfterOnlineServesServerItems();
	// Локальные фото (схема v9): стейджинг → коммит → чтение → удаление;
	// отмена стейджинга не оставляет файла.
	void photoStagesCommitsAndDeletes();

private:
	static QByteArray Exercise_Array_Body();
	static STest_Http_Response JSON_Response(int in_status, const QByteArray &in_body);
	// Поднимает связку сервер + база + репозиторий; is_with_session включает
	// сессию пользователя user-1 (для проверки is_own).
	void Make_Repository(ATest_Http_Server &in_server, QTemporaryDir &out_dir, bool in_with_session,
	                     AExercises_Repository *&out_repository);
};
//-------------------------------------------------------------------------------------------------------------
STest_Http_Response AExercises_Repository_Test::JSON_Response(int in_status, const QByteArray &in_body)
{
	STest_Http_Response response;

	response.Status_Code = in_status;
	response.Body = in_body;

	return response;
}
//-------------------------------------------------------------------------------------------------------------
QByteArray AExercises_Repository_Test::Exercise_Array_Body()
{
	// Голый массив; второе упражнение — своё (owner_id = user-1).
	return QByteArray("["
	                  "{\"id\":\"aaaaaaaa-1111-2222-3333-444444444444\",\"name\":\"Bench press\","
	                  "\"description\":\"Сжимай.\",\"muscle_groups\":[\"chest\"],"
	                  "\"equipment\":[\"barbell\"],\"created_at\":\"2026-10-01T10:00:00Z\"},"
	                  "{\"id\":\"bbbbbbbb-1111-2222-3333-444444444444\",\"name\":\"Push up\","
	                  "\"description\":\"Отжимайся.\",\"owner_id\":\"user-1\","
	                  "\"muscle_groups\":[\"chest\",\"triceps\"],\"equipment\":[],"
	                  "\"created_at\":\"2026-10-02T10:00:00Z\"}"
	                  "]");
}
//-------------------------------------------------------------------------------------------------------------
void AExercises_Repository_Test::Make_Repository(ATest_Http_Server &in_server, QTemporaryDir &out_dir,
                                                 bool in_with_session, AExercises_Repository *&out_repository)
{
	out_repository = 0;
	AsSql_Database *database = new AsSql_Database(&in_server);
	AsToken_Store *store = new AsToken_Store(database, &in_server);
	AsHttp_Client *client = new AsHttp_Client((QUrl()), &in_server);
	AsAuth_Session *session = new AsAuth_Session(client, store, &in_server);
	AExercises_Repository *repository = new AExercises_Repository(client, session, database, &in_server);

	QVERIFY(out_dir.isValid());
	QVERIFY(database->Open(out_dir.filePath("cache.sqlite3")));
	QVERIFY(database->Migrate());
	QVERIFY(in_server.Start());
	QVERIFY(client->Set_Base_URL(in_server.Base_URL().resolved(QUrl("/api/v1"))));
	client->Set_Max_Retry_Count(0);

	if (in_with_session)
	{
		QVERIFY(store->Save_Tokens("user-1", "stored-access", "stored-refresh", QDateTime(), QDateTime()));
		QVERIFY(session->Restore_From_Store());
	}

	out_repository = repository;
}
//-------------------------------------------------------------------------------------------------------------
void AExercises_Repository_Test::migrationCreatesCacheTables()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;

	QVERIFY(directory.isValid());
	QVERIFY(database.Open(directory.filePath("cache.sqlite3")));
	QVERIFY(database.Migrate());

	// Миграция v8 (v7 — кэш упражнений и справочников; v8 — метрики правил
	// питания; v9 — локальные фото техники): таблицы кэша созданы, версия
	// схемы поднята до текущей (v9).
	QCOMPARE(database.Get_Schema_Version(), 9);
	QCOMPARE(database.Get_Current_Schema_Version(), 9);

	QSqlDatabase connection = database.Sql_Connection();
	QSqlQuery probe(connection);

	QVERIFY(probe.exec("SELECT id, name, description, owner_id, is_own, muscle_groups, equipment, updated_at "
	                   "FROM exercises_cache WHERE 0"));
	QVERIFY(probe.exec("SELECT name FROM muscle_groups_ref WHERE 0"));
	QVERIFY(probe.exec("SELECT name FROM equipments_ref WHERE 0"));
	// v9 — таблица локальных фото техники.
	QVERIFY(probe.exec("SELECT exercise_id, file_name FROM exercise_photos WHERE 0"));
}
//-------------------------------------------------------------------------------------------------------------
void AExercises_Repository_Test::listUpsertsIntoCache()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AExercises_Repository *repository = 0;

	Make_Repository(server, directory, true, repository);
	QSignalSpy spy(repository, &AExercises_Repository::listFinished);
	QList<QVariant> arguments;
	QVector<SExercises_Item> items;
	bool from_cache = true;
	AApi_Error error;

	// Первая страница кладёт оба упражнения в кэш.
	server.Enqueue_Response(JSON_Response(200, Exercise_Array_Body()));
	QVERIFY(repository->Load_List("all", QStringList(), QStringList(), QString(), 50, 0) > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	items = qvariant_cast<QVector<SExercises_Item>>(arguments.at(1));
	from_cache = qvariant_cast<bool>(arguments.at(2));
	error = qvariant_cast<AApi_Error>(arguments.at(3));

	QCOMPARE(items.size(), 2);
	QVERIFY(!from_cache);
	QVERIFY(!error.Is_Error());

	// Повторная страница с обновлённым описанием переписывает upsert'ом,
	// а не дублирует строки кэша.
	server.Enqueue_Response(JSON_Response(200, QByteArray(Exercise_Array_Body()).replace("Сжимай.", "Сжимай плавно.")));
	QVERIFY(repository->Load_List("all", QStringList(), QStringList(), QString(), 50, 0) > 0);
	QVERIFY(spy.wait(5000));

	QVERIFY(repository->Load_Cached_List(items));
	QCOMPARE(items.size(), 2);
	QVERIFY(items[0].Description == QString("Сжимай плавно."));
	// «Своё» распознаётся по владельцу сессии (user-1).
	QVERIFY(items[1].Has_Owner);
	QVERIFY(repository->Is_Cached_Own("bbbbbbbb-1111-2222-3333-444444444444"));
	QVERIFY(!repository->Is_Cached_Own("aaaaaaaa-1111-2222-3333-444444444444"));
}
//-------------------------------------------------------------------------------------------------------------
void AExercises_Repository_Test::ownExercisesSurviveBasicRefresh()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AExercises_Repository *repository = 0;

	Make_Repository(server, directory, true, repository);
	QSignalSpy spy(repository, &AExercises_Repository::listFinished);
	QVector<SExercises_Item> items;

	// «Свои» пришли в общем наборе и легли в кэш.
	server.Enqueue_Response(JSON_Response(200, Exercise_Array_Body()));
	QVERIFY(repository->Load_List("all", QStringList(), QStringList(), QString(), 50, 0) > 0);
	QVERIFY(spy.wait(5000));
	spy.clear();

	// Обновление базовых (type=basic не содержит своего) не должно выкинуть
	// «своё» из кэша: наборы списков не вытесняют чужие наборы.
	server.Enqueue_Response(JSON_Response(200, QByteArray("[{\"id\":\"aaaaaaaa-1111-2222-3333-444444444444\","
	                                                      "\"name\":\"Bench press\",\"muscle_groups\":[\"chest\"],"
	                                                      "\"equipment\":[\"barbell\"],"
	                                                      "\"created_at\":\"2026-10-01T10:00:00Z\"}]")));
	QVERIFY(repository->Load_List("basic", QStringList(), QStringList(), QString(), 50, 0) > 0);
	QVERIFY(spy.wait(5000));

	QVERIFY(repository->Load_Cached_List(items));
	QCOMPARE(items.size(), 2);
}
//-------------------------------------------------------------------------------------------------------------
void AExercises_Repository_Test::offlineListFallsBackToCache()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AExercises_Repository *repository = 0;

	Make_Repository(server, directory, false, repository);
	QSignalSpy spy(repository, &AExercises_Repository::listFinished);
	QList<QVariant> arguments;
	QVector<SExercises_Item> items;
	bool from_cache = false;
	AApi_Error error;

	// Кэш прогревается успешным запросом.
	server.Enqueue_Response(JSON_Response(200, Exercise_Array_Body()));
	QVERIFY(repository->Load_List("all", QStringList(), QStringList(), QString(), 50, 0) > 0);
	QVERIFY(spy.wait(5000));
	spy.clear();

	// Сеть отвалилась: вместо ошибки отдаётся кэш с маркером from_cache.
	server.Stop();

	QVERIFY(repository->Load_List("basic", QStringList(), QStringList(), QString(), 50, 0) > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	items = qvariant_cast<QVector<SExercises_Item>>(arguments.at(1));
	from_cache = qvariant_cast<bool>(arguments.at(2));
	error = qvariant_cast<AApi_Error>(arguments.at(3));

	// Офлайн-список = серверный кэш + локальный каталог: серверные строки
	// идут первыми (источник истины), следом — строки локального каталога
	// (базовые примитивы, засеянные схемой). Без них упражнение, стоящее в дне
	// программы, не находилось бы в окне выбора.
	QVERIFY(items.size() >= 2);
	QCOMPARE(items[0].Name, QString("Bench press"));

	bool has_local_catalog_row = false;

	for (int i = 0; i < items.size(); ++i)
	{
		if (items.at(i).ID == QString("ex_squat"))
			has_local_catalog_row = true;
	}

	QVERIFY(has_local_catalog_row);
	QVERIFY(from_cache);
	QVERIFY(!error.Is_Error());
}
//-------------------------------------------------------------------------------------------------------------
void AExercises_Repository_Test::catalogsFallBackToCachedValues()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AExercises_Repository *repository = 0;

	Make_Repository(server, directory, false, repository);
	QSignalSpy groups_spy(repository, &AExercises_Repository::muscleGroupsFinished);
	QSignalSpy equipments_spy(repository, &AExercises_Repository::equipmentsFinished);
	QList<QVariant> arguments;
	QStringList values;
	bool from_cache = false;
	AApi_Error error;

	server.Enqueue_Response(JSON_Response(200, QByteArray("[\"chest\",\"biceps\"]")));
	QVERIFY(repository->Load_Muscle_Groups() > 0);
	QVERIFY(groups_spy.wait(5000));

	server.Enqueue_Response(JSON_Response(200, QByteArray("[\"barbell\"]")));
	QVERIFY(repository->Load_Equipments() > 0);
	QVERIFY(equipments_spy.wait(5000));

	// Справочники тоже кэшируются: офлайн отдаёт сохранённые значения.
	server.Stop();

	server.Enqueue_Response(JSON_Response(200, QByteArray("[]")));
	QVERIFY(repository->Load_Muscle_Groups() > 0);
	QVERIFY(groups_spy.wait(5000));

	arguments = groups_spy.takeLast();
	values = qvariant_cast<QStringList>(arguments.at(1));
	from_cache = qvariant_cast<bool>(arguments.at(2));
	error = qvariant_cast<AApi_Error>(arguments.at(3));

	QCOMPARE(values, QStringList({"chest", "biceps"}));
	QVERIFY(from_cache);
	QVERIFY(!error.Is_Error());
}
//-------------------------------------------------------------------------------------------------------------
// Первый запуск без сети: кэш пуст, сервера нет — вместо «не из чего выбрать»
// репозиторий сам засевает базовый каталог упражнений и справочники. Засев
// обязан доехать и до локального exercises: FK дня программы иначе отверг бы
// выбор базового упражнения.
void AExercises_Repository_Test::offlineEmptyCacheSeedsBaseCatalog()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database *database = new AsSql_Database(&server);
	AsToken_Store *store = new AsToken_Store(database, &server);
	AsHttp_Client *client = new AsHttp_Client((QUrl()), &server);
	AsAuth_Session *session = new AsAuth_Session(client, store, &server);
	AExercises_Repository *repository = new AExercises_Repository(client, session, database, &server);

	QVERIFY(directory.isValid());
	QVERIFY(database->Open(directory.filePath("cache.sqlite3")));
	QVERIFY(database->Migrate());
	QVERIFY(server.Start());
	QVERIFY(client->Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client->Set_Max_Retry_Count(0);

	QSignalSpy list_spy(repository, &AExercises_Repository::listFinished);
	QSignalSpy groups_spy(repository, &AExercises_Repository::muscleGroupsFinished);
	QSignalSpy equipments_spy(repository, &AExercises_Repository::equipmentsFinished);
	QList<QVariant> arguments;
	QVector<SExercises_Item> items;
	QStringList values;

	// Офлайн с самого начала: кэш никто не грел.
	server.Stop();

	QVERIFY(repository->Load_List("all", QStringList(), QStringList(), QString(), 50, 0) > 0);
	QVERIFY(list_spy.wait(5000));

	arguments = list_spy.takeFirst();
	items = qvariant_cast<QVector<SExercises_Item>>(arguments.at(1));

	// Базовый каталог не пуст и это не ошибка: офлайн — нормальное состояние.
	QVERIFY(items.size() >= 10);
	QVERIFY(arguments.at(2).toBool());
	QVERIFY(!qvariant_cast<AApi_Error>(arguments.at(3)).Is_Error());

	QVERIFY(repository->Load_Muscle_Groups() > 0);
	QVERIFY(groups_spy.wait(5000));
	values = qvariant_cast<QStringList>(groups_spy.takeFirst().at(1));
	QVERIFY(values.contains("chest"));
	QVERIFY(values.contains("quads"));

	QVERIFY(repository->Load_Equipments() > 0);
	QVERIFY(equipments_spy.wait(5000));
	values = qvariant_cast<QStringList>(equipments_spy.takeFirst().at(1));
	QVERIFY(values.contains("barbell"));
	QVERIFY(values.contains("bodyweight"));

	// Зеркало в локальном каталоге: FK program_day_exercises.exercise_id →
	// exercises.id обязан принимать базовый идентификатор.
	QSqlQuery probe(database->Sql_Connection());

	QVERIFY(probe.exec("SELECT COUNT(*) FROM exercises WHERE id = 'builtin-squat'"));
	QVERIFY(probe.next());
	QCOMPARE(probe.value(0).toInt(), 1);
}
//-------------------------------------------------------------------------------------------------------------
// Тёплый кэш и офлайн: серверные строки остаются первыми (источник истины),
// к ним добавляется локальный каталог — упражнения, которыми уже пользуются
// программы. Примитивы базового набора (builtin-*) в список не подмешиваются:
// базовый каталог засевается только в пустой кэш.
void AExercises_Repository_Test::offlineAfterOnlineServesServerItems()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AExercises_Repository *repository = 0;

	Make_Repository(server, directory, false, repository);
	QSignalSpy spy(repository, &AExercises_Repository::listFinished);
	QList<QVariant> arguments;
	QVector<SExercises_Item> items;

	server.Enqueue_Response(JSON_Response(200, Exercise_Array_Body()));
	QVERIFY(repository->Load_List("all", QStringList(), QStringList(), QString(), 50, 0) > 0);
	QVERIFY(spy.wait(5000));
	spy.clear();

	server.Stop();

	QVERIFY(repository->Load_List("all", QStringList(), QStringList(), QString(), 50, 0) > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	items = qvariant_cast<QVector<SExercises_Item>>(arguments.at(1));

	QVERIFY(items.size() >= 2);
	QCOMPARE(items[0].Name, QString("Bench press"));
	QCOMPARE(items[1].Name, QString("Push up"));

	for (int i = 0; i < items.size(); ++i)
		QVERIFY(!items[i].ID.startsWith("builtin-"));
}
//-------------------------------------------------------------------------------------------------------------
// Локальные фото (схема v9): стейджинг → коммит → чтение → удаление.
// Файлы пишутся в стандартный каталог приложения: тестовый режим
// QStandardPaths держит их в песочнице теста, а не в %APPDATA%.
//-------------------------------------------------------------------------------------------------------------
void AExercises_Repository_Test::photoStagesCommitsAndDeletes()
{
	qputenv("QT_STANDARDPATHS_TESTING", "1");

	ATest_Http_Server server;
	QTemporaryDir directory;
	AExercises_Repository *repository = 0;

	Make_Repository(server, directory, false, repository);

	// Источник — временный файл без суффикса: формат падает в jpg по умолчанию.
	QTemporaryFile source;

	QVERIFY(source.open());
	source.write("jpeg-bytes");
	source.close();

	// Нечитаемый источник — отказ с ключом, форма показывает ошибку.
	QString staged;
	QString error;

	QVERIFY(!repository->Stage_Photo(QUrl::fromLocalFile("no-such-photo.jpg"), staged, error));
	QCOMPARE(error, QString("photo_failed"));

	// Стейджинг: копия под временным именем, до коммита.
	QVERIFY(repository->Stage_Photo(QUrl::fromLocalFile(source.fileName()), staged, error));
	QVERIFY(error.isEmpty());
	QVERIFY(staged.endsWith(".jpg"));
	QVERIFY(QFile::exists(staged));

	// Коммит: файл переименовывается в <id>.<ext>, строка пишется в базу.
	QVERIFY(repository->Commit_Photo("cccccccc-1111-2222-3333-444444444444", staged));
	QVERIFY(!QFile::exists(staged));

	QString photo_file;

	QVERIFY(repository->Photo_File("cccccccc-1111-2222-3333-444444444444", photo_file));
	QVERIFY(photo_file.endsWith("cccccccc-1111-2222-3333-444444444444.jpg"));

	// Чужой id — фото нет; произвольный путь не отменяется.
	QString none;

	QVERIFY(!repository->Photo_File("unknown-id", none));
	QVERIFY(!repository->Cancel_Staged_Photo(source.fileName()));
	QVERIFY(QFile::exists(source.fileName()));

	// Удаление: и файл, и строка.
	QVERIFY(repository->Delete_Photo("cccccccc-1111-2222-3333-444444444444"));
	QVERIFY(!repository->Photo_File("cccccccc-1111-2222-3333-444444444444", none));
	QVERIFY(!QFile::exists(photo_file));

	// Отмена стейджинга: незакоммиченное фото не остаётся мусором.
	QVERIFY(repository->Stage_Photo(QUrl::fromLocalFile(source.fileName()), staged, error));
	QVERIFY(repository->Cancel_Staged_Photo(staged));
	QVERIFY(!QFile::exists(staged));

	// Песочница теста не переживает тест.
	QDir(AExercises_Repository::Photos_Directory()).removeRecursively();
}

QTEST_MAIN(AExercises_Repository_Test)
#include "tst_Exercises_Repository.moc"
