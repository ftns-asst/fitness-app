#include "Data/Exercises/Exercises_Repository.h"

#include <QDateTime>
#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QLoggingCategory>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QStandardPaths>
#include <QTimer>
#include <QUrl>
#include <QUuid>

#include <algorithm>

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
	connect(Exercises_Api_Instance, &AExercises_Api::oneFinished, this, &AExercises_Repository::on_One_Finished);
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
QSqlDatabase AExercises_Repository::Connection() const
{
	if (Database_Instance != 0)
		return Database_Instance->Sql_Connection();

	return QSqlDatabase();
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
	QSqlQuery query(Connection());

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
bool AExercises_Repository::Load_Cached_One(const QString &in_exercise_id, SExercises_Item &out_item)
{
	QSqlQuery query(Connection());

	out_item = SExercises_Item();

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false || in_exercise_id.isEmpty())
		return false;

	query.prepare("SELECT id, name, description, owner_id, muscle_groups, equipment FROM exercises_cache "
	              "WHERE id = ?");
	query.addBindValue(in_exercise_id);

	if (query.exec() == false || query.next() == false)
		return false;

	out_item.ID = query.value(0).toString();
	out_item.Name = query.value(1).toString();
	out_item.Description = query.value(2).toString();
	out_item.Owner_ID = query.value(3).toString();
	out_item.Has_Owner = out_item.Owner_ID.isEmpty() == false;
	out_item.Muscle_Groups = query.value(4).toString().split(',', Qt::SkipEmptyParts);
	out_item.Equipment = query.value(5).toString().split(',', Qt::SkipEmptyParts);

	return true;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Load_Local_Catalog(QVector<SExercises_Item> &out_items)
{
	QSqlQuery query(Connection());

	out_items.clear();

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return false;

	// Локальный каталог — зеркало FK дня программы: он содержит и базовые
	// примитивы, и серверные строки, и свои упражнения. muscle_group здесь
	// одно значение (колонка зеркала), equipment — список через запятую.
	if (query.exec("SELECT id, title, muscle_group, equipment FROM exercises ORDER BY title") == false)
	{
		qCWarning(EXERCISES_Repository_Log).noquote() << "local catalog read failed";
		return false;
	}

	while (query.next())
	{
		SExercises_Item item;

		item.ID = query.value(0).toString();
		item.Name = query.value(1).toString();
		item.Description = QString("");
		item.Owner_ID = QString("");
		item.Has_Owner = false;
		item.Muscle_Groups = query.value(2).toString().split(',', Qt::SkipEmptyParts);
		item.Equipment = query.value(3).toString().split(',', Qt::SkipEmptyParts);

		if (item.ID.isEmpty() || item.Name.isEmpty())
			continue;

		out_items.append(item);
	}

	return true;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Load_Offline_List(QVector<SExercises_Item> &out_items)
{
	QVector<SExercises_Item> cached;
	QVector<SExercises_Item> local;
	QSet<QString> seen_ids;

	out_items.clear();

	// Кэш первым: у него полные описания и владелец, и он источник истины
	// для серверных строк. Локальный каталог дописывает то, чего в кэше нет, —
	// базовые примитивы и упражнения, уже стоящие в программах.
	if (Load_Cached_List(cached) == false)
		return false;

	for (int i = 0; i < cached.size(); ++i)
	{
		out_items.append(cached.at(i));
		seen_ids.insert(cached.at(i).ID);
	}

	if (Load_Local_Catalog(local))
	{
		for (int i = 0; i < local.size(); ++i)
		{
			if (seen_ids.contains(local.at(i).ID))
				continue;

			out_items.append(local.at(i));
			seen_ids.insert(local.at(i).ID);
		}
	}

	// Единый порядок объединённого списка — по названию: кэш и локальный
	// каталог сортируются независимо, и без этого строки шли бы двумя блоками
	// (серверные, затем локальные), что в списке выглядело бы случайным.
	std::sort(out_items.begin(), out_items.end(), [](const SExercises_Item &in_left, const SExercises_Item &in_right)
	          { return QString::localeAwareCompare(in_left.Name, in_right.Name) < 0; });

	return true;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Is_Own_Owner(const QString &in_owner_id)
{
	const QString user_id = Session_Instance != 0 ? Session_Instance->User_ID() : QString();

	return in_owner_id.isEmpty() == false && user_id.isEmpty() == false && in_owner_id == user_id;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Has_Session() const
{
	return Session_Instance != 0 && Session_Instance->Has_Session();
}
//----------------------------------------------------------------------------
QString AExercises_Repository::Local_Owner_Id()
{
	return QStringLiteral("local");
}
//----------------------------------------------------------------------------
// Локальное сохранение своего упражнения. Сервер недоступен для мутаций без
// сессии (приложение без входа) и при transport-сбое — раньше форма в обоих
// случаях отвечала ошибкой, и своё упражнение создать было нельзя вовсе.
// Теперь строка пишется в кэш (is_own = 1) и в зеркало exercises (FK дня
// программы), поэтому сразу видна в каталоге и окне выбора.
bool AExercises_Repository::Save_Local_Exercise(QString &io_exercise_id, const QString &in_name,
                                                const QString &in_description, const QStringList &in_muscle_groups,
                                                const QStringList &in_equipments)
{
	if (Database_Instance == 0 || Database_Instance->Is_Open() == false || in_name.trimmed().isEmpty())
		return false;

	// Пустое описание из формы — null-QString, а колонка NOT NULL.
	const QString description = in_description.isNull() ? QString("") : in_description;
	const bool is_new = io_exercise_id.isEmpty();
	const QString user_id = Session_Instance != 0 ? Session_Instance->User_ID() : QString();
	const QString owner_id = user_id.isEmpty() ? Local_Owner_Id() : user_id;
	const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

	if (is_new)
		io_exercise_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

	QSqlQuery query(Connection());

	query.prepare("INSERT INTO exercises_cache(id, name, description, owner_id, is_own, muscle_groups, equipment, "
	              "updated_at) VALUES(?, ?, ?, ?, 1, ?, ?, ?) "
	              "ON CONFLICT(id) DO UPDATE SET name = excluded.name, description = excluded.description, "
	              "muscle_groups = excluded.muscle_groups, equipment = excluded.equipment, "
	              "updated_at = excluded.updated_at");
	query.addBindValue(io_exercise_id);
	query.addBindValue(in_name.trimmed());
	query.addBindValue(description);
	query.addBindValue(owner_id);
	query.addBindValue(Joined_List(in_muscle_groups));
	query.addBindValue(Joined_List(in_equipments));
	query.addBindValue(now);

	if (query.exec() == false)
	{
		qCWarning(EXERCISES_Repository_Log).noquote() << "local exercise save failed:" << query.lastError().text();

		if (is_new)
			io_exercise_id.clear();

		return false;
	}

	SExercises_Item item;

	item.ID = io_exercise_id;
	item.Name = in_name.trimmed();
	item.Description = description;
	item.Muscle_Groups = in_muscle_groups;
	item.Equipment = in_equipments;
	item.Created_At = now;
	Mirror_Local_Exercise(item);

	// Правка уже зеркалированной строки: INSERT OR IGNORE её не меняет.
	QSqlQuery update(Connection());

	update.prepare("UPDATE exercises SET title = ?, muscle_group = ?, equipment = ?, description = ? WHERE id = ?");
	update.addBindValue(item.Name);
	update.addBindValue(item.Muscle_Groups.isEmpty() ? QString("other") : item.Muscle_Groups.first());
	update.addBindValue(Joined_List(item.Equipment));
	update.addBindValue(item.Description);
	update.addBindValue(io_exercise_id);
	update.exec();

	qCInfo(EXERCISES_Repository_Log).noquote() << "local exercise saved" << io_exercise_id;

	return true;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Delete_Local_Exercise(const QString &in_exercise_id)
{
	if (Database_Instance == 0 || Database_Instance->Is_Open() == false || in_exercise_id.isEmpty())
		return false;

	Delete_Cached(in_exercise_id);

	// Зеркало удаляется, пока на него не ссылается день программы (FK):
	// при отказе строка остаётся — упражнение стоит в программе.
	QSqlQuery query(Connection());

	query.prepare("DELETE FROM exercises WHERE id = ? AND is_custom = 1");
	query.addBindValue(in_exercise_id);

	if (query.exec() == false)
		qCInfo(EXERCISES_Repository_Log).noquote() << "local mirror kept (in use)" << in_exercise_id;

	return true;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Load_Cached_First_Id(QString &out_exercise_id)
{
	QSqlQuery query(Connection());

	out_exercise_id.clear();

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return false;

	if (query.exec("SELECT id FROM exercises_cache ORDER BY name LIMIT 1") == false || query.next() == false)
		return false;

	out_exercise_id = query.value(0).toString();

	return out_exercise_id.isEmpty() == false;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Load_Cached_Muscle_Groups(QStringList &out_groups)
{
	QSqlQuery query(Connection());

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
	QSqlQuery query(Connection());

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
int AExercises_Repository::Load_One(const QString &in_id)
{
	Last_Error_Text.clear();

	int request_id = Exercises_Api_Instance->Get_One(in_id);

	if (request_id == 0)
	{
		Last_Error_Text = Exercises_Api_Instance->Last_Error();
		return 0;
	}

	SPending_Call pending;

	pending.Kind = Kind_One;
	pending.Exercise_Id = in_id.trimmed();
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
// Базовые группы мышц офлайн-каталога. Ключи — из словаря ExerciseTexts
// (qml/Presentation/ExerciseTexts.qml): неизвестный ключ показался бы сырым,
// а здесь весь набор переводим.
QStringList AExercises_Repository::Base_Muscle_Groups()
{
	return QStringList({"chest", "upper_back", "lats", "quads", "hamstrings", "glutes", "front_delts", "side_delts",
	                    "rear_delts", "biceps", "triceps", "abs", "calves", "cardio"});
}
//----------------------------------------------------------------------------
QStringList AExercises_Repository::Base_Equipments()
{
	return QStringList({"bodyweight", "barbell", "dumbbell", "bench", "pull_up_bar", "cable_machine", "kettlebell",
	                    "mat", "treadmill"});
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Ensure_Offline_Base()
{
	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return false;

	// Сначала чтение, и запись только в пустой кэш: сервер — источник истины,
	// и примитивы не должны подмешиваться к серверному каталогу после первой
	// успешной загрузки.
	QSqlQuery probe(Connection());

	if (probe.exec("SELECT COUNT(*) FROM exercises_cache") == false)
	{
		qCWarning(EXERCISES_Repository_Log).noquote() << "base catalog probe failed";
		return false;
	}

	if (probe.next() && probe.value(0).toInt() > 0)
		return true;

	// Справочники раньше упражнений, но только пустые: непустые могли быть
	// прогреты с сервера, и засев затирал бы серверные значения фильтров.
	QSqlQuery groups_probe(Connection());

	if (groups_probe.exec("SELECT COUNT(*) FROM muscle_groups_ref") && groups_probe.next() &&
	    groups_probe.value(0).toInt() == 0)
		Persist_String_List("muscle_groups_ref", Base_Muscle_Groups());

	QSqlQuery equipments_probe(Connection());

	if (equipments_probe.exec("SELECT COUNT(*) FROM equipments_ref") && equipments_probe.next() &&
	    equipments_probe.value(0).toInt() == 0)
		Persist_String_List("equipments_ref", Base_Equipments());

	const QVector<SExercises_Item> base = Base_Exercises();

	// Зеркало раньше кэша: Mirror_Base_Exercise пишет is_custom = 0, а
	// INSERT OR IGNORE Persist_List (своё — is_custom = 1) тогда уже не
	// перепишет базовую строку.
	Mirror_Base_Catalog();

	if (Persist_List(base) == false)
	{
		qCWarning(EXERCISES_Repository_Log).noquote() << "base catalog persist failed";
		return false;
	}

	return true;
}
//----------------------------------------------------------------------------
void AExercises_Repository::Mirror_Base_Catalog()
{
	const QVector<SExercises_Item> base = Base_Exercises();

	for (int i = 0; i < base.size(); ++i)
		Mirror_Base_Exercise(base.at(i));
}
//----------------------------------------------------------------------------
void AExercises_Repository::Mirror_Base_Exercise(const SExercises_Item &in_item)
{
	QSqlQuery query(Connection());

	query.prepare("INSERT OR IGNORE INTO exercises(id, title, subtitle, muscle_group, equipment, is_custom, "
	              "description, created_at) VALUES(?, ?, '', ?, ?, 0, ?, ?)");
	query.addBindValue(in_item.ID);
	query.addBindValue(in_item.Name);
	query.addBindValue(in_item.Muscle_Groups.isEmpty() ? QString("chest") : in_item.Muscle_Groups.first());
	query.addBindValue(Joined_List(in_item.Equipment));
	query.addBindValue(in_item.Description);
	query.addBindValue(in_item.Created_At);

	if (query.exec() == false)
		qCWarning(EXERCISES_Repository_Log).noquote() << "base exercise mirror failed:" << query.lastError().text();
}
//----------------------------------------------------------------------------
QVector<SExercises_Item> AExercises_Repository::Base_Exercises()
{
	// Идентификаторы — «builtin-…»: строка любого вида допустима (локальный
	// каталог сидится так же — ex_squat и т.п.). Данные живут в C++, а не
	// в QML: кэш и FK дня программы — слой данных.
	struct SBase_Row
	{
		const char *Id;
		const char *Name;
		const char *Groups;
		const char *Equipment;
	};

	const SBase_Row rows[] = {
	    {"builtin-squat", "Приседания со штангой", "quads,glutes", "barbell"},
	    {"builtin-bench-press", "Жим штанги лёжа", "chest,front_delts,triceps", "barbell,bench"},
	    {"builtin-deadlift", "Становая тяга", "hamstrings,lower_back,glutes", "barbell"},
	    {"builtin-bent-over-row", "Тяга штанги в наклоне", "upper_back,lats", "barbell"},
	    {"builtin-pull-up", "Подтягивания", "lats,upper_back,biceps", "pull_up_bar"},
	    {"builtin-push-up", "Отжимания", "chest,triceps", "bodyweight"},
	    {"builtin-dumbbell-bench-press", "Жим гантелей лёжа", "chest,front_delts", "dumbbell,bench"},
	    {"builtin-one-arm-dumbbell-row", "Тяга гантели одной рукой", "upper_back,lats", "dumbbell,bench"},
	    {"builtin-seated-dumbbell-press", "Жим гантелей сидя", "front_delts,side_delts", "dumbbell,bench"},
	    {"builtin-lateral-raise", "Махи гантелями в стороны", "side_delts", "dumbbell"},
	    {"builtin-rear-delt-fly", "Обратные разведения", "rear_delts", "dumbbell,bench"},
	    {"builtin-lat-pulldown", "Тяга верхнего блока", "lats,upper_back,biceps", "cable_machine"},
	    {"builtin-dips", "Отжимания на брусьях", "chest,triceps", "bodyweight"},
	    {"builtin-biceps-curl", "Подъём штанги на бицепс", "biceps", "barbell"},
	    {"builtin-overhead-triceps-extension", "Французский жим", "triceps", "dumbbell"},
	    {"builtin-goblet-squat", "Гоблет-присед с гирей", "quads,glutes", "kettlebell"},
	    {"builtin-lunge", "Выпады с гантелями", "quads,glutes", "dumbbell"},
	    {"builtin-plank", "Планка", "abs", "mat"},
	    {"builtin-crunch", "Скручивания", "abs", "mat"},
	    {"builtin-calf-raise", "Подъёмы на носки", "calves", "bodyweight"},
	    {"builtin-running", "Бег", "cardio", "treadmill"}};

	const int row_count = static_cast<int>(sizeof(rows) / sizeof(rows[0]));
	QVector<SExercises_Item> items;

	for (int i = 0; i < row_count; ++i)
	{
		SExercises_Item item;

		item.ID = QString::fromLatin1(rows[i].Id);
		item.Name = QString::fromUtf8(rows[i].Name);
		// description в кэше NOT NULL: пустая строка, а не null-QString.
		item.Description = QString("");
		// owner_id в кэше тоже NOT NULL: у базовых упражнений владельца нет —
		// пустая строка (Has_Owner остаётся false), а не null-QString.
		item.Owner_ID = QString("");
		item.Has_Owner = false;
		item.Muscle_Groups = QString::fromUtf8(rows[i].Groups).split(',', Qt::SkipEmptyParts);
		item.Equipment = QString::fromUtf8(rows[i].Equipment).split(',', Qt::SkipEmptyParts);
		item.Created_At = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

		items.append(item);
	}

	return items;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Delete_Cached(const QString &in_exercise_id)
{
	QSqlQuery query(Connection());

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
	QSqlQuery query(Connection());

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
	QSqlQuery query(Connection());
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
	QSqlQuery query(Connection());

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
	QSqlDatabase connection = Connection();
	QSqlQuery query(connection);

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return false;

	// Справочники малы и не версионируются (docs/03, §3.10): перезапись
	// целиком проще upsert'а и не оставляет исчезнувших значений. Имена
	// таблиц — литералы вызывающего кода, не пользовательский ввод.
	// Очистка и вставка — одна транзакция: сбой посередине не оставляет
	// справочник пустым или обрезанным.
	if (connection.transaction() == false)
	{
		qCWarning(EXERCISES_Repository_Log).noquote() << "catalog transaction failed";
		return false;
	}

	if (query.exec(QString("DELETE FROM %1").arg(QLatin1String(in_table))) == false)
	{
		qCWarning(EXERCISES_Repository_Log).noquote() << "catalog clear failed";
		goto _error_exit;
	}

	for (int i = 0; i < in_values.size(); ++i)
	{
		query.prepare(QString("INSERT INTO %1(name, position) VALUES(?, ?)").arg(QLatin1String(in_table)));
		query.addBindValue(in_values.at(i));
		query.addBindValue(i);

		if (query.exec() == false)
		{
			qCWarning(EXERCISES_Repository_Log).noquote() << "catalog persist failed";
			goto _error_exit;
		}
	}

	if (connection.commit())
		return true;

	qCWarning(EXERCISES_Repository_Log).noquote() << "catalog commit failed";

_error_exit:
	connection.rollback();

	return false;
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

		// Пустой кэш при первом запуске без сети: вместо «не из чего выбрать»
		// засевается базовый каталог (только если кэш пуст).
		Ensure_Offline_Base();

		// Офлайн-список — кэш плюс локальный каталог: упражнение, стоящее в дне
		// программы, обязано находиться в окне выбора и без сети.
		if (Load_Offline_List(cached))
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

		// Пустые справочники при первом запуске без сети: фильтры каталога
		// обязаны показывать хотя бы базовый набор групп.
		Ensure_Offline_Base();

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

		// Пустые справочники при первом запуске без сети: то же правило,
		// что и для групп мышц.
		Ensure_Offline_Base();

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
void AExercises_Repository::on_One_Finished(int in_request_id, const SExercises_Item &in_item,
                                            const AApi_Error &in_error)
{
	// Чужой род — не наш запрос: список слушает те же api-сигналы, но Kind
	// Pending_Calls различает потребителей (тот же приём, что у Kind_Public_List).
	if (Pending_Calls.contains(in_request_id) == false || Pending_Calls.value(in_request_id).Kind != Kind_One)
		return;

	const QString exercise_id = Pending_Calls.take(in_request_id).Exercise_Id;

	// Transport-сбой (офлайн) — не ошибка для вызывающего: детальная карточка
	// обязана открываться без сети (docs/08, §4 U6). Отдаём кэш с маркером
	// from_cache; строки нет — честный not_found.
	if (Is_Transport_Error(in_error))
	{
		SExercises_Item cached;

		qCInfo(EXERCISES_Repository_Log).noquote() << "one offline, serving cache" << in_request_id;

		if (Load_Cached_One(exercise_id, cached))
		{
			emit oneFinished(in_request_id, exercise_id, cached, true, AApi_Error());
			return;
		}

		emit oneFinished(in_request_id, exercise_id, SExercises_Item(), true,
		                 AApi_Error("not_found", "Exercise is not cached", 0, 0));
		return;
	}

	// Успех — истина сервера: кэш обновляется (и зеркало FK — тоже: freshly
	// загруженное упражнение можно сразу добавить в день программы).
	if (in_error.Is_Error() == false && in_item.ID.isEmpty() == false)
	{
		if (Persist_List(QVector<SExercises_Item>{in_item}))
			qCInfo(EXERCISES_Repository_Log).noquote() << "one persisted" << in_item.ID;
	}

	emit oneFinished(in_request_id, exercise_id, in_item, false, in_error);
}
//----------------------------------------------------------------------------
// — Локальные фото техники (схема v9) —
//----------------------------------------------------------------------------
QString AExercises_Repository::Photos_Directory()
{
	// App data — стандартный writable-каталог приложения; пустой (консольные
	// тесты без QGuiApplication) — текущий каталог.
	QString base = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);

	if (base.isEmpty())
		base = QDir::currentPath();

	return base + QLatin1String("/images/exercises");
}
//----------------------------------------------------------------------------
void AExercises_Repository::Clear_Staged_Photos()
{
	const QDir directory(Photos_Directory());
	const QStringList staged = directory.entryList(QStringList() << QLatin1String("staging.*"), QDir::Files);

	for (int i = 0; i < staged.size(); ++i)
		QFile::remove(directory.filePath(staged.at(i)));
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Stage_Photo(const QUrl &in_source, QString &out_staged_file, QString &out_error)
{
	out_error.clear();
	out_staged_file.clear();

	if (in_source.isEmpty() || in_source.isValid() == false)
	{
		out_error = QLatin1String("photo_failed");

		return false;
	}

	// file:// открывается по локальному пути, content:// (Android) — как есть:
	// QFile понимает content-URI собственным файловым движком платформы.
	const QString source_path = in_source.isLocalFile() ? in_source.toLocalFile() : in_source.toString();
	QFile source(source_path);

	if (source.open(QIODevice::ReadOnly) == false)
	{
		qCWarning(EXERCISES_Repository_Log).nospace() << "photo stage: unreadable " << source_path;
		out_error = QLatin1String("photo_failed");

		return false;
	}

	const QByteArray data = source.readAll();

	source.close();

	if (data.isEmpty())
	{
		out_error = QLatin1String("photo_failed");

		return false;
	}

	// Формат — по суффиксу файла; выбор без суффикса (камера, content-URI) —
	// jpg: QImage читает данные по содержимому, суффикс влияет только на имя.
	QString suffix = QFileInfo(source_path).suffix().toLower();

	if (suffix != QLatin1String("jpg") && suffix != QLatin1String("jpeg") && suffix != QLatin1String("png") &&
	    suffix != QLatin1String("webp") && suffix != QLatin1String("gif"))
		suffix = QLatin1String("jpg");

	if (QDir().mkpath(Photos_Directory()) == false)
	{
		out_error = QLatin1String("photo_failed");

		return false;
	}

	// Стейджинг единственный: новое фото отменяет прежнее незакоммиченное,
	// иначе отмена формы оставляла бы за собой мусор.
	Clear_Staged_Photos();

	const QString staged = Photos_Directory() + QLatin1String("/staging.") + suffix;
	QFile target(staged);

	if (target.open(QIODevice::WriteOnly) == false || target.write(data) != data.size())
	{
		target.close();
		QFile::remove(staged);
		out_error = QLatin1String("photo_failed");

		return false;
	}

	target.close();

	out_staged_file = staged;

	return true;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Photo_File_Name(const QString &in_exercise_id, QString &out_file_name)
{
	out_file_name.clear();

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return false;

	QSqlQuery query(Connection());

	query.prepare("SELECT file_name FROM exercise_photos WHERE exercise_id = ?");
	query.addBindValue(in_exercise_id);

	if (query.exec() == false || query.next() == false)
		return false;

	out_file_name = query.value(0).toString();

	return out_file_name.isEmpty() == false;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Commit_Photo(const QString &in_exercise_id, const QString &in_staged_file)
{
	if (Database_Instance == 0 || Database_Instance->Is_Open() == false || in_exercise_id.isEmpty() ||
	    QFile::exists(in_staged_file) == false)
		return false;

	const QString suffix = QFileInfo(in_staged_file).suffix().toLower();
	const QString file_name = in_exercise_id + QLatin1Char('.') + suffix;
	const QString target = Photos_Directory() + QLatin1Char('/') + file_name;

	// Прежнее фото (возможно, другого формата) не остаётся мусором.
	QString previous_name;

	if (Photo_File_Name(in_exercise_id, previous_name) && previous_name != file_name)
		QFile::remove(Photos_Directory() + QLatin1Char('/') + previous_name);

	// rename поверх существующего файла на Windows молча отказывается —
	// файл с тем же именем убирается явно (повторная загрузка того же фото).
	QFile::remove(target);

	if (QFile::rename(in_staged_file, target) == false)
	{
		qCWarning(EXERCISES_Repository_Log).nospace() << "photo commit: rename failed " << in_staged_file;

		return false;
	}

	QSqlQuery query(Connection());

	query.prepare("INSERT INTO exercise_photos (exercise_id, file_name) VALUES (?, ?) "
	              "ON CONFLICT(exercise_id) DO UPDATE SET file_name = excluded.file_name");
	query.addBindValue(in_exercise_id);
	query.addBindValue(file_name);

	if (query.exec() == false)
	{
		qCWarning(EXERCISES_Repository_Log).nospace() << "photo commit: " << query.lastError().text();

		return false;
	}

	return true;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Photo_File(const QString &in_exercise_id, QString &out_file)
{
	out_file.clear();

	QString file_name;

	if (Photo_File_Name(in_exercise_id, file_name) == false)
		return false;

	const QString path = Photos_Directory() + QLatin1Char('/') + file_name;

	if (QFile::exists(path) == false)
		return false;

	out_file = path;

	return true;
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Delete_Photo(const QString &in_exercise_id)
{
	QString file_name;

	if (Photo_File_Name(in_exercise_id, file_name))
		QFile::remove(Photos_Directory() + QLatin1Char('/') + file_name);

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return false;

	QSqlQuery query(Connection());

	query.prepare("DELETE FROM exercise_photos WHERE exercise_id = ?");
	query.addBindValue(in_exercise_id);

	return query.exec();
}
//----------------------------------------------------------------------------
bool AExercises_Repository::Cancel_Staged_Photo(const QString &in_staged_file)
{
	// Убирается только файл внутри каталога фото: произвольный путь (баг
	// привязки или подмена) не удаляется.
	if (in_staged_file.isEmpty() || in_staged_file.startsWith(Photos_Directory()) == false)
		return false;

	return QFile::remove(in_staged_file);
}