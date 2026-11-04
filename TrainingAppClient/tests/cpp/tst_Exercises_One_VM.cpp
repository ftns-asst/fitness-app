#include "Data/Auth/Auth_Session.h"
#include "Data/Auth/Token_Store.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Exercises/Exercises_Repository.h"
#include "Data/Network/Http_Client.h"
#include "Presentation/Exercises_One_VM.h"
#include "Support/Test_Http_Server.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

// Спринт U6 (docs/08-exercises-sprint-plan.md §4): ViewModel детальной
// карточки упражнения. Приватное чтение GET /exercises/{id}: своё — поля и
// is_own; чужое публичное — 403, нормализованный в понятный QML-ключ отказ;
// офлайн — карточка из кэша с маркером from_cache.
//
// Ожидание — через QTRY_*: stateChanged эмитится и при старте загрузки, и по
// завершению, поэтому spy.wait ловил бы старт, а не ответ.
class Avm_Exercises_One_Test : public QObject
{
	Q_OBJECT

private slots:
	void ownExerciseLoadsFromServer();
	void foreignExerciseRefusesWithReadableText();
	void offlineServesCachedOwn();
	void emptyIdStartsNothing();

private:
	static STest_Http_Response JSON_Response(int in_status, const QByteArray &in_body);
	static QByteArray Own_Exercise_Body();
	static QByteArray Two_Exercise_Body();
	// Сессия обязательна: GET /exercises/{id} — приватный эндпоинт, без
	// токена запрос не отправляется (docs/03, §3.9).
	void Make_VM(ATest_Http_Server &in_server, QTemporaryDir &out_dir, Avm_Exercises_One *&out_vm,
	             AExercises_Repository *&out_repository);
};

//----------------------------------------------------------------------------
STest_Http_Response Avm_Exercises_One_Test::JSON_Response(int in_status, const QByteArray &in_body)
{
	STest_Http_Response response;

	response.Status_Code = in_status;
	response.Body = in_body;

	return response;
}

//----------------------------------------------------------------------------
QByteArray Avm_Exercises_One_Test::Own_Exercise_Body()
{
	return QByteArray("{\"id\":\"cccccccc-1111-2222-3333-444444444444\","
	                  "\"name\":\"Моё упражнение\",\"description\":\"Техника выполнения.\","
	                  "\"owner_id\":\"user-1\",\"muscle_groups\":[\"chest\",\"biceps\"],"
	                  "\"equipment\":[\"barbell\"],\"created_at\":\"2026-10-15T09:00:00Z\"}");
}

//----------------------------------------------------------------------------
QByteArray Avm_Exercises_One_Test::Two_Exercise_Body()
{
	// Прогрев кэша: второе упражнение — своё (owner_id = user-1).
	return QByteArray("["
	                  "{\"id\":\"aaaaaaaa-1111-2222-3333-444444444444\",\"name\":\"Bench press\","
	                  "\"muscle_groups\":[\"chest\"],\"equipment\":[\"barbell\"],"
	                  "\"created_at\":\"2026-10-01T10:00:00Z\"},"
	                  "{\"id\":\"bbbbbbbb-1111-2222-3333-444444444444\",\"name\":\"Push up\","
	                  "\"owner_id\":\"user-1\",\"muscle_groups\":[\"chest\",\"triceps\"],"
	                  "\"equipment\":[],\"created_at\":\"2026-10-02T10:00:00Z\"}"
	                  "]");
}

//----------------------------------------------------------------------------
void Avm_Exercises_One_Test::Make_VM(ATest_Http_Server &in_server, QTemporaryDir &out_dir, Avm_Exercises_One *&out_vm,
                                     AExercises_Repository *&out_repository)
{
	AsSql_Database *database = new AsSql_Database(&in_server);
	AsToken_Store *store = new AsToken_Store(database, &in_server);
	AsHttp_Client *client = new AsHttp_Client((QUrl()), &in_server);
	AsAuth_Session *session = new AsAuth_Session(client, store, &in_server);
	AExercises_Repository *repository = new AExercises_Repository(client, session, database, &in_server);

	QVERIFY(out_dir.isValid());
	QVERIFY(database->Open(out_dir.filePath("cache.sqlite3")));
	QVERIFY(database->Migrate());
	QVERIFY(in_server.Start());
	QVERIFY(client->Set_Base_URL(in_server.Base_URL().resolved(QUrl("/api/v1"))));
	client->Set_Max_Retry_Count(0);

	// Сессия пользователя user-1: is_own определяется совпадением owner_id.
	QVERIFY(store->Save_Tokens("user-1", "stored-access", "stored-refresh", QDateTime(), QDateTime()));
	QVERIFY(session->Restore_From_Store());

	out_vm = new Avm_Exercises_One(repository, &in_server);
	out_repository = repository;
}

//----------------------------------------------------------------------------
void Avm_Exercises_One_Test::ownExerciseLoadsFromServer()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	Avm_Exercises_One *vm = 0;

	AExercises_Repository *repository = 0;

	Make_VM(server, directory, vm, repository);
	QSignalSpy spy(vm, &Avm_Exercises_One::stateChanged);

	server.Enqueue_Response(JSON_Response(200, Own_Exercise_Body()));
	vm->load("cccccccc-1111-2222-3333-444444444444");
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Loading(), false, 5000);

	// Поля карточки — контракт Exercise (docs/03, §3.9).
	QCOMPARE(vm->Exercise_Id(), QString("cccccccc-1111-2222-3333-444444444444"));
	QCOMPARE(vm->Name(), QString("Моё упражнение"));
	QCOMPARE(vm->Description(), QString("Техника выполнения."));
	QVERIFY(vm->Muscle_Groups().contains("chest"));
	QVERIFY(vm->Equipments().contains("barbell"));
	QVERIFY(vm->Is_From_Cache() == false);
	QVERIFY(vm->Error_Key().isEmpty());

	// Своё: владелец совпадает с сессией — правка доступна.
	QVERIFY(vm->Is_Own());
	QVERIFY(vm->Has_Exercise());
	QVERIFY(spy.count() >= 2);
}

//----------------------------------------------------------------------------
void Avm_Exercises_One_Test::foreignExerciseRefusesWithReadableText()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	Avm_Exercises_One *vm = 0;

	AExercises_Repository *repository = 0;

	Make_VM(server, directory, vm, repository);

	// 403 без тела: fallback-ключ undefined_error, статус различает отказ.
	server.Enqueue_Response(JSON_Response(403, QByteArray()));
	vm->load("dddddddd-1111-2222-3333-444444444444");
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Loading(), false, 5000);

	// Чужое публичное детально не открывается: понятный текст вместо общего
	// «Не удалось выполнить операцию» (docs/08, §4 U6).
	QCOMPARE(vm->Error_Key(), QString("forbidden_exercise"));
	QVERIFY(vm->Has_Exercise() == false);
	QVERIFY(vm->Name().isEmpty());
	QVERIFY(vm->Is_Own() == false);
}

//----------------------------------------------------------------------------
void Avm_Exercises_One_Test::offlineServesCachedOwn()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	Avm_Exercises_One *vm = 0;
	AExercises_Repository *repository = 0;

	// Кэш прогревается живым списком: «своё» persist'ится вместе с каталогом.
	Make_VM(server, directory, vm, repository);

	QSignalSpy list_spy(repository, &AExercises_Repository::listFinished);
	server.Enqueue_Response(JSON_Response(200, Two_Exercise_Body()));
	QVERIFY(repository->Load_List("all", QStringList(), QStringList(), QString(), 50, 0) > 0);
	QTRY_COMPARE_WITH_TIMEOUT(list_spy.count(), 1, 5000);

	// Сеть отвалилась: карточка отдаётся из кэша, не ошибкой.
	server.Stop();

	vm->load("bbbbbbbb-1111-2222-3333-444444444444");
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Loading(), false, 5000);

	QCOMPARE(vm->Name(), QString("Push up"));
	QVERIFY(vm->Is_From_Cache());
	QVERIFY(vm->Error_Key().isEmpty());
	QVERIFY(vm->Is_Own());
	QVERIFY(vm->Has_Exercise());
}

//----------------------------------------------------------------------------
void Avm_Exercises_One_Test::emptyIdStartsNothing()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	Avm_Exercises_One *vm = 0;

	AExercises_Repository *repository = 0;

	Make_VM(server, directory, vm, repository);
	QSignalSpy spy(vm, &Avm_Exercises_One::stateChanged);

	vm->load("");

	// Пустой идентификатор — не загрузка и не ошибка: экран показывает
	// приглушённое «Упражнение не выбрано».
	QCOMPARE(vm->Is_Loading(), false);
	QVERIFY(vm->Error_Key().isEmpty());
	QVERIFY(vm->Has_Exercise() == false);
	QCOMPARE(spy.count(), 1);
}

QTEST_MAIN(Avm_Exercises_One_Test)
#include "tst_Exercises_One_VM.moc"
