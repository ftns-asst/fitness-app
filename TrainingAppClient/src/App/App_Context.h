#pragma once

#include <QByteArray>
#include <QObject>
#include <QSettings>
#include <QString>
#include <QStringList>

class AAuth_Repository;
class AsAuth_Session;
class AsHttp_Client;
class AsSql_Database;
class AsToken_Store;
class AUsers_Repository;
class Avm_Auth;

//----------------------------------------------------------------------------
// Корневой контекст приложения. Владеет конфигурацией окружения и единым
// AsHttp_Client; SQL- и presentation-сервисы добавляются последующими issues и
// создаются здесь в порядке их зависимостей.
//----------------------------------------------------------------------------
class AsApp_Context : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString apiBaseUrl READ apiBaseUrl CONSTANT)
	Q_PROPERTY(QString apiUrlSource READ apiUrlSource CONSTANT)
	Q_PROPERTY(bool initialized READ initialized CONSTANT)

public:
	explicit AsApp_Context(QObject *in_parent = 0);

	bool Initialize(const QStringList &in_arguments);
	bool Initialize(const QStringList &in_arguments, QSettings &in_settings, const QByteArray &in_environment_url);
	QString apiBaseUrl() const;
	QString apiUrlSource() const;
	bool initialized() const;
	QString Last_Error() const;
	AsHttp_Client *HTTP_Client() const;
	AsSql_Database *SQL_Database() const;
	AsToken_Store *Token_Store() const;
	AsAuth_Session *Auth_Session() const;
	AAuth_Repository *Auth_Repository() const;
	Avm_Auth *Auth_VM() const;
	AUsers_Repository *Users_Repository() const;

	static const QString Default_Api_Base_URL;
	static const QString Environment_Variable_Name;
	static const QString Settings_Key;

private:
	static bool Normalize_Api_Base_URL(const QString &in_value, QString &out_value, QString &out_error);
	static bool Read_Command_Line_URL(const QStringList &in_arguments, QString &out_value, bool &out_found,
	                                  QString &out_error);

	QString Api_Base_URL;
	QString Api_URL_Source;
	bool Initialized = false;
	QString Last_Error_Text;
	AsHttp_Client *HTTP_Client_Instance = 0;
	AsSql_Database *SQL_Database_Instance = 0;
	AsToken_Store *Token_Store_Instance = 0;
	AsAuth_Session *Auth_Session_Instance = 0;
	AAuth_Repository *Auth_Repository_Instance = 0;
	Avm_Auth *Auth_VM_Instance = 0;
	AUsers_Repository *Users_Repository_Instance = 0;
};
//----------------------------------------------------------------------------
