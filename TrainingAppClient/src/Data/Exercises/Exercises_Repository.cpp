#include "Data/Exercises/Exercises_Repository.h"

#include <QDateTime>
#include <QLoggingCategory>
#include <QSqlError>
#include <QSqlQuery>

Q_LOGGING_CATEGORY(EXERCISES_Repository_Log, "trainingapp.exercises.repository")

//----------------------------------------------------------------------------
AExercises_Repository::AExercises_Repository(AsHttp_Client *in_http_client, AsAuth_Session *in_session,
                                             AsSql_Database *in_database, QObject *in_parent)
    : QObject(in_parent), Database_Instance(in_database), Session_Instance(in_session)
{
	// HTTP-адаптер exercises endpoint принадлежит репозиторию.
	Exercises_Api_Instance = new AExercises_Api(in_http_client, in_session, this);

	connect(Exercises_Api_Instance, &AExercises_Api::listFinished, this, &AExercises_Repository::on_List_Finished);
	connect(Exercises_Api_Instance, &AExercises_Api::muscleGroupsFinished, this,
	        &AExercises_Repository::on_Muscle_Groups_Finished);
	connect(Exercises_Api_Instance, &AExercises_Api::equipmentsFinished, this,
	        &AExercises_Repository::on_Equipments_Finished);
	connect(Exercises_Api_Instance, &AExercises_Api::createFinished, this, &AExercises_Repository::on_Create_Finished);
	connect(Exercises_Api_Instance, &AExercises_Api::updateFinished, this, &AExercises_Repository::on_Update_Finished);
	connect(Exercises_Api_Instance, &AExercises_Api::deleteFinished, this, &AExercises_Repository::on_Delete_Finished);
	connect(Exercises_Api_Instance, &AExercises_Api::listFinished, this,
	        &AExercises_Repository::on_Public_List_Finished);
}
//----------------------------------------------------------------------------
QString AExercises_Repository::Last_Error() const
{
	return Last_Error_Text;
}
//----------------------------------------------------------------------------
QString AExercises_Repository::Joined_List(const QStringList &in_values)
{
	// Пустой список join() отдаёт null-QString: bind null упирается в
	// NOT NULL DEFAULT '' — DEFAULT спасает только опущенную колонку.
	return in_values.isEmpty() ? QString("") : in_values.join(",");
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Is_Transport_Error(const AApi_Error &in_error)
{
	// Транспортный сбой не имеет HTTP-статуса (docs/03, §6: transport-сбой
	// сессию не рвёт) — по нему и отличаем офлайн от ответа сервера.
	return in_error.Is_Error() && in_error.HTTP_Status_Code() == 0;
}
//----------------------------------------------------------------------------
int AExercises_Repository::Load_List(const QString &in_type, const QStringList &in_muscle_groups,
                                     const QStringList &in_equipments, const QString &in_search, int in_limit,
                                     int in_offset)
{
	Last_Error_Text.clear();

	int request_id =
	    Exercises_Api_Instance->Get_List(in_type, in_muscle_groups, in_equipments, in_search, in_limit, in_offset);

	if (request_id == 0)
	{
		Last_Error_Text = Exercises_Api_Instance->Last_Error();
		return 0;
	}

	Pending_Calls.insert(request_id, SPending_Call());

	return request_id;
}
//----------------------------------------------------------------------------
int AExercises_Repository::Load_Muscle_Groups()
{
	Last_Error_Text.clear();

	int request_id = Exercises_Api_Instance->Get_Muscle_Groups();

	if (request_id == 0)
	{
		Last_Error_Text = Exercises_Api_Instance->Last_Error();
		return 0;
	}

	Pending_Calls.insert(request_id, SPending_Call());

	return request_id;
}
//----------------------------------------------------------------------------
int AExercises_Repository::Load_Equipments()
{
	Last_Error_Text.clear();

	int request_id = Exercises_Api_Instance->Get_Equipments();

	if (request_id == 0)
	{
		Last_Error_Text = Exercises_Api_Instance->Last_Error();
		return 0;
	}

	Pending_Calls.insert(request_id, SPending_Call());

	return request_id;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Load_Cached_List(QVector<SExercises_Item> &out_items)
{
	QSqlQuery query(Database_Instance->Sql_Connection());

	out_items.clear();

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return false;

	query.prepare("SELECT id, name, description, owner_id, is_own, muscle_groups, equipment FROM exercises_cache "
	              "ORDER BY name");

	if (query.exec() == false)
	{
		qCWarning(EXERCISES_Repository_Log).noquote() << "cached list read failed";
		return false;
	}

	while (query.next())
	{
		SExercises_Item item;

		item.ID = query.value(0).toString();
		item.Name = query.value(1).toString();
		item.Description = query.value(2).toString();
		item.Owner_ID = query.value(3).toString();
		item.Has_Owner = item.Owner_ID.isEmpty() == false;
		item.Muscle_Groups = query.value(5).toString().split(',', Qt::SkipEmptyParts);
		item.Equipment = query.value(6).toString().split(',', Qt::SkipEmptyParts);

		out_items.append(item);
	}

	return true;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Load_Cached_Muscle_Groups(QStringList &out_groups)
{
	QSqlQuery query(Database_Instance->Sql_Connection());

	out_groups.clear();

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return false;

	if (query.exec("SELECT name FROM muscle_groups_ref ORDER BY position") == false)
	{
		qCWarning(EXERCISES_Repository_Log).noquote() << "cached muscle groups read failed";
		return false;
	}

	while (query.next())
		out_groups.append(query.value(0).toString());

	return true;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Load_Cached_Equipments(QStringList &out_equipments)
{
	QSqlQuery query(Database_Instance->Sql_Connection());

	out_equipments.clear();

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return false;

	if (query.exec("SELECT name FROM equipments_ref ORDER BY position") == false)
	{
		qCWarning(EXERCISES_Repository_Log).noquote() << "cached equipments read failed";
		return false;
	}

	while (query.next())
		out_equipments.append(query.value(0).toString());

	return true;
}
//----------------------------------------------------------------------------
int AExercises_Repository::Load_Public_List()
{
	Last_Error_Text.clear();

	int request_id = Exercises_Api_Instance->Get_List("public", QStringList(), QStringList(), QString(), 1000, 0);

	if (request_id == 0)
	{
		Last_Error_Text = Exercises_Api_Instance->Last_Error();
		return 0;
	}

	SPending_Call pending;

	pending.Kind = Kind_Public_List;
	Pending_Calls.insert(request_id, pending);

	return request_id;
}
//----------------------------------------------------------------------------
int AExercises_Repository::Create_Exercise(const QString &in_name, bool in_is_public, const QString &in_description,
                                           const QStringList &in_muscle_groups, const QStringList &in_equipments)
{
	Last_Error_Text.clear();

	int request_id =
	    Exercises_Api_Instance->Create_Exercise(in_name, in_is_public, in_description, in_muscle_groups, in_equipments);

	if (request_id == 0)
	{
		Last_Error_Text = Exercises_Api_Instance->Last_Error();
		return 0;
	}

	SPending_Call pending;

	pending.Kind = Kind_List;
	Pending_Calls.insert(request_id, pending);

	return request_id;
}
//----------------------------------------------------------------------------
int AExercises_Repository::Update_Exercise(const QString &in_id, const QString &in_name, bool in_is_public,
                                           const QString &in_description, const QStringList &in_muscle_groups,
                                           const QStringList &in_equipments)
{
	Last_Error_Text.clear();

	int request_id = Exercises_Api_Instance->Update_Exercise(in_id, in_name, in_is_public, in_description,
	                                                         in_muscle_groups, in_equipments);

	if (request_id == 0)
	{
		Last_Error_Text = Exercises_Api_Instance->Last_Error();
		return 0;
	}

	SPending_Call pending;

	pending.Kind = Kind_List;
	Pending_Calls.insert(request_id, pending);

	return request_id;
}
//----------------------------------------------------------------------------
int AExercises_Repository::Delete_Exercise(const QString &in_id)
{
	Last_Error_Text.clear();

	int request_id = Exercises_Api_Instance->Delete_Exercise(in_id);

	if (request_id == 0)
	{
		Last_Error_Text = Exercises_Api_Instance->Last_Error();
		return 0;
	}

	SPending_Call pending;

	pending.Kind = Kind_List;
	Pending_Calls.insert(request_id, pending);

	return request_id;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Save_Catalogs(const QStringList &in_groups, const QStringList &in_equipments)
{
	return Persist_String_List("muscle_groups_ref", in_groups) && Persist_String_List("equipments_ref", in_equipments);
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Delete_Cached(const QString &in_exercise_id)
{
	QSqlQuery query(Database_Instance->Sql_Connection());

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return false;

	// Сервер удаляет мягко (строка исчезает из списков), кэш — жёстко:
	// удалённое не должно вернуться ни в одном офлайн-списке.
	query.prepare("DELETE FROM exercises_cache WHERE id = ?");
	query.addBindValue(in_exercise_id);

	if (query.exec() == false)
	{
		qCWarning(EXERCISES_Repository_Log).noquote() << "cache delete failed";
		return false;
	}

	return true;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Is_Cached_Own(const QString &in_exercise_id)
{
	QSqlQuery query(Database_Instance->Sql_Connection());

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return false;

	query.prepare("SELECT is_own FROM exercises_cache WHERE id = ?");
	query.addBindValue(in_exercise_id);

	if (query.exec() == false || query.next() == false)
		return false;

	return query.value(0).toInt() != 0;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Persist_List(const QVector<SExercises_Item> &in_items)
{
	QSqlQuery query(Database_Instance->Sql_Connection());
	QString user_id;

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return false;

	user_id = Session_Instance != 0 ? Session_Instance->User_ID() : QString();

	for (int i = 0; i < in_items.size(); ++i)
	{
		const SExercises_Item &item = in_items.at(i);
		bool is_own = item.Has_Owner && item.Owner_ID == user_id;

		query.prepare("INSERT INTO exercises_cache(id, name, description, owner_id, is_own, muscle_groups, "
		              "equipment, updated_at) VALUES(?, ?, ?, ?, ?, ?, ?, ?) "
		              "ON CONFLICT(id) DO UPDATE SET name = excluded.name, description = excluded.description, "
		              "owner_id = excluded.owner_id, is_own = excluded.is_own, muscle_groups = excluded.muscle_groups, "
		              "equipment = excluded.equipment, updated_at = excluded.updated_at");
		query.addBindValue(item.ID);
		query.addBindValue(item.Name);
		query.addBindValue(item.Description);
		query.addBindValue(item.Owner_ID);
		query.addBindValue(is_own ? 1 : 0);
		query.addBindValue(Joined_List(item.Muscle_Groups));
		query.addBindValue(Joined_List(item.Equipment));
		query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

		if (query.exec() == false)
		{
			qCWarning(EXERCISES_Repository_Log).noquote()
			    << "exercises_cache persist failed:" << query.lastError().text();
			return false;
		}

		Mirror_Local_Exercise(item);
	}

	return true;
}
//----------------------------------------------------------------------------
// Зеркало кэша в локальный каталог exercises (схема v2): FK
// program_day_exercises.exercise_id → exercises.id требует строки с
// серверными uuid, иначе выбор упражнения из каталога в дне программы
// отвергается FK. INSERT OR IGNORE: повторное зеркалирование и локальный
// каталог безопасны.
void AExercises_Repository::Mirror_Local_Exercise(const SExercises_Item &in_item)
{
	QSqlQuery query(Database_Instance->Sql_Connection());

	query.prepare("INSERT OR IGNORE INTO exercises(id, title, subtitle, muscle_group, equipment, is_custom, "
	              "description, created_at) VALUES(?, ?, '', ?, ?, 1, ?, ?)");
	query.addBindValue(in_item.ID);
	query.addBindValue(in_item.Name);
	query.addBindValue(in_item.Muscle_Groups.isEmpty() ? QString("other") : in_item.Muscle_Groups.first());
	query.addBindValue(Joined_List(in_item.Equipment));
	query.addBindValue(in_item.Description);
	query.addBindValue(in_item.Created_At);

	if (query.exec() == false)
		qCWarning(EXERCISES_Repository_Log).noquote() << "local exercise mirror failed:" << query.lastError().text();
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Persist_String_List(const char *in_table, const QStringList &in_values)
{
	QSqlQuery query(Database_Instance->Sql_Connection());

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return false;

	// Справочники малы и не версионируются (docs/03, §3.10): перезапись
	// целиком проще upsert'а и не оставляет исчезнувших значений. Имена
	// таблиц — литералы вызывающего кода, не пользовательский ввод.
	if (query.exec(QString("DELETE FROM %1").arg(QLatin1String(in_table))) == false)
	{
		qCWarning(EXERCISES_Repository_Log).noquote() << "catalog clear failed";
		return false;
	}

	for (int i = 0; i < in_values.size(); ++i)
	{
		query.prepare(QString("INSERT INTO %1(name, position) VALUES(?, ?)").arg(QLatin1String(in_table)));
		query.addBindValue(in_values.at(i));
		query.addBindValue(i);

		if (query.exec() == false)
		{
			qCWarning(EXERCISES_Repository_Log).noquote() << "catalog persist failed";
			return false;
		}
	}

	return true;
}
//----------------------------------------------------------------------------
void AExercises_Repository::on_List_Finished(int in_request_id, const QVector<SExercises_Item> &in_items,
                                             const AApi_Error &in_error)
{
	// Сигнал общий с on_Public_List_Finished: чужой род — не наш запрос,
	// и Pending-запись трогать нельзя (её take() сделает соседний слот).
	if (Pending_Calls.contains(in_request_id) == false || Pending_Calls.value(in_request_id).Kind != Kind_List)
		return;

	Pending_Calls.take(in_request_id);

	// Transport-сбой (офлайн) — не ошибка для вызывающего: отдаём кэш с
	// маркером from_cache. HTTP-ответ сервера — истина: успех persist'ит,
	// отказ уходит наружу ошибкой.
	if (Is_Transport_Error(in_error))
	{
		QVector<SExercises_Item> cached;

		qCInfo(EXERCISES_Repository_Log).noquote() << "list offline, serving cache" << in_request_id;

		if (Load_Cached_List(cached))
		{
			emit listFinished(in_request_id, cached, true, AApi_Error());
			return;
		}
	}
	else if (in_error.Is_Error() == false && Persist_List(in_items))
	{
		emit listFinished(in_request_id, in_items, false, in_error);
		return;
	}

	emit listFinished(in_request_id, in_items, false, in_error);
}
//----------------------------------------------------------------------------
void AExercises_Repository::on_Muscle_Groups_Finished(int in_request_id, const QStringList &in_groups,
                                                      const AApi_Error &in_error)
{
	if (Pending_Calls.contains(in_request_id) == false)
		return;

	Pending_Calls.take(in_request_id);

	if (Is_Transport_Error(in_error))
	{
		QStringList cached;

		qCInfo(EXERCISES_Repository_Log).noquote() << "muscle-groups offline, serving cache" << in_request_id;

		if (Load_Cached_Muscle_Groups(cached))
		{
			emit muscleGroupsFinished(in_request_id, cached, true, AApi_Error());
			return;
		}
	}
	else if (in_error.Is_Error() == false && Persist_String_List("muscle_groups_ref", in_groups))
	{
		emit muscleGroupsFinished(in_request_id, in_groups, false, in_error);
		return;
	}

	emit muscleGroupsFinished(in_request_id, in_groups, false, in_error);
}
//----------------------------------------------------------------------------
void AExercises_Repository::on_Equipments_Finished(int in_request_id, const QStringList &in_equipments,
                                                   const AApi_Error &in_error)
{
	if (Pending_Calls.contains(in_request_id) == false)
		return;

	Pending_Calls.take(in_request_id);

	if (Is_Transport_Error(in_error))
	{
		QStringList cached;

		qCInfo(EXERCISES_Repository_Log).noquote() << "equipments offline, serving cache" << in_request_id;

		if (Load_Cached_Equipments(cached))
		{
			emit equipmentsFinished(in_request_id, cached, true, AApi_Error());
			return;
		}
	}
	else if (in_error.Is_Error() == false && Persist_String_List("equipments_ref", in_equipments))
	{
		emit equipmentsFinished(in_request_id, in_equipments, false, in_error);
		return;
	}

	emit equipmentsFinished(in_request_id, in_equipments, false, in_error);
}
//----------------------------------------------------------------------------
void AExercises_Repository::on_Create_Finished(int in_request_id, const SExercises_Item &in_item,
                                               const AApi_Error &in_error)
{
	Q_UNUSED(in_request_id);

	// Созданное упражнение сразу живёт в кэше: офлайн-каталог его видит
	// до первой перезагрузки.
	if (in_error.Is_Error() == false && Persist_List(QVector<SExercises_Item>{in_item}) == false)
		qCWarning(EXERCISES_Repository_Log).noquote() << "created exercise cache persist failed";

	emit createFinished(in_request_id, in_item, in_error);
}
//----------------------------------------------------------------------------
void AExercises_Repository::on_Update_Finished(int in_request_id, const SExercises_Item &in_item,
                                               const AApi_Error &in_error)
{
	Q_UNUSED(in_request_id);

	if (in_error.Is_Error() == false && Persist_List(QVector<SExercises_Item>{in_item}) == false)
		qCWarning(EXERCISES_Repository_Log).noquote() << "updated exercise cache persist failed";

	emit updateFinished(in_request_id, in_item, in_error);
}
//----------------------------------------------------------------------------
void AExercises_Repository::on_Delete_Finished(int in_request_id, const AApi_Error &in_error)
{
	Q_UNUSED(in_request_id);

	if (in_error.Is_Error())
	{
		emit deleteFinished(in_request_id, in_error);
		return;
	}

	// id удаляемого неизвестен в ответе (200 без тела) — кэш чистит сам VM,
	// который знает, что удалял. Здесь ничего не читаем.
	emit deleteFinished(in_request_id, in_error);
}
//----------------------------------------------------------------------------
void AExercises_Repository::on_Public_List_Finished(int in_request_id, const QVector<SExercises_Item> &in_items,
                                                    const AApi_Error &in_error)
{
	// Один сигнал listFinished обслуживает два потребителя: каталог (Kind_List)
	// и is_public-проверку редактора (Kind_Public_List). Чужой род — пропуск.
	if (Pending_Calls.contains(in_request_id) == false || Pending_Calls.value(in_request_id).Kind != Kind_Public_List)
		return;

	Pending_Calls.take(in_request_id);

	bool from_cache = false;
	AApi_Error error = in_error;

	if (Is_Transport_Error(in_error))
	{
		from_cache = true;
		error = AApi_Error();
	}
	else if (in_error.Is_Error() == false)
	{
		if (Persist_List(in_items))
			from_cache = false;
	}

	emit publicListFinished(in_request_id, in_items, from_cache, error);
}
//----------------------------------------------------------------------------