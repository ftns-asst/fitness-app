#include "Presentation/Auth_VM.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(AUTH_VM_Log, "trainingapp.auth.vm")

//----------------------------------------------------------------------------
// Avm_Auth
//----------------------------------------------------------------------------
Avm_Auth::Avm_Auth(AAuth_Repository *in_repository, QObject *in_parent)
    : QObject(in_parent), Repository_Instance(in_repository)
{
	Current_State = "idle";

	if (Repository_Instance != 0)
	{
		connect(Repository_Instance, &AAuth_Repository::checkEmailFinished, this, &Avm_Auth::on_Check_Email_Finished);
		connect(Repository_Instance, &AAuth_Repository::signupFinished, this, &Avm_Auth::on_Signup_Finished);
		connect(Repository_Instance, &AAuth_Repository::loginFinished, this, &Avm_Auth::on_Login_Finished);
		connect(Repository_Instance, &AAuth_Repository::loggedOut, this, &Avm_Auth::on_Logged_Out);
		connect(Repository_Instance, &AAuth_Repository::sessionExpired, this, &Avm_Auth::on_Session_Expired);
	}
}
//----------------------------------------------------------------------------
QString Avm_Auth::state() const
{
	return Current_State;
}
//----------------------------------------------------------------------------
QString Avm_Auth::errorKey() const
{
	return Error_Key_Text;
}
//----------------------------------------------------------------------------
bool Avm_Auth::authenticated() const
{
	return Current_State == "authenticated";
}
//----------------------------------------------------------------------------
void Avm_Auth::Check_Email(const QString &in_email)
{
	if (Repository_Instance == 0)
	{
		Set_Error_State("undefined_error");
		return;
	}

	Set_State("checkingEmail");

	if (Repository_Instance->Check_Email(in_email) == 0)
		Set_Error_State("undefined_error");
}
//----------------------------------------------------------------------------
void Avm_Auth::Signup(const QString &in_email, const QString &in_name, const QString &in_password)
{
	if (Repository_Instance == 0)
	{
		Set_Error_State("undefined_error");
		return;
	}

	Set_State("signingUp");

	if (Repository_Instance->Signup(in_email, in_name, in_password) == 0)
		Set_Error_State("undefined_error");
}
//----------------------------------------------------------------------------
void Avm_Auth::Login(const QString &in_email, const QString &in_password)
{
	if (Repository_Instance == 0)
	{
		Set_Error_State("undefined_error");
		return;
	}

	Set_State("loggingIn");

	if (Repository_Instance->Login(in_email, in_password) == 0)
		Set_Error_State("undefined_error");
}
//----------------------------------------------------------------------------
void Avm_Auth::Logout()
{
	if (Repository_Instance == 0)
		return;

	Repository_Instance->Logout();
}
//----------------------------------------------------------------------------
bool Avm_Auth::Restore_Local_Session()
{
	bool restored;

	if (Repository_Instance == 0)
		return false;

	restored = Repository_Instance->Restore_Local_Session();

	if (restored)
		Set_State("authenticated");

	return restored;
}
//----------------------------------------------------------------------------
void Avm_Auth::on_Check_Email_Finished(int in_request_id, bool in_free, const AApi_Error &in_error)
{
	Q_UNUSED(in_request_id);

	if (in_error.Is_Error())
	{
		Set_Error_State(in_error.Key());
		return;
	}

	if (in_free)
		Set_State("idle");
	else
		Set_State("emailTaken");
}
//----------------------------------------------------------------------------
void Avm_Auth::on_Signup_Finished(int in_request_id, const SAuth_User &in_user, const AApi_Error &in_error)
{
	Q_UNUSED(in_request_id);
	Q_UNUSED(in_user);

	if (in_error.Is_Error())
		Set_Error_State(in_error.Key());
	else
		Set_State("authenticated");
}
//----------------------------------------------------------------------------
void Avm_Auth::on_Login_Finished(int in_request_id, const SAuth_User &in_user, const AApi_Error &in_error)
{
	Q_UNUSED(in_request_id);
	Q_UNUSED(in_user);

	if (in_error.Is_Error())
		Set_Error_State(in_error.Key());
	else
		Set_State("authenticated");
}
//----------------------------------------------------------------------------
void Avm_Auth::on_Logged_Out()
{
	Set_State("idle");
}
//----------------------------------------------------------------------------
void Avm_Auth::on_Session_Expired()
{
	Set_State("sessionExpired");
}
//----------------------------------------------------------------------------
void Avm_Auth::Set_State(const QString &in_state)
{
	if (Current_State == in_state && Error_Key_Text.isEmpty())
		return;

	Current_State = in_state;
	Error_Key_Text.clear();
	qCInfo(AUTH_VM_Log).noquote() << "state" << Current_State;
	emit stateChanged();
}
//----------------------------------------------------------------------------
void Avm_Auth::Set_Error_State(const QString &in_error_key)
{
	Current_State = "error";
	Error_Key_Text = in_error_key;
	qCInfo(AUTH_VM_Log).noquote() << "state" << Current_State << "key" << Error_Key_Text;
	emit stateChanged();
}
//----------------------------------------------------------------------------
