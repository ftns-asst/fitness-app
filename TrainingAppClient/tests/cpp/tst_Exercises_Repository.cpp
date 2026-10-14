#include "Data/Auth/Auth_Session.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Exercises/Exercises_Api.h"
#include "Data/Exercises/Exercises_Repository.h"
#include "Data/Network/Http_Client.h"
#include "Support/Test_Http_Server.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlQuery>
#include <QTemporaryDir>
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

private:
	static QByteArray Exercise_Array_Body();
	static STest_Http_Response JSON_Response(int in_status, const QByteArray &in_body);
	// Поднимает связку сервер + база + репозиторий; is_with_session включает
	// сессию пользователя user-1 (для проверки is_own).
	void Make_Repository(ATest_Http_Server &in_server, QTemporaryDir &out_dir, bool in_with_session,
	                     AExercises_Repository *&out_repository);
};
//----------------------------------------------------------------------------
STest_Http_Response AExercises_Repository_Test::JSON_Response(int in_status, const QByteArray &in_body)
{
	STest_Http_Response response;

	response.Status_Code = in_status;
	response.Body = in_body;

	return response;
}
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
void AExercises_Repository_Test::migrationCreatesCacheTables()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;

	QVERIFY(directory.isValid());
	QVERIFY(database.Open(directory.filePath("cache.sqlite3")));
	QVERIFY(database.Migrate());

	// Миграция v8 (v7 — кэш упражнений и справочников; v8 — метрики правил
	// питания): таблицы кэша созданы, версия схемы поднята до текущей (v8).
	QCOMPARE(database.Get_Schema_Version(), 8);
	QCOMPARE(database.Get_Current_Schema_Version(), 8);

	QSqlDatabase connection = database.Sql_Connection();
	QSqlQuery probe(connection);

	QVERIFY(probe.exec("SELECT id, name, description, owner_id, is_own, muscle_groups, equipment, updated_at "
	                   "FROM exercises_cache WHERE 0"));
	QVERIFY(probe.exec("SELECT name FROM muscle_groups_ref WHERE 0"));
	QVERIFY(probe.exec("SELECT name FROM equipments_ref WHERE 0"));
}
//----------------------------------------------------------------------------
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
	QVERIFY(from_cache == false);
	QVERIFY(error.Is_Error() == false);

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
	QVERIFY(repository->Is_Cached_Own("aaaaaaaa-1111-2222-3333-444444444444") == false);
}
//----------------------------------------------------------------------------
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
//----------------------------------------------------------------------------
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

	QCOMPARE(items.size(), 2);
	QCOMPARE(items[0].Name, QString("Bench press"));
	QVERIFY(from_cache);
	QVERIFY(error.Is_Error() == false);
}
//----------------------------------------------------------------------------
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
	QVERIFY(error.Is_Error() == false);
}

QTEST_MAIN(AExercises_Repository_Test)
#include "tst_Exercises_Repository.moc"
