#include "Data/Auth/Auth_Api.h"
#include "Data/Network/Http_Client.h"
#include "Support/Test_Http_Server.h"

#include <QSignalSpy>
#include <QtTest>

class AAuth_Api_Test : public QObject
{
	Q_OBJECT

private slots:
	void checkEmailParsesFreeFlag();
	void checkEmailReportsTakenFlag();
	void checkEmailPreservesBackendErrorKey();
	void checkEmailWithoutFreeFlagReportsUndefinedError();
	void loginParsesUserAndTokens();
	void loginWithoutTokensReportsUndefinedError();
	void signupParsesUserAndTokens();
	void backendErrorKeyPassesThrough();

private:
	static STest_Http_Response JSON_Response(int in_status, const QByteArray &in_body);
	static QByteArray Session_Body();
};

//----------------------------------------------------------------------------
void AAuth_Api_Test::checkEmailParsesFreeFlag()
{
	ATest_Http_Server server;
	AsHttp_Client client((QUrl()));
	AAuth_Api api(&client);
	QSignalSpy spy(&api, &AAuth_Api::checkEmailFinished);
	STest_Http_Request captured;
	QList<QVariant> arguments;
	bool free_flag = true;
	AApi_Error error;

	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(200, "{\"free\":true}"));
	int request_id = api.Check_Email("user@example.com");
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	free_flag = arguments.at(1).toBool();
	error = qvariant_cast<AApi_Error>(arguments.at(2));
	QVERIFY(free_flag);
	QVERIFY(error.Is_Error() == false);

	captured = server.Request_At(0);
	QCOMPARE(captured.Method, QByteArray("POST"));
	QCOMPARE(captured.Target, QByteArray("/api/v1/auth/check-email"));
	QCOMPARE(captured.Headers.value("content-type"), QByteArray("application/json"));
	QVERIFY(captured.Body.contains("user@example.com"));
}
//----------------------------------------------------------------------------
void AAuth_Api_Test::checkEmailReportsTakenFlag()
{
	ATest_Http_Server server;
	AsHttp_Client client((QUrl()));
	AAuth_Api api(&client);
	QSignalSpy spy(&api, &AAuth_Api::checkEmailFinished);
	QList<QVariant> arguments;
	bool free_flag = true;
	AApi_Error error;

	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	// free=false — успешный ответ, а не API error.
	server.Enqueue_Response(JSON_Response(200, "{\"free\":false}"));
	int request_id = api.Check_Email("user@example.com");
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	free_flag = arguments.at(1).toBool();
	error = qvariant_cast<AApi_Error>(arguments.at(2));
	QVERIFY(free_flag == false);
	QVERIFY(error.Is_Error() == false);
}
//----------------------------------------------------------------------------
void AAuth_Api_Test::checkEmailPreservesBackendErrorKey()
{
	ATest_Http_Server server;
	AsHttp_Client client((QUrl()));
	AAuth_Api api(&client);
	QSignalSpy spy(&api, &AAuth_Api::checkEmailFinished);
	QList<QVariant> arguments;
	AApi_Error error;

	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(400, "{\"key\":\"invalid_email\",\"message\":\"bad email\"}"));
	int request_id = api.Check_Email("not-an-email");
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	error = qvariant_cast<AApi_Error>(arguments.at(2));
	QCOMPARE(error.Key(), QString("invalid_email"));
	QCOMPARE(error.HTTP_Status_Code(), 400);
}
//----------------------------------------------------------------------------
void AAuth_Api_Test::checkEmailWithoutFreeFlagReportsUndefinedError()
{
	ATest_Http_Server server;
	AsHttp_Client client((QUrl()));
	AAuth_Api api(&client);
	QSignalSpy spy(&api, &AAuth_Api::checkEmailFinished);
	QList<QVariant> arguments;
	bool free_flag = true;
	AApi_Error error;

	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	// Успешный ответ без обязательного поля free — неполный ответ (docs/03, §2).
	server.Enqueue_Response(JSON_Response(200, "{}"));
	int request_id = api.Check_Email("user@example.com");
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	free_flag = arguments.at(1).toBool();
	error = qvariant_cast<AApi_Error>(arguments.at(2));
	QVERIFY(free_flag == false);
	QCOMPARE(error.Key(), QString("undefined_error"));
}
//----------------------------------------------------------------------------
void AAuth_Api_Test::loginParsesUserAndTokens()
{
	ATest_Http_Server server;
	AsHttp_Client client((QUrl()));
	AAuth_Api api(&client);
	QSignalSpy spy(&api, &AAuth_Api::loginFinished);
	QList<QVariant> arguments;
	SAuth_Session_Data data;
	AApi_Error error;

	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(200, Session_Body()));
	int request_id = api.Login("user@example.com", "pass-1");
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	data = qvariant_cast<SAuth_Session_Data>(arguments.at(1));
	error = qvariant_cast<AApi_Error>(arguments.at(2));
	QCOMPARE(data.User.ID, QString("user-1"));
	QCOMPARE(data.User.Name, QString("somename"));
	QCOMPARE(data.User.Email, QString("user@example.com"));
	QCOMPARE(data.User.Created_At, QString("2026-09-20T12:00:00Z"));
	QCOMPARE(data.Access_Token, QString("access-1"));
	QCOMPARE(data.Refresh_Token, QString("refresh-1"));
	QVERIFY(error.Is_Error() == false);

	STest_Http_Request captured = server.Request_At(0);
	QCOMPARE(captured.Target, QByteArray("/api/v1/auth/login"));
	QVERIFY(captured.Body.contains("user@example.com"));
	QVERIFY(captured.Body.contains("pass-1"));
}
//----------------------------------------------------------------------------
void AAuth_Api_Test::loginWithoutTokensReportsUndefinedError()
{
	ATest_Http_Server server;
	AsHttp_Client client((QUrl()));
	AAuth_Api api(&client);
	QSignalSpy spy(&api, &AAuth_Api::loginFinished);
	QList<QVariant> arguments;
	SAuth_Session_Data data;
	AApi_Error error;

	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	// Успешный login без пары токенов — неполный ответ (docs/03, сценарий 2).
	server.Enqueue_Response(JSON_Response(200, "{\"user\":{\"id\":\"user-1\",\"name\":\"somename\","
	                                           "\"email\":\"user@example.com\"}}"));
	int request_id = api.Login("user@example.com", "pass-1");
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	data = qvariant_cast<SAuth_Session_Data>(arguments.at(1));
	error = qvariant_cast<AApi_Error>(arguments.at(2));
	QCOMPARE(error.Key(), QString("undefined_error"));
	QVERIFY(data.Access_Token.isEmpty());
	QVERIFY(data.Refresh_Token.isEmpty());
}
//----------------------------------------------------------------------------
void AAuth_Api_Test::signupParsesUserAndTokens()
{
	ATest_Http_Server server;
	AsHttp_Client client((QUrl()));
	AAuth_Api api(&client);
	QSignalSpy spy(&api, &AAuth_Api::signupFinished);
	QList<QVariant> arguments;
	SAuth_Session_Data data;
	AApi_Error error;

	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(200, Session_Body()));
	int request_id = api.Signup("user@example.com", "somename", "pass-1");
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	data = qvariant_cast<SAuth_Session_Data>(arguments.at(1));
	error = qvariant_cast<AApi_Error>(arguments.at(2));
	QCOMPARE(data.User.ID, QString("user-1"));
	QCOMPARE(data.User.Name, QString("somename"));
	QCOMPARE(data.Access_Token, QString("access-1"));
	QCOMPARE(data.Refresh_Token, QString("refresh-1"));
	QVERIFY(error.Is_Error() == false);

	STest_Http_Request captured = server.Request_At(0);
	QCOMPARE(captured.Target, QByteArray("/api/v1/auth/signup"));
	QVERIFY(captured.Body.contains("somename"));
}
//----------------------------------------------------------------------------
void AAuth_Api_Test::backendErrorKeyPassesThrough()
{
	ATest_Http_Server server;
	AsHttp_Client client((QUrl()));
	AAuth_Api api(&client);
	QSignalSpy spy(&api, &AAuth_Api::loginFinished);
	QList<QVariant> arguments;
	SAuth_Session_Data data;
	AApi_Error error;

	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	client.Set_Max_Retry_Count(0);

	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"incorrect_password\",\"message\":\"wrong\"}"));
	int request_id = api.Login("user@example.com", "wrong-pass");
	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	data = qvariant_cast<SAuth_Session_Data>(arguments.at(1));
	error = qvariant_cast<AApi_Error>(arguments.at(2));
	QCOMPARE(error.Key(), QString("incorrect_password"));
	QCOMPARE(error.HTTP_Status_Code(), 401);
	QVERIFY(data.Access_Token.isEmpty());
	QVERIFY(data.Refresh_Token.isEmpty());
}
//----------------------------------------------------------------------------
STest_Http_Response AAuth_Api_Test::JSON_Response(int in_status, const QByteArray &in_body)
{
	STest_Http_Response response;

	response.Status_Code = in_status;
	response.Body = in_body;
	response.Content_Type = "application/json";

	return response;
}
//----------------------------------------------------------------------------
QByteArray AAuth_Api_Test::Session_Body()
{
	return "{\"user\":{\"id\":\"user-1\",\"name\":\"somename\",\"email\":\"user@example.com\","
	       "\"created_at\":\"2026-09-20T12:00:00Z\"},"
	       "\"tokens\":{\"access_token\":\"access-1\",\"refresh_token\":\"refresh-1\"}}";
}
//----------------------------------------------------------------------------

QTEST_MAIN(AAuth_Api_Test)

#include "tst_Auth_Api.moc"
