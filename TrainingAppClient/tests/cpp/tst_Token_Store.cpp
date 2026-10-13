#include "Data/Auth/Token_Store.h"
#include "Data/Database/Sql_Database.h"

#include <QDateTime>
#include <QTemporaryDir>
#include <QtTest>

class AToken_Store_Test : public QObject
{
	Q_OBJECT

private slots:
	void initiallyHasNoTokens();
	void saveAndLoadRoundTrip();
	void saveReplacesPairOnRotation();
	void clearRemovesTokens();
	void tokensPersistAcrossReopen();
	void rejectsIncompleteTokens();
};

//----------------------------------------------------------------------------
void AToken_Store_Test::initiallyHasNoTokens()
{
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	SStored_Tokens tokens;

	QVERIFY(directory.isValid());
	QVERIFY(database.Open(directory.filePath("cache.sqlite3")));

	QVERIFY(store.Has_Tokens() == false);
	QVERIFY(store.Load_Tokens(tokens) == false);
	QVERIFY(tokens.User_ID.isEmpty());
	QVERIFY(tokens.Access_Token.isEmpty());
	QVERIFY(tokens.Refresh_Token.isEmpty());
	QCOMPARE(store.Last_Error(), QString());
}
//----------------------------------------------------------------------------
void AToken_Store_Test::saveAndLoadRoundTrip()
{
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	SStored_Tokens tokens;
	QDateTime access_expiry;
	QDateTime refresh_expiry;

	QVERIFY(directory.isValid());
	QVERIFY(database.Open(directory.filePath("cache.sqlite3")));
	access_expiry = QDateTime::fromString("2026-09-23T12:00:00Z", Qt::ISODate);
	refresh_expiry = QDateTime::fromString("2026-09-24T12:00:00Z", Qt::ISODate);

	QVERIFY(store.Save_Tokens("user-1", "access-1", "refresh-1", access_expiry, refresh_expiry));
	QVERIFY(store.Has_Tokens());
	QVERIFY(store.Load_Tokens(tokens));
	QCOMPARE(tokens.User_ID, QString("user-1"));
	QCOMPARE(tokens.Access_Token, QString("access-1"));
	QCOMPARE(tokens.Refresh_Token, QString("refresh-1"));
	QCOMPARE(tokens.Access_Expires_At, access_expiry);
	QCOMPARE(tokens.Refresh_Expires_At, refresh_expiry);
	QCOMPARE(store.Last_Error(), QString());
}
//----------------------------------------------------------------------------
void AToken_Store_Test::saveReplacesPairOnRotation()
{
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	SStored_Tokens tokens;
	QDateTime access_expiry;
	QDateTime refresh_expiry;

	QVERIFY(directory.isValid());
	QVERIFY(database.Open(directory.filePath("cache.sqlite3")));
	access_expiry = QDateTime::fromString("2026-09-23T12:00:00Z", Qt::ISODate);
	refresh_expiry = QDateTime::fromString("2026-09-24T12:00:00Z", Qt::ISODate);

	QVERIFY(store.Save_Tokens("user-1", "access-1", "refresh-1", access_expiry, refresh_expiry));
	QVERIFY(store.Save_Tokens("user-1", "access-2", "refresh-2", QDateTime(), QDateTime()));
	QVERIFY(store.Load_Tokens(tokens));
	QCOMPARE(tokens.User_ID, QString("user-1"));
	QCOMPARE(tokens.Access_Token, QString("access-2"));
	QCOMPARE(tokens.Refresh_Token, QString("refresh-2"));
	QVERIFY(tokens.Access_Expires_At.isNull());
	QVERIFY(tokens.Refresh_Expires_At.isNull());
}
//----------------------------------------------------------------------------
void AToken_Store_Test::clearRemovesTokens()
{
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);
	SStored_Tokens tokens;

	QVERIFY(directory.isValid());
	QVERIFY(database.Open(directory.filePath("cache.sqlite3")));
	QVERIFY(store.Save_Tokens("user-1", "access-1", "refresh-1", QDateTime(), QDateTime()));

	QVERIFY(store.Clear_Tokens());
	QVERIFY(store.Has_Tokens() == false);
	QVERIFY(store.Load_Tokens(tokens) == false);

	// Повторная очистка результат не меняет.
	QVERIFY(store.Clear_Tokens());
	QVERIFY(store.Has_Tokens() == false);
	QCOMPARE(store.Last_Error(), QString());
}
//----------------------------------------------------------------------------
void AToken_Store_Test::tokensPersistAcrossReopen()
{
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store second_store(&database);
	SStored_Tokens tokens;
	QString path;

	QVERIFY(directory.isValid());
	path = directory.filePath("cache.sqlite3");
	QVERIFY(database.Open(path));

	{
		AsToken_Store store(&database);

		QVERIFY(store.Save_Tokens("user-9", "persist-access", "persist-refresh", QDateTime(), QDateTime()));
	}

	// Cold start: новый запуск приложения открывает ту же базу.
	database.Close();
	QVERIFY(database.Open(path));
	QVERIFY(second_store.Load_Tokens(tokens));
	QCOMPARE(tokens.User_ID, QString("user-9"));
	QCOMPARE(tokens.Access_Token, QString("persist-access"));
	QCOMPARE(tokens.Refresh_Token, QString("persist-refresh"));
}
//----------------------------------------------------------------------------
void AToken_Store_Test::rejectsIncompleteTokens()
{
	QTemporaryDir directory;
	AsSql_Database database;
	AsToken_Store store(&database);

	QVERIFY(directory.isValid());
	QVERIFY(database.Open(directory.filePath("cache.sqlite3")));

	QVERIFY(store.Save_Tokens("", "access-1", "refresh-1", QDateTime(), QDateTime()) == false);
	QVERIFY(store.Save_Tokens("user-1", "", "refresh-1", QDateTime(), QDateTime()) == false);
	QVERIFY(store.Save_Tokens("user-1", "access-1", "", QDateTime(), QDateTime()) == false);
	QVERIFY(store.Last_Error().isEmpty() == false);
	QVERIFY(store.Has_Tokens() == false);

	QVERIFY(store.Save_Tokens("user-1", "access-1", "refresh-1", QDateTime(), QDateTime()));
	QVERIFY(store.Has_Tokens());
}
//----------------------------------------------------------------------------

QTEST_MAIN(AToken_Store_Test)

#include "tst_Token_Store.moc"
