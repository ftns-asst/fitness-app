#include "Data/Auth/Auth_Api.h"

#include <QJsonObject>
#include <QLoggingCategory>
#include <QMap>

Q_LOGGING_CATEGORY(AUTH_Api_Log, "trainingapp.auth.api")

//-------------------------------------------------------------------------------------------------------------
// AAuth_Api
//-------------------------------------------------------------------------------------------------------------
AAuth_Api::AAuth_Api(AsHttp_Client *in_http_client, QObject *in_parent)
    : QObject(in_parent), HTTP_Client_Instance(in_http_client)
{
	qRegisterMetaType<SAuth_User>("SAuth_User");
	qRegisterMetaType<SAuth_Session_Data>("SAuth_Session_Data");

	if (HTTP_Client_Instance != 0)
		connect(HTTP_Client_Instance, &AsHttp_Client::requestFinished, this, &AAuth_Api::on_Http_Finished);
}
//-------------------------------------------------------------------------------------------------------------
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

	Pending_Operations.insert(transport_id, EAuth_Operation::Check_Email);
	qCInfo(AUTH_Api_Log).noquote() << "check-email started" << transport_id;

	return transport_id;
}
//-------------------------------------------------------------------------------------------------------------
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

	Pending_Operations.insert(transport_id, EAuth_Operation::Signup);
	qCInfo(AUTH_Api_Log).noquote() << "signup started" << transport_id;

	return transport_id;
}
//-------------------------------------------------------------------------------------------------------------
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

	Pending_Operations.insert(transport_id, EAuth_Operation::Login);
	qCInfo(AUTH_Api_Log).noquote() << "login started" << transport_id;

	return transport_id;
}
//-------------------------------------------------------------------------------------------------------------
int AAuth_Api::Request_Recovery(const QString &in_email)
{
	QJsonObject body;
	body.insert("email", in_email);

	return Start_Recovery_Operation(EAuth_Operation::Request_Recovery, "auth/password-recovery", body);
}
//-------------------------------------------------------------------------------------------------------------
int AAuth_Api::Verify_Recovery(const QString &in_email, const QString &in_code)
{
	QJsonObject body;
	body.insert("email", in_email);
	body.insert("code", in_code);

	return Start_Recovery_Operation(EAuth_Operation::Verify_Recovery, "auth/password-recovery/verify", body);
}
//-------------------------------------------------------------------------------------------------------------
int AAuth_Api::Reset_Password(const QString &in_reset_token, const QString &in_password)
{
	QJsonObject body;
	body.insert("reset_token", in_reset_token);
	body.insert("new_password", in_password);

	return Start_Recovery_Operation(EAuth_Operation::Reset_Password, "auth/password-recovery/reset", body);
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Api::on_Http_Finished(int in_request_id, const SHttp_Response &in_response)
{
	EAuth_Operation operation;

	if (!Pending_Operations.contains(in_request_id))
		return;

	operation = Pending_Operations.take(in_request_id);

	switch (operation)
	{
	case EAuth_Operation::Check_Email:
		Finish_Check_Email(in_request_id, in_response);
		break;
	case EAuth_Operation::Signup:
	case EAuth_Operation::Login:
	case EAuth_Operation::Reset_Password:
		Finish_Session_Operation(operation, in_request_id, in_response);
		break;
	case EAuth_Operation::Request_Recovery:
	case EAuth_Operation::Verify_Recovery:
		Finish_Recovery_Operation(operation, in_request_id, in_response);
		break;
	default:
		break;
	}
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Api::Finish_Check_Email(int in_request_id, const SHttp_Response &in_response)
{
	AApi_Error error;
	bool free_flag = false;

	if (!in_response.Success)
	{
		error = in_response.Error;
	}
	else if (!in_response.JSON_Valid || !in_response.JSON.object().value("free").isBool())
	{
		// Обязательное поле free отсутствует — неполный успешный ответ.
		error = AApi_Error("undefined_error", "Incomplete check-email response", in_response.HTTP_Status_Code,
		                   in_response.Network_Error_Code);
	}
	else
		free_flag = in_response.JSON.object().value("free").toBool();

	// free не логируем: это перечисление занятости чужих email.
	if (error.Is_Error())
		qCInfo(AUTH_Api_Log).noquote() << "check-email finished" << in_request_id << "error key=" << error.Key();
	else
		qCInfo(AUTH_Api_Log).noquote() << "check-email finished" << in_request_id << "ok";

	emit checkEmailFinished(in_request_id, free_flag, error);
}
//-------------------------------------------------------------------------------------------------------------
int AAuth_Api::Start_Recovery_Operation(EAuth_Operation in_operation, const QString &in_path,
                                        const QJsonObject &in_body)
{
	int request_id;

	if (HTTP_Client_Instance == 0)
		return 0;

	// Одноразовые коды и токены: автоматический повтор POST запрещён.
	request_id = HTTP_Client_Instance->Post_JSON(in_path, in_body, QMap<QByteArray, QByteArray>(), false);
	if (request_id > 0)
		Pending_Operations.insert(request_id, in_operation);

	return request_id;
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Api::Finish_Recovery_Operation(EAuth_Operation in_operation, int in_request_id,
                                          const SHttp_Response &in_response)
{
	AApi_Error error = in_response.Error;
	QString reset_token;

	if (in_response.Success && in_operation == EAuth_Operation::Verify_Recovery)
	{
		reset_token = in_response.JSON.object().value("reset_token").toString();
		if (!in_response.JSON_Valid || reset_token.trimmed().isEmpty())
			error = AApi_Error("undefined_error", "Incomplete recovery response", in_response.HTTP_Status_Code, 0);
	}

	if (in_operation == EAuth_Operation::Request_Recovery)
		emit recoveryRequested(in_request_id, error);
	else
		emit recoveryVerified(in_request_id, reset_token, error);
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Api::Finish_Session_Operation(EAuth_Operation in_operation, int in_request_id,
                                         const SHttp_Response &in_response)
{
	SAuth_Session_Data data;
	AApi_Error error;
	QString operation_name = in_operation == EAuth_Operation::Signup           ? "signup finished"
	                         : in_operation == EAuth_Operation::Reset_Password ? "password reset finished"
	                                                                           : "login finished";

	Parse_Session_Data(in_response, data, error);

	// Обязательные поля валидируются выше; в лог — только исход и ключ ошибки.
	if (error.Is_Error())
		qCInfo(AUTH_Api_Log).noquote() << operation_name << in_request_id << "error key=" << error.Key();
	else
		qCInfo(AUTH_Api_Log).noquote() << operation_name << in_request_id << "ok";

	if (in_operation == EAuth_Operation::Signup)
		emit signupFinished(in_request_id, data, error);
	else if (in_operation == EAuth_Operation::Reset_Password)
		emit passwordReset(in_request_id, data, error);
	else
		emit loginFinished(in_request_id, data, error);
}
//-------------------------------------------------------------------------------------------------------------
void AAuth_Api::Parse_Session_Data(const SHttp_Response &in_response, SAuth_Session_Data &out_data,
                                   AApi_Error &out_error)
{
	QJsonObject root;
	QJsonObject user;
	QJsonObject tokens;

	out_data = SAuth_Session_Data();
	out_error = AApi_Error();

	if (!in_response.Success)
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
	if (!in_response.JSON_Valid || out_data.User.ID.isEmpty() || out_data.Access_Token.isEmpty() ||
	    out_data.Refresh_Token.isEmpty())
	{
		out_data = SAuth_Session_Data();
		out_error = AApi_Error("undefined_error", "Incomplete auth response", in_response.HTTP_Status_Code,
		                       in_response.Network_Error_Code);
	}
}
//-------------------------------------------------------------------------------------------------------------
