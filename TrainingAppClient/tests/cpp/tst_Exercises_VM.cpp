#include "Data/Auth/Auth_Session.h"
#include "Data/Auth/Token_Store.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Exercises/Exercises_Repository.h"
#include "Data/Network/Http_Client.h"
#include "Presentation/Exercises_VM.h"
#include "Support/Test_Http_Server.h"

#include <QQmlComponent>
#include <QQmlContext>
#include <QQmlEngine>
#include <QQuickItem>
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
	void paginationPreservesRowsAndPosition();
	void unchangedReloadPreservesLoadedPages();
	void offlinePaginationReplacesAndFiltersSnapshot();
	void gridPositionSurvivesPaginationAndReload();
	void failedPaginationKeepsRowsAndRetriesOffset();
	void stalePageCannotOverwriteNewSelection();
	void cachedDepthSurvivesInitialRefresh_data();
	void cachedDepthSurvivesInitialRefresh();

private:
	static STest_Http_Response JSON_Response(int in_status, const QByteArray &in_body);
	static QByteArray Two_Exercise_Body();
	static QByteArray Fifty_Exercise_Body();
	static QByteArray Catalog_Page(int in_offset, int in_count);
	// page_size — размер страницы пагинации (продакшен 50, тесты меньше).
	void Make_VM(ATest_Http_Server &in_server, QTemporaryDir &out_dir, int in_page_size, Avm_Exercises *&out_vm);
};

//-------------------------------------------------------------------------------------------------------------
STest_Http_Response Avm_Exercises_Test::JSON_Response(int in_status, const QByteArray &in_body)
{
	STest_Http_Response response;

	response.Status_Code = in_status;
	response.Body = in_body;

	return response;
}
//-------------------------------------------------------------------------------------------------------------
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
//-------------------------------------------------------------------------------------------------------------
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
//-------------------------------------------------------------------------------------------------------------
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
//-------------------------------------------------------------------------------------------------------------
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
	QVERIFY(!vm->Is_From_Cache());
	QVERIFY(vm->Error_Key().isEmpty());
	QVERIFY(!vm->Can_Load_More());

	// Роли — mock-контракт CatalogExerciseRow: title/subtitle (docs/08, §4 U3).
	row = vm->exercise_list()->get(0);
	QCOMPARE(row.value("title").toString(), QString("Bench press"));
	QCOMPARE(row.value("subtitle").toString(), QString("chest"));

	// Роли списка включают id и muscleGroups: окно выбора в дне программы
	// (ExercisePickerPopup) объявляет их required-свойствами делегата, и без
	// роли в roleNames строка не создаётся — список выглядел бы пустым.
	const QHash<int, QByteArray> roles = vm->exercise_list()->roleNames();

	QVERIFY(roles.values().contains("id"));
	QVERIFY(roles.values().contains("title"));
	QVERIFY(roles.values().contains("subtitle"));
	QVERIFY(roles.values().contains("muscleGroups"));

	QCOMPARE(row.value("id").toString(), QString("aaaaaaaa-1111-2222-3333-444444444444"));
	QCOMPARE(row.value("muscleGroups").toStringList(), QStringList({"chest"}));

	// Свойства-справочники для чипов заполняются отдельными запросами.
	// Два запроса параллельны: очередь ответов нельзя привязывать к порядку отправки.
	connect(&server, &ATest_Http_Server::requestReceived, &server,
	        [&server]()
	        {
		        const QByteArray target = server.Request_At(server.Request_Count() - 1).Target;
		        server.Enqueue_Response(JSON_Response(200, target.contains("/muscle-groups")
		                                                       ? QByteArray("[\"chest\",\"biceps\"]")
		                                                       : QByteArray("[\"barbell\"]")));
	        });
	vm->loadCatalogs();

	QTRY_COMPARE_WITH_TIMEOUT(vm->Muscle_Groups(), QStringList({"chest", "biceps"}), 5000);
	QTRY_COMPARE_WITH_TIMEOUT(vm->Equipments(), QStringList({"barbell"}), 5000);
}
//-------------------------------------------------------------------------------------------------------------
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
//-------------------------------------------------------------------------------------------------------------
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
//-------------------------------------------------------------------------------------------------------------
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

	QVERIFY(!vm->Can_Load_More());
	QVERIFY(!vm->loadMore());
	QCOMPARE(server.Request_Count(), 1);

	// Повторный reload с полной страницей включает пагинацию снова.
	server.Enqueue_Response(JSON_Response(200, Fifty_Exercise_Body()));
	QVERIFY(vm->reload());
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Loading(), false, 5000);

	QVERIFY(vm->Can_Load_More());
}
//-------------------------------------------------------------------------------------------------------------
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
	// Список — кэш плюс локальный каталог: упражнения, которыми пользуются
	// программы, остаются выбираемыми и без сети.
	server.Stop();

	QVERIFY(vm->reload());
	QTRY_COMPARE_WITH_TIMEOUT(vm->Is_Loading(), false, 5000);

	QVERIFY(vm->Is_From_Cache());
	QVERIFY(vm->Error_Key().isEmpty());
	QVERIFY(vm->exercise_list()->Row_Count() >= 2);

	QVector<QVariantMap> rows;
	bool has_local_catalog_row = false;

	for (int i = 0; i < vm->exercise_list()->Row_Count(); ++i)
	{
		const QVariantMap row = vm->exercise_list()->get(i);

		if (row.value("id").toString() == QString("ex_squat"))
			has_local_catalog_row = true;
	}

	QVERIFY(has_local_catalog_row);
}
//-------------------------------------------------------------------------------------------------------------

void Avm_Exercises_Test::paginationPreservesRowsAndPosition()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	Avm_Exercises *vm = 0;
	Make_VM(server, directory, 2, vm);
	server.Enqueue_Response(JSON_Response(200, Two_Exercise_Body()));
	QVERIFY(vm->reload());
	QTRY_VERIFY(!vm->Is_Loading());
	AList_Model *model = vm->exercise_list();
	QPersistentModelIndex anchor(model->index(1));
	QSignalSpy resets(model, &AList_Model::modelReset);
	QSignalSpy inserts(model, &AList_Model::rowsInserted);
	QSignalSpy counts(model, &AList_Model::countChanged);
	bool reentrant_load = false;
	connect(model, &AList_Model::rowsInserted, vm, [vm, &reentrant_load]() { reentrant_load = vm->loadMore(); });
	const QByteArray more = Two_Exercise_Body().replace("aaaaaaaa", "cccccccc").replace("bbbbbbbb", "dddddddd");
	server.Enqueue_Response(JSON_Response(200, more));
	QVERIFY(vm->loadMore());
	QTRY_VERIFY(!vm->Is_Loading());

	QCOMPARE(resets.count(), 0);
	QCOMPARE(model->Row_Count(), 4);
	QCOMPARE(inserts.count(), 1);
	QCOMPARE(inserts.at(0).at(1).toInt(), 2);
	QCOMPARE(inserts.at(0).at(2).toInt(), 3);
	QCOMPARE(counts.count(), 1);
	QVERIFY(!reentrant_load);
	QVERIFY(anchor.isValid());
	QCOMPARE(anchor.row(), 1);
	QCOMPARE(anchor.data(Qt::UserRole + 1).toString(), QString("bbbbbbbb-1111-2222-3333-444444444444"));

	server.Enqueue_Response(JSON_Response(200, QByteArray("[]")));
	QVERIFY(vm->loadMore());
	QTRY_VERIFY(!vm->Is_Loading());
	QCOMPARE(resets.count(), 0);
	QCOMPARE(inserts.count(), 1);
	QCOMPARE(counts.count(), 1);
	QVERIFY(!vm->Can_Load_More());
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises_Test::unchangedReloadPreservesLoadedPages()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	Avm_Exercises *vm = 0;
	Make_VM(server, directory, 2, vm);
	const QByteArray more = Two_Exercise_Body().replace("aaaaaaaa", "cccccccc").replace("bbbbbbbb", "dddddddd");
	server.Enqueue_Response(JSON_Response(200, Two_Exercise_Body()));
	QVERIFY(vm->reload());
	QTRY_VERIFY(!vm->Is_Loading());
	server.Enqueue_Response(JSON_Response(200, more));
	QVERIFY(vm->loadMore());
	QTRY_VERIFY(!vm->Is_Loading());
	AList_Model *model = vm->exercise_list();
	QPersistentModelIndex anchor(model->index(3));
	QSignalSpy resets(model, &AList_Model::modelReset);
	QSignalSpy counts(model, &AList_Model::countChanged);
	QSignalSpy changes(model, &AList_Model::dataChanged);

	server.Enqueue_Response(JSON_Response(200, Two_Exercise_Body()));
	server.Enqueue_Response(JSON_Response(200, more));
	QVERIFY(vm->reload());
	QCOMPARE(resets.count(), 0);
	QCOMPARE(model->Row_Count(), 4);
	QTRY_VERIFY(!vm->Is_Loading());
	QCOMPARE(server.Request_Count(), 4);
	QVERIFY(server.Request_At(3).Target.contains("offset=2"));
	QCOMPARE(model->Row_Count(), 4);
	QCOMPARE(resets.count(), 0);
	QCOMPARE(counts.count(), 0);
	QCOMPARE(changes.count(), 0);
	QVERIFY(anchor.isValid());
	QCOMPARE(anchor.row(), 3);
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises_Test::offlinePaginationReplacesAndFiltersSnapshot()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	Avm_Exercises *vm = 0;
	Make_VM(server, directory, 2, vm);
	server.Enqueue_Response(JSON_Response(200, Two_Exercise_Body()));
	QVERIFY(vm->setMuscleGroups({"chest"}));
	QTRY_VERIFY(!vm->Is_Loading());
	server.Stop();
	QVERIFY(vm->loadMore());
	QTRY_VERIFY(!vm->Is_Loading());

	QVERIFY(vm->Is_From_Cache());
	QVERIFY(!vm->Can_Load_More());
	QStringList ids;
	for (int i = 0; i < vm->exercise_list()->Row_Count(); ++i)
	{
		const QVariantMap row = vm->exercise_list()->get(i);
		QVERIFY(row.value("muscleGroups").toStringList().contains("chest"));
		QVERIFY(!ids.contains(row.value("id").toString()));
		ids.append(row.value("id").toString());
	}
	QVERIFY(ids.contains("aaaaaaaa-1111-2222-3333-444444444444"));
	QSignalSpy resets(vm->exercise_list(), &AList_Model::modelReset);
	QSignalSpy counts(vm->exercise_list(), &AList_Model::countChanged);
	QVERIFY(vm->reload());
	QTRY_VERIFY(!vm->Is_Loading());
	QCOMPARE(resets.count(), 0);
	QCOMPARE(counts.count(), 0);
}

void Avm_Exercises_Test::gridPositionSurvivesPaginationAndReload()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	Avm_Exercises *vm = 0;
	Make_VM(server, directory, 50, vm);
	server.Enqueue_Response(JSON_Response(200, Fifty_Exercise_Body()));
	QVERIFY(vm->reload());
	QTRY_VERIFY(!vm->Is_Loading());

	QQmlEngine engine;
	engine.rootContext()->setContextProperty("catalogModel", vm->exercise_list());
	QQmlComponent component(&engine);
	component.setData("import QtQuick\n"
	                  "GridView { width: 200; height: 100; cellWidth: 100; cellHeight: 50;"
	                  "model: catalogModel; delegate: Rectangle { width: 100; height: 50 } }",
	                  QUrl());
	QObject *grid = component.create();
	QVERIFY2(grid != 0, qPrintable(component.errorString()));
	grid->setParent(&engine);
	QVERIFY(QMetaObject::invokeMethod(grid, "forceLayout"));
	grid->setProperty("contentY", 650.0);
	const double position = grid->property("contentY").toDouble();
	QCOMPARE(position, 650.0);

	const QByteArray more = Fifty_Exercise_Body().replace("\"10", "\"20");
	server.Enqueue_Response(JSON_Response(200, more));
	QVERIFY(vm->loadMore());
	QTRY_VERIFY(!vm->Is_Loading());
	QVERIFY(QMetaObject::invokeMethod(grid, "forceLayout"));
	QCOMPARE(grid->property("count").toInt(), 100);
	QCOMPARE(grid->property("contentY").toDouble(), position);

	server.Enqueue_Response(JSON_Response(200, Fifty_Exercise_Body()));
	server.Enqueue_Response(JSON_Response(200, more));
	QVERIFY(vm->reload());
	QCOMPARE(grid->property("contentY").toDouble(), position);
	QTRY_VERIFY(!vm->Is_Loading());
	QVERIFY(QMetaObject::invokeMethod(grid, "forceLayout"));
	QCOMPARE(grid->property("count").toInt(), 100);
	QCOMPARE(grid->property("contentY").toDouble(), position);
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises_Test::failedPaginationKeepsRowsAndRetriesOffset()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	Avm_Exercises *vm = 0;
	Make_VM(server, directory, 2, vm);
	server.Enqueue_Response(JSON_Response(200, Two_Exercise_Body()));
	QVERIFY(vm->reload());
	QTRY_VERIFY(!vm->Is_Loading());
	QSignalSpy resets(vm->exercise_list(), &AList_Model::modelReset);
	server.Enqueue_Response(JSON_Response(500, "{\"key\":\"undefined_error\"}"));
	QVERIFY(vm->loadMore());
	QTRY_VERIFY(!vm->Is_Loading());
	QVERIFY(!vm->Error_Key().isEmpty());
	QCOMPARE(vm->exercise_list()->Row_Count(), 2);
	QCOMPARE(resets.count(), 0);
	server.Enqueue_Response(JSON_Response(200, "[]"));
	QVERIFY(vm->loadMore());
	QTRY_VERIFY(!vm->Is_Loading());
	QVERIFY(server.Request_At(2).Target.contains("offset=2"));
	QCOMPARE(vm->exercise_list()->Row_Count(), 2);
	QVERIFY(vm->Error_Key().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises_Test::stalePageCannotOverwriteNewSelection()
{
	ATest_Http_Server server;
	QTemporaryDir directory;
	Avm_Exercises *vm = 0;
	Make_VM(server, directory, 2, vm);
	server.Enqueue_Response(JSON_Response(200, Two_Exercise_Body()));
	QVERIFY(vm->reload());
	QTRY_VERIFY(!vm->Is_Loading());
	STest_Http_Response delayed = JSON_Response(200, Two_Exercise_Body());
	delayed.Delay_Ms = 200;
	server.Enqueue_Response(delayed);
	QVERIFY(vm->loadMore());
	QTRY_COMPARE(server.Request_Count(), 2);
	server.Enqueue_Response(JSON_Response(200, "[]"));
	QVERIFY(vm->setMuscleGroups({"missing-group"}));
	QCOMPARE(vm->exercise_list()->Row_Count(), 0);
	QTRY_VERIFY(!vm->Is_Loading());
	QTest::qWait(250);
	QCOMPARE(vm->exercise_list()->Row_Count(), 0);
	QVERIFY(!vm->Can_Load_More());
}

QByteArray Avm_Exercises_Test::Catalog_Page(int in_offset, int in_count)
{
	QByteArray body("[");
	for (int i = in_offset; i < in_offset + in_count; ++i)
	{
		if (i > in_offset)
			body += ',';
		body += QString("{\"id\":\"scroll-%1\",\"name\":\"Scroll %1\",\"muscle_groups\":[\"scroll-test\"],"
		                "\"equipment\":[],\"created_at\":\"2026-10-01T10:00:00Z\"}")
		            .arg(i, 4, 10, QChar('0'))
		            .toUtf8();
	}
	return body + ']';
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises_Test::cachedDepthSurvivesInitialRefresh_data()
{
	QTest::addColumn<bool>("fail_first");
	QTest::newRow("initial-refresh") << false;
	QTest::newRow("retry-after-cache-error") << true;
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises_Test::cachedDepthSurvivesInitialRefresh()
{
	QFETCH(bool, fail_first);
	ATest_Http_Server server;
	QTemporaryDir directory;
	Avm_Exercises *seed_vm = 0;
	Make_VM(server, directory, 50, seed_vm);
	server.Enqueue_Response(JSON_Response(200, Catalog_Page(0, 200)));
	QVERIFY(seed_vm->reload());
	QTRY_VERIFY(!seed_vm->Is_Loading());
	AExercises_Repository *repository = server.findChild<AExercises_Repository *>();
	QVERIFY(repository != 0);
	Avm_Exercises vm(repository, 50);
	if (fail_first)
		server.Enqueue_Response(JSON_Response(500, "{\"key\":\"undefined_error\"}"));
	for (int i = 0; i < 4; ++i)
	{
		STest_Http_Response response = JSON_Response(200, Catalog_Page(i * 50, 50));
		response.Delay_Ms = 50;
		server.Enqueue_Response(response);
	}
	QVERIFY(vm.setMuscleGroups({"scroll-test"}));
	AList_Model *model = vm.exercise_list();
	QCOMPARE(model->Row_Count(), 200);
	QVERIFY(vm.Is_From_Cache());
	QPersistentModelIndex anchor(model->index(150));
	QSignalSpy resets(model, &AList_Model::modelReset);
	QSignalSpy removes(model, &AList_Model::rowsRemoved);
	if (fail_first)
	{
		QTRY_VERIFY(!vm.Is_Loading());
		QVERIFY(!vm.Error_Key().isEmpty());
		QCOMPARE(model->Row_Count(), 200);
		QVERIFY(vm.Is_From_Cache());
		QCOMPARE(resets.count(), 0);
		QVERIFY(anchor.isValid());
		QVERIFY(vm.loadMore());
	}
	QTRY_VERIFY(!vm.Is_Loading());
	QCOMPARE(model->Row_Count(), 200);
	QCOMPARE(server.Request_Count(), fail_first ? 6 : 5);
	QCOMPARE(resets.count(), 0);
	QCOMPARE(removes.count(), 0);
	QVERIFY(anchor.isValid());
	QCOMPARE(anchor.row(), 150);
	QVERIFY(vm.Error_Key().isEmpty());
	QVERIFY(!vm.Is_From_Cache());
}

QTEST_MAIN(Avm_Exercises_Test)
#include "tst_Exercises_VM.moc"
