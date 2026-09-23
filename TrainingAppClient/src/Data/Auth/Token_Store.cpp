#include "Data/Auth/Token_Store.h"

#include "Data/Database/Sql_Database.h"

#include <QLoggingCategory>
#include <QSqlError>
#include <QSqlQuery>
#include <QVariant>

Q_LOGGING_CATEGORY(AUTH_Store_Log, "trainingapp.auth.store")

//----------------------------------------------------------------------------
// AsToken_Store
//----------------------------------------------------------------------------
AsToken_Store::AsToken_Store(AsSql_Database *in_database, QObject *in_parent)
    : QObject(in_parent), Database_Instance(in_database)
{
}
//----------------------------------------------------------------------------
bool AsToken_Store::Save_Tokens(const QString &in_user_id, const QString &in_access_token,
                                const QString &in_refresh_token, const QDateTime &in_access_expires_at,
                                const QDateTime &in_refresh_expires_at)
{
	Last_Error_Text.clear();

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return Set_Error("SQLite не открыта");

	QSqlQuery query(Database_Instance->Sql_Connection());

	if (in_user_id.trimmed().isEmpty() || in_access_token.isEmpty() || in_refresh_token.isEmpty())
		return Set_Error("Нельзя сохранять пустые токены сессии");

	query.prepare("INSERT INTO auth_tokens(id, user_id, access_token, refresh_token, access_expires_at, "
	              "refresh_expires_at, updated_at) VALUES(1, ?, ?, ?, ?, ?, ?) "
	              "ON CONFLICT(id) DO UPDATE SET user_id = excluded.user_id, access_token = excluded.access_token, "
	              "refresh_token = excluded.refresh_token, access_expires_at = excluded.access_expires_at, "
	              "refresh_expires_at = excluded.refresh_expires_at, updated_at = excluded.updated_at");
	query.addBindValue(in_user_id);
	query.addBindValue(in_access_token);
	query.addBindValue(in_refresh_token);
	query.addBindValue(To_ISO_Date(in_access_expires_at));
	query.addBindValue(To_ISO_Date(in_refresh_expires_at));
	query.addBindValue(To_ISO_Date(QDateTime::currentDateTimeUtc()));

	if (query.exec() == false)
		return Set_Error(QString("Не удалось сохранить токены: %1").arg(query.lastError().text()));

	qCInfo(AUTH_Store_Log).noquote() << "tokens saved";

	return true;
}
//----------------------------------------------------------------------------
bool AsToken_Store::Load_Tokens(SStored_Tokens &out_tokens) const
{
	out_tokens = SStored_Tokens();

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
	{
		// Ничего не возвращаем: чтение без открытой базы — не ошибка для вызывающего.
		return false;
	}

	QSqlQuery query(Database_Instance->Sql_Connection());

	if (query.exec("SELECT user_id, access_token, refresh_token, access_expires_at, refresh_expires_at "
	               "FROM auth_tokens WHERE id = 1") == false)
	{
		qCWarning(AUTH_Store_Log).noquote() << "tokens load failed";
		return false;
	}

	if (query.next() == false)
		return false;

	out_tokens.User_ID = query.value(0).toString();
	out_tokens.Access_Token = query.value(1).toString();
	out_tokens.Refresh_Token = query.value(2).toString();
	out_tokens.Access_Expires_At = From_ISO_Date(query.value(3).toString());
	out_tokens.Refresh_Expires_At = From_ISO_Date(query.value(4).toString());

	return out_tokens.Access_Token.isEmpty() == false;
}
//----------------------------------------------------------------------------
bool AsToken_Store::Clear_Tokens()
{
	Last_Error_Text.clear();

	if (Database_Instance == 0 || Database_Instance->Is_Open() == false)
		return Set_Error("SQLite не открыта");

	QSqlQuery query(Database_Instance->Sql_Connection());

	if (query.exec("DELETE FROM auth_tokens") == false)
		return Set_Error(QString("Не удалось очистить токены: %1").arg(query.lastError().text()));

	qCInfo(AUTH_Store_Log).noquote() << "tokens cleared";

	return true;
}
//----------------------------------------------------------------------------
bool AsToken_Store::Has_Tokens() const
{
	SStored_Tokens tokens;

	return Load_Tokens(tokens);
}
//----------------------------------------------------------------------------
QString AsToken_Store::Last_Error() const
{
	return Last_Error_Text;
}
//----------------------------------------------------------------------------
bool AsToken_Store::Set_Error(const QString &in_error)
{
	Last_Error_Text = in_error;
	qCWarning(AUTH_Store_Log).noquote() << in_error; // текст ошибки не содержит значений токенов

	return false;
}
//----------------------------------------------------------------------------
QString AsToken_Store::To_ISO_Date(const QDateTime &in_moment)
{
	if (in_moment.isValid() == false)
		return QString();

	return in_moment.toUTC().toString(Qt::ISODate);
}
//----------------------------------------------------------------------------
QDateTime AsToken_Store::From_ISO_Date(const QString &in_value)
{
	if (in_value.trimmed().isEmpty())
		return QDateTime();

	return QDateTime::fromString(in_value, Qt::ISODate).toUTC();
}
//----------------------------------------------------------------------------
