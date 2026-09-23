#pragma once

#include "Data/Auth/Auth_Session.h"
#include "Data/Network/Api_Error.h"
#include "Data/Network/Http_Client.h"

#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QString>

//----------------------------------------------------------------------------
// Профиль пользователя из `GET /users/{id}?withProfile=true`. Единицы
// height → Height_Cm и weight → Weight_Kg — assumed (docs/03, §4).
// Age = -1 означает «поле отсутствует в ответе».
//----------------------------------------------------------------------------
struct SUsers_Profile
{
	int Age = -1;
	QString Gender;
	double Height_Cm = 0;
	double Weight_Kg = 0;
};

//----------------------------------------------------------------------------
// User DTO приватного endpoint. Has_Profile = false — профиль не заполнен,
// это не ошибка (подтверждено backend, docs/03, §3.5).
//----------------------------------------------------------------------------
struct SUsers_User
{
	QString ID;
	QString Name;
	QString Email;
	QString Created_At;
	SUsers_Profile Profile;
	bool Has_Profile = false;
};

//----------------------------------------------------------------------------
// HTTP-адаптер приватного users endpoint. Запросы идут через AsAuth_Session
// (Bearer-заголовок и single-flight refresh). GET идемпотентен, transport
// retry разрешён. Обязательное поле id; неполный успешный ответ —
// undefined_error.
//----------------------------------------------------------------------------
class AUsers_Api : public QObject
{
	Q_OBJECT

public:
	explicit AUsers_Api(AsAuth_Session *in_session, QObject *in_parent = 0);

	int Get_User(const QString &in_user_id, bool in_with_profile);

signals:
	void getUserFinished(int requestId, const SUsers_User &user, const AApi_Error &error);

private slots:
	void on_Session_Finished(int in_request_id, const SHttp_Response &in_response);

private:
	void Finish_Get_User(int in_request_id, const QString &in_user_id, const SHttp_Response &in_response);
	static void Parse_User(const SHttp_Response &in_response, SUsers_User &out_user, AApi_Error &out_error);

	AsAuth_Session *Session_Instance;
	QHash<int, QString> Pending_Gets; // логический id → запрошенный user id
};
//----------------------------------------------------------------------------

Q_DECLARE_METATYPE(SUsers_Profile)
Q_DECLARE_METATYPE(SUsers_User)
