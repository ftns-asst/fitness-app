#include "Data/Auth/Auth_Repository.h"

#include <QDateTime>
#include <QLoggingCategory>
#include <QSqlError>
#include <QSqlQuery>

Q_LOGGING_CATEGORY(AUTH_Repository_Log, "trainingapp.auth.repository")

//----------------------------------------------------------------------------
// AAuth_Repository
//----------------------------------------------------------------------------
AAuth_Repository::AAuth_Repository(AsHttp_Client *in_http_client, AsSql_Database *in_database,
                                   AsToken_Store *in_token_store, AsAuth_Session *in_session, QObject *in_parent)
    : QObject(in_parent), Database_Instance(in_database), Token_Store_Instance(in_token_store),
      Session_Instance(in_session)
{
	// HTTP-адаптер auth endpoints принадлежит репозиторию.
	Auth_Api_Instance = new AAuth_Api(in_http_client, this);

	connect(Auth_Api_Instance, &AAuth_Api::checkEmailFinished, this, &AAuth_Repository::on_Check_Email_Finished);
	connect(Auth_Api_Instance, &AAuth_Api::signupFinished, this, &AAuth_Repository::on_Signup_Finished);
	connect(Auth_Api_Instance, &AAuth_Api::loginFinished, this, &AAuth_Repository::on_Login_Finished);

	if (Session_Instance != 0)
		connect(Session_Instance, &AsAuth_Session::sessionExpired, this, &AAuth_Repository::on_Session_Expired);
}
//----------------------------------------------------------------------------
int AAuth_Repository::Check_Email(const QString &in_email)
{
	return Auth_Api_Instance->Check_Email(in_email);
}
//----------------------------------------------------------------------------
int AAuth_Repository::Signup(const QString &in_email, const QString &in_name, const QString &in_password)
{
	return Auth_Api_Instance->Signup(in_email, in_name, in_password);
}
//----------------------------------------------------------------------------
int AAuth_Repository::Login(const QString &in_email, const QString &in_password)
{
	return Auth_Api_Instance->Login(in_email, in_password);
}
//----------------------------------------------------------------------------
void AAuth_Repository::Logout()
{
	// Локальный logout (docs/03, §7): серверный revocation endpoint не
	// подтверждён backend, поэтому завершаем только клиентскую сессию.
	if (Token_Store_Instance != 0)
		Token_Store_Instance->Clear_Tokens();

	if (Session_Instance != 0)
		Session_Instance->Drop_Session();

	qCInfo(AUTH_Repository_Log).noquote() << "logged out";

	emit loggedOut();
}
//----------------------------------------------------------------------------
bool AAuth_Repository::Restore_Local_Session()
{
	if (Session_Instance == 0)
		return false;

	return Session_Instance->Restore_From_Store();
}
//----------------------------------------------------------------------------
bool AAuth_Repository::Has_Local_Session() const
{
	if (Token_Store_Instance == 0)
		return false;

	return Token_Store_Instance->Has_Tokens();
}
//----------------------------------------------------------------------------
void AAuth_Repository::on_Check_Email_Finished(int in_request_id, bool in_free, const AApi_Error &in_error)
{
	emit checkEmailFinished(in_request_id, in_free, in_error);
}
//----------------------------------------------------------------------------
void AAuth_Repository::on_Signup_Finished(int in_request_id, const SAuth_Session_Data &in_session_data,
                                          const AApi_Error &in_error)
{
	AApi_Error error = in_error;

	if (error.Is_Error() == false && Persist_Session(in_session_data, error) == false)
		qCWarning(AUTH_Repository_Log).noquote() << "signup persist failed" << in_request_id;

	emit signupFinished(in_request_id, in_session_data.User, error);
}
//----------------------------------------------------------------------------
void AAuth_Repository::on_Login_Finished(int in_request_id, const SAuth_Session_Data &in_session_data,
                                         const AApi_Error &in_error)
{
	AApi_Error error = in_error;

	if (error.Is_Error() == false && Persist_Session(in_session_data, error) == false)
		qCWarning(AUTH_Repository_Log).noquote() << "login persist failed" << in_request_id;

	emit loginFinished(in_request_id, in_session_data.User, error);
}
//----------------------------------------------------------------------------
void AAuth_Repository::on_Session_Expired()
{
	emit sessionExpired();
}
//----------------------------------------------------------------------------
bool AAuth_Repository::Persist_Session(const SAuth_Session_Data &in_session_data, AApi_Error &out_error)
{
	SStored_Tokens tokens;
	QSqlQuery query(Database_Instance->Sql_Connection());

	// Порядок фиксирован: кэш пользователя → пара токенов → активация сессии.
	tokens.User_ID = in_session_data.User.ID;
	tokens.Access_Token = in_session_data.Access_Token;
	tokens.Refresh_Token = in_session_data.Refresh_Token;

	query.prepare("INSERT INTO user_cache(id, email, display_name, created_at, updated_at) "
	              "VALUES(?, ?, ?, ?, ?) "
	              "ON CONFLICT(id) DO UPDATE SET email = excluded.email, display_name = excluded.display_name, "
	              "created_at = excluded.created_at, updated_at = excluded.updated_at");
	query.addBindValue(in_session_data.User.ID);
	query.addBindValue(in_session_data.User.Email);
	query.addBindValue(in_session_data.User.Name);
	query.addBindValue(in_session_data.User.Created_At);
	query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));

	if (query.exec() == false)
	{
		qCWarning(AUTH_Repository_Log).noquote() << "user cache persist failed";
		out_error = AApi_Error("undefined_error", "Local session persist failed", 0, 0);
		return false;
	}

	if (Token_Store_Instance->Save_Tokens(tokens.User_ID, tokens.Access_Token, tokens.Refresh_Token, QDateTime(),
	                                      QDateTime()) == false)
	{
		qCWarning(AUTH_Repository_Log).noquote() << "token persist failed";
		out_error = AApi_Error("undefined_error", "Local session persist failed", 0, 0);
		return false;
	}

	if (Session_Instance->Adopt_Session(tokens) == false)
	{
		qCWarning(AUTH_Repository_Log).noquote() << "session adopt failed";
		out_error = AApi_Error("undefined_error", "Local session persist failed", 0, 0);
		return false;
	}

	return true;
}
//----------------------------------------------------------------------------
