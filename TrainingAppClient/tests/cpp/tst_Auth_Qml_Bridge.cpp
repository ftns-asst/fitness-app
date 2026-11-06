#include "Presentation/Auth_Qml_Bridge.h"

#include "Data/Auth/Auth_Repository.h"
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
#include <QVariantMap>
#include <QtTest>

static QStringList Captured_Messages;

//-------------------------------------------------------------------------------------------------------------
static void Capture_Message(QtMsgType in_type, const QMessageLogContext &in_context, const QString &in_message)
{
	Q_UNUSED(in_type);
	Q_UNUSED(in_context);
	Captured_Messages.append(in_message);
}

class AAuth_Qml_Bridge_Test : public QObject
{
	Q_OBJECT

private slots:
	void checkEmailMapsFreeFlagAndOperation();
	void checkEmailErrorMapsKeyAndStatus();
	void signupEmitsCamelCaseUserAndSavesSession();
	void signupErrorPassthrough();
	void loginEmitsCamelCaseUser();
	void logoutEmitsLoggedOutSignal();
	void sessionExpiredIsForwarded();
	void restoreSessionEmitsCachedUser();
	void saveLocalProfileWritesRealDbForSessionUser();
	void clearLocalProfileRemovesSavedRow();
	void logsDoNotContainCredentials();

private:
	static STest_Http_Response JSON_Response(int in_status, const QByteArray &in_body);
	static QByteArray Session_Body();
	static QSqlDatabase Open_Inspection_Database(const QString &in_path, const QString &in_name);
	static void Close_Inspection_Database(QSqlDatabase &in_database);
};

//-------------------------------------------------------------------------------------------------------------
void AAuth_Qml_Bridge_Test::checkEmailMapsFreeFlagAndOperation()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository auth_repository(&client, &database, &store, &session);
	AUsers_Repository users_repository(&session, &database);
	Avm_Auth_Bridge bridge(&auth_repository, &users_repository, &session);
	QSignalSpy spy(&bridge, &Avm_Auth_Bridge::requestFinished);
	QList<QVariant> arguments;
	QString operation;
	QVariantMap data;
	QVariantMap error;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	int request_id = bridge.checkEmail("user@example.com");
	QVERIFY(request_id > 0);
	server.Enqueue_Response(JSON_Response(200, "{\"free\":true}"));
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	operation = arguments.at(1).toString();
	data = arguments.at(3).toMap();
	error = arguments.at(4).toMap();
	QCOMPARE(arguments.at(0).toInt(), request_id);
	QCOMPARE(operation, QString("checkEmail"));
	QCOMPARE(arguments.at(2).toBool(), true);
	QCOMPARE(data.value("free").toBool(), true);
	QVERIFY(error.isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Qml_Bridge_Test::checkEmailErrorMapsKeyAndStatus()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository auth_repository(&client, &database, &store, &session);
	AUsers_Repository users_repository(&session, &database);
	Avm_Auth_Bridge bridge(&auth_repository, &users_repository, &session);
	QSignalSpy spy(&bridge, &Avm_Auth_Bridge::requestFinished);
	QList<QVariant> arguments;
	QVariantMap error;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(400, "{\"key\":\"invalid_email\",\"message\":\"bad email\"}"));
	int request_id = bridge.checkEmail("not-an-email");
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	error = arguments.at(4).toMap();
	QCOMPARE(arguments.at(1).toString(), QString("checkEmail"));
	QCOMPARE(arguments.at(2).toBool(), false);
	QCOMPARE(error.value("key").toString(), QString("invalid_email"));
	QCOMPARE(error.value("httpStatus").toInt(), 400);
	QCOMPARE(error.value("message").toString(), QString("bad email"));
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Qml_Bridge_Test::signupEmitsCamelCaseUserAndSavesSession()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository auth_repository(&client, &database, &store, &session);
	AUsers_Repository users_repository(&session, &database);
	Avm_Auth_Bridge bridge(&auth_repository, &users_repository, &session);
	QSignalSpy spy(&bridge, &Avm_Auth_Bridge::requestFinished);
	QList<QVariant> arguments;
	QVariantMap request;
	QVariantMap data;
	QVariantMap user;
	SStored_Tokens tokens;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(200, Session_Body()));
	request.insert("email", "user@example.com");
	request.insert("name", "somename");
	request.insert("password", "pass-1");
	int request_id = bridge.signup(request);
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	data = arguments.at(3).toMap();
	user = data.value("user").toMap();
	QCOMPARE(arguments.at(1).toString(), QString("signup"));
	QCOMPARE(arguments.at(2).toBool(), true);
	QCOMPARE(user.value("id").toString(), QString("user-1"));
	QCOMPARE(user.value("name").toString(), QString("somename"));
	QCOMPARE(user.value("email").toString(), QString("user@example.com"));
	QCOMPARE(user.value("createdAt").toString(), QString("2026-09-20T12:00:00Z"));
	QVERIFY(arguments.at(4).toMap().isEmpty());

	// Реальная сессия сохранена: токены и user_cache.
	QVERIFY(session.Has_Session());
	QVERIFY(store.Load_Tokens(tokens));
	QCOMPARE(tokens.Access_Token, QString("access-1"));
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Qml_Bridge_Test::signupErrorPassthrough()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository auth_repository(&client, &database, &store, &session);
	AUsers_Repository users_repository(&session, &database);
	Avm_Auth_Bridge bridge(&auth_repository, &users_repository, &session);
	QSignalSpy spy(&bridge, &Avm_Auth_Bridge::requestFinished);
	QList<QVariant> arguments;
	QVariantMap request;
	QVariantMap error;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(400, "{\"key\":\"email_taken\",\"message\":\"occupied\"}"));
	request.insert("email", "user@example.com");
	request.insert("name", "somename");
	request.insert("password", "pass-1");
	int request_id = bridge.signup(request);
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	error = arguments.at(4).toMap();
	QCOMPARE(arguments.at(1).toString(), QString("signup"));
	QCOMPARE(arguments.at(2).toBool(), false);
	QCOMPARE(error.value("key").toString(), QString("email_taken"));
	QCOMPARE(error.value("httpStatus").toInt(), 400);
	QVERIFY(!session.Has_Session());
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Qml_Bridge_Test::loginEmitsCamelCaseUser()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository auth_repository(&client, &database, &store, &session);
	AUsers_Repository users_repository(&session, &database);
	Avm_Auth_Bridge bridge(&auth_repository, &users_repository, &session);
	QSignalSpy spy(&bridge, &Avm_Auth_Bridge::requestFinished);
	QList<QVariant> arguments;
	QVariantMap data;
	QVariantMap user;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(200, Session_Body()));
	int request_id = bridge.login("user@example.com", "pass-1");
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	data = arguments.at(3).toMap();
	user = data.value("user").toMap();
	QCOMPARE(arguments.at(1).toString(), QString("login"));
	QCOMPARE(arguments.at(2).toBool(), true);
	QCOMPARE(user.value("id").toString(), QString("user-1"));
	QCOMPARE(user.value("email").toString(), QString("user@example.com"));
	QVERIFY(session.Has_Session());
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Qml_Bridge_Test::logoutEmitsLoggedOutSignal()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository auth_repository(&client, &database, &store, &session);
	AUsers_Repository users_repository(&session, &database);
	Avm_Auth_Bridge bridge(&auth_repository, &users_repository, &session);
	QSignalSpy spy(&bridge, &Avm_Auth_Bridge::requestFinished);
	QSignalSpy logout_spy(&bridge, &Avm_Auth_Bridge::loggedOut);
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(200, Session_Body()));
	QVERIFY(bridge.login("user@example.com", "pass-1") > 0);
	QVERIFY(spy.wait(5000));
	QVERIFY(session.Has_Session());

	bridge.logout();
	QCOMPARE(logout_spy.count(), 1);
	QVERIFY(!session.Has_Session());
	QVERIFY(!store.Has_Tokens());
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Qml_Bridge_Test::sessionExpiredIsForwarded()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository auth_repository(&client, &database, &store, &session);
	AUsers_Repository users_repository(&session, &database);
	Avm_Auth_Bridge bridge(&auth_repository, &users_repository, &session);
	QSignalSpy expired_spy(&bridge, &Avm_Auth_Bridge::sessionExpired);
	SHttp_Request request;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	QVERIFY(store.Save_Tokens("user-1", "expired-access", "expired-refresh", QDateTime(), QDateTime()));
	QVERIFY(bridge.restoreSession());
	QCOMPARE(expired_spy.count(), 0);

	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"token_expired\",\"message\":\"expired\"}"));
	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"invalid_token\",\"message\":\"refresh rejected\"}"));
	request.Method = "GET";
	request.Path = "users/user-1";
	request.Retry_Enabled = true;
	QVERIFY(session.Send_Authed(request) > 0);
	QVERIFY(expired_spy.wait(5000));
	QCOMPARE(expired_spy.count(), 1);
	QVERIFY(!store.Has_Tokens());
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Qml_Bridge_Test::restoreSessionEmitsCachedUser()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsHttp_Client client((QUrl()));
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	{
		AsToken_Store store(&database);
		AsAuth_Session session(&client, &store);
		AAuth_Repository auth_repository(&client, &database, &store, &session);
		AUsers_Repository users_repository(&session, &database);
		Avm_Auth_Bridge bridge(&auth_repository, &users_repository, &session);
		QSignalSpy spy(&bridge, &Avm_Auth_Bridge::requestFinished);

		QVERIFY(database.Open(path));
		server.Enqueue_Response(JSON_Response(200, Session_Body()));
		QVERIFY(bridge.login("user@example.com", "pass-1") > 0);
		QVERIFY(spy.wait(5000));
		database.Close();
	}

	// Cold start: сессия и пользователь восстанавливаются из SQLite.
	QVERIFY(database.Open(path));
	AsToken_Store second_store(&database);
	AsAuth_Session second_session(&client, &second_store);
	AAuth_Repository second_auth_repository(&client, &database, &second_store, &second_session);
	AUsers_Repository second_users_repository(&second_session, &database);
	Avm_Auth_Bridge second_bridge(&second_auth_repository, &second_users_repository, &second_session);
	QSignalSpy restored_spy(&second_bridge, &Avm_Auth_Bridge::sessionRestored);

	QVERIFY(second_bridge.restoreSession());
	QCOMPARE(restored_spy.count(), 1);
	QVERIFY(second_session.Has_Session());
	QCOMPARE(second_session.User_ID(), QString("user-1"));

	QList<QVariant> arguments = restored_spy.takeFirst();
	QVariantMap user = arguments.at(0).toMap();
	QCOMPARE(user.value("id").toString(), QString("user-1"));
	QCOMPARE(user.value("name").toString(), QString("somename"));
	QCOMPARE(user.value("email").toString(), QString("user@example.com"));
	QCOMPARE(user.value("createdAt").toString(), QString("2026-09-20T12:00:00Z"));
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Qml_Bridge_Test::saveLocalProfileWritesRealDbForSessionUser()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository auth_repository(&client, &database, &store, &session);
	AUsers_Repository users_repository(&session, &database);
	Avm_Auth_Bridge bridge(&auth_repository, &users_repository, &session);
	QSignalSpy spy(&bridge, &Avm_Auth_Bridge::requestFinished);
	QVariantMap local_profile;
	QSqlDatabase inspection;
	QSqlQuery query;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(200, Session_Body()));
	QVERIFY(bridge.login("user@example.com", "pass-1") > 0);
	QVERIFY(spy.wait(5000));

	// Локальные параметры после signup/login пишутся в реальную SQLite.
	local_profile.insert("level", "beginner");
	local_profile.insert("goal", "lose_weight");
	local_profile.insert("workoutsPerWeek", 3);
	QVERIFY(bridge.saveLocalProfile(local_profile));

	inspection = Open_Inspection_Database(path, "inspect_bridge_local");
	query = QSqlQuery(inspection);
	QVERIFY(query.exec("SELECT level, goal, workouts_per_week FROM profile_local WHERE user_id = 'user-1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toString(), QString("beginner"));
	QCOMPARE(query.value(1).toString(), QString("lose_weight"));
	QCOMPARE(query.value(2).toInt(), 3);
	Close_Inspection_Database(inspection);
	QCOMPARE(users_repository.Pending_Outbox_Count(), 1);
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Qml_Bridge_Test::clearLocalProfileRemovesSavedRow()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository auth_repository(&client, &database, &store, &session);
	AUsers_Repository users_repository(&session, &database);
	Avm_Auth_Bridge bridge(&auth_repository, &users_repository, &session);
	QSignalSpy spy(&bridge, &Avm_Auth_Bridge::requestFinished);
	QVariantMap local_profile;
	QSqlDatabase inspection;
	QSqlQuery query;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(200, Session_Body()));
	QVERIFY(bridge.login("user@example.com", "pass-1") > 0);
	QVERIFY(spy.wait(5000));

	local_profile.insert("level", "beginner");
	local_profile.insert("goal", "lose_weight");
	local_profile.insert("workoutsPerWeek", 3);
	QVERIFY(bridge.saveLocalProfile(local_profile));

	bridge.clearLocalProfile();
	inspection = Open_Inspection_Database(path, "inspect_bridge_clear");
	query = QSqlQuery(inspection);
	QVERIFY(query.exec("SELECT COUNT(*) FROM profile_local WHERE user_id = 'user-1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 0);
	Close_Inspection_Database(inspection);
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Qml_Bridge_Test::logsDoNotContainCredentials()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository auth_repository(&client, &database, &store, &session);
	AUsers_Repository users_repository(&session, &database);
	Avm_Auth_Bridge bridge(&auth_repository, &users_repository, &session);
	QSignalSpy spy(&bridge, &Avm_Auth_Bridge::requestFinished);
	QtMessageHandler previous_handler;
	QString all_messages;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(200, Session_Body()));

	Captured_Messages.clear();
	previous_handler = qInstallMessageHandler(Capture_Message);
	int request_id = bridge.login("user@example.com", "pass-1");
	bool finished = spy.wait(5000);
	qInstallMessageHandler(previous_handler);
	all_messages = Captured_Messages.join('\n');

	QVERIFY(request_id > 0);
	QVERIFY(finished);

	// Лог важного процесса есть, но без email, пароля и значений токенов.
	QVERIFY(all_messages.contains("login finished"));
	QVERIFY(!all_messages.contains("user@example.com"));
	QVERIFY(!all_messages.contains("pass-1"));
	QVERIFY(!all_messages.contains("access-1"));
	QVERIFY(!all_messages.contains("refresh-1"));
}
//-------------------------------------------------------------------------------------------------------------
STest_Http_Response AAuth_Qml_Bridge_Test::JSON_Response(int in_status, const QByteArray &in_body)
{
	STest_Http_Response response;

	response.Status_Code = in_status;
	response.Body = in_body;
	response.Content_Type = "application/json";

	return response;
}
//-------------------------------------------------------------------------------------------------------------
QByteArray AAuth_Qml_Bridge_Test::Session_Body()
{
	return "{\"user\":{\"id\":\"user-1\",\"name\":\"somename\",\"email\":\"user@example.com\","
	       "\"created_at\":\"2026-09-20T12:00:00Z\"},"
	       "\"tokens\":{\"access_token\":\"access-1\",\"refresh_token\":\"refresh-1\"}}";
}
//-------------------------------------------------------------------------------------------------------------
QSqlDatabase AAuth_Qml_Bridge_Test::Open_Inspection_Database(const QString &in_path, const QString &in_name)
{
	QSqlDatabase database;

	database = QSqlDatabase::addDatabase("QSQLITE", in_name);
	database.setDatabaseName(in_path);
	database.open();

	return database;
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Qml_Bridge_Test::Close_Inspection_Database(QSqlDatabase &in_database)
{
	QString name;

	name = in_database.connectionName();
	in_database.close();
	in_database = QSqlDatabase();
	QSqlDatabase::removeDatabase(name);
}
//-------------------------------------------------------------------------------------------------------------

QTEST_MAIN(AAuth_Qml_Bridge_Test)

#include "tst_Auth_Qml_Bridge.moc"