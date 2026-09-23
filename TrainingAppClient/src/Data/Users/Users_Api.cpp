#include "Data/Users/Users_Api.h"

#include <QJsonObject>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(USERS_Api_Log, "trainingapp.users.api")

//----------------------------------------------------------------------------
// AUsers_Api
//----------------------------------------------------------------------------
AUsers_Api::AUsers_Api(AsAuth_Session *in_session, QObject *in_parent)
    : QObject(in_parent), Session_Instance(in_session)
{
	qRegisterMetaType<SUsers_Profile>("SUsers_Profile");
	qRegisterMetaType<SUsers_User>("SUsers_User");

	if (Session_Instance != 0)
		connect(Session_Instance, &AsAuth_Session::requestFinished, this, &AUsers_Api::on_Session_Finished);
}
//----------------------------------------------------------------------------
int AUsers_Api::Get_User(const QString &in_user_id, bool in_with_profile)
{
	SHttp_Request request;
	int request_id;

	if (Session_Instance == 0 || in_user_id.trimmed().isEmpty())
		return 0;

	request.Method = "GET";
	request.Path =
	    in_with_profile ? QString("users/%1?withProfile=true").arg(in_user_id) : QString("users/%1").arg(in_user_id);
	request.Retry_Enabled = true;

	request_id = Session_Instance->Send_Authed(request);

	if (request_id == 0)
		return 0;

	Pending_Gets.insert(request_id, in_user_id);

	return request_id;
}
//----------------------------------------------------------------------------
void AUsers_Api::on_Session_Finished(int in_request_id, const SHttp_Response &in_response)
{
	QString user_id;

	if (Pending_Gets.contains(in_request_id) == false)
		return;

	user_id = Pending_Gets.take(in_request_id);
	Finish_Get_User(in_request_id, user_id, in_response);
}
//----------------------------------------------------------------------------
void AUsers_Api::Finish_Get_User(int in_request_id, const QString &in_user_id, const SHttp_Response &in_response)
{
	SUsers_User user;
	AApi_Error error;

	Q_UNUSED(in_user_id);

	Parse_User(in_response, user, error);

	emit getUserFinished(in_request_id, user, error);
}
//----------------------------------------------------------------------------
void AUsers_Api::Parse_User(const SHttp_Response &in_response, SUsers_User &out_user, AApi_Error &out_error)
{
	QJsonObject root;
	QJsonObject profile;

	out_user = SUsers_User();
	out_error = AApi_Error();

	if (in_response.Success == false)
	{
		out_error = in_response.Error;
		return;
	}

	root = in_response.JSON.object();
	out_user.ID = root.value("id").toString();
	out_user.Name = root.value("name").toString();
	out_user.Email = root.value("email").toString();
	out_user.Created_At = root.value("created_at").toString();

	// Обязательное поле id успешного ответа (docs/03, §3.5).
	if (in_response.JSON_Valid == false || out_user.ID.isEmpty())
	{
		out_user = SUsers_User();
		out_error = AApi_Error("undefined_error", "Incomplete users response", in_response.HTTP_Status_Code,
		                       in_response.Network_Error_Code);
		return;
	}

	// Отсутствующий profile — «не заполнен», не ошибка.
	if (root.value("profile").isObject() == false)
		return;

	profile = root.value("profile").toObject();
	out_user.Has_Profile = true;
	out_user.Profile.Age = profile.value("age").toInt(-1);
	out_user.Profile.Gender = profile.value("gender").toString();
	out_user.Profile.Height_Cm = profile.value("height").toDouble();
	out_user.Profile.Weight_Kg = profile.value("weight").toDouble();
}
//----------------------------------------------------------------------------
