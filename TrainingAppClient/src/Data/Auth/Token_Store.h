#pragma once

#include <QDateTime>
#include <QObject>
#include <QString>

class AsSql_Database;

//----------------------------------------------------------------------------
// Сохранённая пара токенов активной сессии. Expiry-поля могут быть пустыми:
// backend пока не публикует срок действия токенов (docs/03-api-contract.md).
//----------------------------------------------------------------------------
struct SStored_Tokens
{
	QString User_ID;
	QString Access_Token;
	QString Refresh_Token;
	QDateTime Access_Expires_At;
	QDateTime Refresh_Expires_At;
};

//----------------------------------------------------------------------------
// Хранилище пары токенов в приватной SQLite (таблица auth_tokens, единственная
// строка id = 1). Ротация заменяет пару атомарным upsert. Значения токенов
// никогда не логируются.
//----------------------------------------------------------------------------
class AsToken_Store : public QObject
{
	Q_OBJECT

public:
	explicit AsToken_Store(AsSql_Database *in_database, QObject *in_parent = 0);

	bool Save_Tokens(const QString &in_user_id, const QString &in_access_token, const QString &in_refresh_token,
	                 const QDateTime &in_access_expires_at, const QDateTime &in_refresh_expires_at);
	bool Load_Tokens(SStored_Tokens &out_tokens) const;
	bool Clear_Tokens();
	bool Has_Tokens() const;
	QString Last_Error() const;

private:
	bool Set_Error(const QString &in_error);
	static QString To_ISO_Date(const QDateTime &in_moment);
	static QDateTime From_ISO_Date(const QString &in_value);

	AsSql_Database *Database_Instance;
	QString Last_Error_Text;
};
//----------------------------------------------------------------------------
