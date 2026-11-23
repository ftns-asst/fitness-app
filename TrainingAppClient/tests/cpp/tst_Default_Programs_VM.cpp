#include "Data/Network/Http_Client.h"
#include "Presentation/Default_Programs_VM.h"
#include "Support/Test_Http_Server.h"

#include <QtTest>

class ADefault_Programs_VM_Test : public QObject
{
	Q_OBJECT

private slots:
	void publicFiltersAndDetail();
	void distinguishesEmptyCatalogFromMalformedResponse();
	void newerFilterIgnoresLateResponse();
};

//-------------------------------------------------------------------------------------------------------------
void ADefault_Programs_VM_Test::publicFiltersAndDetail()
{
	ATest_Http_Server server;
	QVERIFY(server.Start());
	AsHttp_Client http(server.Base_URL().resolved(QUrl("/api/v1")));
	Avm_Default_Programs catalog(&http);
	const QString id = "068a58aa-9345-4a1c-b951-e19894566198";
	const QByteArray row =
	    R"({"id":"068a58aa-9345-4a1c-b951-e19894566198","name":"Upper","weeks_count":2,"days_per_week":2})";
	STest_Http_Response response;
	response.Body = "[" + row + "]";
	server.Enqueue_Response(response);

	catalog.load("strength", "beginner", {1, 4});
	QTRY_VERIFY(catalog.loaded());
	QCOMPARE(server.Request_At(0).Target,
	         QByteArray("/api/v1/default-programs?goal=strength&level=beginner&day_of_week=1&day_of_week=4"));
	QVERIFY(!server.Request_At(0).Headers.contains("authorization"));
	QCOMPARE(catalog.programs().first().toMap().value("id").toString(), id);

	response.Body = row.chopped(1) +
	                R"(,"workouts":[{"week_number":1,"order_index":0,"template":{"name":"Upper","exercises":[]}}]})";
	server.Enqueue_Response(response);
	catalog.loadDetail(id);
	QTRY_VERIFY(!catalog.detailLoading());
	QCOMPARE(server.Request_At(1).Target, QByteArray("/api/v1/default-programs/") + id.toUtf8());
	QCOMPARE(catalog.detail().value("workouts").toList().size(), 1);
}
//-------------------------------------------------------------------------------------------------------------
void ADefault_Programs_VM_Test::distinguishesEmptyCatalogFromMalformedResponse()
{
	ATest_Http_Server server;
	QVERIFY(server.Start());
	AsHttp_Client http(server.Base_URL().resolved(QUrl("/api/v1")));
	Avm_Default_Programs catalog(&http);
	STest_Http_Response response;
	response.Body = "[]";
	server.Enqueue_Response(response);

	catalog.load("", "", {});
	QTRY_VERIFY(catalog.loaded());
	QVERIFY(catalog.programs().isEmpty());

	response.Body = "[{}]";
	server.Enqueue_Response(response);
	catalog.load("", "", {});
	QTRY_VERIFY(!catalog.loading());
	QVERIFY(!catalog.loaded());
	QCOMPARE(catalog.errorKey(), QString("undefined_error"));
}
//-------------------------------------------------------------------------------------------------------------
void ADefault_Programs_VM_Test::newerFilterIgnoresLateResponse()
{
	ATest_Http_Server server;
	QVERIFY(server.Start());
	AsHttp_Client http(server.Base_URL().resolved(QUrl("/api/v1")));
	Avm_Default_Programs catalog(&http);
	STest_Http_Response response;
	response.Body = "[{}]";
	response.Delay_Ms = 250;
	server.Enqueue_Response(response);
	response.Body = "[]";
	response.Delay_Ms = 0;
	server.Enqueue_Response(response);

	QSignalSpy replies(&http, &AsHttp_Client::requestFinished);
	catalog.load("strength", "", {});
	QTRY_COMPARE(server.Request_Count(), 1);
	catalog.load("fitness", "", {});
	QTRY_COMPARE(replies.size(), 2);
	QVERIFY(catalog.loaded());
	QVERIFY(catalog.errorKey().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
QTEST_GUILESS_MAIN(ADefault_Programs_VM_Test)
#include "tst_Default_Programs_VM.moc"
