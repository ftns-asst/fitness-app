#include "Presentation/Auth_Qml_Bridge.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(AUTH_Bridge_Log, "trainingapp.auth.bridge")

//----------------------------------------------------------------------------
// Avm_Auth_Bridge
//----------------------------------------------------------------------------
Avm_Auth_Bridge::Avm_Auth_Bridge(AAuth_Repository *in_auth_repository, AUsers_Repository *in_users_repository,
                                 AsAuth_Session *in_session, QObject *in_parent)
    : QObject(in_parent), Auth_Repository_Instance(in_auth_repository), Users_Repository_Instance(in_users_repository),
      Session_Instance(in_session)
{
	connect(Auth_Repository_Instance, &AAuth_Repository::checkEmailFinished, this,
	        &Avm_Auth_Bridge::on_Check_Email_Finished);
	connect(Auth_Repository_Instance, &AAuth_Repository::signupFinished, this, &Avm_Auth_Bridge::on_Signup_Finished);
	connect(Auth_Repository_Instance, &AAuth_Repository::loginFinished, this, &Avm_Auth_Bridge::on_Login_Finished);
	connect(Auth_Repository_Instance, &AAuth_Repository::loggedOut, this, &Avm_Auth_Bridge::on_Logged_Out);
	connect(Auth_Repository_Instance, &AAuth_Repository::sessionExpired, this, &Avm_Auth_Bridge::on_Session_Expired);
}
//----------------------------------------------------------------------------
int Avm_Auth_Bridge::checkEmail(const QString &in_email)
{
	int request_id;

	request_id = Auth_Repository_Instance->Check_Email(in_email);

	if (request_id == 0)
		return 0;

	Pending_Operations.insert(request_id, "checkEmail");

	return request_id;
}
//----------------------------------------------------------------------------
int Avm_Auth_Bridge::login(const QString &in_email, const QString &in_password)
{
	int request_id;

	request_id = Auth_Repository_Instance->Login(in_email, in_password);

	if (request_id == 0)
		return 0;

	Pending_Operations.insert(request_id, "login");

	return request_id;
}
//----------------------------------------------------------------------------
int Avm_Auth_Bridge::signup(const QVariantMap &in_request)
{
	int request_id;

	request_id =
	    Auth_Repository_Instance->Signup(in_request.value("email").toString(), in_request.value("name").toString(),
	                                     in_request.value("password").toString());

	if (request_id == 0)
		return 0;

	Pending_Operations.insert(request_id, "signup");

	return request_id;
}
//----------------------------------------------------------------------------
void Avm_Auth_Bridge::logout()
{
	Auth_Repository_Instance->Logout();
}
//----------------------------------------------------------------------------
bool Avm_Auth_Bridge::restoreSession()
{
	QVariantMap user;
	SUsers_User cached_user;

	if (Auth_Repository_Instance->Restore_Local_Session() == false)
		return false;

	if (Session_Instance != 0)
		Current_User_ID = Session_Instance->User_ID();

	// Cold start: пользователь показывается из локального кэша.
	if (Users_Repository_Instance != 0 && Users_Repository_Instance->Get_Cached_User(Current_User_ID, cached_user))
		user = User_DTO(cached_user);
	else
		user.insert("id", Current_User_ID);

	qCInfo(AUTH_Bridge_Log).noquote() << "session restored";
	emit sessionRestored(user);

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Auth_Bridge::saveLocalProfile(const QVariantMap &in_local_profile)
{
	QString status;

	if (Current_User_ID.isEmpty())
		return false;

	if (Users_Repository_Instance == 0)
		return false;

	return Users_Repository_Instance->Save_Local_Profile(Current_User_ID, in_local_profile.value("level").toString(),
	                                                     in_local_profile.value("goal").toString(),
	                                                     in_local_profile.value("workoutsPerWeek").toInt(), status);
}
//----------------------------------------------------------------------------
void Avm_Auth_Bridge::clearLocalProfile()
{
	if (Users_Repository_Instance == 0 || Current_User_ID.isEmpty())
		return;

	Users_Repository_Instance->Clear_Local_Profile(Current_User_ID);
}
//----------------------------------------------------------------------------
void Avm_Auth_Bridge::on_Check_Email_Finished(int in_request_id, bool in_free, const AApi_Error &in_error)
{
	QString operation = Take_Operation(in_request_id);

	if (operation != "checkEmail")
		return;

	Finish_Check_Email(in_request_id, in_free, in_error);
}
//----------------------------------------------------------------------------
void Avm_Auth_Bridge::on_Signup_Finished(int in_request_id, const SAuth_User &in_user, const AApi_Error &in_error)
{
	QString operation = Take_Operation(in_request_id);

	if (operation != "signup")
		return;

	Finish_Session_Operation(operation, in_request_id, in_user, in_error);
}
//----------------------------------------------------------------------------
void Avm_Auth_Bridge::on_Login_Finished(int in_request_id, const SAuth_User &in_user, const AApi_Error &in_error)
{
	QString operation = Take_Operation(in_request_id);

	if (operation != "login")
		return;

	Finish_Session_Operation(operation, in_request_id, in_user, in_error);
}
//----------------------------------------------------------------------------
void Avm_Auth_Bridge::on_Logged_Out()
{
	Current_User_ID.clear();
	emit loggedOut();
}
//----------------------------------------------------------------------------
void Avm_Auth_Bridge::on_Session_Expired()
{
	Current_User_ID.clear();
	emit sessionExpired();
}
//----------------------------------------------------------------------------
void Avm_Auth_Bridge::Finish_Session_Operation(const QString &in_operation, int in_request_id,
                                               const SAuth_User &in_user, const AApi_Error &in_error)
{
	QVariantMap data;

	if (in_error.Is_Error() == false)
	{
		Current_User_ID = in_user.ID;
		data.insert("user", User_DTO(in_user));
	}

	emit requestFinished(in_request_id, in_operation, in_error.Is_Error() == false, data, Error_DTO(in_error));
}
//----------------------------------------------------------------------------
void Avm_Auth_Bridge::Finish_Check_Email(int in_request_id, bool in_free, const AApi_Error &in_error)
{
	QVariantMap data;

	if (in_error.Is_Error() == false)
		data.insert("free", in_free);

	emit requestFinished(in_request_id, "checkEmail", in_error.Is_Error() == false, data, Error_DTO(in_error));
}
//----------------------------------------------------------------------------
QVariantMap Avm_Auth_Bridge::User_DTO(const SAuth_User &in_user)
{
	QVariantMap user;

	user.insert("id", in_user.ID);
	user.insert("name", in_user.Name);
	user.insert("email", in_user.Email);
	user.insert("createdAt", in_user.Created_At);

	return user;
}
//----------------------------------------------------------------------------
QVariantMap Avm_Auth_Bridge::User_DTO(const SUsers_User &in_user)
{
	QVariantMap user;

	user.insert("id", in_user.ID);
	user.insert("name", in_user.Name);
	user.insert("email", in_user.Email);
	user.insert("createdAt", in_user.Created_At);

	return user;
}
//----------------------------------------------------------------------------
QVariantMap Avm_Auth_Bridge::Error_DTO(const AApi_Error &in_error)
{
	QVariantMap error;

	if (in_error.Is_Error() == false)
		return error;

	error.insert("key", in_error.Key());
	error.insert("message", in_error.Message());
	error.insert("httpStatus", in_error.HTTP_Status_Code());

	return error;
}
//----------------------------------------------------------------------------
QString Avm_Auth_Bridge::Take_Operation(int in_request_id)
{
	if (Pending_Operations.contains(in_request_id) == false)
		return QString();

	return Pending_Operations.take(in_request_id);
}
//----------------------------------------------------------------------------