#include "Data/Users/Users_Repository.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSqlError>
#include <QSqlQuery>

Q_LOGGING_CATEGORY(USERS_Repository_Log, "trainingapp.users.repository")

//----------------------------------------------------------------------------
// AUsers_Repository
//----------------------------------------------------------------------------
const char *AUsers_Repository::Status_Saved_Locally = "saved_locally";
//----------------------------------------------------------------------------
AUsers_Repository::AUsers_Repository(AsAuth_Session *in_session, AsSql_Database *in_database, QObject *in_parent)
    : QObject(in_parent), Database_Instance(in_database), Session_Instance(in_session)
{
	// HTTP-адаптер users endpoint принадлежит репозиторию.
	Users_Api_Instance = new AUsers_Api(Session_Instance, this);

	connect(Users_Api_Instance, &AUsers_Api::getUserFinished, this, &AUsers_Repository::on_Get_User_Finished);
}
//----------------------------------------------------------------------------
int AUsers_Repository::Get_User(const QString &in_user_id, bool in_with_profile)
{
	int request_id;

	request_id = Users_Api_Instance->Get_User(in_user_id, in_with_profile);

	if (request_id == 0)
		return 0;

	Pending_Requests.insert(request_id, in_user_id);

	return request_id;
}
//----------------------------------------------------------------------------
bool AUsers_Repository::Get_Cached_User(const QString &in_user_id, SUsers_User &out_user)
{
	QSqlQuery query(Database_Instance->Sql_Connection());

	out_user = SUsers_User();

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return false;

	query.prepare("SELECT u.email, u.display_name, u.created_at, p.age, p.gender, p.height_cm, p.weight_kg "
	              "FROM user_cache u LEFT JOIN profile_cache p ON p.user_id = u.id WHERE u.id = ?");
	query.addBindValue(in_user_id);

	if (query.exec() == false || query.next() == false)
		return false;

	out_user.ID = in_user_id;
	out_user.Email = query.value(0).toString();
	out_user.Name = query.value(1).toString();
	out_user.Created_At = query.value(2).toString();

	if (query.isNull(3) == false)
	{
		out_user.Has_Profile = true;
		out_user.Profile.Age = query.value(3).toInt();
		out_user.Profile.Gender = query.value(4).toString();
		out_user.Profile.Height_Cm = query.value(5).toDouble();
		out_user.Profile.Weight_Kg = query.value(6).toDouble();
	}

	return true;
}
//----------------------------------------------------------------------------
bool AUsers_Repository::Load_Local_Profile(const QString &in_user_id, QString &out_level, QString &out_goal,
                                           int &out_workouts_per_week)
{
	QSqlQuery query(Database_Instance->Sql_Connection());

	out_level.clear();
	out_goal.clear();
	out_workouts_per_week = 0;

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return false;

	query.prepare("SELECT level, goal, workouts_per_week FROM profile_local WHERE user_id = ?");
	query.addBindValue(in_user_id);

	if (query.exec() == false || query.next() == false)
		return false;

	out_level = query.value(0).toString();
	out_goal = query.value(1).toString();
	out_workouts_per_week = query.value(2).toInt();

	return true;
}
//----------------------------------------------------------------------------
bool AUsers_Repository::Save_Local_Profile(const QString &in_user_id, const QString &in_level, const QString &in_goal,
                                           int in_workouts_per_week, QString &out_status)
{
	QJsonObject payload;
	QSqlQuery query(Database_Instance->Sql_Connection());

	out_status.clear();

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return false;

	// Изменения фиксируются локально; доставка на сервер — воркер issue 34.
	query.prepare("INSERT INTO profile_local(user_id, level, goal, workouts_per_week, updated_at) "
	              "VALUES(?, ?, ?, ?, ?) "
	              "ON CONFLICT(user_id) DO UPDATE SET level = excluded.level, goal = excluded.goal, "
	              "workouts_per_week = excluded.workouts_per_week, updated_at = excluded.updated_at");
	query.addBindValue(in_user_id);
	query.addBindValue(in_level);
	query.addBindValue(in_goal);
	query.addBindValue(in_workouts_per_week);
	query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

	if (query.exec() == false)
	{
		qCWarning(USERS_Repository_Log).noquote() << "profile_local persist failed";
		return false;
	}

	payload.insert("level", in_level);
	payload.insert("goal", in_goal);
	payload.insert("workouts_per_week", in_workouts_per_week);
	query.prepare("INSERT INTO sync_outbox(entity_type, entity_id, operation, payload_json, created_at, "
	              "attempt_count, next_attempt_at, last_error) "
	              "VALUES('profile_local', ?, 'update', ?, ?, 0, NULL, NULL)");
	query.addBindValue(in_user_id);
	query.addBindValue(QString::fromUtf8(QJsonDocument(payload).toJson(QJsonDocument::Compact)));
	query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

	if (query.exec() == false)
	{
		qCWarning(USERS_Repository_Log).noquote() << "sync_outbox persist failed";
		return false;
	}

	out_status = Status_Saved_Locally;
	qCInfo(USERS_Repository_Log).noquote() << "local profile saved";

	return true;
}
//----------------------------------------------------------------------------
int AUsers_Repository::Pending_Outbox_Count() const
{
	QSqlQuery query(Database_Instance->Sql_Connection());

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return 0;

	if (query.exec("SELECT COUNT(*) FROM sync_outbox") == false || query.next() == false)
		return 0;

	return query.value(0).toInt();
}
//----------------------------------------------------------------------------
void AUsers_Repository::on_Get_User_Finished(int in_request_id, const SUsers_User &in_user, const AApi_Error &in_error)
{
	SUsers_User user = in_user;
	AApi_Error error = in_error;
	QString user_id;
	bool from_cache = false;

	user_id = Pending_Requests.contains(in_request_id) ? Pending_Requests.take(in_request_id) : user.ID;

	if (error.Is_Error() == false)
	{
		if (Persist_User(user) == false)
		{
			// Сеть ответила успешно, но локальная база не приняла кэш.
			error = AApi_Error("undefined_error", "Local user cache persist failed", 0, 0);
		}
	}
	else if (Get_Cached_User(user_id, user))
	{
		// Офлайн: кэш с пометкой, ошибка передаётся вызывающему как маркер.
		from_cache = true;
	}

	emit getUserFinished(in_request_id, user, from_cache, error);
}
//----------------------------------------------------------------------------
bool AUsers_Repository::Persist_User(const SUsers_User &in_user)
{
	QSqlQuery query(Database_Instance->Sql_Connection());

	query.prepare("INSERT INTO user_cache(id, email, display_name, created_at, updated_at) "
	              "VALUES(?, ?, ?, ?, ?) "
	              "ON CONFLICT(id) DO UPDATE SET email = excluded.email, display_name = excluded.display_name, "
	              "created_at = excluded.created_at, updated_at = excluded.updated_at");
	query.addBindValue(in_user.ID);
	query.addBindValue(in_user.Email);
	query.addBindValue(in_user.Name);
	query.addBindValue(in_user.Created_At);
	query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

	if (query.exec() == false)
	{
		qCWarning(USERS_Repository_Log).noquote() << "user_cache persist failed";
		return false;
	}

	if (in_user.Has_Profile && Persist_Profile_Cache(in_user.ID, in_user.Profile) == false)
		return false;

	return true;
}
//----------------------------------------------------------------------------
bool AUsers_Repository::Persist_Profile_Cache(const QString &in_user_id, const SUsers_Profile &in_profile)
{
	QSqlQuery query(Database_Instance->Sql_Connection());

	query.prepare("INSERT INTO profile_cache(user_id, age, gender, height_cm, weight_kg, updated_at) "
	              "VALUES(?, ?, ?, ?, ?, ?) "
	              "ON CONFLICT(user_id) DO UPDATE SET age = excluded.age, gender = excluded.gender, "
	              "height_cm = excluded.height_cm, weight_kg = excluded.weight_kg, "
	              "updated_at = excluded.updated_at");
	query.addBindValue(in_user_id);
	query.addBindValue(in_profile.Age < 0 ? QVariant() : QVariant(in_profile.Age));
	query.addBindValue(in_profile.Gender);
	query.addBindValue(in_profile.Height_Cm);
	query.addBindValue(in_profile.Weight_Kg);
	query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

	if (query.exec() == false)
	{
		qCWarning(USERS_Repository_Log).noquote() << "profile_cache persist failed";
		return false;
	}

	return true;
}
//----------------------------------------------------------------------------
