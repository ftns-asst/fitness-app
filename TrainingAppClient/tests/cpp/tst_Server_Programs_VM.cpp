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
	void startsReadyDraftAndShowsItInProgramList();
	void preparesWorkoutWithLatestPersonalRecords();
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
void AServer_Programs_VM_Test::startsReadyDraftAndShowsItInProgramList()
{
	ATest_Http_Server server;
	QVERIFY(server.Start());
	AsHttp_Client http(server.Base_URL().resolved(QUrl("/api/v1")));
	AsToken_Store tokens(0);
	AsAuth_Session session(&http, &tokens);
	Avm_Server_Programs programs(&session);
	QVERIFY(session.Adopt_Session({User_Id, "access", "refresh", {}, {}}));

	QJsonObject program{
	    {"id", Program_Id}, {"state", "draft"}, {"source_kind", "default"}, {"required_measurements", QJsonArray{}}};
	STest_Http_Response reply;
	reply.Body = QJsonDocument(program).toJson();
	server.Enqueue_Response(reply);
	programs.loadDraft(Program_Id);
	QTRY_COMPARE(programs.draft().value("state").toString(), QString("draft"));

	program.insert("state", "in_progress");
	reply.Body = QJsonDocument(program).toJson();
	server.Enqueue_Response(reply);
	programs.startDraft(Program_Id);
	QTRY_COMPARE(programs.draft().value("state").toString(), QString("in_progress"));
	QCOMPARE(server.Request_At(1).Method, QByteArray("POST"));
	QCOMPARE(server.Request_At(1).Target, QByteArray("/api/v1/programs/") + Program_Id.toUtf8() + "/start");

	reply.Body = QJsonDocument(QJsonArray{program}).toJson();
	server.Enqueue_Response(reply);
	programs.loadDrafts();
	QTRY_COMPARE(programs.drafts().size(), 1);
	QCOMPARE(programs.drafts().first().toMap().value("state").toString(), QString("in_progress"));
}
//-------------------------------------------------------------------------------------------------------------
void AServer_Programs_VM_Test::preparesWorkoutWithLatestPersonalRecords()
{
	const QString other_exercise = "1a65804f-17c4-4dec-886f-711c79cdae2f";
	const QString missing_exercise = "b09b173d-bd35-46e8-8a46-2d6df0b94f1a";
	ATest_Http_Server server;
	QVERIFY(server.Start());
	AsHttp_Client http(server.Base_URL().resolved(QUrl("/api/v1")));
	AsToken_Store tokens(0);
	AsAuth_Session session(&http, &tokens);
	Avm_Server_Programs programs(&session);
	QVERIFY(session.Adopt_Session({User_Id, "access", "refresh", {}, {}}));

	const QJsonObject fixed{{"exercise_id", other_exercise}, {"sets", 3}, {"target_reps", 8}, {"target_weight_kg", 30}};
	const QJsonObject percent{{"exercise_id", Exercise_Id},
	                          {"sets", 3},
	                          {"target_reps", 6},
	                          {"target_percent_1rm", 80},
	                          {"rest_seconds", 120}};
	QJsonObject missing = percent;
	missing.insert("exercise_id", missing_exercise);
	const QJsonArray days{QJsonObject{{"day_of_week", 1}, {"exercises", QJsonArray{fixed}}},
	                      QJsonObject{{"day_of_week", 4}, {"exercises", QJsonArray{percent, fixed, missing}}}};
	STest_Http_Response reply;
	reply.Body = QJsonDocument(QJsonObject{{"id", Program_Id},
	                                       {"name", "Upper"},
	                                       {"state", "in_progress"},
	                                       {"required_measurements", QJsonArray{}},
	                                       {"weeks", QJsonArray{QJsonObject{{"week_number", 2}, {"days", days}}}}})
	                 .toJson();
	server.Enqueue_Response(reply);
	programs.loadDraft(Program_Id);
	QTRY_COMPARE(programs.draft().value("id").toString(), Program_Id);
	QSignalSpy ready(&programs, &Avm_Server_Programs::workoutReady);
	programs.prepareWorkout(2, 1);
	QCOMPARE(ready.size(), 1);
	QCOMPARE(server.Request_Count(), 1);
	QCOMPARE(ready.takeFirst().at(4).toList().first().toMap().value("weightKg").toDouble(), 30.0);

	reply.Body = QJsonDocument(QJsonArray{QJsonObject{{"exercise_id", other_exercise}, {"weight_kg", 200}},
	                                      QJsonObject{{"exercise_id", Exercise_Id}, {"weight_kg", 116.67}}})
	                 .toJson();
	server.Enqueue_Response(reply);
	programs.prepareWorkout(2, 4);
	QTRY_COMPARE(ready.size(), 1);
	const STest_Http_Request request = server.Request_At(1);
	QCOMPARE(request.Target, QByteArray("/api/v1/users/") + User_Id.toUtf8() + "/prs");
	QCOMPARE(request.Headers.value("authorization"), QByteArray("Bearer access"));
	const QList<QVariant> args = ready.takeFirst();
	QCOMPARE(args.at(0).toString(), Program_Id);
	QCOMPARE(args.at(1).toInt(), 2);
	QCOMPARE(args.at(2).toInt(), 4);
	const QVariantList rows = args.at(4).toList();
	QCOMPARE(rows.size(), 3);
	QCOMPARE(rows.at(0).toMap().value("weightKg").toDouble(), 93.34);
	QVERIFY(rows.at(0).toMap().value("hasWeight").toBool());
	QCOMPARE(rows.at(1).toMap().value("weightKg").toDouble(), 30.0);
	QVERIFY(!rows.at(2).toMap().value("hasWeight").toBool());
	QCOMPARE(rows.at(2).toMap().value("targetPercent").toDouble(), 80.0);

	reply.Status_Code = 403;
	reply.Body = R"({"key":"forbidden"})";
	server.Enqueue_Response(reply);
	programs.prepareWorkout(2, 4);
	QTRY_COMPARE(ready.size(), 1);
	const QVariantList offline_plan = ready.takeFirst().at(4).toList();
	QVERIFY(!offline_plan.at(0).toMap().value("hasWeight").toBool());
	QCOMPARE(offline_plan.at(1).toMap().value("weightKg").toDouble(), 30.0);
}
//-------------------------------------------------------------------------------------------------------------
QTEST_GUILESS_MAIN(AServer_Programs_VM_Test)
#include "tst_Server_Programs_VM.moc"
