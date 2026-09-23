#pragma once

#include "Data/Auth/Auth_Session.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Network/Api_Error.h"
#include "Data/Users/Users_Api.h"

#include <QHash>
#include <QObject>
#include <QString>

//----------------------------------------------------------------------------
// Оркестрация профиля пользователя: получение с сервера с записью в
// user_cache/profile_cache, офлайн-чтение кэша с пометкой from_cache, локальные
// параметры (уровень, цель, частота) в profile_local с записью в sync_outbox
// (воркер синхронизации — issue 34). Статус «сохранено локально» —
// Status_Saved_Locally.
//----------------------------------------------------------------------------
class AUsers_Repository : public QObject
{
	Q_OBJECT

public:
	explicit AUsers_Repository(AsAuth_Session *in_session, AsSql_Database *in_database, QObject *in_parent = 0);

	int Get_User(const QString &in_user_id, bool in_with_profile);
	bool Get_Cached_User(const QString &in_user_id, SUsers_User &out_user);
	bool Load_Local_Profile(const QString &in_user_id, QString &out_level, QString &out_goal,
	                        int &out_workouts_per_week);
	bool Save_Local_Profile(const QString &in_user_id, const QString &in_level, const QString &in_goal,
	                        int in_workouts_per_week, QString &out_status);
	int Pending_Outbox_Count() const;

	static const char *Status_Saved_Locally;

signals:
	void getUserFinished(int requestId, const SUsers_User &user, bool from_cache, const AApi_Error &error);

private slots:
	void on_Get_User_Finished(int in_request_id, const SUsers_User &in_user, const AApi_Error &in_error);

private:
	bool Persist_User(const SUsers_User &in_user);
	bool Persist_Profile_Cache(const QString &in_user_id, const SUsers_Profile &in_profile);

	AsSql_Database *Database_Instance;
	AsAuth_Session *Session_Instance;
	AUsers_Api *Users_Api_Instance;
	QHash<int, QString> Pending_Requests; // request id → user id
};
//----------------------------------------------------------------------------
