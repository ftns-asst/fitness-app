#include "Presentation/Auth_VM.h"

#include "Data/Auth/Auth_Repository.h"
#include "Data/Auth/Auth_Session.h"
#include "Data/Auth/Token_Store.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Network/Http_Client.h"
#include "Support/Test_Http_Server.h"

#include <QCoreApplication>
#include <QDateTime>
#include <QElapsedTimer>
#include <QTemporaryDir>
#include <QtTest>

class Avm_Auth_Test : public QObject
{
	Q_OBJECT

private slots:
	void initialStateIsIdle();
	void checkEmailFreeReturnsToIdle();
	void checkEmailTakenSetsEmailTakenState();
	void checkEmailNetworkErrorSetsErrorState();
	void signupSuccessSetsAuthenticated();
	void signupBackendErrorSetsErrorKey();
	void loginSuccessSetsAuthenticated();
	void loginWrongPasswordSetsErrorKey();
	void logoutReturnsToIdle();
	void sessionExpiredSetsExpiredState();
	void coldStartRestoresAuthenticatedState();

private:
	static STest_Http_Response JSON_Response(int in_status, const QByteArray &in_body);
	static QByteArray Session_Body();
	static bool Wait_Until_State(Avm_Auth *in_vm, const QString &in_state, int in_timeout_ms = 5000);
};

//----------------------------------------------------------------------------
void Avm_Auth_Test::initialStateIsIdle()
{
	Avm_Auth vm(0);

	QCOMPARE(vm.state(), QString("idle"));
	QVERIFY(vm.authenticated() == false);
	QCOMPARE(vm.errorKey(), QString());
}
//----------------------------------------------------------------------------
void Avm_Auth_Test::checkEmailFreeReturnsToIdle()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository repository(&client, &database, &store, &session);
	Avm_Auth vm(&repository);

	QVERIFY(directory.isValid());
	QVERIFY(database.Open(directory.filePath("cache.sqlite3")));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(200, "{\"free\":true}"));
	vm.Check_Email("user@example.com");
	QCOMPARE(vm.state(), QString("checkingEmail"));
	QVERIFY(Wait_Until_State(&vm, "idle"));
	QCOMPARE(vm.errorKey(), QString());
}
//----------------------------------------------------------------------------
void Avm_Auth_Test::checkEmailTakenSetsEmailTakenState()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository repository(&client, &database, &store, &session);
	Avm_Auth vm(&repository);

	QVERIFY(directory.isValid());
	QVERIFY(database.Open(directory.filePath("cache.sqlite3")));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(200, "{\"free\":false}"));
	vm.Check_Email("user@example.com");
	QCOMPARE(vm.state(), QString("checkingEmail"));
	QVERIFY(Wait_Until_State(&vm, "emailTaken"));
	QVERIFY(vm.authenticated() == false);
}
//----------------------------------------------------------------------------
void Avm_Auth_Test::checkEmailNetworkErrorSetsErrorState()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository repository(&client, &database, &store, &session);
	Avm_Auth vm(&repository);
	STest_Http_Response disconnect_response;

	QVERIFY(directory.isValid());
	QVERIFY(database.Open(directory.filePath("cache.sqlite3")));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	disconnect_response.Disconnect_Without_Response = true;
	server.Enqueue_Response(disconnect_response);
	vm.Check_Email("user@example.com");
	QCOMPARE(vm.state(), QString("checkingEmail"));
	QVERIFY(Wait_Until_State(&vm, "error"));
	QCOMPARE(vm.errorKey(), QString("network_error"));
}
//----------------------------------------------------------------------------
void Avm_Auth_Test::signupSuccessSetsAuthenticated()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository repository(&client, &database, &store, &session);
	Avm_Auth vm(&repository);

	QVERIFY(directory.isValid());
	QVERIFY(database.Open(directory.filePath("cache.sqlite3")));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(200, Session_Body()));
	vm.Signup("user@example.com", "somename", "pass-1");
	QCOMPARE(vm.state(), QString("signingUp"));
	QVERIFY(Wait_Until_State(&vm, "authenticated"));
	QVERIFY(vm.authenticated());
}
//----------------------------------------------------------------------------
void Avm_Auth_Test::signupBackendErrorSetsErrorKey()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository repository(&client, &database, &store, &session);
	Avm_Auth vm(&repository);

	QVERIFY(directory.isValid());
	QVERIFY(database.Open(directory.filePath("cache.sqlite3")));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(400, "{\"key\":\"email_taken\",\"message\":\"occupied\"}"));
	vm.Signup("user@example.com", "somename", "pass-1");
	QCOMPARE(vm.state(), QString("signingUp"));
	QVERIFY(Wait_Until_State(&vm, "error"));
	QCOMPARE(vm.errorKey(), QString("email_taken"));
	QVERIFY(vm.authenticated() == false);
}
//----------------------------------------------------------------------------
void Avm_Auth_Test::loginSuccessSetsAuthenticated()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository repository(&client, &database, &store, &session);
	Avm_Auth vm(&repository);

	QVERIFY(directory.isValid());
	QVERIFY(database.Open(directory.filePath("cache.sqlite3")));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(200, Session_Body()));
	vm.Login("user@example.com", "pass-1");
	QCOMPARE(vm.state(), QString("loggingIn"));
	QVERIFY(Wait_Until_State(&vm, "authenticated"));
	QVERIFY(vm.authenticated());
}
//----------------------------------------------------------------------------
void Avm_Auth_Test::loginWrongPasswordSetsErrorKey()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository repository(&client, &database, &store, &session);
	Avm_Auth vm(&repository);

	QVERIFY(directory.isValid());
	QVERIFY(database.Open(directory.filePath("cache.sqlite3")));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"incorrect_password\",\"message\":\"wrong\"}"));
	vm.Login("user@example.com", "wrong-pass");
	QCOMPARE(vm.state(), QString("loggingIn"));
	QVERIFY(Wait_Until_State(&vm, "error"));
	QCOMPARE(vm.errorKey(), QString("incorrect_password"));
	QVERIFY(vm.authenticated() == false);
}
//----------------------------------------------------------------------------
void Avm_Auth_Test::logoutReturnsToIdle()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository repository(&client, &database, &store, &session);
	Avm_Auth vm(&repository);

	QVERIFY(directory.isValid());
	QVERIFY(database.Open(directory.filePath("cache.sqlite3")));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(200, Session_Body()));
	vm.Login("user@example.com", "pass-1");
	QVERIFY(Wait_Until_State(&vm, "authenticated"));

	// Локальный выход возвращает ViewModel в гостевое состояние.
	vm.Logout();
	QCOMPARE(vm.state(), QString("idle"));
	QVERIFY(vm.authenticated() == false);
	QVERIFY(repository.Has_Local_Session() == false);
}
//----------------------------------------------------------------------------
void Avm_Auth_Test::sessionExpiredSetsExpiredState()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	AsHttp_Client client((QUrl()));
	AsAuth_Session session(&client, &store);
	AAuth_Repository repository(&client, &database, &store, &session);
	Avm_Auth vm(&repository);
	SHttp_Request request;

	QVERIFY(directory.isValid());
	QVERIFY(database.Open(directory.filePath("cache.sqlite3")));
	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	QVERIFY(store.Save_Tokens("user-1", "expired-access", "expired-refresh", QDateTime(), QDateTime()));
	QVERIFY(vm.Restore_Local_Session());
	QCOMPARE(vm.state(), QString("authenticated"));

	// Неудача single-flight refresh переводит ViewModel в sessionExpired.
	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"token_expired\",\"message\":\"expired\"}"));
	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"invalid_token\",\"message\":\"refresh rejected\"}"));
	request.Method = "GET";
	request.Path = "users/user-1";
	request.Retry_Enabled = true;
	QVERIFY(session.Send_Authed(request) > 0);
	QVERIFY(Wait_Until_State(&vm, "sessionExpired"));
	QVERIFY(vm.authenticated() == false);
}
//----------------------------------------------------------------------------
void Avm_Auth_Test::coldStartRestoresAuthenticatedState()
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
		Avm_Auth vm(&repository);

		QVERIFY(database.Open(path));
		server.Enqueue_Response(JSON_Response(200, Session_Body()));
		vm.Login("user@example.com", "pass-1");
		QVERIFY(Wait_Until_State(&vm, "authenticated"));
		database.Close();
	}

	// Cold start: вход по сохранённым токенам без сети и повторного логина.
	QVERIFY(database.Open(path));
	AsToken_Store second_store(&database);
	AsAuth_Session second_session(&client, &second_store);
	AAuth_Repository second_repository(&client, &database, &second_store, &second_session);
	Avm_Auth second_vm(&second_repository);

	QCOMPARE(second_vm.state(), QString("idle"));
	QVERIFY(second_vm.Restore_Local_Session());
	QCOMPARE(second_vm.state(), QString("authenticated"));
	QVERIFY(second_vm.authenticated());
}
//----------------------------------------------------------------------------
STest_Http_Response Avm_Auth_Test::JSON_Response(int in_status, const QByteArray &in_body)
{
	STest_Http_Response response;

	response.Status_Code = in_status;
	response.Body = in_body;
	response.Content_Type = "application/json";

	return response;
}
//----------------------------------------------------------------------------
QByteArray Avm_Auth_Test::Session_Body()
{
	return "{\"user\":{\"id\":\"user-1\",\"name\":\"somename\",\"email\":\"user@example.com\","
	       "\"created_at\":\"2026-09-20T12:00:00Z\"},"
	       "\"tokens\":{\"access_token\":\"access-1\",\"refresh_token\":\"refresh-1\"}}";
}
//----------------------------------------------------------------------------
bool Avm_Auth_Test::Wait_Until_State(Avm_Auth *in_vm, const QString &in_state, int in_timeout_ms)
{
	QElapsedTimer timer;

	timer.start();

	while (in_vm->state() != in_state)
	{
		QCoreApplication::processEvents(QEventLoop::AllEvents, 25);

		if (timer.elapsed() > in_timeout_ms)
			return false;
	}

	return true;
}
//----------------------------------------------------------------------------

QTEST_MAIN(Avm_Auth_Test)

#include "tst_Avm_Auth.moc"
