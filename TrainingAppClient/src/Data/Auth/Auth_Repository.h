#pragma once

#include "Data/Auth/Auth_Api.h"
#include "Data/Auth/Auth_Session.h"
#include "Data/Auth/Token_Store.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Network/Http_Client.h"

#include <QObject>
#include <QString>

class QDateTime;

//-------------------------------------------------------------------------------------------------------------
// Оркестрация auth-сценариев приложения: публичные auth endpoints, атомарное
// сохранение пользователя и пары токенов, локальный logout и восстановление
// сессии при cold start. Кэш пользователя при logout не удаляется (docs/03,
// сценарий 5). UI получает только SAuth_User и стабильные ключи ошибок.
//-------------------------------------------------------------------------------------------------------------
class AAuth_Repository : public QObject
{
	Q_OBJECT

public:
	explicit AAuth_Repository(AsHttp_Client *in_http_client, AsSql_Database *in_database, AsToken_Store *in_token_store,
	                          AsAuth_Session *in_session, QObject *in_parent = 0);

	int Check_Email(const QString &in_email);
	int Signup(const QString &in_email, const QString &in_name, const QString &in_password);
	int Login(const QString &in_email, const QString &in_password);
	int Request_Recovery(const QString &in_email);
	int Verify_Recovery(const QString &in_email, const QString &in_code);
	int Reset_Password(const QString &in_reset_token, const QString &in_password);
	void Logout();
	bool Restore_Local_Session();
	bool Has_Local_Session() const;

signals:
	void recoveryRequested(int requestId, const AApi_Error &error);
	void recoveryVerified(int requestId, const QString &resetToken, const AApi_Error &error);
	void passwordReset(int requestId, const SAuth_User &user, const AApi_Error &error);
	void checkEmailFinished(int requestId, bool free, const AApi_Error &error);
	void signupFinished(int requestId, const SAuth_User &user, const AApi_Error &error);
	void loginFinished(int requestId, const SAuth_User &user, const AApi_Error &error);
	void loggedOut();
	void sessionExpired();

private slots:
	void on_Password_Reset(int in_request_id, const SAuth_Session_Data &in_session_data, const AApi_Error &in_error);
	void on_Check_Email_Finished(int in_request_id, bool in_free, const AApi_Error &in_error);
	void on_Signup_Finished(int in_request_id, const SAuth_Session_Data &in_session_data, const AApi_Error &in_error);
	void on_Login_Finished(int in_request_id, const SAuth_Session_Data &in_session_data, const AApi_Error &in_error);
	void on_Session_Expired();

private:
	// Null-безопасное соединение: без базы отдаёт пустое QSqlDatabase, и
	// запрос откатится проверкой Is_Open() ниже, а не разыменованием 0.
	QSqlDatabase Connection() const;
	bool Persist_Session(const SAuth_Session_Data &in_session_data, AApi_Error &out_error);

	AsSql_Database *Database_Instance;
	AsToken_Store *Token_Store_Instance;
	AsAuth_Session *Session_Instance;
	AAuth_Api *Auth_Api_Instance;
};
//-------------------------------------------------------------------------------------------------------------
