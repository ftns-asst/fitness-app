#pragma once

#include "Data/Auth/Auth_Repository.h"
#include "Data/Auth/Auth_Session.h"
#include "Data/Users/Users_Repository.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QVariantMap>

//-------------------------------------------------------------------------------------------------------------
// QML-мост реального auth (issue 8, финальное подключение UI). Повторяет
// контракт MockApi: методы возвращают requestId, результат приходит сигналом
// requestFinished(id, operation, ok, data, error); user-DTO — lowerCamelCase
// (id/name/email/createdAt) по правилу границы Presentation. Реальные
// access/refresh токены остаются в C++ (SQLite) и в QML не передаются.
// Имена методов — lowerCamelCase намеренно: это QML-граница (§7 docs/01).
//-------------------------------------------------------------------------------------------------------------
class Avm_Auth_Bridge : public QObject
{
	Q_OBJECT

public:
	explicit Avm_Auth_Bridge(AAuth_Repository *in_auth_repository, AUsers_Repository *in_users_repository,
	                         AsAuth_Session *in_session, QObject *in_parent = 0);

	Q_INVOKABLE int checkEmail(const QString &in_email);
	Q_INVOKABLE int login(const QString &in_email, const QString &in_password);
	Q_INVOKABLE int signup(const QVariantMap &in_request);
	Q_INVOKABLE void logout();
	Q_INVOKABLE bool restoreSession();
	Q_INVOKABLE bool saveLocalProfile(const QVariantMap &in_local_profile);
	Q_INVOKABLE void clearLocalProfile();

signals:
	void requestFinished(int requestId, const QString &operation, bool ok, const QVariantMap &data,
	                     const QVariantMap &error);
	void loggedOut();
	void sessionExpired();
	void sessionRestored(const QVariantMap &user);

private slots:
	void on_Check_Email_Finished(int in_request_id, bool in_free, const AApi_Error &in_error);
	void on_Signup_Finished(int in_request_id, const SAuth_User &in_user, const AApi_Error &in_error);
	void on_Login_Finished(int in_request_id, const SAuth_User &in_user, const AApi_Error &in_error);
	void on_Logged_Out();
	void on_Session_Expired();

private:
	void Finish_Session_Operation(const QString &in_operation, int in_request_id, const SAuth_User &in_user,
	                              const AApi_Error &in_error);
	void Finish_Check_Email(int in_request_id, bool in_free, const AApi_Error &in_error);
	static QVariantMap User_DTO(const SAuth_User &in_user);
	static QVariantMap User_DTO(const SUsers_User &in_user);
	static QVariantMap Error_DTO(const AApi_Error &in_error);
	QString Take_Operation(int in_request_id);

	AAuth_Repository *Auth_Repository_Instance;
	AUsers_Repository *Users_Repository_Instance;
	AsAuth_Session *Session_Instance;
	QHash<int, QString> Pending_Operations; // request id → операция
	QString Current_User_ID;
};
//-------------------------------------------------------------------------------------------------------------