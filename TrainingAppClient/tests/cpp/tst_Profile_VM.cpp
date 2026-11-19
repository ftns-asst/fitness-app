#include "Data/Auth/Token_Store.h"
#include "Presentation/Profile_VM.h"
#include "Support/Test_Http_Server.h"

#include <QSignalSpy>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

struct SProfile_Fixture
{
	QTemporaryDir Directory;
	AsSql_Database Database;
	AsToken_Store Store{&Database};
	AsHttp_Client Client{QUrl()};
	AsAuth_Session Session{&Client, &Store};
	AsNetwork_Monitor Network{0, false};
	AUsers_Repository Repository{&Session, &Database};
	AProfile_Sync Sync{&Repository, &Session, &Network};
	Avm_Profile Model{&Repository, &Network, &Sync};
	ATest_Http_Server Server;

	bool Initialize(const QString &in_path = QString());
	void Respond(const QByteArray &in_body, int in_status = 200);
	void Load();
};
//-------------------------------------------------------------------------------------------------------------
bool SProfile_Fixture::Initialize(const QString &in_path)
{
	return Database.Open(in_path.isEmpty() ? Directory.filePath("cache.sqlite3") : in_path) && Server.Start() &&
	       Client.Set_Base_URL(Server.Base_URL().resolved(QUrl("/api/v1"))) &&
	       Store.Save_Tokens("user-1", "access", "refresh", QDateTime(), QDateTime()) && Session.Restore_From_Store();
}
//-------------------------------------------------------------------------------------------------------------
void SProfile_Fixture::Respond(const QByteArray &in_body, int in_status)
{
	STest_Http_Response response;
	response.Body = in_body;
	response.Status_Code = in_status;
	Server.Enqueue_Response(response);
}
//-------------------------------------------------------------------------------------------------------------
void SProfile_Fixture::Load()
{
	Respond("{\"id\":\"user-1\",\"name\":\"User\",\"email\":\"user@example.com\","
	        "\"created_at\":\"2026-11-13T00:00:00Z\",\"profile\":{\"weight\":70,\"height\":180}}");
	Model.reload("user-1");
}
//-------------------------------------------------------------------------------------------------------------
class AProfile_VM_Test : public QObject
{
	Q_OBJECT

private slots:
	void savesProfileAndCachesOmittedAge();
	void failureKeepsLocalChangesPending();
	void offlineSaveSyncsOnReconnect();
	void userChangeIgnoresOldReply();
	void validatesChangesBeforeSending();
	void logoutDuringSaveIgnoresOldResponse();
	void pendingProfileSurvivesRestart();
	void newerEditsSurviveOlderAcknowledgement();
	void temporaryFailureRetriesAutomatically();
	void anotherAccountDoesNotSendPendingProfile();
	void staleReadDoesNotOverwriteLocalSave();
	void failedQueueWriteRollsBackProfile();
};
//-------------------------------------------------------------------------------------------------------------
void AProfile_VM_Test::savesProfileAndCachesOmittedAge()
{
	SProfile_Fixture fixture;
	QVERIFY(fixture.Initialize());
	fixture.Load();
	QTRY_VERIFY(!fixture.Model.loading());
	QSignalSpy saved(&fixture.Model, &Avm_Profile::profileSaved);
	fixture.Respond("{\"weight\":72.5,\"height\":180}");
	QVERIFY(fixture.Model.saveProfile(QVariantMap{{"weightKg", 72.5}}));
	QTRY_COMPARE(saved.count(), 1);
	QTRY_VERIFY(!fixture.Model.pendingSync());
	QCOMPARE(fixture.Model.profile().value("weightKg").toDouble(), 72.5);
	SUsers_User cached;
	QVERIFY(fixture.Repository.Get_Cached_User("user-1", cached));
	QVERIFY(cached.Has_Profile);
	QCOMPARE(cached.Profile.Weight_Kg, 72.5);
	QCOMPARE(cached.Profile.Age, -1);
	QCOMPARE(fixture.Repository.Pending_Outbox_Count(), 0);
}
//-------------------------------------------------------------------------------------------------------------
void AProfile_VM_Test::failureKeepsLocalChangesPending()
{
	SProfile_Fixture fixture;
	QVERIFY(fixture.Initialize());
	fixture.Load();
	QTRY_VERIFY(!fixture.Model.loading());
	fixture.Respond("{\"key\":\"validation_error\"}", 400);
	QVERIFY(fixture.Model.saveProfile(QVariantMap{{"weightKg", 72.5}}));
	QTRY_VERIFY(!fixture.Model.saving());
	QCOMPARE(fixture.Model.profile().value("weightKg").toDouble(), 72.5);
	QTRY_VERIFY(fixture.Model.pendingSync());
	QTRY_COMPARE(fixture.Model.syncErrorKey(), QString("validation_error"));
	QVERIFY(fixture.Model.syncBlocked());
	fixture.Respond("{\"weight\":73,\"height\":180}");
	QVERIFY(fixture.Model.saveProfile(QVariantMap{{"weightKg", 73}}));
	QTRY_VERIFY(!fixture.Model.pendingSync());
	QCOMPARE(fixture.Model.profile().value("weightKg").toInt(), 73);
}
//-------------------------------------------------------------------------------------------------------------
void AProfile_VM_Test::offlineSaveSyncsOnReconnect()
{
	SProfile_Fixture fixture;
	QVERIFY(fixture.Initialize());
	fixture.Load();
	QTRY_VERIFY(!fixture.Model.loading());
	fixture.Network.Update_State(QNetworkInformation::Reachability::Disconnected, false, true);
	QVERIFY(fixture.Model.saveProfile(QVariantMap{{"age", 30}}));
	QCOMPARE(fixture.Model.profile().value("age").toInt(), 30);
	QVERIFY(fixture.Model.pendingSync());
	QCOMPARE(fixture.Server.Request_Count(), 1);
	QCOMPARE(fixture.Repository.Pending_Outbox_Count(), 1);
	fixture.Respond("{\"age\":30,\"height\":180,\"weight\":70}");
	fixture.Network.Update_State(QNetworkInformation::Reachability::Online, false, true);
	QTRY_VERIFY(!fixture.Model.pendingSync());
	QCOMPARE(fixture.Repository.Pending_Outbox_Count(), 0);
}
//-------------------------------------------------------------------------------------------------------------
void AProfile_VM_Test::userChangeIgnoresOldReply()
{
	SProfile_Fixture fixture;
	QVERIFY(fixture.Initialize());
	fixture.Load();
	fixture.Model.reload("");
	QTest::qWait(100);
	QVERIFY(fixture.Model.profile().isEmpty());
	QVERIFY(fixture.Model.userId().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void AProfile_VM_Test::validatesChangesBeforeSending()
{
	SProfile_Fixture fixture;
	QVERIFY(fixture.Initialize());
	fixture.Load();
	QTRY_VERIFY(!fixture.Model.loading());
	QVERIFY(!fixture.Model.saveProfile(QVariantMap{{"age", 30.5}}));
	QVERIFY(!fixture.Model.saveProfile(QVariantMap{{"heightCm", 180.5}}));
	QVERIFY(!fixture.Model.saveProfile(QVariantMap{{"gender", "other"}}));
	QVERIFY(!fixture.Model.saveProfile(QVariantMap{{"weightKg", 501}}));
	QCOMPARE(fixture.Server.Request_Count(), 1);
}
//-------------------------------------------------------------------------------------------------------------
void AProfile_VM_Test::logoutDuringSaveIgnoresOldResponse()
{
	SProfile_Fixture fixture;
	QVERIFY(fixture.Initialize());
	fixture.Load();
	QTRY_VERIFY(!fixture.Model.loading());
	STest_Http_Response response;
	response.Body = "{\"weight\":72.5}";
	response.Delay_Ms = 50;
	fixture.Server.Enqueue_Response(response);
	QVERIFY(fixture.Model.saveProfile(QVariantMap{{"weightKg", 72.5}}));
	fixture.Model.reload("");
	QTest::qWait(150);
	QVERIFY(fixture.Model.userId().isEmpty());
	QVERIFY(fixture.Model.profile().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void AProfile_VM_Test::pendingProfileSurvivesRestart()
{
	QTemporaryDir root;
	QString path = root.filePath("cache.sqlite3");
	{
		SProfile_Fixture fixture;
		QVERIFY(fixture.Initialize(path));
		fixture.Load();
		QTRY_VERIFY(!fixture.Model.loading());
		fixture.Network.Update_State(QNetworkInformation::Reachability::Disconnected, false, true);
		QVERIFY(fixture.Model.saveProfile(QVariantMap{{"weightKg", 73.5}}));
		QVERIFY(fixture.Model.pendingSync());
	}
	SProfile_Fixture restarted;
	restarted.Respond("{\"weight\":73.5,\"height\":180}");
	QVERIFY(restarted.Initialize(path));
	QTRY_COMPARE(restarted.Repository.Pending_Outbox_Count(), 0);
	SUsers_User cached;
	QVERIFY(restarted.Repository.Get_Cached_User("user-1", cached));
	QCOMPARE(cached.Profile.Weight_Kg, 73.5);
	QCOMPARE(restarted.Server.Request_Count(), 1);
}
//-------------------------------------------------------------------------------------------------------------
void AProfile_VM_Test::newerEditsSurviveOlderAcknowledgement()
{
	SProfile_Fixture fixture;
	QVERIFY(fixture.Initialize());
	fixture.Load();
	QTRY_VERIFY(!fixture.Model.loading());
	fixture.Network.Update_State(QNetworkInformation::Reachability::Disconnected, false, true);
	QVERIFY(fixture.Model.saveProfile(QVariantMap{{"weightKg", 72}}));
	STest_Http_Response first;
	first.Body = "{\"weight\":72,\"height\":180}";
	first.Delay_Ms = 100;
	fixture.Server.Enqueue_Response(first);
	STest_Http_Response second;
	second.Body = "{\"age\":31,\"weight\":73,\"height\":180}";
	second.Delay_Ms = 300;
	fixture.Server.Enqueue_Response(second);
	fixture.Network.Update_State(QNetworkInformation::Reachability::Online, false, true);
	QVERIFY(fixture.Model.saveProfile(QVariantMap{{"weightKg", 73}, {"age", 31}}));
	QTRY_COMPARE(fixture.Server.Request_Count(), 3);
	QCOMPARE(fixture.Model.profile().value("weightKg").toInt(), 73);
	QCOMPARE(fixture.Model.profile().value("age").toInt(), 31);
	QTRY_VERIFY(!fixture.Model.pendingSync());
}
//-------------------------------------------------------------------------------------------------------------
void AProfile_VM_Test::temporaryFailureRetriesAutomatically()
{
	SProfile_Fixture fixture;
	QVERIFY(fixture.Initialize());
	fixture.Load();
	QTRY_VERIFY(!fixture.Model.loading());
	fixture.Respond("{\"key\":\"server_error\"}", 500);
	fixture.Respond("{\"weight\":73,\"height\":180}");
	QVERIFY(fixture.Model.saveProfile(QVariantMap{{"weightKg", 73}}));
	QTRY_COMPARE(fixture.Model.syncErrorKey(), QString("server_error"));
	QVERIFY(!fixture.Model.syncBlocked());
	QTRY_VERIFY_WITH_TIMEOUT(!fixture.Model.pendingSync(), 10000);
	QCOMPARE(fixture.Server.Request_Count(), 3);
}
//-------------------------------------------------------------------------------------------------------------
void AProfile_VM_Test::anotherAccountDoesNotSendPendingProfile()
{
	SProfile_Fixture fixture;
	QVERIFY(fixture.Initialize());
	fixture.Load();
	QTRY_VERIFY(!fixture.Model.loading());
	fixture.Network.Update_State(QNetworkInformation::Reachability::Disconnected, false, true);
	QVERIFY(fixture.Model.saveProfile(QVariantMap{{"age", 31}}));
	fixture.Model.reload("");
	fixture.Session.Drop_Session();
	QVERIFY(fixture.Store.Save_Tokens("user-2", "other-access", "other-refresh", QDateTime(), QDateTime()));
	QVERIFY(fixture.Session.Restore_From_Store());
	fixture.Network.Update_State(QNetworkInformation::Reachability::Online, false, true);
	QTest::qWait(100);
	QCOMPARE(fixture.Server.Request_Count(), 1);
	SProfile_Pending pending;
	QVERIFY(fixture.Repository.Load_Pending_Profile("user-1", pending));
	QCOMPARE(pending.Changes.value("age").toInt(), 31);
}
//-------------------------------------------------------------------------------------------------------------
void AProfile_VM_Test::staleReadDoesNotOverwriteLocalSave()
{
	SProfile_Fixture fixture;
	QVERIFY(fixture.Initialize());
	fixture.Load();
	QTRY_VERIFY(!fixture.Model.loading());
	STest_Http_Response response;
	response.Body = "{\"id\":\"user-1\",\"email\":\"user@example.com\",\"name\":\"User\","
	                "\"created_at\":\"2026-11-13T00:00:00Z\",\"profile\":{\"weight\":70,\"height\":180}}";
	response.Delay_Ms = 100;
	fixture.Server.Enqueue_Response(response);
	fixture.Model.reload("user-1");
	fixture.Network.Update_State(QNetworkInformation::Reachability::Disconnected, false, true);
	QVERIFY(fixture.Model.saveProfile(QVariantMap{{"weightKg", 74}}));
	QTest::qWait(200);
	SUsers_User cached;
	QVERIFY(fixture.Repository.Get_Cached_User("user-1", cached));
	QCOMPARE(cached.Profile.Weight_Kg, 74.0);
	QCOMPARE(fixture.Model.profile().value("weightKg").toInt(), 74);
}
//-------------------------------------------------------------------------------------------------------------
void AProfile_VM_Test::failedQueueWriteRollsBackProfile()
{
	SProfile_Fixture fixture;
	QVERIFY(fixture.Initialize());
	fixture.Load();
	QTRY_VERIFY(!fixture.Model.loading());
	QSqlQuery query(fixture.Database.Sql_Connection());
	QVERIFY(query.exec("CREATE TEMP TRIGGER fail_profile_queue BEFORE INSERT ON sync_outbox "
	                   "WHEN NEW.entity_type = 'user_profile' BEGIN SELECT RAISE(ABORT, 'test failure'); END"));
	QSignalSpy saved(&fixture.Model, &Avm_Profile::profileSaved);
	QVERIFY(!fixture.Model.saveProfile(QVariantMap{{"weightKg", 74}}));
	QCOMPARE(saved.count(), 0);
	QCOMPARE(fixture.Repository.Pending_Outbox_Count(), 0);
	SUsers_User cached;
	QVERIFY(fixture.Repository.Get_Cached_User("user-1", cached));
	QCOMPARE(cached.Profile.Weight_Kg, 70.0);
	QCOMPARE(fixture.Model.errorKey(), QString("local_save_failed"));
}
//-------------------------------------------------------------------------------------------------------------
QTEST_GUILESS_MAIN(AProfile_VM_Test)
#include "tst_Profile_VM.moc"
