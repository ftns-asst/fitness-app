#pragma once

#include "Data/Auth/Auth_Repository.h"

#include <QObject>
#include <QString>

//----------------------------------------------------------------------------
// ViewModel auth-сценариев. Стабильные состояния для UI: idle, checkingEmail,
// emailTaken, signingUp, loggingIn, error (ключ ошибки — errorKey),
// authenticated, sessionExpired. UI не знает transport-DTO и backend-имён.
//----------------------------------------------------------------------------
class Avm_Auth : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString state READ state NOTIFY stateChanged)
	Q_PROPERTY(QString errorKey READ errorKey NOTIFY stateChanged)
	Q_PROPERTY(bool authenticated READ authenticated NOTIFY stateChanged)

public:
	explicit Avm_Auth(AAuth_Repository *in_repository, QObject *in_parent = 0);

	QString state() const;
	QString errorKey() const;
	bool authenticated() const;

	void Check_Email(const QString &in_email);
	void Signup(const QString &in_email, const QString &in_name, const QString &in_password);
	void Login(const QString &in_email, const QString &in_password);
	void Logout();
	bool Restore_Local_Session();

signals:
	void stateChanged();

private slots:
	void on_Check_Email_Finished(int in_request_id, bool in_free, const AApi_Error &in_error);
	void on_Signup_Finished(int in_request_id, const SAuth_User &in_user, const AApi_Error &in_error);
	void on_Login_Finished(int in_request_id, const SAuth_User &in_user, const AApi_Error &in_error);
	void on_Logged_Out();
	void on_Session_Expired();

private:
	void Set_State(const QString &in_state);
	void Set_Error_State(const QString &in_error_key);

	AAuth_Repository *Repository_Instance;
	QString Current_State;
	QString Error_Key_Text;
};
//----------------------------------------------------------------------------
