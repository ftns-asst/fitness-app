#include "Data/Auth/Auth_Repository.h"
#include "Data/Auth/Auth_Session.h"
#include "Data/Auth/Token_Store.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Network/Http_Client.h"
#include "Support/Test_Http_Server.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QSqlDatabase>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class AAuth_Repository_Test : public QObject
{
	Q_OBJECT

private slots:
	void loginSavesSessionAndUserCache();
	void signupSavesSessionAndUserCache();
	void failedLoginKeepsSessionEmpty();
	void logoutClearsTokensAndKeepsUserCache();
	void coldStartRestoresPersistedSession();
	void forwardsSessionExpired();

private:
	static STest_Http_Response JSON_Response(int in_status, const QByteArray &in_body);
	static QByteArray Session_Body();
	static QSqlDatabase Open_Inspection_Database(const QString &in_path, const QString &in_name);
	static void Close_Inspection_Database(QSqlDatabase &in_database);
};

//----------------------------------------------------------------------------
void AAuth_Repository_Test::loginSavesSessionAndUserCache()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository repository(&client, &database, &store, &session);
	QSignalSpy spy(&repository, &AAuth_Repository::loginFinished);
	QList<QVariant> arguments;
	SStored_Tokens tokens;
	QSqlDatabase inspection;
	QSqlQuery query;
	AApi_Error error;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(200, Session_Body()));
	int request_id = repository.Login("user@example.com", "pass-1");
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	error = qvariant_cast<AApi_Error>(arguments.at(2));
	QVERIFY(error.Is_Error() == false);

	// Сессия активна, токены и пользователь атомарно сохранены.
	QVERIFY(session.Has_Session());
	QCOMPARE(session.User_ID(), QString("user-1"));
	QVERIFY(store.Load_Tokens(tokens));
	QCOMPARE(tokens.User_ID, QString("user-1"));
	QCOMPARE(tokens.Access_Token, QString("access-1"));
	QCOMPARE(tokens.Refresh_Token, QString("refresh-1"));

	inspection = Open_Inspection_Database(path, "inspect_login");
	query = QSqlQuery(inspection);
	QVERIFY(query.exec("SELECT email, display_name, created_at FROM user_cache WHERE id = 'user-1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toString(), QString("user@example.com"));
	QCOMPARE(query.value(1).toString(), QString("somename"));
	QCOMPARE(query.value(2).toString(), QString("2026-09-20T12:00:00Z"));
	Close_Inspection_Database(inspection);
}
//----------------------------------------------------------------------------
void AAuth_Repository_Test::signupSavesSessionAndUserCache()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository repository(&client, &database, &store, &session);
	QSignalSpy spy(&repository, &AAuth_Repository::signupFinished);
	QList<QVariant> arguments;
	QSqlDatabase inspection;
	QSqlQuery query;
	AApi_Error error;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(200, Session_Body()));
	int request_id = repository.Signup("user@example.com", "somename", "pass-1");
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	error = qvariant_cast<AApi_Error>(arguments.at(2));
	QVERIFY(error.Is_Error() == false);

	QVERIFY(session.Has_Session());
	QCOMPARE(session.User_ID(), QString("user-1"));
	QVERIFY(store.Has_Tokens());

	inspection = Open_Inspection_Database(path, "inspect_signup");
	query = QSqlQuery(inspection);
	QVERIFY(query.exec("SELECT display_name FROM user_cache WHERE id = 'user-1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toString(), QString("somename"));
	Close_Inspection_Database(inspection);
}
//----------------------------------------------------------------------------
void AAuth_Repository_Test::failedLoginKeepsSessionEmpty()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository repository(&client, &database, &store, &session);
	QSignalSpy spy(&repository, &AAuth_Repository::loginFinished);
	QList<QVariant> arguments;
	QSqlDatabase inspection;
	QSqlQuery query;
	AApi_Error error;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"incorrect_password\",\"message\":\"wrong\"}"));
	int request_id = repository.Login("user@example.com", "wrong-pass");
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	error = qvariant_cast<AApi_Error>(arguments.at(2));
	QCOMPARE(error.Key(), QString("incorrect_password"));

	// Неудачный вход не оставляет ни сессии, ни токенов, ни кэша пользователя.
	QVERIFY(session.Has_Session() == false);
	QVERIFY(store.Has_Tokens() == false);

	inspection = Open_Inspection_Database(path, "inspect_failed");
	query = QSqlQuery(inspection);
	QVERIFY(query.exec("SELECT COUNT(*) FROM user_cache"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 0);
	Close_Inspection_Database(inspection);
}
//----------------------------------------------------------------------------
void AAuth_Repository_Test::logoutClearsTokensAndKeepsUserCache()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository repository(&client, &database, &store, &session);
	QSignalSpy spy(&repository, &AAuth_Repository::loginFinished);
	QSignalSpy logout_spy(&repository, &AAuth_Repository::loggedOut);
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
	QVERIFY(repository.Login("user@example.com", "pass-1") > 0);
	QVERIFY(spy.wait(5000));
	spy.clear();

	// Локальный logout: токены и сессия очищаются, кэш пользователя остаётся.
	repository.Logout();
	QCOMPARE(logout_spy.count(), 1);
	QVERIFY(session.Has_Session() == false);
	QVERIFY(store.Has_Tokens() == false);

	inspection = Open_Inspection_Database(path, "inspect_logout");
	query = QSqlQuery(inspection);
	QVERIFY(query.exec("SELECT COUNT(*) FROM user_cache"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 1);
	Close_Inspection_Database(inspection);
}
//----------------------------------------------------------------------------
void AAuth_Repository_Test::coldStartRestoresPersistedSession()
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
		AAuth_Repository repository(&client, &database, &store, &session);
		QSignalSpy spy(&repository, &AAuth_Repository::loginFinished);

		QVERIFY(database.Open(path));
		server.Enqueue_Response(JSON_Response(200, Session_Body()));
		QVERIFY(repository.Login("user@example.com", "pass-1") > 0);
		QVERIFY(spy.wait(5000));
		database.Close();
	}

	// Новый запуск приложения: сессия восстанавливается из приватной SQLite.
	QVERIFY(database.Open(path));
	AsToken_Store second_store(&database);
	AsAuth_Session second_session(&client, &second_store);
	AAuth_Repository second_repository(&client, &database, &second_store, &second_session);

	QVERIFY(second_repository.Restore_Local_Session());
	QVERIFY(second_repository.Has_Local_Session());
	QVERIFY(second_session.Has_Session());
	QCOMPARE(second_session.User_ID(), QString("user-1"));
}
//----------------------------------------------------------------------------
void AAuth_Repository_Test::forwardsSessionExpired()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository repository(&client, &database, &store, &session);
	QSignalSpy expired_spy(&repository, &AAuth_Repository::sessionExpired);
	SHttp_Request request;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	QVERIFY(store.Save_Tokens("user-1", "expired-access", "expired-refresh", QDateTime(), QDateTime()));
	QVERIFY(repository.Restore_Local_Session());

	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"token_expired\",\"message\":\"expired\"}"));
	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"invalid_token\",\"message\":\"refresh rejected\"}"));
	request.Method = "GET";
	request.Path = "users/user-1";
	request.Retry_Enabled = true;
	QVERIFY(session.Send_Authed(request) > 0);
	QVERIFY(expired_spy.wait(5000));
	QCOMPARE(expired_spy.count(), 1);
	QVERIFY(store.Has_Tokens() == false);
}
//----------------------------------------------------------------------------
STest_Http_Response AAuth_Repository_Test::JSON_Response(int in_status, const QByteArray &in_body)
{
	STest_Http_Response response;

	response.Status_Code = in_status;
	response.Body = in_body;
	response.Content_Type = "application/json";

	return response;
}
//----------------------------------------------------------------------------
QByteArray AAuth_Repository_Test::Session_Body()
{
	return "{\"user\":{\"id\":\"user-1\",\"name\":\"somename\",\"email\":\"user@example.com\","
	       "\"created_at\":\"2026-09-20T12:00:00Z\"},"
	       "\"tokens\":{\"access_token\":\"access-1\",\"refresh_token\":\"refresh-1\"}}";
}
//----------------------------------------------------------------------------
QSqlDatabase AAuth_Repository_Test::Open_Inspection_Database(const QString &in_path, const QString &in_name)
{
	QSqlDatabase database;

	database = QSqlDatabase::addDatabase("QSQLITE", in_name);
	database.setDatabaseName(in_path);
	database.open();

	return database;
}
//----------------------------------------------------------------------------
void AAuth_Repository_Test::Close_Inspection_Database(QSqlDatabase &in_database)
{
	QString name;

	name = in_database.connectionName();
	in_database.close();
	in_database = QSqlDatabase();
	QSqlDatabase::removeDatabase(name);
}
//----------------------------------------------------------------------------

QTEST_MAIN(AAuth_Repository_Test)

#include "tst_Auth_Repository.moc"
