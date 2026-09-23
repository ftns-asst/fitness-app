#include "Data/Auth/Auth_Session.h"
#include "Data/Auth/Token_Store.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Network/Http_Client.h"
#include "Data/Users/Users_Repository.h"
#include "Support/Test_Http_Server.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class AUsers_Repository_Test : public QObject
{
	Q_OBJECT

private slots:
	void getUserPersistsUserAndProfileCache();
	void getUserWithoutProfileLeavesProfileCacheEmpty();
	void networkFailureFallsBackToCacheWithOfflineMarker();
	void networkFailureWithoutCacheReportsError();
	void cachedUserAvailableWithoutNetwork();
	void localProfileSaveWritesDbAndOutbox();
	void localProfileLoadReturnsSavedFields();

private:
	static STest_Http_Response JSON_Response(int in_status, const QByteArray &in_body);
	static QByteArray User_With_Profile_Body();
	static QByteArray User_Without_Profile_Body();
	static QSqlDatabase Open_Inspection_Database(const QString &in_path, const QString &in_name);
	static void Close_Inspection_Database(QSqlDatabase &in_database);
};

//----------------------------------------------------------------------------
void AUsers_Repository_Test::getUserPersistsUserAndProfileCache()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AUsers_Repository repository(&session, &database);
	QSignalSpy spy(&repository, &AUsers_Repository::getUserFinished);
	QList<QVariant> arguments;
	QSqlDatabase inspection;
	QSqlQuery query;
	SUsers_User user;
	AApi_Error error;
	bool from_cache = true;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);
	QVERIFY(store.Save_Tokens("user-1", "stored-access", "stored-refresh", QDateTime(), QDateTime()));
	QVERIFY(session.Restore_From_Store());

	server.Enqueue_Response(JSON_Response(200, User_With_Profile_Body()));
	int request_id = repository.Get_User("user-1", true);
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	user = qvariant_cast<SUsers_User>(arguments.at(1));
	from_cache = arguments.at(2).toBool();
	error = qvariant_cast<AApi_Error>(arguments.at(3));
	QVERIFY(error.Is_Error() == false);
	QVERIFY(from_cache == false);
	QCOMPARE(user.ID, QString("user-1"));
	QVERIFY(user.Has_Profile);

	// Ответ сервера атомарно попал в user_cache и profile_cache.
	inspection = Open_Inspection_Database(path, "inspect_user");
	query = QSqlQuery(inspection);
	QVERIFY(query.exec("SELECT email, display_name FROM user_cache WHERE id = 'user-1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toString(), QString("user@example.com"));
	QCOMPARE(query.value(1).toString(), QString("somename"));

	QVERIFY(query.exec("SELECT age, gender, height_cm, weight_kg FROM profile_cache WHERE user_id = 'user-1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 31);
	QCOMPARE(query.value(1).toString(), QString("male"));
	QCOMPARE(query.value(2).toInt(), 179);
	QCOMPARE(query.value(3).toDouble(), 74.5);
	Close_Inspection_Database(inspection);
}
//----------------------------------------------------------------------------
void AUsers_Repository_Test::getUserWithoutProfileLeavesProfileCacheEmpty()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AUsers_Repository repository(&session, &database);
	QSignalSpy spy(&repository, &AUsers_Repository::getUserFinished);
	QList<QVariant> arguments;
	QSqlDatabase inspection;
	QSqlQuery query;
	SUsers_User user;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);
	QVERIFY(store.Save_Tokens("user-1", "stored-access", "stored-refresh", QDateTime(), QDateTime()));
	QVERIFY(session.Restore_From_Store());

	server.Enqueue_Response(JSON_Response(200, User_Without_Profile_Body()));
	int request_id = repository.Get_User("user-1", true);
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	user = qvariant_cast<SUsers_User>(arguments.at(1));
	QVERIFY(user.Has_Profile == false);

	inspection = Open_Inspection_Database(path, "inspect_no_profile");
	query = QSqlQuery(inspection);
	QVERIFY(query.exec("SELECT COUNT(*) FROM profile_cache"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 0);
	Close_Inspection_Database(inspection);
}
//----------------------------------------------------------------------------
void AUsers_Repository_Test::networkFailureFallsBackToCacheWithOfflineMarker()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AUsers_Repository repository(&session, &database);
	QSignalSpy spy(&repository, &AUsers_Repository::getUserFinished);
	QList<QVariant> arguments;
	STest_Http_Response timeout_response;
	SUsers_User user;
	AApi_Error error;
	bool from_cache = false;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);
	// GET Qt ретраит при разрыве соединения (урок issue 4), поэтому сетевой
	// сбой эмулируем задержкой ответа дольше transfer timeout.
	client.Set_Timeout_Ms(200);
	QVERIFY(store.Save_Tokens("user-1", "stored-access", "stored-refresh", QDateTime(), QDateTime()));
	QVERIFY(session.Restore_From_Store());

	// Первый запрос наполняет кэш из сети.
	server.Enqueue_Response(JSON_Response(200, User_With_Profile_Body()));
	QVERIFY(repository.Get_User("user-1", true) > 0);
	QVERIFY(spy.wait(5000));
	spy.clear();

	// Второй запрос без сети отдаёт кэш с офлайн-пометкой (docs/03, §3.5).
	timeout_response.Delay_Ms = 1000;
	server.Enqueue_Response(timeout_response);
	int request_id = repository.Get_User("user-1", true);
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	user = qvariant_cast<SUsers_User>(arguments.at(1));
	from_cache = arguments.at(2).toBool();
	error = qvariant_cast<AApi_Error>(arguments.at(3));
	QVERIFY(from_cache);
	QCOMPARE(user.ID, QString("user-1"));
	QCOMPARE(user.Email, QString("user@example.com"));
	QVERIFY(user.Has_Profile);
	QCOMPARE(user.Profile.Age, 31);
	QCOMPARE(error.Key(), QString("timeout"));
	QCOMPARE(server.Request_Count(), 2);
}
//----------------------------------------------------------------------------
void AUsers_Repository_Test::networkFailureWithoutCacheReportsError()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AUsers_Repository repository(&session, &database);
	QSignalSpy spy(&repository, &AUsers_Repository::getUserFinished);
	QList<QVariant> arguments;
	STest_Http_Response timeout_response;
	SUsers_User user;
	AApi_Error error;
	bool from_cache = true;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);
	client.Set_Timeout_Ms(200);
	QVERIFY(store.Save_Tokens("user-1", "stored-access", "stored-refresh", QDateTime(), QDateTime()));
	QVERIFY(session.Restore_From_Store());

	// Сетевой сбой без кэша отдаёт только ошибку (docs/03, §3.5).
	timeout_response.Delay_Ms = 1000;
	server.Enqueue_Response(timeout_response);
	int request_id = repository.Get_User("user-1", true);
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	user = qvariant_cast<SUsers_User>(arguments.at(1));
	from_cache = arguments.at(2).toBool();
	error = qvariant_cast<AApi_Error>(arguments.at(3));
	QCOMPARE(error.Key(), QString("timeout"));
	QVERIFY(from_cache == false);
	QVERIFY(user.ID.isEmpty());
}
//----------------------------------------------------------------------------
void AUsers_Repository_Test::cachedUserAvailableWithoutNetwork()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AUsers_Repository repository(&session, &database);
	QSignalSpy spy(&repository, &AUsers_Repository::getUserFinished);
	SUsers_User cached_user;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);
	QVERIFY(store.Save_Tokens("user-1", "stored-access", "stored-refresh", QDateTime(), QDateTime()));
	QVERIFY(session.Restore_From_Store());

	server.Enqueue_Response(JSON_Response(200, User_With_Profile_Body()));
	QVERIFY(repository.Get_User("user-1", true) > 0);
	QVERIFY(spy.wait(5000));

	// Прямое чтение кэша без обращения к сети.
	QVERIFY(repository.Get_Cached_User("user-1", cached_user));
	QCOMPARE(cached_user.ID, QString("user-1"));
	QCOMPARE(cached_user.Name, QString("somename"));
	QCOMPARE(cached_user.Email, QString("user@example.com"));
	QVERIFY(cached_user.Has_Profile);
	QCOMPARE(cached_user.Profile.Gender, QString("male"));
	QCOMPARE(cached_user.Profile.Weight_Kg, 74.5);
	QCOMPARE(server.Request_Count(), 1);
}
//----------------------------------------------------------------------------
void AUsers_Repository_Test::localProfileSaveWritesDbAndOutbox()
{
	QTemporaryDir directory;
	AsSql_Database database;
	AUsers_Repository repository(0, &database);
	QSqlDatabase inspection;
	QSqlQuery query;
	QString status;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));

	// Локальные параметры профиля: profile_local + запись в sync_outbox
	// (воркер синхронизации — issue 34), статус «сохранено локально».
	QVERIFY(repository.Save_Local_Profile("user-1", "beginner", "lose_weight", 3, status));
	QCOMPARE(status, QString("saved_locally"));
	QCOMPARE(repository.Pending_Outbox_Count(), 1);

	inspection = Open_Inspection_Database(path, "inspect_local");
	query = QSqlQuery(inspection);
	QVERIFY(query.exec("SELECT level, goal, workouts_per_week FROM profile_local WHERE user_id = 'user-1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toString(), QString("beginner"));
	QCOMPARE(query.value(1).toString(), QString("lose_weight"));
	QCOMPARE(query.value(2).toInt(), 3);

	QVERIFY(query.exec("SELECT entity_type, entity_id, operation, payload_json, attempt_count FROM sync_outbox"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toString(), QString("profile_local"));
	QCOMPARE(query.value(1).toString(), QString("user-1"));
	QCOMPARE(query.value(2).toString(), QString("update"));
	QVERIFY(query.value(3).toString().contains("beginner"));
	QVERIFY(query.value(3).toString().contains("lose_weight"));
	QVERIFY(query.value(3).toString().contains("3"));
	QCOMPARE(query.value(4).toInt(), 0);
	Close_Inspection_Database(inspection);

	// Повторное сохранение обновляет поля и добавляет новую запись outbox.
	QVERIFY(repository.Save_Local_Profile("user-1", "advanced", "gain_muscle", 5, status));
	QCOMPARE(repository.Pending_Outbox_Count(), 2);

	inspection = Open_Inspection_Database(path, "inspect_local_update");
	query = QSqlQuery(inspection);
	QVERIFY(query.exec("SELECT level, workouts_per_week FROM profile_local WHERE user_id = 'user-1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toString(), QString("advanced"));
	QCOMPARE(query.value(1).toInt(), 5);
	Close_Inspection_Database(inspection);
}
//----------------------------------------------------------------------------
void AUsers_Repository_Test::localProfileLoadReturnsSavedFields()
{
	QTemporaryDir directory;
	AsSql_Database database;
	AUsers_Repository repository(0, &database);
	QString level;
	QString goal;
	int workouts_per_week = 0;
	QString status;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));

	// Нет сохранённых локальных параметров — не ошибка, полей нет.
	QVERIFY(repository.Load_Local_Profile("user-1", level, goal, workouts_per_week) == false);

	QVERIFY(repository.Save_Local_Profile("user-1", "beginner", "lose_weight", 3, status));
	QVERIFY(repository.Load_Local_Profile("user-1", level, goal, workouts_per_week));
	QCOMPARE(level, QString("beginner"));
	QCOMPARE(goal, QString("lose_weight"));
	QCOMPARE(workouts_per_week, 3);
}
//----------------------------------------------------------------------------
STest_Http_Response AUsers_Repository_Test::JSON_Response(int in_status, const QByteArray &in_body)
{
	STest_Http_Response response;

	response.Status_Code = in_status;
	response.Body = in_body;
	response.Content_Type = "application/json";

	return response;
}
//----------------------------------------------------------------------------
QByteArray AUsers_Repository_Test::User_With_Profile_Body()
{
	return "{\"id\":\"user-1\",\"name\":\"somename\",\"email\":\"user@example.com\","
	       "\"created_at\":\"2026-09-20T12:00:00Z\","
	       "\"profile\":{\"age\":31,\"gender\":\"male\",\"height\":179,\"weight\":74.5}}";
}
//----------------------------------------------------------------------------
QByteArray AUsers_Repository_Test::User_Without_Profile_Body()
{
	return "{\"id\":\"user-1\",\"name\":\"somename\",\"email\":\"user@example.com\","
	       "\"created_at\":\"2026-09-20T12:00:00Z\"}";
}
//----------------------------------------------------------------------------
QSqlDatabase AUsers_Repository_Test::Open_Inspection_Database(const QString &in_path, const QString &in_name)
{
	QSqlDatabase database;

	database = QSqlDatabase::addDatabase("QSQLITE", in_name);
	database.setDatabaseName(in_path);
	database.open();

	return database;
}
//----------------------------------------------------------------------------
void AUsers_Repository_Test::Close_Inspection_Database(QSqlDatabase &in_database)
{
	QString name;

	name = in_database.connectionName();
	in_database.close();
	in_database = QSqlDatabase();
	QSqlDatabase::removeDatabase(name);
}
//----------------------------------------------------------------------------

QTEST_MAIN(AUsers_Repository_Test)

#include "tst_Users_Repository.moc"
