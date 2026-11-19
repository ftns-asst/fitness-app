#include "Data/Users/Users_Repository.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QSqlError>
#include <QSqlQuery>

Q_LOGGING_CATEGORY(USERS_Repository_Log, "trainingapp.users.repository")

//-------------------------------------------------------------------------------------------------------------
// AUsers_Repository
//-------------------------------------------------------------------------------------------------------------
const char *AUsers_Repository::Status_Saved_Locally = "saved_locally";
//-------------------------------------------------------------------------------------------------------------
AUsers_Repository::AUsers_Repository(AsAuth_Session *in_session, AsSql_Database *in_database, QObject *in_parent)
    : QObject(in_parent), Database_Instance(in_database), Session_Instance(in_session)
{
	// HTTP-адаптер users endpoint принадлежит репозиторию.
	Users_Api_Instance = new AUsers_Api(Session_Instance, this);

	connect(Users_Api_Instance, &AUsers_Api::getUserFinished, this, &AUsers_Repository::on_Get_User_Finished);
	connect(Users_Api_Instance, &AUsers_Api::profileUpdated, this, &AUsers_Repository::on_Profile_Updated);
}
//-------------------------------------------------------------------------------------------------------------
int AUsers_Repository::Get_User(const QString &in_user_id, bool in_with_profile)
{
	int request_id;

	request_id = Users_Api_Instance->Get_User(in_user_id, in_with_profile);

	if (request_id == 0)
		return 0;

	Pending_Requests.insert(request_id, in_user_id);
	Pending_Read_Generations.insert(request_id, Profile_Generations.value(in_user_id));

	return request_id;
}
//-------------------------------------------------------------------------------------------------------------
int AUsers_Repository::Update_Profile(const QString &in_user_id, const QJsonObject &in_changes)
{
	if (Session_Instance == 0 || in_user_id.isEmpty() || Session_Instance->User_ID() != in_user_id)
		return 0;

	int request_id = Users_Api_Instance->Update_Profile(in_changes);
	if (request_id > 0)
		Pending_Updates.insert(request_id, in_user_id);

	return request_id;
}
//-------------------------------------------------------------------------------------------------------------
bool AUsers_Repository::Save_Profile_Locally(const QString &in_user_id, const QJsonObject &in_changes)
{
	SUsers_User user;
	SProfile_Pending pending;
	QJsonObject merged;
	QSqlDatabase connection = Connection();
	QSqlQuery query(connection);

	if (in_changes.isEmpty() || Session_Instance == 0 || Session_Instance->User_ID() != in_user_id ||
	    !Get_Cached_User(in_user_id, user) || !connection.transaction())
		return false;

	if (Load_Pending_Profile(in_user_id, pending))
		merged = pending.Changes;
	for (auto iterator = in_changes.begin(); iterator != in_changes.end(); ++iterator)
		merged.insert(iterator.key(), iterator.value());
	Apply_Profile_Changes(user.Profile, in_changes);
	if (!Persist_Profile_Cache(in_user_id, user.Profile))
	{
		connection.rollback();
		return false;
	}
	query.prepare("DELETE FROM sync_outbox WHERE entity_type = 'user_profile' AND entity_id = ?");
	query.addBindValue(in_user_id);
	if (!query.exec())
	{
		connection.rollback();
		return false;
	}
	QJsonObject envelope;
	envelope.insert("changes", merged);
	envelope.insert("blocked", false);
	query.prepare("INSERT INTO sync_outbox(entity_type, entity_id, operation, payload_json, created_at) "
	              "VALUES('user_profile', ?, 'update', ?, ?)");
	query.addBindValue(in_user_id);
	query.addBindValue(QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact)));
	query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODateWithMs));
	if (!query.exec() || !connection.commit())
	{
		connection.rollback();
		return false;
	}
	++Profile_Generations[in_user_id];
	emit profileChanged(in_user_id);
	emit profileQueued(in_user_id);

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AUsers_Repository::Load_Pending_Profile(const QString &in_user_id, SProfile_Pending &out_pending)
{
	QSqlQuery query(Connection());
	query.prepare("SELECT id, payload_json, attempt_count, next_attempt_at, last_error FROM sync_outbox "
	              "WHERE entity_type = 'user_profile' AND entity_id = ? ORDER BY id LIMIT 1");
	query.addBindValue(in_user_id);
	out_pending = SProfile_Pending();
	if (!query.exec() || !query.next())
		return false;

	QJsonObject envelope = QJsonDocument::fromJson(query.value(1).toByteArray()).object();
	out_pending.ID = query.value(0).toLongLong();
	out_pending.Changes = envelope.value("changes").toObject();
	out_pending.Is_Blocked = envelope.value("blocked").toBool();
	out_pending.Attempts = query.value(2).toInt();
	out_pending.Next_Attempt = QDateTime::fromString(query.value(3).toString(), Qt::ISODateWithMs);
	out_pending.Last_Error = query.value(4).toString();

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AUsers_Repository::Confirm_Profile_Update(const QString &in_user_id, qint64 in_through_id,
                                               const SUsers_Profile &in_profile)
{
	QSqlDatabase connection = Connection();
	QSqlQuery query(connection);
	SProfile_Pending pending;
	SUsers_Profile profile = in_profile;
	if (!connection.transaction())
		return false;

	query.prepare("DELETE FROM sync_outbox WHERE entity_type = 'user_profile' AND entity_id = ? AND id <= ?");
	query.addBindValue(in_user_id);
	query.addBindValue(in_through_id);
	if (!query.exec())
	{
		connection.rollback();
		return false;
	}
	if (Load_Pending_Profile(in_user_id, pending))
		Apply_Profile_Changes(profile, pending.Changes);
	if (!Persist_Profile_Cache(in_user_id, profile) || !connection.commit())
	{
		connection.rollback();
		return false;
	}
	++Profile_Generations[in_user_id];
	emit profileChanged(in_user_id);

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AUsers_Repository::Defer_Profile_Update(const QString &in_user_id, qint64 in_id, const AApi_Error &in_error,
                                             bool in_retryable, int in_delay_ms)
{
	SProfile_Pending pending;
	QSqlQuery query(Connection());
	if (!Load_Pending_Profile(in_user_id, pending) || pending.ID != in_id)
		return true;

	QJsonObject envelope;
	envelope.insert("changes", pending.Changes);
	envelope.insert("blocked", !in_retryable);
	query.prepare(
	    "UPDATE sync_outbox SET payload_json = ?, attempt_count = attempt_count + 1, "
	    "next_attempt_at = ?, last_error = ? WHERE id = ? AND entity_type = 'user_profile' AND entity_id = ?");
	query.addBindValue(QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact)));
	query.addBindValue(in_retryable ? QDateTime::currentDateTimeUtc().addMSecs(in_delay_ms).toString(Qt::ISODateWithMs)
	                                : QString(""));
	query.addBindValue(in_error.Key());
	query.addBindValue(in_id);
	query.addBindValue(in_user_id);

	return query.exec();
}
//-------------------------------------------------------------------------------------------------------------
bool AUsers_Repository::Reset_Profile_Retry(const QString &in_user_id)
{
	SProfile_Pending pending;
	QSqlQuery query(Connection());
	if (!Load_Pending_Profile(in_user_id, pending))
		return true;

	QJsonObject envelope;
	envelope.insert("changes", pending.Changes);
	envelope.insert("blocked", false);
	query.prepare("UPDATE sync_outbox SET payload_json = ?, attempt_count = 0, next_attempt_at = NULL, "
	              "last_error = NULL WHERE id = ? AND entity_type = 'user_profile' AND entity_id = ?");
	query.addBindValue(QString::fromUtf8(QJsonDocument(envelope).toJson(QJsonDocument::Compact)));
	query.addBindValue(pending.ID);
	query.addBindValue(in_user_id);

	return query.exec();
}
//-------------------------------------------------------------------------------------------------------------
bool AUsers_Repository::Get_Cached_User(const QString &in_user_id, SUsers_User &out_user)
{
	QSqlQuery query(Connection());

	out_user = SUsers_User();

	if (Database_Instance == 0 || !Database_Instance->Is_Open())
		return false;

	query.prepare("SELECT u.email, u.display_name, u.created_at, p.age, p.gender, p.height_cm, p.weight_kg, p.user_id "
	              "FROM user_cache u LEFT JOIN profile_cache p ON p.user_id = u.id WHERE u.id = ?");
	query.addBindValue(in_user_id);

	if (!query.exec() || !query.next())
		return false;

	out_user.ID = in_user_id;
	out_user.Email = query.value(0).toString();
	out_user.Name = query.value(1).toString();
	out_user.Created_At = query.value(2).toString();

	if (!query.isNull(7))
	{
		out_user.Has_Profile = true;
		out_user.Profile.Age = query.isNull(3) ? -1 : query.value(3).toInt();
		out_user.Profile.Gender = query.value(4).toString();
		out_user.Profile.Height_Cm = query.value(5).toDouble();
		out_user.Profile.Weight_Kg = query.value(6).toDouble();
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AUsers_Repository::Load_Local_Profile(const QString &in_user_id, QString &out_level, QString &out_goal,
                                           int &out_workouts_per_week)
{
	QSqlQuery query(Connection());

	out_level.clear();
	out_goal.clear();
	out_workouts_per_week = 0;

	if (Database_Instance == 0 || !Database_Instance->Is_Open())
		return false;

	query.prepare("SELECT level, goal, workouts_per_week FROM profile_local WHERE user_id = ?");
	query.addBindValue(in_user_id);

	if (!query.exec() || !query.next())
		return false;

	out_level = query.value(0).toString();
	out_goal = query.value(1).toString();
	out_workouts_per_week = query.value(2).toInt();

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AUsers_Repository::Save_Local_Profile(const QString &in_user_id, const QString &in_level, const QString &in_goal,
                                           int in_workouts_per_week, QString &out_status)
{
	QJsonObject payload;
	QSqlQuery query(Connection());

	out_status.clear();

	if (Database_Instance == 0 || !Database_Instance->Is_Open())
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

	if (!query.exec())
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

	if (!query.exec())
	{
		qCWarning(USERS_Repository_Log).noquote() << "sync_outbox persist failed";
		return false;
	}

	out_status = Status_Saved_Locally;
	qCInfo(USERS_Repository_Log).noquote() << "local profile saved";

	return true;
}
//-------------------------------------------------------------------------------------------------------------
// Параметры авторасчёта КБЖУ (спринт S7): дата рождения и уровень активности.
// Запись идёт отдельным upsert, чтобы не трогать уровень и цель: их вызывает
// редактор программы, и общий upsert затирал бы их пустыми значениями при
// заполнении только этих двух полей.
//-------------------------------------------------------------------------------------------------------------
bool AUsers_Repository::Load_Macro_Params(const QString &in_user_id, QString &out_birth_date,
                                          QString &out_activity_level)
{
	QSqlQuery query(Connection());

	out_birth_date.clear();
	out_activity_level.clear();

	if (Database_Instance == 0 || !Database_Instance->Is_Open())
		return false;

	query.prepare("SELECT birth_date, activity_level FROM profile_local WHERE user_id = ?");
	query.addBindValue(in_user_id);

	// Отсутствие строки — не ошибка: параметры просто не заданы, и экран
	// покажет пустую форму вместо сообщения о сбое.
	if (!query.exec() || !query.next())
		return false;

	out_birth_date = query.value(0).toString();
	out_activity_level = query.value(1).toString();

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AUsers_Repository::Save_Macro_Params(const QString &in_user_id, const QString &in_birth_date,
                                          const QString &in_activity_level)
{
	QSqlQuery query(Connection());

	if (Database_Instance == 0 || !Database_Instance->Is_Open())
		return false;

	// Пустые значения сохраняются как есть (NULL в QString): незаполненное
	// поле должно остаться незаполненным и в следующий раз, а не превращаться в
	// дату 0000-01-01 или выдуманный уровень активности.
	query.prepare("INSERT INTO profile_local(user_id, level, goal, workouts_per_week, birth_date, activity_level, "
	              "updated_at) VALUES(?, '', '', 0, ?, ?, ?) "
	              "ON CONFLICT(user_id) DO UPDATE SET birth_date = excluded.birth_date, "
	              "activity_level = excluded.activity_level, updated_at = excluded.updated_at");
	query.addBindValue(in_user_id);
	query.addBindValue(in_birth_date);
	query.addBindValue(in_activity_level);
	query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

	if (!query.exec())
	{
		qCWarning(USERS_Repository_Log).noquote() << "macro params persist failed" << query.lastError().text();

		return false;
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
// Результат авторасчёта КБЖУ (миграция v6). Строка профиля существует ради
// уровня и цели, поэтому её наличие ещё не означает, что расчёт делали: признак
// «считали» — заполненная дата расчёта, и NULL в ней возвращает false, а не
// нули, которые на экране выглядели бы как честный результат.
//-------------------------------------------------------------------------------------------------------------
bool AUsers_Repository::Load_Macro_Result(const QString &in_user_id, AsMacro_Calculator::SResult &out_result,
                                          QString &out_calculated_at)
{
	QSqlQuery query(Connection());

	out_result = AsMacro_Calculator::SResult();
	out_calculated_at.clear();

	if (Database_Instance == 0 || !Database_Instance->Is_Open())
		return false;

	query.prepare("SELECT macro_calculated_at, macro_bmr_kcal, macro_tdee_kcal, macro_calories_kcal, macro_protein_g, "
	              "macro_fat_g, macro_carbs_g FROM profile_local WHERE user_id = ?");
	query.addBindValue(in_user_id);

	if (!query.exec() || !query.next())
		return false;

	if (query.value(0).isNull() || query.value(3).isNull())
		return false;

	out_calculated_at = query.value(0).toString();
	out_result.Bmr_Kcal = query.value(1).toDouble();
	out_result.Tdee_Kcal = query.value(2).toDouble();
	out_result.Calories_Kcal = query.value(3).toDouble();
	out_result.Protein_G = query.value(4).toDouble();
	out_result.Fat_G = query.value(5).toDouble();
	out_result.Carbs_G = query.value(6).toDouble();

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AUsers_Repository::Save_Macro_Result(const QString &in_user_id, const AsMacro_Calculator::SResult &in_result)
{
	QSqlQuery query(Connection());
	const QString now = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

	if (Database_Instance == 0 || !Database_Instance->Is_Open())
		return false;

	// Отдельный upsert, как и у параметров: строка profile_local принадлежит
	// нескольким потребителям (редактор программы пишет уровень и цель), и
	// общий upsert затирал бы чужие поля пустыми значениями.
	query.prepare("INSERT INTO profile_local(user_id, level, goal, workouts_per_week, macro_calculated_at, "
	              "macro_bmr_kcal, macro_tdee_kcal, macro_calories_kcal, macro_protein_g, macro_fat_g, macro_carbs_g, "
	              "updated_at) VALUES(?, '', '', 0, ?, ?, ?, ?, ?, ?, ?, ?) "
	              "ON CONFLICT(user_id) DO UPDATE SET macro_calculated_at = excluded.macro_calculated_at, "
	              "macro_bmr_kcal = excluded.macro_bmr_kcal, macro_tdee_kcal = excluded.macro_tdee_kcal, "
	              "macro_calories_kcal = excluded.macro_calories_kcal, macro_protein_g = excluded.macro_protein_g, "
	              "macro_fat_g = excluded.macro_fat_g, macro_carbs_g = excluded.macro_carbs_g, "
	              "updated_at = excluded.updated_at");
	query.addBindValue(in_user_id);
	query.addBindValue(now);
	query.addBindValue(in_result.Bmr_Kcal);
	query.addBindValue(in_result.Tdee_Kcal);
	query.addBindValue(in_result.Calories_Kcal);
	query.addBindValue(in_result.Protein_G);
	query.addBindValue(in_result.Fat_G);
	query.addBindValue(in_result.Carbs_G);
	query.addBindValue(now);

	if (!query.exec())
	{
		qCWarning(USERS_Repository_Log).noquote() << "macro result persist failed" << query.lastError().text();

		return false;
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AUsers_Repository::Clear_Local_Profile(const QString &in_user_id)
{
	if (Database_Instance == 0 || !Database_Instance->Is_Open())
		return false;

	QSqlQuery query(Connection());

	query.prepare("DELETE FROM profile_local WHERE user_id = ?");
	query.addBindValue(in_user_id);

	if (!query.exec())
	{
		qCWarning(USERS_Repository_Log).noquote() << "profile_local clear failed";
		return false;
	}

	qCInfo(USERS_Repository_Log).noquote() << "local profile cleared";

	return true;
}
//-------------------------------------------------------------------------------------------------------------
int AUsers_Repository::Pending_Outbox_Count() const
{
	QSqlQuery query(Connection());

	if (Database_Instance == 0 || !Database_Instance->Is_Open())
		return 0;

	if (!query.exec("SELECT COUNT(*) FROM sync_outbox") || !query.next())
		return 0;

	return query.value(0).toInt();
}
//-------------------------------------------------------------------------------------------------------------
void AUsers_Repository::on_Profile_Updated(int in_request_id, const SUsers_Profile &in_profile,
                                           const AApi_Error &in_error)
{
	if (!Pending_Updates.contains(in_request_id))
		return;

	Pending_Updates.remove(in_request_id);
	emit profileUpdated(in_request_id, in_profile, in_error);
}
//-------------------------------------------------------------------------------------------------------------
void AUsers_Repository::on_Get_User_Finished(int in_request_id, const SUsers_User &in_user, const AApi_Error &in_error)
{
	SUsers_User user = in_user;
	AApi_Error error = in_error;
	QString user_id;
	bool from_cache = false;
	SProfile_Pending pending;

	user_id = Pending_Requests.contains(in_request_id) ? Pending_Requests.take(in_request_id) : user.ID;
	int generation = Pending_Read_Generations.take(in_request_id);
	if (generation != Profile_Generations.value(user_id))
	{
		if (Get_Cached_User(user_id, user))
		{
			from_cache = true;
			error = AApi_Error();
		}
		emit getUserFinished(in_request_id, user, from_cache, error);
		return;
	}
	if (!error.Is_Error() && Load_Pending_Profile(user_id, pending))
	{
		Apply_Profile_Changes(user.Profile, pending.Changes);
		user.Has_Profile = true;
	}

	if (!error.Is_Error())
	{
		if (!Persist_User(user))
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
//-------------------------------------------------------------------------------------------------------------
QSqlDatabase AUsers_Repository::Connection() const
{
	if (Database_Instance != 0)
		return Database_Instance->Sql_Connection();

	return QSqlDatabase();
}
//-------------------------------------------------------------------------------------------------------------
bool AUsers_Repository::Persist_User(const SUsers_User &in_user)
{
	QSqlQuery query(Connection());

	query.prepare("INSERT INTO user_cache(id, email, display_name, created_at, updated_at) "
	              "VALUES(?, ?, ?, ?, ?) "
	              "ON CONFLICT(id) DO UPDATE SET email = excluded.email, display_name = excluded.display_name, "
	              "created_at = excluded.created_at, updated_at = excluded.updated_at");
	query.addBindValue(in_user.ID);
	query.addBindValue(in_user.Email);
	query.addBindValue(in_user.Name);
	query.addBindValue(in_user.Created_At);
	query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

	if (!query.exec())
	{
		qCWarning(USERS_Repository_Log).noquote() << "user_cache persist failed";
		return false;
	}

	if (in_user.Has_Profile && !Persist_Profile_Cache(in_user.ID, in_user.Profile))
		return false;

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AUsers_Repository::Persist_Profile_Cache(const QString &in_user_id, const SUsers_Profile &in_profile)
{
	QSqlQuery query(Connection());

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

	if (!query.exec())
	{
		qCWarning(USERS_Repository_Log).noquote() << "profile_cache persist failed";
		return false;
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
void AUsers_Repository::Apply_Profile_Changes(SUsers_Profile &in_out_profile, const QJsonObject &in_changes)
{
	if (in_changes.contains("age"))
		in_out_profile.Age = in_changes.value("age").toInt();
	if (in_changes.contains("gender"))
		in_out_profile.Gender = in_changes.value("gender").toString();
	if (in_changes.contains("height"))
		in_out_profile.Height_Cm = in_changes.value("height").toDouble();
	if (in_changes.contains("weight"))
		in_out_profile.Weight_Kg = in_changes.value("weight").toDouble();
}
//-------------------------------------------------------------------------------------------------------------
