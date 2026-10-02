#pragma once

#include <QByteArray>

#include "Domain/Error_State.h"

class AProgram_Repository;
class ARule_Repository;
class AMeasurement_Repository;
class AStats_Repository;
class AWorkout_Repository;
class AsProgram_Service;
class AsWorkout_Logger;
class Avm_Program;
class Avm_Program_Stats;
class Avm_Program_Rules;
class Avm_Program_Edit;
class Avm_Program_Catalog;
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
class Avm_Auth_Bridge;

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

	// Стек модели программы (срез W11). Создаётся в том же порядке, что и
	// зависимости: репозитории → доменные сервисы → ViewModel. Экземпляры
	// отдаются экранам 41–44 и логгеру тренировок.
	AProgram_Repository *Program_Repository() const;
	ARule_Repository *Rule_Repository() const;
	AMeasurement_Repository *Measurement_Repository() const;
	AStats_Repository *Stats_Repository() const;
	AWorkout_Repository *Workout_Repository() const;
	AsProgram_Service *Program_Service() const;
	AsWorkout_Logger *Workout_Logger() const;
	Avm_Program *Program_VM() const;
	Avm_Program_Stats *Program_Stats_VM() const;
	Avm_Program_Rules *Program_Rules_VM() const;
	Avm_Program_Edit *Program_Edit_VM() const;
	Avm_Program_Catalog *Program_Catalog_VM() const;
	Avm_Auth_Bridge *Auth_Bridge() const;

	static const QString Default_Api_Base_URL;
	static const QString Environment_Variable_Name;
	static const QString Settings_Key;

private:
	// Создаёт стек модели программы в порядке зависимостей.
	void Create_Program_Stack();

	static bool Normalize_Api_Base_URL(const QString &in_value, QString &out_value, QString &out_error);
	static bool Read_Command_Line_URL(const QStringList &in_arguments, QString &out_value, bool &out_found,
	                                  QString &out_error);

	// Хранилище ошибки (рефакторинг ошибок): состояние и логирование в
	// APP_Context_Log живут в AError_State.
	bool Set_Last_Error(const QString &in_error);

	QString Api_Base_URL;
	QString Api_URL_Source;
	bool Initialized = false;
	AError_State Error_State;
	AsHttp_Client *HTTP_Client_Instance = 0;
	AsSql_Database *SQL_Database_Instance = 0;
	AsToken_Store *Token_Store_Instance = 0;
	AsAuth_Session *Auth_Session_Instance = 0;
	AAuth_Repository *Auth_Repository_Instance = 0;
	Avm_Auth *Auth_VM_Instance = 0;
	AUsers_Repository *Users_Repository_Instance = 0;

	// Стек модели программы: точка создания и владения всех зависимостей
	// среза W (§3.1–3.9). Порядок уничтожения обратен порядку создания.
	AProgram_Repository *Program_Repository_Instance = 0;
	ARule_Repository *Rule_Repository_Instance = 0;
	AMeasurement_Repository *Measurement_Repository_Instance = 0;
	AStats_Repository *Stats_Repository_Instance = 0;
	AWorkout_Repository *Workout_Repository_Instance = 0;
	AsProgram_Service *Program_Service_Instance = 0;
	AsWorkout_Logger *Workout_Logger_Instance = 0;
	Avm_Program *Program_VM_Instance = 0;
	Avm_Program_Stats *Program_Stats_VM_Instance = 0;
	Avm_Program_Rules *Program_Rules_VM_Instance = 0;
	Avm_Program_Edit *Program_Edit_VM_Instance = 0;
	Avm_Program_Catalog *Program_Catalog_VM_Instance = 0;
	Avm_Auth_Bridge *Auth_Bridge_Instance = 0;
};
//----------------------------------------------------------------------------
