#include "Data/Auth/Auth_Session.h"
#include "Presentation/Server_Programs_VM.h"
#include "Support/Test_Http_Server.h"

#include <QtTest>

namespace
{
const QString User_Id = "7ba915c8-c619-4e8b-93d0-486ff4a5eef6";
const QString Program_Id = "0509f3a5-5e1e-42eb-8311-8e00ccf4ba54";
const QString Template_Id = "c42c228a-e139-482a-8394-f0f465dc6a57";
const QString Exercise_Id = "3584410e-8021-488b-ba46-c5fbf673681a";
} // namespace

class AServer_Programs_VM_Test : public QObject
{
	Q_OBJECT

private slots:
	void createsDraftAndRefreshesAfterSavingRecords();
	void createIsNotRetriedAfterServerError();
};

//-------------------------------------------------------------------------------------------------------------
void AServer_Programs_VM_Test::createsDraftAndRefreshesAfterSavingRecords()
{
	ATest_Http_Server server;
	QVERIFY(server.Start());
	AsHttp_Client http(server.Base_URL().resolved(QUrl("/api/v1")));
	AsToken_Store tokens(0);
	AsAuth_Session session(&http, &tokens);
	Avm_Server_Programs programs(&session);
	QVERIFY(session.Adopt_Session({User_Id, "access", "refresh", {}, {}}));

	STest_Http_Response reply;
	reply.Status_Code = 201;
	reply.Body =
	    QJsonDocument(QJsonObject{{"id", Program_Id},
	                              {"state", "needs_measurement"},
	                              {"required_measurements",
	                               QJsonArray{QJsonObject{{"exercise_id", Exercise_Id}, {"exercise_name", "Press"}}}}})
	        .toJson();
	server.Enqueue_Response(reply);
	QSignalSpy created(&programs, &Avm_Server_Programs::draftCreated);

	programs.createDraft(Template_Id, {1, 4});
	programs.createDraft(Template_Id, {2, 4});
	QTRY_COMPARE(created.size(), 1);
	QCOMPARE(server.Request_Count(), 1);
	const STest_Http_Request create = server.Request_At(0);
	QCOMPARE(create.Target, QByteArray("/api/v1/programs/from-default"));
	QCOMPARE(create.Headers.value("authorization"), QByteArray("Bearer access"));
	const QJsonObject expected_create{{"default_program_id", Template_Id}, {"days_of_week", QJsonArray{1, 4}}};
	QCOMPARE(QJsonDocument::fromJson(create.Body).object(), expected_create);
	QCOMPARE(programs.draft().value("state").toString(), QString("needs_measurement"));

	reply.Body = "[]";
	server.Enqueue_Response(reply);
	reply.Status_Code = 200;
	reply.Body =
	    QJsonDocument(QJsonObject{{"id", Program_Id}, {"state", "draft"}, {"required_measurements", QJsonArray{}}})
	        .toJson();
	server.Enqueue_Response(reply);
	programs.saveRecords(Program_Id, {QVariantMap{{"exercise_id", Exercise_Id}, {"weight_kg", 85.5}, {"reps", 5}}});
	QTRY_COMPARE(programs.draft().value("state").toString(), QString("draft"));
	const STest_Http_Request records = server.Request_At(1);
	QCOMPARE(records.Target, QByteArray("/api/v1/users/") + User_Id.toUtf8() + "/prs/bulk");
	const QJsonArray values = QJsonDocument::fromJson(records.Body).object().value("records").toArray();
	QCOMPARE(values.size(), 1);
	QCOMPARE(values.first().toObject().value("weight_kg").toDouble(), 85.5);
	QCOMPARE(values.first().toObject().value("reps").toInt(), 5);
	QCOMPARE(server.Request_At(2).Target, QByteArray("/api/v1/programs/") + Program_Id.toUtf8());

	session.Drop_Session();
	QVERIFY(programs.draft().isEmpty());
	QVERIFY(!programs.authenticated());
}
//-------------------------------------------------------------------------------------------------------------
void AServer_Programs_VM_Test::createIsNotRetriedAfterServerError()
{
	ATest_Http_Server server;
	QVERIFY(server.Start());
	AsHttp_Client http(server.Base_URL().resolved(QUrl("/api/v1")));
	AsToken_Store tokens(0);
	AsAuth_Session session(&http, &tokens);
	Avm_Server_Programs programs(&session);
	QVERIFY(session.Adopt_Session({User_Id, "access", "refresh", {}, {}}));

	STest_Http_Response reply;
	reply.Status_Code = 500;
	reply.Body = R"({"key":"undefined_error","message":"internal server error"})";
	server.Enqueue_Response(reply);
	programs.createDraft(Template_Id, {1, 2});
	QTRY_VERIFY(!programs.busy());
	QCOMPARE(programs.errorKey(), QString("program_create_unconfirmed"));
	QCOMPARE(server.Request_Count(), 1);
}
//-------------------------------------------------------------------------------------------------------------
QTEST_GUILESS_MAIN(AServer_Programs_VM_Test)
#include "tst_Server_Programs_VM.moc"
