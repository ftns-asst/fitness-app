#include "Data/Auth/Auth_Session.h"
#include "Data/Auth/Token_Store.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Network/Http_Client.h"
#include "Support/Test_Http_Server.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

static QStringList Captured_Messages;

//-------------------------------------------------------------------------------------------------------------
static void Capture_Message(QtMsgType in_type, const QMessageLogContext &in_context, const QString &in_message)
{
	Q_UNUSED(in_type);
	Q_UNUSED(in_context);
	Captured_Messages.append(in_message);
}

class AAuth_Session_Test : public QObject
{
	Q_OBJECT

private slots:
	void authedRequestCarriesStoredBearerToken();
	void sendWithoutSessionIsRejected();
	void expiredAccessTriggersSingleRefreshAndRetriesOnce();
	void parallelFailuresShareSingleRefresh();
	void failedRefreshClearsTokensAndEmitsSessionExpired();
	void networkRefreshFailureKeepsLocalSession();
	void secondAuthFailureAfterRefreshIsNotRetried();
	void coldStartRestoresSessionFromStore();
	void logsDoNotContainTokens();
	void serverErrorOnRefreshKeepsSession();
	void malformedRefreshSuccessKeepsSession();
	void replacedSessionIgnoresStaleRefresh();
	void lateAuthFailureAfterRefreshSkipsSecondRefresh();

private:
	struct SFixture
	{
		ATest_Http_Server Server;
		QTemporaryDir Directory;
		AsSql_Database Database;
		AsToken_Store Store;
		AsHttp_Client Client;
		AsAuth_Session Session;

		SFixture() : Store(&Database), Client((QUrl())), Session(&Client, &Store)
		{
		}
	};

	// Открытая база, запущенный сервер и восстановленная сессия user-1 с
	// парой expired-access/old-refresh.
	static bool Start_Fixture(SFixture &in_fixture);
	static int Count_Refreshes(const ATest_Http_Server &in_server);
	static bool Wait_For_Response(QSignalSpy &in_spy, SHttp_Response &out_response, int in_timeout_ms = 5000);
	static STest_Http_Response JSON_Response(int in_status, const QByteArray &in_body);
	static SHttp_Request Authed_Get(const QString &in_path);
};

//-------------------------------------------------------------------------------------------------------------
void AAuth_Session_Test::authedRequestCarriesStoredBearerToken()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	QSignalSpy finished_spy(&session, &AsAuth_Session::requestFinished);
	SHttp_Response response;
	STest_Http_Request captured;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	QVERIFY(store.Save_Tokens("user-1", "stored-access", "stored-refresh", QDateTime(), QDateTime()));
	QVERIFY(session.Restore_From_Store());
	QVERIFY(session.Has_Session());
	QCOMPARE(session.User_ID(), QString("user-1"));

	server.Enqueue_Response(JSON_Response(200, "{\"id\":\"user-1\"}"));
	int request_id = session.Send_Authed(Authed_Get("users/user-1"));
	QVERIFY(request_id > 0);
	QVERIFY(Wait_For_Response(finished_spy, response));
	QCOMPARE(response.Request_Id, request_id);
	QVERIFY(response.Success);
	QCOMPARE(response.HTTP_Status_Code, 200);
	QCOMPARE(server.Request_Count(), 1);

	captured = server.Request_At(0);
	QCOMPARE(captured.Method, QByteArray("GET"));
	QCOMPARE(captured.Target, QByteArray("/api/v1/users/user-1"));
	QCOMPARE(captured.Headers.value("authorization"), QByteArray("Bearer stored-access"));
	QCOMPARE(session.Last_Error(), QString());
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Session_Test::sendWithoutSessionIsRejected()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	QSignalSpy finished_spy(&session, &AsAuth_Session::requestFinished);

	QVERIFY(directory.isValid());
	QVERIFY(database.Open(directory.filePath("cache.sqlite3")));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));

	QVERIFY(!session.Restore_From_Store());
	QVERIFY(!session.Has_Session());
	QCOMPARE(session.Send_Authed(Authed_Get("users/user-1")), 0);
	QVERIFY(!session.Last_Error().isEmpty());
	QCOMPARE(finished_spy.count(), 0);
	QCOMPARE(server.Request_Count(), 0);
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Session_Test::expiredAccessTriggersSingleRefreshAndRetriesOnce()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	QSignalSpy finished_spy(&session, &AsAuth_Session::requestFinished);
	SHttp_Response response;
	STest_Http_Request refresh_request;
	STest_Http_Request retried_request;
	SStored_Tokens rotated;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	QVERIFY(store.Save_Tokens("user-1", "expired-access", "old-refresh", QDateTime(), QDateTime()));
	QVERIFY(session.Restore_From_Store());

	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"token_expired\",\"message\":\"access expired\"}"));
	server.Enqueue_Response(
	    JSON_Response(200, "{\"tokens\":{\"access_token\":\"new-access\",\"refresh_token\":\"new-refresh\"}}"));
	server.Enqueue_Response(JSON_Response(200, "{\"id\":\"user-1\",\"email\":\"user@example.com\"}"));

	int request_id = session.Send_Authed(Authed_Get("users/user-1"));
	QVERIFY(request_id > 0);
	QVERIFY(Wait_For_Response(finished_spy, response));
	QCOMPARE(response.Request_Id, request_id);
	QVERIFY(response.Success);
	QCOMPARE(response.HTTP_Status_Code, 200);
	QCOMPARE(server.Request_Count(), 3);

	// Single-flight: один POST /auth/refresh со старым refresh-токеном.
	refresh_request = server.Request_At(1);
	QCOMPARE(refresh_request.Method, QByteArray("POST"));
	QCOMPARE(refresh_request.Target, QByteArray("/api/v1/auth/refresh"));
	QCOMPARE(refresh_request.Headers.value("content-type"), QByteArray("application/json"));
	QVERIFY(refresh_request.Body.contains("old-refresh"));
	QVERIFY(!refresh_request.Body.contains("expired-access"));

	// Исходный запрос повторён один раз с новым access-токеном.
	retried_request = server.Request_At(2);
	QCOMPARE(retried_request.Target, QByteArray("/api/v1/users/user-1"));
	QCOMPARE(retried_request.Headers.value("authorization"), QByteArray("Bearer new-access"));

	// Ротация атомарно сохранена в SQLite.
	QVERIFY(store.Load_Tokens(rotated));
	QCOMPARE(rotated.User_ID, QString("user-1"));
	QCOMPARE(rotated.Access_Token, QString("new-access"));
	QCOMPARE(rotated.Refresh_Token, QString("new-refresh"));
	QVERIFY(session.Has_Session());
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Session_Test::parallelFailuresShareSingleRefresh()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	QSignalSpy finished_spy(&session, &AsAuth_Session::requestFinished);
	SHttp_Response first_response;
	SHttp_Response second_response;
	SHttp_Response response_a;
	SHttp_Response response_b;
	QStringList retried_targets;
	QString path;
	int refresh_count;
	int i;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	QVERIFY(store.Save_Tokens("user-1", "expired-access", "old-refresh", QDateTime(), QDateTime()));
	QVERIFY(session.Restore_From_Store());

	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"token_expired\",\"message\":\"expired\"}"));
	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"token_expired\",\"message\":\"expired\"}"));
	server.Enqueue_Response(
	    JSON_Response(200, "{\"tokens\":{\"access_token\":\"new-access\",\"refresh_token\":\"new-refresh\"}}"));
	server.Enqueue_Response(JSON_Response(200, "{\"id\":\"user-1\",\"email\":\"first\"}"));
	server.Enqueue_Response(JSON_Response(200, "{\"id\":\"user-1\",\"email\":\"second\"}"));

	int first_id = session.Send_Authed(Authed_Get("users/first"));
	int second_id = session.Send_Authed(Authed_Get("users/second"));
	QVERIFY(first_id > 0);
	QVERIFY(second_id > 0);
	QVERIFY(first_id != second_id);

	QVERIFY(Wait_For_Response(finished_spy, response_a));
	QVERIFY(Wait_For_Response(finished_spy, response_b));

	// Порядок ответов зависит от того, чей 401 пришёл первым, — сверяем по id.
	if (response_a.Request_Id == first_id)
	{
		first_response = response_a;
		second_response = response_b;
	}
	else
	{
		first_response = response_b;
		second_response = response_a;
	}

	QVERIFY(first_response.Success);
	QVERIFY(second_response.Success);
	QCOMPARE(first_response.Request_Id, first_id);
	QCOMPARE(second_response.Request_Id, second_id);
	QCOMPARE(server.Request_Count(), 5);

	refresh_count = 0;
	for (i = 0; i < server.Request_Count(); ++i)
	{
		if (server.Request_At(i).Target == QByteArray("/api/v1/auth/refresh"))
			++refresh_count;

		// Повторные запросы (позиции 3 и 4) идут с новым access-токеном.
		if (i >= 3)
			QCOMPARE(server.Request_At(i).Headers.value("authorization"), QByteArray("Bearer new-access"));
	}
	QCOMPARE(refresh_count, 1);

	// Оба исходных запроса повторены по одному разу.
	retried_targets.append(server.Request_At(3).Target);
	retried_targets.append(server.Request_At(4).Target);
	QVERIFY(retried_targets.contains("/api/v1/users/first"));
	QVERIFY(retried_targets.contains("/api/v1/users/second"));
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Session_Test::failedRefreshClearsTokensAndEmitsSessionExpired()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	QSignalSpy finished_spy(&session, &AsAuth_Session::requestFinished);
	QSignalSpy expired_spy(&session, &AsAuth_Session::sessionExpired);
	SHttp_Response response;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	QVERIFY(store.Save_Tokens("user-1", "expired-access", "expired-refresh", QDateTime(), QDateTime()));
	QVERIFY(session.Restore_From_Store());

	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"token_expired\",\"message\":\"access expired\"}"));
	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"invalid_token\",\"message\":\"refresh rejected\"}"));

	int request_id = session.Send_Authed(Authed_Get("users/user-1"));
	QVERIFY(request_id > 0);
	QVERIFY(Wait_For_Response(finished_spy, response));
	QCOMPARE(response.Request_Id, request_id);
	QVERIFY(!response.Success);
	QCOMPARE(response.HTTP_Status_Code, 401);
	QCOMPARE(response.Error.Key(), QString("token_expired"));
	QCOMPARE(server.Request_Count(), 2);

	// Токены очищены, сессия завершена, UI получает sessionExpired.
	QCOMPARE(expired_spy.count(), 1);
	QVERIFY(!store.Has_Tokens());
	QVERIFY(!session.Has_Session());
	QVERIFY(session.User_ID().isEmpty());

	// Новые приватные запросы отклоняются до нового входа.
	QCOMPARE(session.Send_Authed(Authed_Get("users/user-1")), 0);
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Session_Test::networkRefreshFailureKeepsLocalSession()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	QSignalSpy finished_spy(&session, &AsAuth_Session::requestFinished);
	QSignalSpy expired_spy(&session, &AsAuth_Session::sessionExpired);
	STest_Http_Response disconnect_response;
	SHttp_Response response;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	QVERIFY(store.Save_Tokens("user-1", "expired-access", "old-refresh", QDateTime(), QDateTime()));
	QVERIFY(session.Restore_From_Store());

	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"token_expired\",\"message\":\"access expired\"}"));
	disconnect_response.Disconnect_Without_Response = true;
	server.Enqueue_Response(disconnect_response);

	int request_id = session.Send_Authed(Authed_Get("users/user-1"));
	QVERIFY(request_id > 0);
	QVERIFY(Wait_For_Response(finished_spy, response));
	QCOMPARE(response.Request_Id, request_id);
	QVERIFY(!response.Success);
	QCOMPARE(response.HTTP_Status_Code, 401);
	QCOMPARE(server.Request_Count(), 2);

	// Transport-сбой refresh не уничтожает локальную сессию (docs/03, §6).
	QCOMPARE(expired_spy.count(), 0);
	QVERIFY(store.Has_Tokens());
	QVERIFY(session.Has_Session());
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Session_Test::secondAuthFailureAfterRefreshIsNotRetried()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	QSignalSpy finished_spy(&session, &AsAuth_Session::requestFinished);
	QSignalSpy expired_spy(&session, &AsAuth_Session::sessionExpired);
	SHttp_Response response;
	QString path;
	int refresh_count;
	int i;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	QVERIFY(store.Save_Tokens("user-1", "expired-access", "old-refresh", QDateTime(), QDateTime()));
	QVERIFY(session.Restore_From_Store());

	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"token_expired\",\"message\":\"access expired\"}"));
	server.Enqueue_Response(
	    JSON_Response(200, "{\"tokens\":{\"access_token\":\"new-access\",\"refresh_token\":\"new-refresh\"}}"));
	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"invalid_token\",\"message\":\"still rejected\"}"));

	int request_id = session.Send_Authed(Authed_Get("users/user-1"));
	QVERIFY(request_id > 0);
	QVERIFY(Wait_For_Response(finished_spy, response));
	QCOMPARE(response.Request_Id, request_id);
	QVERIFY(!response.Success);
	QCOMPARE(response.HTTP_Status_Code, 401);
	QCOMPARE(response.Error.Key(), QString("invalid_token"));

	// Один refresh, один повтор исходного запроса — без второго refresh.
	QCOMPARE(server.Request_Count(), 3);
	refresh_count = 0;
	for (i = 0; i < server.Request_Count(); ++i)
	{
		if (server.Request_At(i).Target == QByteArray("/api/v1/auth/refresh"))
			++refresh_count;
	}
	QCOMPARE(refresh_count, 1);
	QCOMPARE(expired_spy.count(), 0);

	// Повторный auth failure не сбрасывает сохранённую пару токенов.
	QVERIFY(store.Has_Tokens());
	QVERIFY(session.Has_Session());
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Session_Test::coldStartRestoresSessionFromStore()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store second_store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &second_store);
	QSignalSpy finished_spy(&session, &AsAuth_Session::requestFinished);
	SHttp_Response response;
	STest_Http_Request captured;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	{
		// Первый запуск приложения: токены уже сохранены login-сценарием.
		AsSql_Database first_database;

		QVERIFY(first_database.Open(path));
		AsToken_Store first_store(&first_database);
		QVERIFY(first_store.Save_Tokens("user-7", "cold-access", "cold-refresh", QDateTime(), QDateTime()));
		first_database.Close();
	}

	// Cold start: сессия восстанавливается из приватной SQLite.
	QVERIFY(database.Open(path));
	QVERIFY(!session.Has_Session());
	QVERIFY(session.Restore_From_Store());
	QVERIFY(session.Has_Session());
	QCOMPARE(session.User_ID(), QString("user-7"));

	server.Enqueue_Response(JSON_Response(200, "{\"id\":\"user-7\"}"));
	int request_id = session.Send_Authed(Authed_Get("users/user-7"));
	QVERIFY(request_id > 0);
	QVERIFY(Wait_For_Response(finished_spy, response));
	QVERIFY(response.Success);
	QCOMPARE(server.Request_Count(), 1);

	captured = server.Request_At(0);
	QCOMPARE(captured.Headers.value("authorization"), QByteArray("Bearer cold-access"));
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Session_Test::logsDoNotContainTokens()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	QSignalSpy finished_spy(&session, &AsAuth_Session::requestFinished);
	SHttp_Response response;
	QtMessageHandler previous_handler;
	QString all_messages;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	QVERIFY(store.Save_Tokens("user-1", "expired-access", "old-refresh", QDateTime(), QDateTime()));
	QVERIFY(session.Restore_From_Store());

	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"token_expired\",\"message\":\"secret-message\"}"));
	server.Enqueue_Response(
	    JSON_Response(200, "{\"tokens\":{\"access_token\":\"new-access\",\"refresh_token\":\"new-refresh\"}}"));
	server.Enqueue_Response(JSON_Response(200, "{\"id\":\"user-1\"}"));

	Captured_Messages.clear();
	previous_handler = qInstallMessageHandler(Capture_Message);
	int request_id = session.Send_Authed(Authed_Get("users/user-1"));
	bool response_received = Wait_For_Response(finished_spy, response);
	qInstallMessageHandler(previous_handler);
	all_messages = Captured_Messages.join('\n');

	QVERIFY(request_id > 0);
	QVERIFY(response_received);
	QVERIFY(!all_messages.contains("expired-access"));
	QVERIFY(!all_messages.contains("old-refresh"));
	QVERIFY(!all_messages.contains("new-access"));
	QVERIFY(!all_messages.contains("new-refresh"));
	QVERIFY(!all_messages.contains("secret-message"));
	QVERIFY(!all_messages.contains("Authorization"));
}
//-------------------------------------------------------------------------------------------------------------
bool AAuth_Session_Test::Wait_For_Response(QSignalSpy &in_spy, SHttp_Response &out_response, int in_timeout_ms)
{
	QList<QVariant> arguments;

	if (in_spy.isEmpty() && !in_spy.wait(in_timeout_ms))
		return false;

	arguments = in_spy.takeFirst();

	if (arguments.size() != 2)
		return false;

	out_response = qvariant_cast<SHttp_Response>(arguments.at(1));

	return true;
}
//-------------------------------------------------------------------------------------------------------------
STest_Http_Response AAuth_Session_Test::JSON_Response(int in_status, const QByteArray &in_body)
{
	STest_Http_Response response;

	response.Status_Code = in_status;
	response.Body = in_body;
	response.Content_Type = "application/json";

	return response;
}
//-------------------------------------------------------------------------------------------------------------
SHttp_Request AAuth_Session_Test::Authed_Get(const QString &in_path)
{
	SHttp_Request request;

	request.Method = "GET";
	request.Path = in_path;
	request.Retry_Enabled = true;

	return request;
}
//-------------------------------------------------------------------------------------------------------------
bool AAuth_Session_Test::Start_Fixture(SFixture &in_fixture)
{
	if (!in_fixture.Directory.isValid() || !in_fixture.Database.Open(in_fixture.Directory.filePath("cache.sqlite3")))
		return false;

	if (!in_fixture.Server.Start() ||
	    !in_fixture.Client.Set_Base_URL(in_fixture.Server.Base_URL().resolved(QUrl("/api/v1"))))
		return false;

	in_fixture.Client.Set_Max_Retry_Count(0);

	if (!in_fixture.Store.Save_Tokens("user-1", "expired-access", "old-refresh", QDateTime(), QDateTime()))
		return false;

	return in_fixture.Session.Restore_From_Store();
}
//-------------------------------------------------------------------------------------------------------------
int AAuth_Session_Test::Count_Refreshes(const ATest_Http_Server &in_server)
{
	int count = 0;

	for (int i = 0; i < in_server.Request_Count(); ++i)
	{
		if (in_server.Request_At(i).Target == QByteArray("/api/v1/auth/refresh"))
			++count;
	}

	return count;
}
//-------------------------------------------------------------------------------------------------------------
// 5xx на refresh — сбой сервера, а не отказ: сессия и токены сохраняются.
//-------------------------------------------------------------------------------------------------------------
void AAuth_Session_Test::serverErrorOnRefreshKeepsSession()
{
	SFixture fixture;
	QSignalSpy finished_spy(&fixture.Session, &AsAuth_Session::requestFinished);
	QSignalSpy expired_spy(&fixture.Session, &AsAuth_Session::sessionExpired);
	SHttp_Response response;

	QVERIFY(Start_Fixture(fixture));
	fixture.Server.Enqueue_Response(JSON_Response(401, "{\"key\":\"token_expired\",\"message\":\"expired\"}"));
	fixture.Server.Enqueue_Response(JSON_Response(503, "{\"key\":\"unavailable\",\"message\":\"down\"}"));

	QVERIFY(fixture.Session.Send_Authed(Authed_Get("users/user-1")) > 0);
	QVERIFY(Wait_For_Response(finished_spy, response));
	QVERIFY(!response.Success);

	QCOMPARE(expired_spy.count(), 0);
	QVERIFY(fixture.Session.Has_Session());
	QVERIFY(fixture.Store.Has_Tokens());
}
//-------------------------------------------------------------------------------------------------------------
// 200 без пары токенов трактуется как transport-сбой (docs/03, сценарий 4).
//-------------------------------------------------------------------------------------------------------------
void AAuth_Session_Test::malformedRefreshSuccessKeepsSession()
{
	SFixture fixture;
	QSignalSpy finished_spy(&fixture.Session, &AsAuth_Session::requestFinished);
	QSignalSpy expired_spy(&fixture.Session, &AsAuth_Session::sessionExpired);
	SHttp_Response response;

	QVERIFY(Start_Fixture(fixture));
	fixture.Server.Enqueue_Response(JSON_Response(401, "{\"key\":\"token_expired\",\"message\":\"expired\"}"));
	fixture.Server.Enqueue_Response(JSON_Response(200, "not json"));

	QVERIFY(fixture.Session.Send_Authed(Authed_Get("users/user-1")) > 0);
	QVERIFY(Wait_For_Response(finished_spy, response));

	QCOMPARE(expired_spy.count(), 0);
	QVERIFY(fixture.Session.Has_Session());
	QVERIFY(fixture.Store.Has_Tokens());
}
//-------------------------------------------------------------------------------------------------------------
// Logout и вход другим пользователем во время refresh: запоздалая ротация
// прежней сессии не попадает в новую, очередь прежней сессии не отправляется.
//-------------------------------------------------------------------------------------------------------------
void AAuth_Session_Test::replacedSessionIgnoresStaleRefresh()
{
	SFixture fixture;
	QSignalSpy finished_spy(&fixture.Session, &AsAuth_Session::requestFinished);
	STest_Http_Response slow_refresh;
	SStored_Tokens second_user;
	SHttp_Response response;

	QVERIFY(Start_Fixture(fixture));
	fixture.Server.Enqueue_Response(JSON_Response(401, "{\"key\":\"token_expired\",\"message\":\"expired\"}"));
	slow_refresh = JSON_Response(200, "{\"tokens\":{\"access_token\":\"stale-access\","
	                                  "\"refresh_token\":\"stale-refresh\"}}");
	slow_refresh.Delay_Ms = 300;
	fixture.Server.Enqueue_Response(slow_refresh);
	fixture.Server.Enqueue_Response(JSON_Response(200, "{\"id\":\"user-2\"}"));

	QVERIFY(fixture.Session.Send_Authed(Authed_Get("users/user-1")) > 0);
	QTRY_COMPARE_WITH_TIMEOUT(fixture.Server.Request_Count(), 2, 5000);

	// Запрос прежней сессии завершается сразу, а не после чужого refresh.
	fixture.Session.Drop_Session();
	QCOMPARE(finished_spy.count(), 1);
	QVERIFY(Wait_For_Response(finished_spy, response));
	QCOMPARE(response.HTTP_Status_Code, 401);

	second_user.User_ID = "user-2";
	second_user.Access_Token = "access-2";
	second_user.Refresh_Token = "refresh-2";
	QVERIFY(fixture.Session.Adopt_Session(second_user));
	QTest::qWait(600);

	QCOMPARE(fixture.Server.Request_Count(), 2);
	QCOMPARE(fixture.Session.User_ID(), QString("user-2"));

	QVERIFY(fixture.Session.Send_Authed(Authed_Get("users/user-2")) > 0);
	QVERIFY(Wait_For_Response(finished_spy, response));
	QCOMPARE(fixture.Server.Request_At(2).Headers.value("authorization"), QByteArray("Bearer access-2"));
}
//-------------------------------------------------------------------------------------------------------------
// 401 по старому токену, пришедший после завершённого refresh, повторяется с
// новым access без второго обмена одноразового refresh-токена.
//-------------------------------------------------------------------------------------------------------------
void AAuth_Session_Test::lateAuthFailureAfterRefreshSkipsSecondRefresh()
{
	SFixture fixture;
	QSignalSpy finished_spy(&fixture.Session, &AsAuth_Session::requestFinished);
	STest_Http_Response late_failure;
	SHttp_Response first_response;
	SHttp_Response second_response;

	QVERIFY(Start_Fixture(fixture));
	fixture.Server.Enqueue_Response(JSON_Response(401, "{\"key\":\"token_expired\",\"message\":\"expired\"}"));
	late_failure = JSON_Response(401, "{\"key\":\"token_expired\",\"message\":\"expired\"}");
	late_failure.Delay_Ms = 500;
	fixture.Server.Enqueue_Response(late_failure);
	fixture.Server.Enqueue_Response(
	    JSON_Response(200, "{\"tokens\":{\"access_token\":\"new-access\",\"refresh_token\":\"new-refresh\"}}"));
	fixture.Server.Enqueue_Response(JSON_Response(200, "{\"id\":\"user-1\"}"));
	fixture.Server.Enqueue_Response(JSON_Response(200, "{\"id\":\"user-1\"}"));

	QVERIFY(fixture.Session.Send_Authed(Authed_Get("users/first")) > 0);
	QVERIFY(fixture.Session.Send_Authed(Authed_Get("users/second")) > 0);
	QVERIFY(Wait_For_Response(finished_spy, first_response));
	QVERIFY(Wait_For_Response(finished_spy, second_response));

	QVERIFY(first_response.Success);
	QVERIFY(second_response.Success);
	QCOMPARE(Count_Refreshes(fixture.Server), 1);
	QCOMPARE(fixture.Server.Request_Count(), 5);
	QCOMPARE(fixture.Server.Request_At(4).Headers.value("authorization"), QByteArray("Bearer new-access"));
	QVERIFY(fixture.Session.Has_Session());
}
//-------------------------------------------------------------------------------------------------------------

QTEST_MAIN(AAuth_Session_Test)

#include "tst_Auth_Session.moc"
