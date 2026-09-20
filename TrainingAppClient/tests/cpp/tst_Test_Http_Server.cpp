#include "Support/Test_Http_Server.h"

#include <QElapsedTimer>
#include <QNetworkAccessManager>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QSignalSpy>
#include <QTcpSocket>
#include <QtTest>

class ATest_Http_Server_Test : public QObject
{
	Q_OBJECT

private slots:
	void getReturnsQueuedResponse();
	void postCapturesBodyAndHeaders();
	void responsesAreConsumedInOrder();
	void delayedResponseIsDelayed();
	void disconnectProducesNetworkError();
	void malformedRequestReturnsBadRequest();
	void splitRequestBodyIsBuffered();
	void missingResponseReturnsServerError();

private:
	static bool Wait_For_Reply(QNetworkReply *in_reply, int in_timeout_ms = 2000);
};

//----------------------------------------------------------------------------
void ATest_Http_Server_Test::getReturnsQueuedResponse()
{
	ATest_Http_Server server;
	STest_Http_Response response;
	QNetworkAccessManager manager;
	QNetworkReply *reply;
	STest_Http_Request request;
	QUrl url;

	QVERIFY(server.Start());
	response.Body = "{\"ok\":true}";
	response.Headers.insert("X-Test-Scenario", "get-success");
	server.Enqueue_Response(response);
	url = server.Base_URL().resolved(QUrl("/api/v1/ping?probe=1"));
	reply = manager.get(QNetworkRequest(url));

	QVERIFY(Wait_For_Reply(reply));
	QCOMPARE(reply->error(), QNetworkReply::NoError);
	QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
	QCOMPARE(reply->rawHeader("X-Test-Scenario"), QByteArray("get-success"));
	QCOMPARE(reply->readAll(), QByteArray("{\"ok\":true}"));
	QCOMPARE(server.Request_Count(), 1);
	request = server.Request_At(0);
	QCOMPARE(request.Method, QByteArray("GET"));
	QCOMPARE(request.Target, QByteArray("/api/v1/ping?probe=1"));
	reply->deleteLater();
}
//----------------------------------------------------------------------------
void ATest_Http_Server_Test::postCapturesBodyAndHeaders()
{
	ATest_Http_Server server;
	STest_Http_Response response;
	QNetworkAccessManager manager;
	QNetworkRequest network_request;
	QNetworkReply *reply;
	STest_Http_Request request;
	QByteArray body;

	QVERIFY(server.Start());
	response.Status_Code = 201;
	response.Body = "{\"created\":true}";
	server.Enqueue_Response(response);
	body = "{\"email\":\"user@example.com\"}";
	network_request.setUrl(server.Base_URL().resolved(QUrl("/api/v1/auth/signup")));
	network_request.setHeader(QNetworkRequest::ContentTypeHeader, "application/json");
	network_request.setRawHeader("Authorization", "Bearer access-token-for-test");
	reply = manager.post(network_request, body);

	QVERIFY(Wait_For_Reply(reply));
	QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 201);
	QCOMPARE(server.Request_Count(), 1);
	request = server.Request_At(0);
	QCOMPARE(request.Method, QByteArray("POST"));
	QCOMPARE(request.Target, QByteArray("/api/v1/auth/signup"));
	QCOMPARE(request.Headers.value("content-type"), QByteArray("application/json"));
	QCOMPARE(request.Headers.value("authorization"), QByteArray("Bearer access-token-for-test"));
	QCOMPARE(request.Body, body);
	reply->deleteLater();
}
//----------------------------------------------------------------------------
void ATest_Http_Server_Test::responsesAreConsumedInOrder()
{
	ATest_Http_Server server;
	STest_Http_Response first_response;
	STest_Http_Response second_response;
	QNetworkAccessManager manager;
	QNetworkReply *first_reply;
	QNetworkReply *second_reply;

	QVERIFY(server.Start());
	first_response.Status_Code = 500;
	first_response.Body = "first";
	second_response.Status_Code = 200;
	second_response.Body = "second";
	server.Enqueue_Response(first_response);
	server.Enqueue_Response(second_response);

	first_reply = manager.get(QNetworkRequest(server.Base_URL().resolved(QUrl("/first"))));
	QVERIFY(Wait_For_Reply(first_reply));
	QCOMPARE(first_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 500);
	QCOMPARE(first_reply->readAll(), QByteArray("first"));

	second_reply = manager.get(QNetworkRequest(server.Base_URL().resolved(QUrl("/second"))));
	QVERIFY(Wait_For_Reply(second_reply));
	QCOMPARE(second_reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 200);
	QCOMPARE(second_reply->readAll(), QByteArray("second"));
	QCOMPARE(server.Pending_Response_Count(), 0);
	QCOMPARE(server.Request_Count(), 2);
	first_reply->deleteLater();
	second_reply->deleteLater();
}
//----------------------------------------------------------------------------
void ATest_Http_Server_Test::delayedResponseIsDelayed()
{
	ATest_Http_Server server;
	STest_Http_Response response;
	QNetworkAccessManager manager;
	QNetworkReply *reply;
	QElapsedTimer elapsed;

	QVERIFY(server.Start());
	response.Body = "delayed";
	response.Delay_Ms = 120;
	server.Enqueue_Response(response);
	reply = manager.get(QNetworkRequest(server.Base_URL().resolved(QUrl("/delayed"))));
	elapsed.start();

	QTest::qWait(40);
	QVERIFY(reply->isFinished() == false);
	QVERIFY(Wait_For_Reply(reply));
	QVERIFY(elapsed.elapsed() >= 100);
	QCOMPARE(reply->readAll(), QByteArray("delayed"));
	reply->deleteLater();
}
//----------------------------------------------------------------------------
void ATest_Http_Server_Test::disconnectProducesNetworkError()
{
	ATest_Http_Server server;
	STest_Http_Response response;
	QNetworkAccessManager manager;
	QNetworkReply *reply;

	QVERIFY(server.Start());
	response.Disconnect_Without_Response = true;
	server.Enqueue_Response(response);
	reply = manager.post(QNetworkRequest(server.Base_URL().resolved(QUrl("/disconnect"))), QByteArray("request"));

	QVERIFY(Wait_For_Reply(reply));
	QVERIFY(reply->error() != QNetworkReply::NoError);
	QCOMPARE(server.Request_Count(), 1);
	reply->deleteLater();
}
//----------------------------------------------------------------------------
void ATest_Http_Server_Test::malformedRequestReturnsBadRequest()
{
	ATest_Http_Server server;
	QTcpSocket socket;
	QSignalSpy ready_read_spy(&socket, &QTcpSocket::readyRead);
	QByteArray response;

	QVERIFY(server.Start());
	socket.connectToHost(server.Base_URL().host(), server.Base_URL().port());
	QVERIFY(socket.waitForConnected(2000));
	socket.write("BROKEN REQUEST\r\nHeader-Without-Colon\r\n\r\n");
	socket.flush();

	QVERIFY(ready_read_spy.wait(2000));
	response = socket.readAll();
	QVERIFY(response.startsWith("HTTP/1.1 400 Bad Request\r\n"));
	QCOMPARE(server.Request_Count(), 0);
}
//----------------------------------------------------------------------------
void ATest_Http_Server_Test::splitRequestBodyIsBuffered()
{
	ATest_Http_Server server;
	STest_Http_Response server_response;
	QTcpSocket socket;
	QSignalSpy ready_read_spy(&socket, &QTcpSocket::readyRead);
	STest_Http_Request request;
	QByteArray response;

	QVERIFY(server.Start());
	server_response.Body = "accepted";
	server.Enqueue_Response(server_response);
	socket.connectToHost(server.Base_URL().host(), server.Base_URL().port());
	QVERIFY(socket.waitForConnected(2000));
	socket.write("POST /split HTTP/1.1\r\nHost: 127.0.0.1\r\nContent-Length: 11\r\n\r\nhello");
	socket.flush();

	QTest::qWait(40);
	QCOMPARE(ready_read_spy.count(), 0);
	QCOMPARE(server.Request_Count(), 0);

	socket.write(" world");
	socket.flush();
	QVERIFY(ready_read_spy.wait(2000));
	response = socket.readAll();
	QVERIFY(response.startsWith("HTTP/1.1 200 OK\r\n"));
	QCOMPARE(server.Request_Count(), 1);
	request = server.Request_At(0);
	QCOMPARE(request.Body, QByteArray("hello world"));
}
//----------------------------------------------------------------------------
void ATest_Http_Server_Test::missingResponseReturnsServerError()
{
	ATest_Http_Server server;
	QNetworkAccessManager manager;
	QNetworkReply *reply;

	QVERIFY(server.Start());
	reply = manager.get(QNetworkRequest(server.Base_URL().resolved(QUrl("/without-scenario"))));

	QVERIFY(Wait_For_Reply(reply));
	QCOMPARE(reply->attribute(QNetworkRequest::HttpStatusCodeAttribute).toInt(), 500);
	QVERIFY(reply->readAll().contains("No fake response queued"));
	reply->deleteLater();
}
//----------------------------------------------------------------------------
bool ATest_Http_Server_Test::Wait_For_Reply(QNetworkReply *in_reply, int in_timeout_ms)
{
	QSignalSpy finished_spy(in_reply, &QNetworkReply::finished);

	if (in_reply->isFinished())
		return true;

	return finished_spy.wait(in_timeout_ms);
}
//----------------------------------------------------------------------------

QTEST_MAIN(ATest_Http_Server_Test)

#include "tst_Test_Http_Server.moc"