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
class AsMacro_Calculator;
class Avm_Program;
class Avm_Program_Stats;
class Avm_Program_Rules;
class Avm_Program_Edit;
class Avm_Program_Catalog;
class Avm_Today;
class Avm_Macro;
class Avm_Exercises;
class Avm_Exercises_Editor;
class AExercises_Repository;
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
class ADemo_Seeder;

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
	// Идентификатор текущего пользователя. До появления реальной авторизации
	// он единственный источник «кто я»: экраны программы брали литерал «u1»
	// в пяти местах, и любое другое значение — например, посев демо-данных —
	// делало программу невидимой. Теперь значение одно и задаётся здесь.
	Q_PROPERTY(QString currentUserId READ currentUserId CONSTANT)

public:
	explicit AsApp_Context(QObject *in_parent = 0);

	bool Initialize(const QStringList &in_arguments);
	bool Initialize(const QStringList &in_arguments, QSettings &in_settings, const QByteArray &in_environment_url);
	QString apiBaseUrl() const;
	QString apiUrlSource() const;
	QString currentUserId() const;
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
	// Главный экран «Сегодня» на данных программы (issue 27).
	Avm_Today *Today_VM() const;
	// Авторасчёт КБЖУ (спринт S7, issue 50).
	Avm_Macro *Macro_VM() const;
	// Каталог упражнений (спринт U4, issue 30).
	Avm_Exercises *Exercises_VM() const;
	// Редактор своего упражнения (спринт U5).
	Avm_Exercises_Editor *Exercises_Editor_VM() const;
	Avm_Auth_Bridge *Auth_Bridge() const;

	static const QString Default_Api_Base_URL;
	static const QString Environment_Variable_Name;
	static const QString Settings_Key;
	// Флаг запуска, включающий посев демо-данных, и идентификатор
	// пользователя, которому они принадлежат.
	static const QString Demo_Flag;
	static const QString Demo_User_Id;
	static const QString Current_User_Id;
	ADemo_Seeder *Demo_Seeder() const;

private:
	// Создаёт стек модели программы в порядке зависимостей.
	void Create_Program_Stack();

	// Наполняет базу демо-данными, если передан флаг запуска (--demo-seed).
	// Отдельным шагом, а не внутри Create_Program_Stack: он единственный
	// пишущий шаг инициализации, и его должно быть видно при чтении.
	void Seed_Demo_Data(const QStringList &in_arguments);

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
	Avm_Today *Today_VM_Instance = 0;
	AsMacro_Calculator *Macro_Calculator_Instance = 0;
	Avm_Macro *Macro_VM_Instance = 0;
	AExercises_Repository *Exercises_Repository_Instance = 0;
	Avm_Exercises *Exercises_VM_Instance = 0;
	Avm_Exercises_Editor *Exercises_Editor_VM_Instance = 0;
	// Сидер демо-данных. Создаётся только при --demo-seed и живёт до конца
	// приложения: указатель хранится, чтобы не посеять данные дважды.
	ADemo_Seeder *Demo_Seeder_Instance = 0;
	Avm_Auth_Bridge *Auth_Bridge_Instance = 0;
};
//----------------------------------------------------------------------------
