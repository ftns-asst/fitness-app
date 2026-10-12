#include "Data/Auth/Auth_Session.h"
#include "Data/Auth/Token_Store.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Exercises/Exercises_Repository.h"
#include "Data/Network/Http_Client.h"
#include "Presentation/Exercises_VM.h"
#include "Support/Test_Http_Server.h"

#include <QSignalSpy>
#include <QTemporaryDir>
#include <QtTest>

// Спринт U3 (docs/08-exercises-sprint-plan.md §4): ViewModel каталога
// упражнений. Инкапсулирует debounce-поиск (быстрый ввод даёт один запрос),
// multi-фильтры (смена фильтра сбрасывает пагинацию), пагинацию «загрузить
// ещё» (конец — по короткой странице) и роли строк под CatalogExerciseRow
// (title/subtitle — mock-контракт).
//
// Ожидание — через QTRY_*: stateChanged эмитится и при старте загрузки
// (loading=true), и по завершении, поэтому spy.wait ловил бы старт, а не ответ.
class Avm_Exercises_Test : public QObject
{
	Q_OBJECT

private slots:
	void rolesMatchMockContract();
	void debounceCoalescesTyping();
	void filterChangeResetsOffset();
	void shortPageEndsPagination();
	void offlineShowsCacheWithMarker();

private:
	static STest_Http_Response JSON_Response(int in_status, const QByteArray &in_body);
	static QByteArray Two_Exercise_Body();
	static QByteArray Fifty_Exercise_Body();
	// page_size — размер страницы пагинации (продакшен 50, тесты меньше).
	void Make_VM(ATest_Http_Server &in_server, QTemporaryDir &out_dir, int in_page_size, Avm_Exercises *&out_vm);
};

//----------------------------------------------------------------------------
STest_Http_Response Avm_Exercises_Test::JSON_Response(int in_status, const QByteArray &in_body)
{
	STest_Http_Response response;

	response.Status_Code = in_status;
	response.Body = in_body;

	return response;
}
//----------------------------------------------------------------------------
QByteArray Avm_Exercises_Test::Two_Exercise_Body()
{
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
QByteArray Avm_Exercises_Test::Fifty_Exercise_Body()
{
	QString body = "[";

	for (int i = 0; i < 50; ++i)
		body += QString("{\"id\":\"%1\",\"name\":\"Exercise %2\",\"muscle_groups\":[\"chest\"],"
		                "\"equipment\":[],\"created_at\":\"2026-10-01T10:00:00Z\"}")
		            .arg(1000 + i, 4, 10, QChar('0'))
		            .arg(i) +
		        (i < 49 ? "," : "");

	return body.toUtf8() + "]";
}
//----------------------------------------------------------------------------
void Avm_Exercises_Test::Make_VM(ATest_Http_Server &in_server, QTemporaryDir &out_dir, int in_page_size,
                                 Avm_Exercises *&out_vm)
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

	out_vm = new Avm_Exercises(repository, in_page_size, &in_server);
}
//----------------------------------------------------------------------------
void Avm_Exercises_Test::rolesMatchMockContract()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	Avm_Exercises *vm = 0;
	Make_VM(server, directory, 50, vm);
	QVariantMap row;

	// Каталог открывается одним запросом первой страницы.
	server.Enqueue_Response(JSON_Response(200, Two_Exercise_Body()));
	QVERIFY(vm->reload());
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Loading(), false, 5000);

	QCOMPARE(vm->exercise_list()->Row_Count(), 2);
	QVERIFY(vm->Is_From_Cache() == false);
	QVERIFY(vm->Error_Key().isEmpty());
	QVERIFY(vm->Can_Load_More() == false);

	// Роли — mock-контракт CatalogExerciseRow: title/subtitle (docs/08, §4 U3).
	row = vm->exercise_list()->get(0);
	QCOMPARE(row.value("title").toString(), QString("Bench press"));
	QCOMPARE(row.value("subtitle").toString(), QString("chest"));

	// Свойства-справочники для чипов заполняются отдельными запросами.
	server.Enqueue_Response(JSON_Response(200, QByteArray("[\"chest\",\"biceps\"]")));
	server.Enqueue_Response(JSON_Response(200, QByteArray("[\"barbell\"]")));
	vm->loadCatalogs();

	QTRY_COMPARE_WITH_TIMEOUT(vm->Muscle_Groups(), QStringList({"chest", "biceps"}), 5000);
	QTRY_COMPARE_WITH_TIMEOUT(vm->Equipments(), QStringList({"barbell"}), 5000);
}
//----------------------------------------------------------------------------
void Avm_Exercises_Test::debounceCoalescesTyping()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	Avm_Exercises *vm = 0;
	Make_VM(server, directory, 50, vm);

	// Стартовый reload — первый запрос.
	server.Enqueue_Response(JSON_Response(200, QByteArray("[]")));
	QVERIFY(vm->reload());
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Loading(), false, 5000);

	// Быстрый ввод трёх символов укладывается в окно debounce: все три
	// вызова дают один серверный запрос (docs/08, §4 U3).
	vm->setQuery("ж");
	vm->setQuery("жи");
	vm->setQuery("жим");
	QTest::qWait(450);
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Loading(), false, 5000);

	QCOMPARE(server.Request_Count(), 2);

	STest_Http_Request captured = server.Request_At(1);

	QVERIFY(captured.Target.contains(QByteArray("search=")));
}
//----------------------------------------------------------------------------
void Avm_Exercises_Test::filterChangeResetsOffset()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	Avm_Exercises *vm = 0;
	Make_VM(server, directory, 2, vm);

	// Полная страница (ровно page_size) оставляет canLoadMore включённым.
	server.Enqueue_Response(JSON_Response(200, Two_Exercise_Body()));
	QVERIFY(vm->reload());
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Loading(), false, 5000);

	QVERIFY(vm->Can_Load_More());
	QCOMPARE(vm->exercise_list()->Row_Count(), 2);

	// Вторая страница уходит с offset=2.
	server.Enqueue_Response(JSON_Response(200, QByteArray("[]")));
	QVERIFY(vm->loadMore());
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Loading(), false, 5000);

	STest_Http_Request more_request = server.Request_At(1);

	QVERIFY(more_request.Target.contains(QByteArray("offset=2")));

	// Смена фильтра перезапускает выборку с начала: offset=0, а не накоплен.
	server.Enqueue_Response(JSON_Response(200, Two_Exercise_Body()));
	QVERIFY(vm->setMuscleGroups(QStringList({"chest"})));
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Loading(), false, 5000);

	STest_Http_Request filtered = server.Request_At(2);

	QVERIFY(filtered.Target.contains(QByteArray("offset=0")));
	QVERIFY(filtered.Target.contains(QByteArray("muscle_group=chest")));
}
//----------------------------------------------------------------------------
void Avm_Exercises_Test::shortPageEndsPagination()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	Avm_Exercises *vm = 0;
	Make_VM(server, directory, 5, vm);

	// Короткая страница (2 < 5) — данных больше нет: canLoadMore выключен,
	// и loadMore запроса не отправляет.
	server.Enqueue_Response(JSON_Response(200, Two_Exercise_Body()));
	QVERIFY(vm->reload());
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Loading(), false, 5000);

	QVERIFY(vm->Can_Load_More() == false);
	QVERIFY(vm->loadMore() == false);
	QCOMPARE(server.Request_Count(), 1);

	// Повторный reload с полной страницей включает пагинацию снова.
	server.Enqueue_Response(JSON_Response(200, Fifty_Exercise_Body()));
	QVERIFY(vm->reload());
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Loading(), false, 5000);

	QVERIFY(vm->Can_Load_More());
}
//----------------------------------------------------------------------------
void Avm_Exercises_Test::offlineShowsCacheWithMarker()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	Avm_Exercises *vm = 0;
	Make_VM(server, directory, 50, vm);

	// Кэш прогревается успехом.
	server.Enqueue_Response(JSON_Response(200, Two_Exercise_Body()));
	QVERIFY(vm->reload());
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Loading(), false, 5000);

	// Офлайн: репозиторий отдаёт кэш, VM поднимает fromCache и не показывает
	// ошибку — каталог читаем, пользователь видит плашку «сохранённый».
	server.Stop();

	QVERIFY(vm->reload());
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Loading(), false, 5000);

	QVERIFY(vm->Is_From_Cache());
	QVERIFY(vm->Error_Key().isEmpty());
	QCOMPARE(vm->exercise_list()->Row_Count(), 2);
}
//----------------------------------------------------------------------------

QTEST_MAIN(Avm_Exercises_Test)
#include "tst_Exercises_VM.moc"
