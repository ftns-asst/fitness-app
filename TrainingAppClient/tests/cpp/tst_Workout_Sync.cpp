#include "Data/Auth/Auth_Session.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Network/Network_Monitor.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Workout_Repository.h"
#include "Data/Program/Workout_Sync.h"
#include "Domain/Workout_Logger.h"
#include "Presentation/Workout_VM.h"
#include "Support/Test_Http_Server.h"

#include <QJsonArray>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

namespace
{
const QString Owner = "8fafae4a-f603-41de-ba55-bd4470774f7c";
const QString Other_Owner = "92a91c10-6322-4f4a-97fb-c710b2752084";
const QString Exercise = "3fade794-a9b0-4af1-a6d1-c4023a264f2c";
const QString Cloud_Workout = "c54946be-7a9e-4e52-bbb6-e2ac2bd56e33";

bool Insert_Exercise(AsSql_Database &in_database)
{
	QSqlQuery query(in_database.Sql_Connection());
	query.prepare("INSERT INTO exercises(id, title, muscle_group, equipment, created_at) VALUES(?, 'Press', 'chest', "
	              "'barbell', '2026-11-01')");
	query.addBindValue(Exercise);
	return query.exec();
}

QVariantList Plan()
{
	return {QVariantMap{{"exerciseId", Exercise}, {"weightKg", 62.5}, {"sets", 2}, {"repsMin", 6}, {"repsMax", 8}}};
}
} // namespace

class AWorkout_Sync_Test : public QObject
{
	Q_OBJECT

private slots:
	void savesOfflineAndRestoresOnOwnerReconnect();
	void failedQueueWriteLeavesWorkoutAsDraft();
	void syncsServerProgramDayAfterRestart();
};

//-------------------------------------------------------------------------------------------------------------
void AWorkout_Sync_Test::savesOfflineAndRestoresOnOwnerReconnect()
{
	QTemporaryDir directory;
	AsSql_Database database;
	const QString path = directory.filePath("workout-sync.sqlite");
	QVERIFY(database.Open(path));
	QVERIFY(Insert_Exercise(database));
	QString workout_id;
	QByteArray queued_payload;
	{
		AProgram_Repository programs(&database);
		AWorkout_Repository workouts(&database);
		AsWorkout_Logger logger(&workouts, &programs);
		Avm_Workout vm(&logger, &workouts, &programs);
		QVERIFY(vm.startSolo(Owner, "2026-11-24", Plan()));
		workout_id = vm.sessionId();
		QVERIFY(vm.saveSet(0, 1, 62.5, 7));
		QVERIFY(vm.finish());

		QSqlQuery query(database.Sql_Connection());
		QVERIFY(query.exec("SELECT payload_json FROM sync_outbox WHERE entity_type = 'workout'"));
		QVERIFY(query.next());
		queued_payload = query.value(0).toString().toUtf8();
	}
	database.Close();
	QVERIFY(database.Open(path));
	AWorkout_Repository workouts(&database);
	ATest_Http_Server server;
	QVERIFY(server.Start());
	AsHttp_Client http(server.Base_URL().resolved(QUrl("/api/v1")));
	AsToken_Store tokens(0);
	AsAuth_Session session(&http, &tokens);
	AsNetwork_Monitor network(0, false);
	network.Update_State(QNetworkInformation::Reachability::Disconnected, false, true);
	AWorkout_Sync sync(&database, &workouts, &session, &network);
	int uploads = 0;
	int conflicts = 0;
	QByteArray first_payload;
	connect(&server, &ATest_Http_Server::requestReceived, this,
	        [&]
	        {
		        const STest_Http_Request request = server.Request_At(server.Request_Count() - 1);
		        STest_Http_Response reply;
		        if (request.Method == "PUT")
		        {
			        if (request.Target != QByteArray("/api/v1/workouts/") + workout_id.toUtf8())
			        {
				        ++conflicts;
				        reply.Status_Code = 409;
				        reply.Body = R"({"key":"conflict"})";
				        server.Enqueue_Response(reply);
				        return;
			        }
			        ++uploads;
			        QCOMPARE(request.Target, QByteArray("/api/v1/workouts/") + workout_id.toUtf8());
			        QCOMPARE(request.Headers.value("authorization"), QByteArray("Bearer owner-access"));
			        QCOMPARE(request.Body, queued_payload);
			        if (uploads == 1)
			        {
				        first_payload = request.Body;
				        reply.Status_Code = 503;
				        reply.Body = R"({"key":"undefined_error"})";
			        }
			        else
			        {
				        QCOMPARE(request.Body, first_payload);
				        reply.Status_Code = 200;
				        reply.Body = QJsonDocument(QJsonObject{{"id", workout_id}}).toJson();
			        }
		        }
		        else if (request.Target.endsWith(Cloud_Workout.toUtf8()))
			        reply.Body =
			            QJsonDocument(QJsonObject{{"id", Cloud_Workout}, {"exercises", QJsonArray{}}}).toJson();
		        else if (request.Headers.value("authorization") == "Bearer owner-access")
			        reply.Body =
			            QJsonDocument(
			                QJsonArray{QJsonObject{{"id", workout_id}, {"completed_at", "2026-11-24T12:00:00Z"}},
			                           QJsonObject{{"id", Cloud_Workout}, {"completed_at", "2026-11-23T12:00:00Z"}}})
			                .toJson();
		        else
			        reply.Body = "[]";
		        server.Enqueue_Response(reply);
	        });

	QVERIFY(session.Adopt_Session({Other_Owner, "other-access", "refresh", {}, {}}));
	network.Update_State(QNetworkInformation::Reachability::Online, false, true);
	QTRY_VERIFY(server.Request_Count() > 0 && !sync.loading());
	QCOMPARE(uploads, 0);
	QVERIFY(sync.history().isEmpty());
	QVERIFY(session.Adopt_Session({Owner, "owner-access", "refresh", {}, {}}));
	QTRY_VERIFY_WITH_TIMEOUT(uploads >= 2, 9000);
	SWorkout_Session completed;
	QTRY_VERIFY(workouts.Find_Session(workout_id, completed) && completed.Sync_State == ESync_State::Synced);
	QCOMPARE(sync.history().size(), 2);
	sync.openWorkout(Cloud_Workout);
	QTRY_COMPARE(sync.detail().value("id").toString(), Cloud_Workout);

	AProgram_Repository programs(&database);
	AsWorkout_Logger logger(&workouts, &programs);
	Avm_Workout vm(&logger, &workouts, &programs);
	QVERIFY(vm.startSolo(Owner, "2026-11-24", Plan()));
	QVERIFY(vm.saveSet(0, 1, 80, 6));
	QVERIFY(vm.finish());
	sync.Flush();
	const auto conflict_visible = [&]
	{
		for (const QVariant &value : sync.history())
		{
			const QVariantMap row = value.toMap();
			if (row.value("id").toString() == vm.sessionId())
				return row.value("sync").toString() == "workout_conflict";
		}
		return false;
	};
	QTRY_VERIFY(conflict_visible());
	sync.Flush();
	QTest::qWait(100);
	QCOMPARE(conflicts, 1);
	session.Drop_Session();
	QVERIFY(sync.history().isEmpty());
	QVERIFY(sync.detail().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Sync_Test::failedQueueWriteLeavesWorkoutAsDraft()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QVERIFY(database.Open(directory.filePath("workout-sync.sqlite")));
	QVERIFY(Insert_Exercise(database));
	AProgram_Repository programs(&database);
	AWorkout_Repository workouts(&database);
	AsWorkout_Logger logger(&workouts, &programs);
	Avm_Workout vm(&logger, &workouts, &programs);
	QVERIFY(vm.startSolo(Owner, "2026-11-24", Plan()));
	QVERIFY(vm.saveSet(0, 1, 62.5, 7));
	QSqlQuery query(database.Sql_Connection());
	QVERIFY(
	    query.exec("CREATE TRIGGER fail_queue BEFORE INSERT ON sync_outbox BEGIN SELECT RAISE(ABORT, 'test'); END"));
	QVERIFY(!vm.finish());
	SWorkout_Session session;
	QVERIFY(workouts.Find_Session(vm.sessionId(), session));
	QCOMPARE(session.State, EWorkout_State::Draft);
	QVERIFY(query.exec("DROP TRIGGER fail_queue"));
	QVERIFY(vm.finish());
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Sync_Test::syncsServerProgramDayAfterRestart()
{
	const QString program_id = "27d528cf-6e78-4880-8ca4-eb9abb55b9e1";
	QTemporaryDir directory;
	AsSql_Database database;
	const QString path = directory.filePath("program-workout-sync.sqlite");
	QVERIFY(database.Open(path));
	QVERIFY(Insert_Exercise(database));
	QString workout_id;
	{
		AProgram_Repository programs(&database);
		AWorkout_Repository workouts(&database);
		AsWorkout_Logger logger(&workouts, &programs);
		Avm_Workout vm(&logger, &workouts, &programs);
		QVERIFY(vm.startServerDay(Owner, "2026-11-24", program_id, 2, 4, "Press day", Plan()));
		workout_id = vm.sessionId();
		QVERIFY(vm.saveSet(0, 1, 65, 7));
	}
	database.Close();
	QVERIFY(database.Open(path));
	AProgram_Repository programs(&database);
	AWorkout_Repository workouts(&database);
	AsWorkout_Logger logger(&workouts, &programs);
	Avm_Workout vm(&logger, &workouts, &programs);
	QVERIFY(!vm.startServerDay(Owner, "2026-11-24", program_id, 3, 4, "Other day", Plan()));
	QVERIFY(vm.startServerDay(Owner, "2026-11-25", program_id, 2, 4, "Press day", Plan()));
	QCOMPARE(vm.sessionId(), workout_id);
	QCOMPARE(vm.completedSets(), 1);
	QVERIFY(vm.saveSet(0, 2, 65, 6));
	QVERIFY(vm.finish());

	ATest_Http_Server server;
	QVERIFY(server.Start());
	AsHttp_Client http(server.Base_URL().resolved(QUrl("/api/v1")));
	AsToken_Store tokens(0);
	AsAuth_Session session(&http, &tokens);
	AsNetwork_Monitor network(0, false);
	network.Update_State(QNetworkInformation::Reachability::Disconnected, false, true);
	AWorkout_Sync sync(&database, &workouts, &session, &network);
	QVERIFY(session.Adopt_Session({Owner, "access", "refresh", {}, {}}));
	sync.Flush();
	QCOMPARE(server.Request_Count(), 0);
	QVERIFY(sync.history().isEmpty());
	QJsonObject payload;
	connect(&server, &ATest_Http_Server::requestReceived, this,
	        [&]
	        {
		        const STest_Http_Request request = server.Request_At(server.Request_Count() - 1);
		        STest_Http_Response reply;
		        reply.Body = "[]";
		        if (request.Method == "PUT")
		        {
			        QCOMPARE(request.Target, QByteArray("/api/v1/workouts/") + workout_id.toUtf8());
			        payload = QJsonDocument::fromJson(request.Body).object();
			        reply.Status_Code = 201;
			        reply.Body = QJsonDocument(QJsonObject{{"id", workout_id}}).toJson();
		        }
		        server.Enqueue_Response(reply);
	        });
	network.Update_State(QNetworkInformation::Reachability::Online, false, true);
	SWorkout_Session completed;
	QTRY_VERIFY(workouts.Find_Session(workout_id, completed) && completed.Sync_State == ESync_State::Synced);
	QCOMPARE(payload.value("program_id").toString(), program_id);
	QCOMPARE(payload.value("week_number").toInt(), 2);
	QCOMPARE(payload.value("day_of_week").toInt(), 4);
	const QJsonObject exercise = payload.value("exercises").toArray().first().toObject();
	QCOMPARE(exercise.value("target_sets").toInt(), 2);
	QCOMPARE(exercise.value("target_reps").toInt(), 6);
	QCOMPARE(exercise.value("target_weight_kg").toDouble(), 62.5);
	QCOMPARE(exercise.value("sets").toArray().size(), 2);
	QVERIFY(sync.history().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
QTEST_GUILESS_MAIN(AWorkout_Sync_Test)
#include "tst_Workout_Sync.moc"
