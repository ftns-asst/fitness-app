#include "Data/Auth/Auth_Api.h"

#include <QJsonObject>
#include <QLoggingCategory>
#include <QMap>

Q_LOGGING_CATEGORY(AUTH_Api_Log, "trainingapp.auth.api")

//----------------------------------------------------------------------------
// AAuth_Api
//----------------------------------------------------------------------------
AAuth_Api::AAuth_Api(AsHttp_Client *in_http_client, QObject *in_parent)
    : QObject(in_parent), HTTP_Client_Instance(in_http_client)
{
	qRegisterMetaType<SAuth_User>("SAuth_User");
	qRegisterMetaType<SAuth_Session_Data>("SAuth_Session_Data");

	if (HTTP_Client_Instance != 0)
		connect(HTTP_Client_Instance, &AsHttp_Client::requestFinished, this, &AAuth_Api::on_Http_Finished);
}
//----------------------------------------------------------------------------
int AAuth_Api::Check_Email(const QString &in_email)
{
	QJsonObject body;
	int transport_id;

	if (HTTP_Client_Instance == 0)
		return 0;

	body.insert("email", in_email);
	transport_id = HTTP_Client_Instance->Post_JSON("auth/check-email", body, QMap<QByteArray, QByteArray>(), false);

	if (transport_id == 0)
		return 0;

	Pending_Operations.insert(transport_id, EOp_Check_Email);

	return transport_id;
}
//----------------------------------------------------------------------------
int AAuth_Api::Signup(const QString &in_email, const QString &in_name, const QString &in_password)
{
	QJsonObject body;
	int transport_id;

	if (HTTP_Client_Instance == 0)
		return 0;

	body.insert("email", in_email);
	body.insert("name", in_name);
	body.insert("password", in_password);
	transport_id = HTTP_Client_Instance->Post_JSON("auth/signup", body, QMap<QByteArray, QByteArray>(), false);

	if (transport_id == 0)
		return 0;

	Pending_Operations.insert(transport_id, EOp_Signup);

	return transport_id;
}
//----------------------------------------------------------------------------
int AAuth_Api::Login(const QString &in_email, const QString &in_password)
{
	QJsonObject body;
	int transport_id;

	if (HTTP_Client_Instance == 0)
		return 0;

	body.insert("email", in_email);
	body.insert("password", in_password);
	transport_id = HTTP_Client_Instance->Post_JSON("auth/login", body, QMap<QByteArray, QByteArray>(), false);

	if (transport_id == 0)
		return 0;

	Pending_Operations.insert(transport_id, EOp_Login);

	return transport_id;
}
//----------------------------------------------------------------------------
void AAuth_Api::on_Http_Finished(int in_request_id, const SHttp_Response &in_response)
{
	int operation;

	if (Pending_Operations.contains(in_request_id) == false)
		return;

	operation = Pending_Operations.take(in_request_id);

	switch (operation)
	{
	case EOp_Check_Email:
		Finish_Check_Email(in_request_id, in_response);
		break;
	case EOp_Signup:
	case EOp_Login:
		Finish_Session_Operation((EAuth_Operation)operation, in_request_id, in_response);
		break;
	default:
		break;
	}
}
//----------------------------------------------------------------------------
void AAuth_Api::Finish_Check_Email(int in_request_id, const SHttp_Response &in_response)
{
	AApi_Error error;
	bool free_flag = false;

	if (in_response.Success == false)
	{
		error = in_response.Error;
	}
	else if (in_response.JSON_Valid == false || in_response.JSON.object().value("free").isBool() == false)
	{
		// Обязательное поле free отсутствует — неполный успешный ответ.
		error = AApi_Error("undefined_error", "Incomplete check-email response", in_response.HTTP_Status_Code,
		                   in_response.Network_Error_Code);
	}
	else
		free_flag = in_response.JSON.object().value("free").toBool();

	emit checkEmailFinished(in_request_id, free_flag, error);
}
//----------------------------------------------------------------------------
void AAuth_Api::Finish_Session_Operation(EAuth_Operation in_operation, int in_request_id,
                                         const SHttp_Response &in_response)
{
	SAuth_Session_Data data;
	AApi_Error error;

	Parse_Session_Data(in_response, data, error);

	if (in_operation == EOp_Signup)
		emit signupFinished(in_request_id, data, error);
	else
		emit loginFinished(in_request_id, data, error);
}
//----------------------------------------------------------------------------
void AAuth_Api::Parse_Session_Data(const SHttp_Response &in_response, SAuth_Session_Data &out_data,
                                   AApi_Error &out_error)
{
	QJsonObject root;
	QJsonObject user;
	QJsonObject tokens;

	out_data = SAuth_Session_Data();
	out_error = AApi_Error();

	if (in_response.Success == false)
	{
		out_error = in_response.Error;
		return;
	}

	root = in_response.JSON.object();
	user = root.value("user").toObject();
	tokens = root.value("tokens").toObject();
	out_data.User.ID = user.value("id").toString();
	out_data.User.Name = user.value("name").toString();
	out_data.User.Email = user.value("email").toString();
	out_data.User.Created_At = user.value("created_at").toString();
	out_data.Access_Token = tokens.value("access_token").toString();
	out_data.Refresh_Token = tokens.value("refresh_token").toString();

	// Обязательные поля успешного auth-ответа (docs/03, сценарий 2).
	if (in_response.JSON_Valid == false || out_data.User.ID.isEmpty() || out_data.Access_Token.isEmpty() ||
	    out_data.Refresh_Token.isEmpty())
	{
		out_data = SAuth_Session_Data();
		out_error = AApi_Error("undefined_error", "Incomplete auth response", in_response.HTTP_Status_Code,
		                       in_response.Network_Error_Code);
	}
}
//----------------------------------------------------------------------------
