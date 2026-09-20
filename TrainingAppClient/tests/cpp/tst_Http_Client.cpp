#include "Data/Network/Http_Client.h"
#include "Support/Test_Http_Server.h"

#include <QElapsedTimer>
#include <QJsonObject>
#include <QNetworkReply>
#include <QSignalSpy>
#include <QtTest>

static QStringList Captured_Messages;

//----------------------------------------------------------------------------
static void Capture_Message(QtMsgType in_type, const QMessageLogContext &in_context, const QString &in_message)
{
	Q_UNUSED(in_type);
	Q_UNUSED(in_context);
	Captured_Messages.append(in_message);
}

class AHttp_Client_Test : public QObject
{
	Q_OBJECT

private slots:
	void successReturnsUnifiedResponse();
	void malformedSuccessBodyIsReportedWithoutTransportFailure();
	void postJsonSendsBodyAndDoesNotRetryByDefault();
	void backendErrorKeyHasPriority();
	void fallbackErrorMapping_data();
	void fallbackErrorMapping();
	void retriesServerErrorsWithBackoff();
	void retriesNetworkDisconnect();
	void retryExhaustionReturnsLastError();
	void timeoutProducesTimeoutError();
	void invalidRequestIsRejectedSynchronously();
	void logsDoNotContainSecretsOrQuery();

private:
	static bool Wait_For_Response(QSignalSpy &in_spy, SHttp_Response &out_response, int in_timeout_ms = 3000);
	static STest_Http_Response JSON_Response(int in_status, const QByteArray &in_body);
};

//----------------------------------------------------------------------------
void AHttp_Client_Test::successReturnsUnifiedResponse()
{
	ATest_Http_Server server;
	STest_Http_Response server_response;
	QUrl base_url;
	AsHttp_Client client(base_url);
	QSignalSpy response_spy(&client, &AsHttp_Client::requestFinished);
	SHttp_Response response;
	STest_Http_Request captured_request;
	QMap<QByteArray, QByteArray> headers;
	int request_id;

	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL().resolved(QUrl("/api/v1"))));
	server_response = JSON_Response(200, "{\"free\":true}");
	server_response.Headers.insert("X-Trace-Id", "trace-1");
	server.Enqueue_Response(server_response);
	headers.insert("Authorization", "Bearer unit-test-access-token");
	request_id = client.Get("auth/check-email?source=test", headers);

	QVERIFY(request_id > 0);
	QVERIFY(Wait_For_Response(response_spy, response));
	QCOMPARE(response.Request_Id, request_id);
	QVERIFY(response.Success);
	QCOMPARE(response.HTTP_Status_Code, 200);
	QCOMPARE(response.Network_Error_Code, (int)QNetworkReply::NoError);
	QCOMPARE(response.Attempt_Count, 1);
	QVERIFY(response.JSON_Valid);
	QVERIFY(response.JSON.object().value("free").toBool());
	QCOMPARE(response.Headers.value("x-trace-id"), QByteArray("trace-1"));
	QVERIFY(response.Error.Is_Error() == false);
	QCOMPARE(server.Request_Count(), 1);
	captured_request = server.Request_At(0);
	QCOMPARE(captured_request.Method, QByteArray("GET"));
	QCOMPARE(captured_request.Target, QByteArray("/api/v1/auth/check-email?source=test"));
	QCOMPARE(captured_request.Headers.value("accept"), QByteArray("application/json"));
	QCOMPARE(captured_request.Headers.value("authorization"), QByteArray("Bearer unit-test-access-token"));
}
//----------------------------------------------------------------------------
void AHttp_Client_Test::malformedSuccessBodyIsReportedWithoutTransportFailure()
{
	ATest_Http_Server server;
	QUrl base_url;
	AsHttp_Client client(base_url);
	QSignalSpy response_spy(&client, &AsHttp_Client::requestFinished);
	SHttp_Response response;

	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL()));
	server.Enqueue_Response(JSON_Response(200, "not-json"));
	QVERIFY(client.Get("malformed-success") > 0);

	QVERIFY(Wait_For_Response(response_spy, response));
	QVERIFY(response.Success);
	QVERIFY(response.JSON_Valid == false);
	QCOMPARE(response.Body, QByteArray("not-json"));
	QVERIFY(response.Error.Is_Error() == false);
}
//----------------------------------------------------------------------------
void AHttp_Client_Test::postJsonSendsBodyAndDoesNotRetryByDefault()
{
	ATest_Http_Server server;
	QUrl base_url;
	AsHttp_Client client(base_url);
	QSignalSpy response_spy(&client, &AsHttp_Client::requestFinished);
	SHttp_Response response;
	QJsonObject body;
	STest_Http_Request captured_request;

	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL()));
	client.Set_Initial_Retry_Delay_Ms(1);
	server.Enqueue_Response(JSON_Response(503, "{\"key\":\"server_error\",\"message\":\"busy\"}"));
	server.Enqueue_Response(JSON_Response(200, "{\"ok\":true}"));
	body.insert("email", "user@example.com");
	QVERIFY(client.Post_JSON("auth/login", body) > 0);

	QVERIFY(Wait_For_Response(response_spy, response));
	QVERIFY(response.Success == false);
	QCOMPARE(response.Attempt_Count, 1);
	QCOMPARE(server.Request_Count(), 1);
	QCOMPARE(server.Pending_Response_Count(), 1);
	captured_request = server.Request_At(0);
	QCOMPARE(captured_request.Method, QByteArray("POST"));
	QCOMPARE(captured_request.Headers.value("content-type"), QByteArray("application/json"));
	QCOMPARE(QJsonDocument::fromJson(captured_request.Body).object().value("email").toString(),
	         QString("user@example.com"));
}
//----------------------------------------------------------------------------
void AHttp_Client_Test::backendErrorKeyHasPriority()
{
	ATest_Http_Server server;
	QUrl base_url;
	AsHttp_Client client(base_url);
	QSignalSpy response_spy(&client, &AsHttp_Client::requestFinished);
	SHttp_Response response;

	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL()));
	server.Enqueue_Response(JSON_Response(400, "{\"key\":\"email_taken\",\"message\":\"already used\"}"));
	QVERIFY(client.Get("error") > 0);

	QVERIFY(Wait_For_Response(response_spy, response));
	QVERIFY(response.Success == false);
	QCOMPARE(response.Error.Key(), QString("email_taken"));
	QCOMPARE(response.Error.Message(), QString("already used"));
	QCOMPARE(response.Error.HTTP_Status_Code(), 400);
}
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
void AHttp_Client_Test::fallbackErrorMapping_data()
{
	QTest::addColumn<int>("httpStatus");
	QTest::addColumn<int>("networkError");
	QTest::addColumn<QString>("expectedKey");

	QTest::newRow("validation") << 400 << (int)QNetworkReply::NoError << QString("validation_error");
	QTest::newRow("unauthorized") << 401 << (int)QNetworkReply::AuthenticationRequiredError << QString("invalid_token");
	QTest::newRow("not-found") << 404 << (int)QNetworkReply::ContentNotFoundError << QString("not_found");
	QTest::newRow("request-timeout") << 408 << (int)QNetworkReply::ContentAccessDenied << QString("timeout");
	QTest::newRow("rate-limited") << 429 << (int)QNetworkReply::UnknownContentError << QString("rate_limited");
	QTest::newRow("server") << 503 << (int)QNetworkReply::ServiceUnavailableError << QString("server_error");
	QTest::newRow("unknown") << 418 << (int)QNetworkReply::UnknownContentError << QString("undefined_error");
	QTest::newRow("network") << 0 << (int)QNetworkReply::RemoteHostClosedError << QString("network_error");
	QTest::newRow("timeout-network") << 0 << (int)QNetworkReply::TimeoutError << QString("timeout");
}
//----------------------------------------------------------------------------
void AHttp_Client_Test::fallbackErrorMapping()
{
	QFETCH(int, httpStatus);
	QFETCH(int, networkError);
	QFETCH(QString, expectedKey);
	AApi_Error error;

	error = AApi_Error::From_Response(httpStatus, networkError, "transport detail", "not-json");
	QCOMPARE(error.Key(), expectedKey);
	QCOMPARE(error.HTTP_Status_Code(), httpStatus);
	QCOMPARE(error.Network_Error_Code(), networkError);
}
//----------------------------------------------------------------------------
void AHttp_Client_Test::retriesServerErrorsWithBackoff()
{
	ATest_Http_Server server;
	QUrl base_url;
	AsHttp_Client client(base_url);
	QSignalSpy response_spy(&client, &AsHttp_Client::requestFinished);
	SHttp_Response response;
	QElapsedTimer elapsed;

	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL()));
	client.Set_Max_Retry_Count(2);
	client.Set_Initial_Retry_Delay_Ms(20);
	server.Enqueue_Response(JSON_Response(500, "{\"key\":\"server_error\"}"));
	server.Enqueue_Response(JSON_Response(502, "{\"key\":\"server_error\"}"));
	server.Enqueue_Response(JSON_Response(200, "{\"ok\":true}"));
	elapsed.start();
	QVERIFY(client.Get("retry") > 0);

	QVERIFY(Wait_For_Response(response_spy, response));
	QVERIFY(response.Success);
	QCOMPARE(response.Attempt_Count, 3);
	QCOMPARE(server.Request_Count(), 3);
	QVERIFY(elapsed.elapsed() >= 50);
}
//----------------------------------------------------------------------------
void AHttp_Client_Test::retriesNetworkDisconnect()
{
	ATest_Http_Server server;
	STest_Http_Response disconnect_response;
	QUrl base_url;
	AsHttp_Client client(base_url);
	QSignalSpy response_spy(&client, &AsHttp_Client::requestFinished);
	SHttp_Response response;

	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL()));
	client.Set_Max_Retry_Count(1);
	client.Set_Initial_Retry_Delay_Ms(1);
	disconnect_response.Disconnect_Without_Response = true;
	server.Enqueue_Response(disconnect_response);
	server.Enqueue_Response(JSON_Response(200, "{\"ok\":true}"));
	QVERIFY(client.Post_JSON("retry-network", QJsonObject(), QMap<QByteArray, QByteArray>(), true) > 0);

	QVERIFY(Wait_For_Response(response_spy, response));
	QVERIFY(response.Success);
	QCOMPARE(response.Attempt_Count, 2);
	QCOMPARE(server.Request_Count(), 2);
}
//----------------------------------------------------------------------------
void AHttp_Client_Test::retryExhaustionReturnsLastError()
{
	ATest_Http_Server server;
	QUrl base_url;
	AsHttp_Client client(base_url);
	QSignalSpy response_spy(&client, &AsHttp_Client::requestFinished);
	SHttp_Response response;
	int i;

	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL()));
	client.Set_Max_Retry_Count(2);
	client.Set_Initial_Retry_Delay_Ms(1);

	for (i = 0; i < 3; ++i)
		server.Enqueue_Response(JSON_Response(503, "not-json"));

	QVERIFY(client.Get("retry-exhaustion") > 0);
	QVERIFY(Wait_For_Response(response_spy, response));
	QVERIFY(response.Success == false);
	QCOMPARE(response.Attempt_Count, 3);
	QCOMPARE(response.Error.Key(), QString("server_error"));
	QCOMPARE(server.Request_Count(), 3);
}

//----------------------------------------------------------------------------
void AHttp_Client_Test::timeoutProducesTimeoutError()
{
	ATest_Http_Server server;
	STest_Http_Response delayed_response;
	QUrl base_url;
	AsHttp_Client client(base_url);
	QSignalSpy response_spy(&client, &AsHttp_Client::requestFinished);
	SHttp_Response response;

	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL()));
	client.Set_Timeout_Ms(40);
	client.Set_Max_Retry_Count(0);
	delayed_response = JSON_Response(200, "{\"ok\":true}");
	delayed_response.Delay_Ms = 250;
	server.Enqueue_Response(delayed_response);
	QVERIFY(client.Get("timeout") > 0);

	QVERIFY(Wait_For_Response(response_spy, response));
	QVERIFY(response.Success == false);
	QCOMPARE(response.HTTP_Status_Code, 0);
	QCOMPARE(response.Network_Error_Code, (int)QNetworkReply::TimeoutError);
	QCOMPARE(response.Error.Key(), QString("timeout"));
}
//----------------------------------------------------------------------------
void AHttp_Client_Test::invalidRequestIsRejectedSynchronously()
{
	AsHttp_Client client(QUrl("https://example.com/api/v1"));
	SHttp_Request request;
	QSignalSpy response_spy(&client, &AsHttp_Client::requestFinished);

	request.Method = "GET";
	request.Path = "https://attacker.example/private";
	QCOMPARE(client.Send(request), 0);
	QVERIFY(client.Last_Error().contains("relative URL"));
	QCOMPARE(response_spy.count(), 0);

	request.Path = "users/1#token";
	QCOMPARE(client.Send(request), 0);
	QVERIFY(client.Last_Error().contains("fragment"));

	request.Path = "../outside-api";
	QCOMPARE(client.Send(request), 0);
	QVERIFY(client.Last_Error().contains("base path"));

	request.Method = "GET\r\nX-Evil: yes";
	request.Path = "users/1";
	QCOMPARE(client.Send(request), 0);
	QVERIFY(client.Last_Error().contains("A-Z"));

	QVERIFY(client.Set_Base_URL(QUrl("file:///tmp/api")) == false);
	QVERIFY(client.Base_URL().isEmpty());
}
//----------------------------------------------------------------------------
void AHttp_Client_Test::logsDoNotContainSecretsOrQuery()
{
	ATest_Http_Server server;
	QUrl base_url;
	AsHttp_Client client(base_url);
	QSignalSpy response_spy(&client, &AsHttp_Client::requestFinished);
	SHttp_Response response;
	QMap<QByteArray, QByteArray> headers;
	QtMessageHandler previous_handler;
	QString all_messages;
	int request_id;
	bool response_received;

	QVERIFY(server.Start());
	QVERIFY(client.Set_Base_URL(server.Base_URL()));
	client.Set_Max_Retry_Count(0);
	server.Enqueue_Response(JSON_Response(401, "{\"key\":\"token_expired\",\"message\":\"secret-message\"}"));
	headers.insert("Authorization", "Bearer super-secret-access-token");
	Captured_Messages.clear();
	previous_handler = qInstallMessageHandler(Capture_Message);
	request_id = client.Get("users/id?refresh_token=super-secret-refresh-token", headers);
	response_received = Wait_For_Response(response_spy, response);
	qInstallMessageHandler(previous_handler);
	all_messages = Captured_Messages.join('\n');

	QVERIFY(request_id > 0);
	QVERIFY(response_received);
	QCOMPARE(response.Error.Key(), QString("token_expired"));
	QVERIFY(all_messages.contains("/users/id"));
	QVERIFY(all_messages.contains("super-secret-access-token") == false);
	QVERIFY(all_messages.contains("super-secret-refresh-token") == false);
	QVERIFY(all_messages.contains("secret-message") == false);
	QVERIFY(all_messages.contains("Authorization") == false);
}
//----------------------------------------------------------------------------
bool AHttp_Client_Test::Wait_For_Response(QSignalSpy &in_spy, SHttp_Response &out_response, int in_timeout_ms)
{
	QList<QVariant> arguments;

	if (in_spy.isEmpty() && in_spy.wait(in_timeout_ms) == false)
		return false;

	arguments = in_spy.takeFirst();

	if (arguments.size() != 2)
		return false;

	out_response = qvariant_cast<SHttp_Response>(arguments.at(1));

	return true;
}
//----------------------------------------------------------------------------
STest_Http_Response AHttp_Client_Test::JSON_Response(int in_status, const QByteArray &in_body)
{
	STest_Http_Response response;

	response.Status_Code = in_status;
	response.Body = in_body;
	response.Content_Type = "application/json";

	return response;
}
//----------------------------------------------------------------------------

QTEST_MAIN(AHttp_Client_Test)

#include "tst_Http_Client.moc"
