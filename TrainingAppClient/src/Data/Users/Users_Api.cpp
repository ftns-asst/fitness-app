#include "Data/Users/Users_Api.h"

#include <QJsonObject>
#include <QLoggingCategory>
#include <QUrl>

Q_LOGGING_CATEGORY(USERS_Api_Log, "trainingapp.users.api")

//-------------------------------------------------------------------------------------------------------------
// AUsers_Api
//-------------------------------------------------------------------------------------------------------------
AUsers_Api::AUsers_Api(AsAuth_Session *in_session, QObject *in_parent)
    : QObject(in_parent), Session_Instance(in_session)
{
	qRegisterMetaType<SUsers_Profile>("SUsers_Profile");
	qRegisterMetaType<SUsers_User>("SUsers_User");

	if (Session_Instance != 0)
		connect(Session_Instance, &AsAuth_Session::requestFinished, this, &AUsers_Api::on_Session_Finished);
}
//-------------------------------------------------------------------------------------------------------------
int AUsers_Api::Get_User(const QString &in_user_id, bool in_with_profile)
{
	SHttp_Request request;
	QString encoded_id;
	int request_id;

	if (Session_Instance == 0 || in_user_id.trimmed().isEmpty())
		return 0;

	// id процент-кодируется: `/`, `?`, `#` из значения не меняют endpoint.
	encoded_id = QString::fromLatin1(QUrl::toPercentEncoding(in_user_id));
	request.Method = "GET";
	request.Path = QString("users/") + encoded_id;

	if (in_with_profile)
		request.Path += "?withProfile=true";
	request.Retry_Enabled = true;

	request_id = Session_Instance->Send_Authed(request);

	if (request_id == 0)
		return 0;

	Pending_Gets.insert(request_id);
	qCInfo(USERS_Api_Log).noquote() << "get-user started" << request_id;

	return request_id;
}
//-------------------------------------------------------------------------------------------------------------
int AUsers_Api::Update_Profile(const QJsonObject &in_changes)
{
	SHttp_Request request;
	int request_id;

	if (Session_Instance == 0 || in_changes.isEmpty())
		return 0;

	request.Method = "PATCH";
	request.Path = "users/me/profile";
	request.Body = QJsonDocument(in_changes).toJson(QJsonDocument::Compact);
	request.Headers.insert("Content-Type", "application/json");
	request.Retry_Enabled = false;
	request_id = Session_Instance->Send_Authed(request);
	if (request_id > 0)
		Pending_Updates.insert(request_id);

	return request_id;
}
//-------------------------------------------------------------------------------------------------------------
void AUsers_Api::on_Session_Finished(int in_request_id, const SHttp_Response &in_response)
{
	if (Pending_Updates.remove(in_request_id))
	{
		SUsers_Profile profile;
		AApi_Error error = in_response.Error;
		if (in_response.Success && (!in_response.JSON_Valid || !in_response.JSON.isObject() ||
		                            !Parse_Profile(in_response.JSON.object(), profile)))
			error = AApi_Error("undefined_error", "Invalid profile response", in_response.HTTP_Status_Code, 0);
		emit profileUpdated(in_request_id, profile, error);
		return;
	}

	if (Pending_Gets.remove(in_request_id))
		Finish_Get_User(in_request_id, in_response);
}
//-------------------------------------------------------------------------------------------------------------
void AUsers_Api::Finish_Get_User(int in_request_id, const SHttp_Response &in_response)
{
	SUsers_User user;
	AApi_Error error;

	Parse_User(in_response, user, error);

	if (error.Is_Error())
		qCInfo(USERS_Api_Log).noquote() << "get-user finished" << in_request_id << "error key=" << error.Key();
	else
		qCInfo(USERS_Api_Log).noquote() << "get-user finished" << in_request_id << "ok";

	emit getUserFinished(in_request_id, user, error);
}
//-------------------------------------------------------------------------------------------------------------
void AUsers_Api::Parse_User(const SHttp_Response &in_response, SUsers_User &out_user, AApi_Error &out_error)
{
	QJsonObject root;
	QJsonObject profile;

	out_user = SUsers_User();
	out_error = AApi_Error();

	if (!in_response.Success)
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
	if (!in_response.JSON_Valid || out_user.ID.isEmpty())
	{
		out_user = SUsers_User();
		out_error = AApi_Error("undefined_error", "Incomplete users response", in_response.HTTP_Status_Code,
		                       in_response.Network_Error_Code);
		return;
	}

	// Отсутствующий profile — «не заполнен», не ошибка.
	if (!root.value("profile").isObject())
		return;

	profile = root.value("profile").toObject();
	out_user.Has_Profile = true;
	if (!Parse_Profile(profile, out_user.Profile))
		out_error = AApi_Error("undefined_error", "Invalid profile response", in_response.HTTP_Status_Code, 0);
}
//-------------------------------------------------------------------------------------------------------------
bool AUsers_Api::Parse_Profile(const QJsonObject &in_profile, SUsers_Profile &out_profile)
{
	for (const QString &key : {QString("age"), QString("height"), QString("weight")})
	{
		if (in_profile.contains(key) && !in_profile.value(key).isDouble())
			return false;
	}
	if (in_profile.contains("gender") && !in_profile.value("gender").isString())
		return false;

	out_profile.Age = in_profile.value("age").toInt(-1);
	out_profile.Gender = in_profile.value("gender").toString();
	out_profile.Height_Cm = in_profile.value("height").toDouble();
	out_profile.Weight_Kg = in_profile.value("weight").toDouble();

	return true;
}
//-------------------------------------------------------------------------------------------------------------
