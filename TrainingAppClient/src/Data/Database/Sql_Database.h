#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>

//----------------------------------------------------------------------------
// Локальная SQLite-база приложения. Схема v1 хранит только клиентские данные:
// auth_tokens, кэш users/user_profiles, локальные параметры профиля и outbox.
// Серверный pass_hash никогда не является частью клиентской схемы.
//----------------------------------------------------------------------------
class AsSql_Database : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool open READ Is_Open CONSTANT)
	Q_PROPERTY(QString path READ Database_Path CONSTANT)
	Q_PROPERTY(int schemaVersion READ Schema_Version CONSTANT)

public:
	explicit AsSql_Database(QObject *in_parent = 0);
	~AsSql_Database() override;

	bool Open(const QString &in_path = QString());
	void Close();
	bool Migrate();
	bool Is_Open() const;
	QString Get_Database_Path() const;
	int Get_Schema_Version() const;
	QString Get_Last_Error() const;

	static QString Get_Default_Database_Path();
	static int Get_Current_Schema_Version();

private:
	bool Ensure_Directory(const QString &in_path);
	bool Set_Last_Error(const QString &in_error);
	bool Execute_SQL(const QString &in_sql);
	bool Create_Schema_V1();
	bool Read_Schema_Version(int &out_version);
	bool Write_Schema_Version(int in_version);
	QString Connection_Name() const;

	QSqlDatabase *Database_Handle = 0;
	QString Database_File_Path;
	QString Connection_Name_Text;
	QString Last_Error_Text;
	int Current_Version = 0;
};
//----------------------------------------------------------------------------
