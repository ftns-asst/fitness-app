#pragma once

#include "Data/Auth/Token_Store.h"
#include "Data/Network/Api_Error.h"
#include "Data/Network/Http_Client.h"

#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QString>

//----------------------------------------------------------------------------
// User DTO успешных auth-ответов. snake case конвертируется на границе Data
// (docs/03-api-contract.md, §4), QML/ViewModel имён backend не видят.
//----------------------------------------------------------------------------
struct SAuth_User
{
	QString ID;
	QString Name;
	QString Email;
	QString Created_At;
};

//----------------------------------------------------------------------------
// Пользователь и пара токенов успешного signup/login.
//----------------------------------------------------------------------------
struct SAuth_Session_Data
{
	SAuth_User User;
	QString Access_Token;
	QString Refresh_Token;
};

//----------------------------------------------------------------------------
// HTTP-адаптер публичных auth endpoints: check-email, signup, login. Refresh
// выполняется AsAuth_Session и сюда не входит. POST-операции неидемпотентны,
// transport retry запрещён. Обязательные поля валидируются; неполный
// успешный ответ даёт undefined_error (docs/03, §2).
//----------------------------------------------------------------------------
class AAuth_Api : public QObject
{
	Q_OBJECT

public:
	explicit AAuth_Api(AsHttp_Client *in_http_client, QObject *in_parent = 0);

	int Check_Email(const QString &in_email);
	int Signup(const QString &in_email, const QString &in_name, const QString &in_password);
	int Login(const QString &in_email, const QString &in_password);

signals:
	void checkEmailFinished(int requestId, bool free, const AApi_Error &error);
	void signupFinished(int requestId, const SAuth_Session_Data &session_data, const AApi_Error &error);
	void loginFinished(int requestId, const SAuth_Session_Data &session_data, const AApi_Error &error);

private slots:
	void on_Http_Finished(int in_request_id, const SHttp_Response &in_response);

private:
	// Вид публичной auth-операции, ожидающей ответа (ключ — transport id).
	enum EAuth_Operation
	{
		EOp_None = 0,
		EOp_Check_Email,
		EOp_Signup,
		EOp_Login
	};

	void Finish_Check_Email(int in_request_id, const SHttp_Response &in_response);
	void Finish_Session_Operation(EAuth_Operation in_operation, int in_request_id, const SHttp_Response &in_response);
	static void Parse_Session_Data(const SHttp_Response &in_response, SAuth_Session_Data &out_data,
	                               AApi_Error &out_error);

	AsHttp_Client *HTTP_Client_Instance;
	QHash<int, int> Pending_Operations;
};
//----------------------------------------------------------------------------

Q_DECLARE_METATYPE(SAuth_User)
Q_DECLARE_METATYPE(SAuth_Session_Data)
