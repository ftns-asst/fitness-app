#include "Data/Auth/Auth_Session.h"
#include "Data/Auth/Token_Store.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Exercises/Exercises_Repository.h"
#include "Data/Network/Http_Client.h"
#include "Presentation/Exercises_Editor_VM.h"
#include "Support/Test_Http_Server.h"

#include <QDateTime>
#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

// Спринт U5 (docs/08-exercises-sprint-plan.md §4): форма своего упражнения.
// Валидация до отправки (пустое имя), тело POST (name/is_public/muscle_groups/
// equipment), PATCH с id в теле и сохранением формы при 403, удаление с
// очисткой кэша. Ответ POST парсится как Exercise (схема Swagger дефектная).
class Avm_Exercises_Editor_Test : public QObject
{
	Q_OBJECT

private slots:
	void createRejectsEmptyName();
	void createPostsBodyAndSaves();
	void editSendsPatchWithIdAndKeepsFormOn403();
	void deleteRemovesFromCache();

private:
	static STest_Http_Response JSON_Response(int in_status, const QByteArray &in_body);
	static QByteArray Exercise_Object_Body();
	void Make_VM(ATest_Http_Server &in_server, QTemporaryDir &out_dir, AExercises_Repository *&out_repository,
	             Avm_Exercises_Editor *&out_vm);
};
//----------------------------------------------------------------------------
STest_Http_Response Avm_Exercises_Editor_Test::JSON_Response(int in_status, const QByteArray &in_body)
{
	STest_Http_Response response;

	response.Status_Code = in_status;
	response.Body = in_body;

	return response;
}
//----------------------------------------------------------------------------
QByteArray Avm_Exercises_Editor_Test::Exercise_Object_Body()
{
	// Ответ POST/PATCH — Exercise (объект, не тело запроса, docs/03, §3.9).
	return QByteArray("{\"id\":\"cccccccc-1111-2222-3333-444444444444\",\"name\":\"My exercise\","
	                  "\"owner_id\":\"user-1\",\"muscle_groups\":[\"chest\"],\"equipment\":[\"barbell\"],"
	                  "\"created_at\":\"2026-10-05T10:00:00Z\"}");
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor_Test::Make_VM(ATest_Http_Server &in_server, QTemporaryDir &out_dir,
                                        AExercises_Repository *&out_repository, Avm_Exercises_Editor *&out_vm)
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

	// Редактору нужны сессия (кто владелец) и справочники в кэше.
	QVERIFY(store->Save_Tokens("user-1", "stored-access", "stored-refresh", QDateTime(), QDateTime()));
	QVERIFY(session->Restore_From_Store());
	QVERIFY(repository->Save_Catalogs(QStringList({"chest", "biceps"}), QStringList({"barbell", "dumbbell"})));

	out_repository = repository;
	out_vm = new Avm_Exercises_Editor(repository, &in_server);
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor_Test::createRejectsEmptyName()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AExercises_Repository *repository = 0;
	Avm_Exercises_Editor *vm = 0;

	Make_VM(server, directory, repository, vm);
	QSignalSpy saved_spy(vm, &Avm_Exercises_Editor::saved);

	// Пустое имя (и из одних пробелов) не отправляет запрос вовсе.
	vm->set_Name("   ");
	QVERIFY(vm->submit() == false);
	QCOMPARE(server.Request_Count(), 0);
	QCOMPARE(vm->Error_Key(), QString("invalid_name"));
	QCOMPARE(saved_spy.count(), 0);
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor_Test::createPostsBodyAndSaves()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AExercises_Repository *repository = 0;
	Avm_Exercises_Editor *vm = 0;

	Make_VM(server, directory, repository, vm);
	QSignalSpy saved_spy(vm, &Avm_Exercises_Editor::saved);
	QList<QVariant> arguments;

	vm->set_Name("  My exercise  ");
	vm->set_Description("Комментарий");
	vm->set_Is_Public(true);
	QVERIFY(vm->toggleMuscleGroup("chest"));
	QVERIFY(vm->toggleEquipment("barbell"));

	server.Enqueue_Response(JSON_Response(200, Exercise_Object_Body()));
	QVERIFY(vm->submit());
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Busy(), false, 5000);

	QCOMPARE(saved_spy.count(), 1);
	arguments = saved_spy.takeFirst();
	QCOMPARE(arguments.at(0).toString(), QString("cccccccc-1111-2222-3333-444444444444"));
	QVERIFY(vm->Error_Key().isEmpty());

	// Тело POST: trim имени, is_public, списки — по документации backend.
	STest_Http_Request captured = server.Request_At(0);
	QJsonDocument body = QJsonDocument::fromJson(captured.Body);

	QCOMPARE(captured.Method, QByteArray("POST"));
	QCOMPARE(captured.Target, QByteArray("/api/v1/exercises"));
	QCOMPARE(body.object().value("name").toString(), QString("My exercise"));
	QCOMPARE(body.object().value("is_public").toBool(), true);
	QCOMPARE(body.object().value("muscle_groups").toArray().size(), 1);
	QCOMPARE(body.object().value("equipment").toArray().at(0).toString(), QString("barbell"));

	// Созданное стало «правкой» (id известен) и лежит в кэше своим.
	QVERIFY(vm->Is_Selected());
	QVERIFY(repository->Is_Cached_Own("cccccccc-1111-2222-3333-444444444444"));
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor_Test::editSendsPatchWithIdAndKeepsFormOn403()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AExercises_Repository *repository = 0;
	Avm_Exercises_Editor *vm = 0;

	Make_VM(server, directory, repository, vm);
	QSignalSpy saved_spy(vm, &Avm_Exercises_Editor::saved);
	QList<QVariant> arguments;

	// Кэш прогрет созданием (тест выше), а здесь — прямой persist через список.
	server.Enqueue_Response(JSON_Response(200, QByteArray("[{\"id\":\"cccccccc-1111-2222-3333-444444444444\","
	                                                      "\"name\":\"My exercise\",\"owner_id\":\"user-1\","
	                                                      "\"muscle_groups\":[\"chest\"],\"equipment\":[\"barbell\"],"
	                                                      "\"description\":\"Старое описание\","
	                                                      "\"created_at\":\"2026-10-05T10:00:00Z\"}]")));
	// Прогрев кэша: правка читает поля из кэша репозитория.
	QSignalSpy list_spy(repository, &AExercises_Repository::listFinished);
	QVERIFY(repository->Load_List("all", QStringList(), QStringList(), QString(), 50, 0) > 0);
	QTRY_COMPARE_WITH_TIMEOUT(list_spy.count(), 1, 5000);

	// Ответ публичного набора для is_public-проверки loadExisting.
	server.Enqueue_Response(
	    JSON_Response(200, QByteArray("[{\"id\":\"cccccccc-1111-2222-3333-444444444444\",\"name\":\"My "
	                                  "exercise\",\"owner_id\":\"user-1\",\"muscle_groups\":[\"chest\"],\"equipment\":["
	                                  "],\"created_at\":\"2026-10-05T10:00:00Z\"}]")));
	QVERIFY(vm->loadExisting("cccccccc-1111-2222-3333-444444444444"));
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Public(), true, 5000);
	QCOMPARE(vm->Name(), QString("My exercise"));
	QCOMPARE(vm->Description(), QString("Старое описание"));

	vm->set_Description("Новое описание");

	// 403 (не ваше) — отказ, но форма жива: поля на месте, можно исправить.
	server.Enqueue_Response(JSON_Response(403, QByteArray("{\"key\":\"forbidden\",\"message\":\"not yours\"}")));
	QVERIFY(vm->submit());
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Busy(), false, 5000);

	QCOMPARE(vm->Error_Key(), QString("forbidden"));
	QCOMPARE(vm->Name(), QString("My exercise"));
	QCOMPARE(vm->Description(), QString("Новое описание"));
	QCOMPARE(saved_spy.count(), 0);

	// PATCH — третий запрос: список прогрева, публичный набор is_public-проверки.
	STest_Http_Request captured = server.Request_At(2);
	QJsonDocument body = QJsonDocument::fromJson(captured.Body);

	QCOMPARE(captured.Method, QByteArray("PATCH"));
	QCOMPARE(captured.Target, QByteArray("/api/v1/exercises/cccccccc-1111-2222-3333-444444444444"));
	QCOMPARE(body.object().value("id").toString(), QString("cccccccc-1111-2222-3333-444444444444"));
	QCOMPARE(body.object().value("description").toString(), QString("Новое описание"));
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor_Test::deleteRemovesFromCache()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	AExercises_Repository *repository = 0;
	Avm_Exercises_Editor *vm = 0;

	Make_VM(server, directory, repository, vm);
	QSignalSpy deleted_spy(vm, &Avm_Exercises_Editor::deleted);

	server.Enqueue_Response(JSON_Response(200, QByteArray("[{\"id\":\"cccccccc-1111-2222-3333-444444444444\","
	                                                      "\"name\":\"My exercise\",\"owner_id\":\"user-1\","
	                                                      "\"muscle_groups\":[\"chest\"],\"equipment\":[],"
	                                                      "\"created_at\":\"2026-10-05T10:00:00Z\"}]")));
	// Прогрев кэша: правка читает поля из кэша репозитория.
	QSignalSpy list_spy(repository, &AExercises_Repository::listFinished);
	QVERIFY(repository->Load_List("all", QStringList(), QStringList(), QString(), 50, 0) > 0);
	QTRY_COMPARE_WITH_TIMEOUT(list_spy.count(), 1, 5000);

	// Ответ публичного набора для is_public-проверки loadExisting.
	server.Enqueue_Response(
	    JSON_Response(200, QByteArray("[{\"id\":\"cccccccc-1111-2222-3333-444444444444\",\"name\":\"My "
	                                  "exercise\",\"owner_id\":\"user-1\",\"muscle_groups\":[\"chest\"],\"equipment\":["
	                                  "],\"created_at\":\"2026-10-05T10:00:00Z\"}]")));
	QVERIFY(vm->loadExisting("cccccccc-1111-2222-3333-444444444444"));
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Public(), true, 5000);

	// Мягкое удаление на сервере; кэш чистится жёстко (docs/03, §3.9).
	server.Enqueue_Response(JSON_Response(200, QByteArray()));
	QVERIFY(vm->remove());
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Busy(), false, 5000);

	QCOMPARE(deleted_spy.count(), 1);
	QVERIFY(vm->Is_Selected() == false);
	QVERIFY(vm->Name().isEmpty());

	// Удалённое не возвращается из кэша (офлайн-каталог его не покажет).
	QVERIFY(repository->Is_Cached_Own("cccccccc-1111-2222-3333-444444444444") == false);

	QVector<SExercises_Item> cached;

	QVERIFY(repository->Load_Cached_List(cached));
	QCOMPARE(cached.size(), 0);
}
//----------------------------------------------------------------------------

QTEST_MAIN(Avm_Exercises_Editor_Test)
#include "tst_Exercises_Editor_VM.moc"
