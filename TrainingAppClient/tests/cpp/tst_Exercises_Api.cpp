#include "Data/Auth/Auth_Session.h"
#include "Data/Auth/Token_Store.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Exercises/Exercises_Api.h"
#include "Data/Network/Http_Client.h"
#include "Support/Test_Http_Server.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

// Спринт U1 (docs/08-exercises-sprint-plan.md §4): HTTP-слой упражнений и
// справочников. Проверяется сборка query (multi-фильтры, обязательные
// limit/offset), опциональная авторизация (анонимно без сессии, Bearer со
// сессией) и разбор голого массива Exercise (docs/03, §3.9).
class AExercises_Api_Test : public QObject
{
	Q_OBJECT

private slots:
	void listParsesExerciseArray();
	void listEncodesFiltersAndPagination();
	void listRejectsInvalidParameters();
	void listGoesAnonymousWithoutSession();
	void listSendsBearerWithSession();
	void listPreservesBackendErrorKey();
	void listIncompleteSuccessReportsUndefinedError();
	void muscleGroupsParseArray();
	void equipmentsParseArray();

private:
	static STest_Http_Response JSON_Response(int in_status, const QByteArray &in_body);
	static QByteArray Exercise_Array_Body();
	void Make_Api(ATest_Http_Server &in_server, AsAuth_Session *&out_session, QTemporaryDir &out_dir,
	              bool in_with_session, AExercises_Api *&out_api);
};
//----------------------------------------------------------------------------
STest_Http_Response AExercises_Api_Test::JSON_Response(int in_status, const QByteArray &in_body)
{
	STest_Http_Response response;

	response.Status_Code = in_status;
	response.Body = in_body;

	return response;
}
//----------------------------------------------------------------------------
QByteArray AExercises_Api_Test::Exercise_Array_Body()
{
	// Голый массив без обёртки и total (docs/03, §3.9); у второго упражнения
	// нет description и owner_id — omitempty (docs/03, §3.9).
	return QByteArray("["
	                  "{\"id\":\"aaaaaaaa-1111-2222-3333-444444444444\",\"name\":\"Bench press\","
	                  "\"description\":\"Сжимай.\",\"owner_id\":\"user-1\","
	                  "\"muscle_groups\":[\"chest\"],\"equipment\":[\"barbell\"],"
	                  "\"created_at\":\"2026-10-01T10:00:00Z\"},"
	                  "{\"id\":\"bbbbbbbb-1111-2222-3333-444444444444\",\"name\":\"Push up\","
	                  "\"muscle_groups\":[\"chest\",\"triceps\"],\"equipment\":[],"
	                  "\"created_at\":\"2026-10-02T10:00:00Z\"}"
	                  "]");
}
//----------------------------------------------------------------------------
void AExercises_Api_Test::Make_Api(ATest_Http_Server &in_server, AsAuth_Session *&out_session, QTemporaryDir &out_dir,
                                   bool in_with_session, AExercises_Api *&out_api)
{
	out_api = 0;
	AsSql_Database *database = new AsSql_Database(&in_server);
	AsToken_Store *store = new AsToken_Store(database, &in_server);
	AsHttp_Client *client = new AsHttp_Client((QUrl()), &in_server);
	AsAuth_Session *session = new AsAuth_Session(client, store, &in_server);
	AExercises_Api *api = new AExercises_Api(client, session, &in_server);

	QVERIFY(out_dir.isValid());
	QVERIFY(database->Open(out_dir.filePath("cache.sqlite3")));
	QVERIFY(in_server.Start());
	QVERIFY(client->Set_Base_URL(in_server.Base_URL().resolved(QUrl("/api/v1"))));
	client->Set_Max_Retry_Count(0);

	if (in_with_session)
	{
		QVERIFY(store->Save_Tokens("user-1", "stored-access", "stored-refresh", QDateTime(), QDateTime()));
		QVERIFY(session->Restore_From_Store());
	}

	out_session = session;
	out_api = api;
}
//----------------------------------------------------------------------------
void AExercises_Api_Test::listParsesExerciseArray()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsAuth_Session *session = 0;
	AExercises_Api *api = 0;

	Make_Api(server, session, directory, false, api);
	QSignalSpy spy(api, &AExercises_Api::listFinished);
	QList<QVariant> arguments;
	QVector<SExercises_Item> items;
	AApi_Error error;

	server.Enqueue_Response(JSON_Response(200, Exercise_Array_Body()));

	int request_id = api->Get_List("all", QStringList(), QStringList(), QString(), 50, 0);

	QVERIFY(request_id > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	items = qvariant_cast<QVector<SExercises_Item>>(arguments.at(1));
	error = qvariant_cast<AApi_Error>(arguments.at(2));

	QCOMPARE(items.size(), 2);
	QCOMPARE(items[0].ID, QString("aaaaaaaa-1111-2222-3333-444444444444"));
	QCOMPARE(items[0].Name, QString("Bench press"));
	QCOMPARE(items[0].Description, QString("Сжимай."));
	QVERIFY(items[0].Has_Owner);
	QCOMPARE(items[0].Owner_ID, QString("user-1"));
	QCOMPARE(items[0].Muscle_Groups, QStringList({"chest"}));
	QCOMPARE(items[0].Equipment, QStringList({"barbell"}));
	QCOMPARE(items[0].Created_At, QString("2026-10-01T10:00:00Z"));
	QCOMPARE(items[1].ID, QString("bbbbbbbb-1111-2222-3333-444444444444"));
	QCOMPARE(items[1].Name, QString("Push up"));
	QVERIFY(items[1].Description.isEmpty());
	QVERIFY(items[1].Has_Owner == false);
	QCOMPARE(items[1].Muscle_Groups, QStringList({"chest", "triceps"}));
	QVERIFY(items[1].Equipment.isEmpty());
	QVERIFY(error.Is_Error() == false);
}
//----------------------------------------------------------------------------
void AExercises_Api_Test::listEncodesFiltersAndPagination()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsAuth_Session *session = 0;
	AExercises_Api *api = 0;

	Make_Api(server, session, directory, false, api);
	QSignalSpy spy(api, &AExercises_Api::listFinished);

	// Multi-формат (docs/03, §3.9): фильтры повторяются параметром, а не
	// склеиваются запятой; search уходит процент-кодированным.
	server.Enqueue_Response(JSON_Response(200, QByteArray("[]")));

	QVERIFY(api->Get_List("all", QStringList({"chest", "biceps"}), QStringList({"barbell"}), QString("жим"), 50, 100) >
	        0);
	QVERIFY(spy.wait(5000));

	STest_Http_Request captured = server.Request_At(0);
	QByteArray target = captured.Target;

	QVERIFY(target.startsWith(QByteArray("/api/v1/exercises?")));
	QVERIFY(target.contains(QByteArray("type=all")));
	QVERIFY(target.contains(QByteArray("muscle_group=chest")));
	QVERIFY(target.contains(QByteArray("muscle_group=biceps")));
	QVERIFY(target.contains(QByteArray("equipment=barbell")));
	QVERIFY(target.contains(QByteArray("limit=50")));
	QVERIFY(target.contains(QByteArray("offset=100")));
	QVERIFY(target.contains(QByteArray("search=")));
	QVERIFY(target.indexOf(QByteArray("жим")) < 0);
	QVERIFY(target.count(QByteArray("muscle_group=")) == 2);
}
//----------------------------------------------------------------------------
void AExercises_Api_Test::listRejectsInvalidParameters()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsAuth_Session *session = 0;
	AExercises_Api *api = 0;

	Make_Api(server, session, directory, false, api);

	// type задан Swagger-enum'ом, limit/offset — диапазонами (docs/03, §3.9):
	// невалидные параметры не отправляют запрос вовсе.
	QCOMPARE(api->Get_List("forbidden", QStringList(), QStringList(), QString(), 50, 0), 0);
	QCOMPARE(api->Get_List(QString(), QStringList(), QStringList(), QString(), 50, 0), 0);
	QCOMPARE(api->Get_List("all", QStringList(), QStringList(), QString(), 0, 0), 0);
	QCOMPARE(api->Get_List("all", QStringList(), QStringList(), QString(), 1001, 0), 0);
	QCOMPARE(api->Get_List("all", QStringList(), QStringList(), QString(), 50, -1), 0);
	QCOMPARE(api->Get_List("all", QStringList(), QStringList(), QString(), 50, 100001), 0);
	QCOMPARE(server.Request_Count(), 0);
	QVERIFY(api->Last_Error().isEmpty() == false);
}
//----------------------------------------------------------------------------
void AExercises_Api_Test::listGoesAnonymousWithoutSession()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsAuth_Session *session = 0;
	AExercises_Api *api = 0;

	Make_Api(server, session, directory, false, api);
	QSignalSpy spy(api, &AExercises_Api::listFinished);

	// Опциональная авторизация: без сессии запрос анонимный и успешный
	// (базовые и публичные), а не отказ (docs/03, §3.9).
	server.Enqueue_Response(JSON_Response(200, QByteArray("[]")));

	QVERIFY(api->Get_List("basic", QStringList(), QStringList(), QString(), 50, 0) > 0);
	QVERIFY(spy.wait(5000));

	QVERIFY(server.Request_At(0).Headers.contains("authorization") == false);
}
//----------------------------------------------------------------------------
void AExercises_Api_Test::listSendsBearerWithSession()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsAuth_Session *session = 0;
	AExercises_Api *api = 0;

	Make_Api(server, session, directory, true, api);
	QSignalSpy spy(api, &AExercises_Api::listFinished);

	// С активной сессией токен передаётся: с валидным токеном ответ
	// дополняется «своими» (docs/03, §3.9).
	server.Enqueue_Response(JSON_Response(200, QByteArray("[]")));

	QVERIFY(api->Get_List("all", QStringList(), QStringList(), QString(), 50, 0) > 0);
	QVERIFY(spy.wait(5000));

	QCOMPARE(server.Request_At(0).Headers.value("authorization"), QByteArray("Bearer stored-access"));
}
//----------------------------------------------------------------------------
void AExercises_Api_Test::listPreservesBackendErrorKey()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsAuth_Session *session = 0;
	AExercises_Api *api = 0;

	Make_Api(server, session, directory, false, api);
	QSignalSpy spy(api, &AExercises_Api::listFinished);
	QList<QVariant> arguments;
	AApi_Error error;

	server.Enqueue_Response(
	    JSON_Response(400, QByteArray("{\"key\":\"user_id_required_for_filter\",\"message\":\"token needed\"}")));

	QVERIFY(api->Get_List("own", QStringList(), QStringList(), QString(), 50, 0) > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	error = qvariant_cast<AApi_Error>(arguments.at(2));

	QVERIFY(error.Is_Error());
	QCOMPARE(error.Key(), QString("user_id_required_for_filter"));
}
//----------------------------------------------------------------------------
void AExercises_Api_Test::listIncompleteSuccessReportsUndefinedError()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsAuth_Session *session = 0;
	AExercises_Api *api = 0;

	Make_Api(server, session, directory, false, api);
	QSignalSpy spy(api, &AExercises_Api::listFinished);
	QList<QVariant> arguments;
	AApi_Error error;

	// Успешный статус с невалидным JSON — undefined_error (docs/03, §3.9).
	server.Enqueue_Response(JSON_Response(200, QByteArray("not-json")));

	QVERIFY(api->Get_List("basic", QStringList(), QStringList(), QString(), 50, 0) > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	error = qvariant_cast<AApi_Error>(arguments.at(2));

	QVERIFY(error.Is_Error());
	QCOMPARE(error.Key(), QString("undefined_error"));
}
//----------------------------------------------------------------------------
void AExercises_Api_Test::muscleGroupsParseArray()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsAuth_Session *session = 0;
	AExercises_Api *api = 0;

	Make_Api(server, session, directory, false, api);
	QSignalSpy spy(api, &AExercises_Api::muscleGroupsFinished);
	QList<QVariant> arguments;
	QStringList groups;
	AApi_Error error;

	server.Enqueue_Response(JSON_Response(200, QByteArray("[\"chest\",\"biceps\"]")));

	QVERIFY(api->Get_Muscle_Groups() > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	groups = qvariant_cast<QStringList>(arguments.at(1));
	error = qvariant_cast<AApi_Error>(arguments.at(2));

	QCOMPARE(groups, QStringList({"chest", "biceps"}));
	QVERIFY(error.Is_Error() == false);
	QCOMPARE(server.Request_At(0).Target, QByteArray("/api/v1/muscle-groups"));
}
//----------------------------------------------------------------------------
void AExercises_Api_Test::equipmentsParseArray()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AsAuth_Session *session = 0;
	AExercises_Api *api = 0;

	Make_Api(server, session, directory, false, api);
	QSignalSpy spy(api, &AExercises_Api::equipmentsFinished);
	QList<QVariant> arguments;
	QStringList equipments;
	AApi_Error error;

	server.Enqueue_Response(JSON_Response(200, QByteArray("[\"barbell\",\"dumbbell\"]")));

	QVERIFY(api->Get_Equipments() > 0);
	QVERIFY(spy.wait(5000));

	arguments = spy.takeFirst();
	equipments = qvariant_cast<QStringList>(arguments.at(1));
	error = qvariant_cast<AApi_Error>(arguments.at(2));

	QCOMPARE(equipments, QStringList({"barbell", "dumbbell"}));
	QVERIFY(error.Is_Error() == false);
	QCOMPARE(server.Request_At(0).Target, QByteArray("/api/v1/equipments"));
}
//----------------------------------------------------------------------------
QTEST_MAIN(AExercises_Api_Test)
#include "tst_Exercises_Api.moc"
