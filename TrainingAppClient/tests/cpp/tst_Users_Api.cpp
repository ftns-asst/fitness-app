#include "Data/Auth/Auth_Session.h"
#include "Data/Auth/Token_Store.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Network/Http_Client.h"
#include "Data/Users/Users_Api.h"
#include "Support/Test_Http_Server.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

class AUsers_Api_Test : public QObject
{
	Q_OBJECT

private slots:
	void getUserParsesUserAndProfile();
	void getUserWithoutProfileIsNotError();
	void getUserPreservesBackendErrorKey();
	void getUserIncompleteSuccessReportsUndefinedError();
	void getUserWithoutSessionIsRejected();
	void refreshRetriesGetUserOnce();

private:
	static STest_Http_Response JSON_Response(int in_status, const QByteArray &in_body);
	static QByteArray User_With_Profile_Body();
	static QByteArray User_Without_Profile_Body();
};

//----------------------------------------------------------------------------
void AUsers_Api_Test::getUserParsesUserAndProfile()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AUsers_Api api(&session);
	QSignalSpy spy(&api, &AUsers_Api::getUserFinished);
	QList<QVariant> arguments;
	SUsers_User user;
	AApi_Error error;
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
	int request_id = api.Get_User("user-1", true);
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	user = qvariant_cast<SUsers_User>(arguments.at(1));
	error = qvariant_cast<AApi_Error>(arguments.at(2));
	QCOMPARE(user.ID, QString("user-1"));
	QCOMPARE(user.Name, QString("somename"));
	QCOMPARE(user.Email, QString("user@example.com"));
	QCOMPARE(user.Created_At, QString("2026-09-20T12:00:00Z"));
	QVERIFY(user.Has_Profile);
	QCOMPARE(user.Profile.Age, 31);
	QCOMPARE(user.Profile.Gender, QString("male"));
	QCOMPARE(user.Profile.Height_Cm, (double)179);
	QCOMPARE(user.Profile.Weight_Kg, 74.5);
	QVERIFY(error.Is_Error() == false);

	STest_Http_Request captured = server.Request_At(0);
	QCOMPARE(captured.Method, QByteArray("GET"));
	QCOMPARE(captured.Target, QByteArray("/api/v1/users/user-1?withProfile=true"));
	QCOMPARE(captured.Headers.value("authorization"), QByteArray("Bearer stored-access"));
}
//----------------------------------------------------------------------------
void AUsers_Api_Test::getUserWithoutProfileIsNotError()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AUsers_Api api(&session);
	QSignalSpy spy(&api, &AUsers_Api::getUserFinished);
	QList<QVariant> arguments;
	SUsers_User user;
	AApi_Error error;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);
	QVERIFY(store.Save_Tokens("user-1", "stored-access", "stored-refresh", QDateTime(), QDateTime()));
	QVERIFY(session.Restore_From_Store());

	// Отсутствующий profile — «не заполнен», не ошибка (docs/03, §3.5).
	server.Enqueue_Response(JSON_Response(200, User_Without_Profile_Body()));
	int request_id = api.Get_User("user-1", false);
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	user = qvariant_cast<SUsers_User>(arguments.at(1));
	error = qvariant_cast<AApi_Error>(arguments.at(2));
	QCOMPARE(user.ID, QString("user-1"));
	QVERIFY(user.Has_Profile == false);
	QVERIFY(error.Is_Error() == false);

	STest_Http_Request captured = server.Request_At(0);
	QCOMPARE(captured.Target, QByteArray("/api/v1/users/user-1"));
}
//----------------------------------------------------------------------------
void AUsers_Api_Test::getUserPreservesBackendErrorKey()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AUsers_Api api(&session);
	QSignalSpy spy(&api, &AUsers_Api::getUserFinished);
	QList<QVariant> arguments;
	SUsers_User user;
	AApi_Error error;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);
	QVERIFY(store.Save_Tokens("user-1", "stored-access", "stored-refresh", QDateTime(), QDateTime()));
	QVERIFY(session.Restore_From_Store());

	server.Enqueue_Response(JSON_Response(404, "{\"key\":\"not_found\",\"message\":\"no user\"}"));
	int request_id = api.Get_User("user-1", true);
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	user = qvariant_cast<SUsers_User>(arguments.at(1));
	error = qvariant_cast<AApi_Error>(arguments.at(2));
	QCOMPARE(error.Key(), QString("not_found"));
	QCOMPARE(error.HTTP_Status_Code(), 404);
	QVERIFY(user.ID.isEmpty());
}
//----------------------------------------------------------------------------
void AUsers_Api_Test::getUserIncompleteSuccessReportsUndefinedError()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AUsers_Api api(&session);
	QSignalSpy spy(&api, &AUsers_Api::getUserFinished);
	QList<QVariant> arguments;
	SUsers_User user;
	AApi_Error error;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);
	QVERIFY(store.Save_Tokens("user-1", "stored-access", "stored-refresh", QDateTime(), QDateTime()));
	QVERIFY(session.Restore_From_Store());

	// Успешный ответ без обязательного id — неполный ответ (docs/03, §2).
	server.Enqueue_Response(JSON_Response(200, "{}"));
	int request_id = api.Get_User("user-1", true);
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	user = qvariant_cast<SUsers_User>(arguments.at(1));
	error = qvariant_cast<AApi_Error>(arguments.at(2));
	QCOMPARE(error.Key(), QString("undefined_error"));
	QVERIFY(user.ID.isEmpty());
}
//----------------------------------------------------------------------------
void AUsers_Api_Test::getUserWithoutSessionIsRejected()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AUsers_Api api(&session);
	QSignalSpy spy(&api, &AUsers_Api::getUserFinished);
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));

	QCOMPARE(api.Get_User("user-1", true), 0);
	QCOMPARE(spy.count(), 0);
	QCOMPARE(server.Request_Count(), 0);
}
//----------------------------------------------------------------------------
void AUsers_Api_Test::refreshRetriesGetUserOnce()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AUsers_Api api(&session);
	QSignalSpy spy(&api, &AUsers_Api::getUserFinished);
	QList<QVariant> arguments;
	SUsers_User user;
	AApi_Error error;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);
	QVERIFY(store.Save_Tokens("user-1", "expired-access", "old-refresh", QDateTime(), QDateTime()));
	QVERIFY(session.Restore_From_Store());

	// Истёкший access: single-flight refresh и один повтор GET users (issue 7).
	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"token_expired\",\"message\":\"expired\"}"));
	server.Enqueue_Response(
	    JSON_Response(200, "{\"tokens\":{\"access_token\":\"new-access\",\"refresh_token\":\"new-refresh\"}}"));
	server.Enqueue_Response(JSON_Response(200, User_With_Profile_Body()));
	int request_id = api.Get_User("user-1", true);
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	user = qvariant_cast<SUsers_User>(arguments.at(1));
	error = qvariant_cast<AApi_Error>(arguments.at(2));
	QVERIFY(error.Is_Error() == false);
	QCOMPARE(user.ID, QString("user-1"));
	QVERIFY(user.Has_Profile);
	QCOMPARE(server.Request_Count(), 3);
	QCOMPARE(server.Request_At(2).Headers.value("authorization"), QByteArray("Bearer new-access"));
	QCOMPARE(server.Request_At(2).Target, QByteArray("/api/v1/users/user-1?withProfile=true"));
}
//----------------------------------------------------------------------------
STest_Http_Response AUsers_Api_Test::JSON_Response(int in_status, const QByteArray &in_body)
{
	STest_Http_Response response;

	response.Status_Code = in_status;
	response.Body = in_body;
	response.Content_Type = "application/json";

	return response;
}
//----------------------------------------------------------------------------
QByteArray AUsers_Api_Test::User_With_Profile_Body()
{
	return "{\"id\":\"user-1\",\"name\":\"somename\",\"email\":\"user@example.com\","
	       "\"created_at\":\"2026-09-20T12:00:00Z\","
	       "\"profile\":{\"age\":31,\"gender\":\"male\",\"height\":179,\"weight\":74.5}}";
}
//----------------------------------------------------------------------------
QByteArray AUsers_Api_Test::User_Without_Profile_Body()
{
	return "{\"id\":\"user-1\",\"name\":\"somename\",\"email\":\"user@example.com\","
	       "\"created_at\":\"2026-09-20T12:00:00Z\"}";
}
//----------------------------------------------------------------------------

QTEST_MAIN(AUsers_Api_Test)

#include "tst_Users_Api.moc"
