#include "App/App_Context.h"

#include "Data/Auth/Auth_Repository.h"
#include "Data/Auth/Auth_Session.h"
#include "Data/Auth/Token_Store.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Network/Http_Client.h"
#include "Presentation/Auth_VM.h"

#include <QSettings>
#include <QTemporaryDir>
#include <QtTest>

class AApp_Context_Test : public QObject
{
	Q_OBJECT

private slots:
	void defaultUrlIsUsed();
	void environmentOverridesDefault();
	void settingsOverrideEnvironment();
	void commandLineOverridesSettings();
	void urlIsNormalized();
	void invalidSelectedSourceFails();
	void missingCommandLineValueFails();
	void duplicateCommandLineValueFails();
	void credentialsAndQueryAreRejected();
};

//----------------------------------------------------------------------------
void AApp_Context_Test::defaultUrlIsUsed()
{
	QTemporaryDir directory;
	QSettings settings(directory.filePath("settings.ini"), QSettings::IniFormat);
	AsApp_Context context;

	QVERIFY(directory.isValid());
	QVERIFY(context.Initialize({"app"}, settings, QByteArray()));
	QCOMPARE(context.apiBaseUrl(), AsApp_Context::Default_Api_Base_URL);
	QCOMPARE(context.apiUrlSource(), QString("default"));
	QVERIFY(context.initialized());
	QVERIFY(context.HTTP_Client() != 0);
	QVERIFY(context.SQL_Database() != 0);
	QVERIFY(context.SQL_Database()->Is_Open());
	QCOMPARE(context.SQL_Database()->Get_Schema_Version(), 1);
	QCOMPARE(context.HTTP_Client()->Base_URL(), QUrl(AsApp_Context::Default_Api_Base_URL));
	QVERIFY(context.Token_Store() != 0);
	QVERIFY(context.Auth_Session() != 0);
	QVERIFY(context.Token_Store()->Has_Tokens() == false);
	QVERIFY(context.Auth_Session()->Has_Session() == false);
	QVERIFY(context.Auth_Repository() != 0);
	QVERIFY(context.Auth_VM() != 0);
	QCOMPARE(context.Auth_VM()->state(), QString("idle"));
	QVERIFY(context.Auth_Repository()->Has_Local_Session() == false);
	QVERIFY(context.Users_Repository() != 0);
}
//----------------------------------------------------------------------------
void AApp_Context_Test::environmentOverridesDefault()
{
	QTemporaryDir directory;
	QSettings settings(directory.filePath("settings.ini"), QSettings::IniFormat);
	AsApp_Context context;

	QVERIFY(context.Initialize({"app"}, settings, "https://env.example/api/v1"));
	QCOMPARE(context.apiBaseUrl(), QString("https://env.example/api/v1"));
	QCOMPARE(context.apiUrlSource(), QString("environment"));
}
//----------------------------------------------------------------------------
void AApp_Context_Test::settingsOverrideEnvironment()
{
	QTemporaryDir directory;
	QSettings settings(directory.filePath("settings.ini"), QSettings::IniFormat);
	AsApp_Context context;

	settings.setValue(AsApp_Context::Settings_Key, "https://settings.example/api/v1");

	QVERIFY(context.Initialize({"app"}, settings, "https://env.example/api/v1"));
	QCOMPARE(context.apiBaseUrl(), QString("https://settings.example/api/v1"));
	QCOMPARE(context.apiUrlSource(), QString("settings"));
}
//----------------------------------------------------------------------------
void AApp_Context_Test::commandLineOverridesSettings()
{
	QTemporaryDir directory;
	QSettings settings(directory.filePath("settings.ini"), QSettings::IniFormat);
	AsApp_Context context;

	settings.setValue(AsApp_Context::Settings_Key, "https://settings.example/api/v1");

	QVERIFY(
	    context.Initialize({"app", "--api-url", "https://cli.example/api/v2"}, settings, "https://env.example/api/v1"));
	QCOMPARE(context.apiBaseUrl(), QString("https://cli.example/api/v2"));
	QCOMPARE(context.apiUrlSource(), QString("commandLine"));
}
//----------------------------------------------------------------------------
void AApp_Context_Test::urlIsNormalized()
{
	QTemporaryDir directory;
	QSettings settings(directory.filePath("settings.ini"), QSettings::IniFormat);
	AsApp_Context context;

	QVERIFY(context.Initialize({"app", "--api-url", "HTTPS://Example.COM/api/v1/"}, settings, QByteArray()));
	QCOMPARE(context.apiBaseUrl(), QString("https://example.com/api/v1"));
}
//----------------------------------------------------------------------------
void AApp_Context_Test::invalidSelectedSourceFails()
{
	QTemporaryDir directory;
	QSettings settings(directory.filePath("settings.ini"), QSettings::IniFormat);
	AsApp_Context context;

	settings.setValue(AsApp_Context::Settings_Key, "file:///tmp/api");

	QVERIFY(context.Initialize({"app"}, settings, "https://env.example/api/v1") == false);
	QVERIFY(context.initialized() == false);
	QVERIFY(context.Last_Error().contains("settings"));
}
//----------------------------------------------------------------------------
void AApp_Context_Test::missingCommandLineValueFails()
{
	QTemporaryDir directory;
	QSettings settings(directory.filePath("settings.ini"), QSettings::IniFormat);
	AsApp_Context context;

	QVERIFY(context.Initialize({"app", "--api-url"}, settings, QByteArray()) == false);
	QVERIFY(context.Last_Error().contains("требуется значение"));
}
//----------------------------------------------------------------------------
void AApp_Context_Test::duplicateCommandLineValueFails()
{
	QTemporaryDir directory;
	QSettings settings(directory.filePath("settings.ini"), QSettings::IniFormat);
	AsApp_Context context;

	QVERIFY(context.Initialize({"app", "--api-url", "https://one.example", "--api-url", "https://two.example"},
	                           settings, QByteArray()) == false);
	QVERIFY(context.Last_Error().contains("более одного раза"));
}
//----------------------------------------------------------------------------
void AApp_Context_Test::credentialsAndQueryAreRejected()
{
	QTemporaryDir directory;
	QSettings settings(directory.filePath("settings.ini"), QSettings::IniFormat);
	AsApp_Context context;

	QVERIFY(context.Initialize({"app", "--api-url", "https://user:password@example.com/api"}, settings, QByteArray()) ==
	        false);
	QVERIFY(context.Last_Error().contains("credentials"));

	QVERIFY(context.Initialize({"app", "--api-url", "https://example.com/api?environment=dev"}, settings,
	                           QByteArray()) == false);
	QVERIFY(context.Last_Error().contains("query"));
}
//----------------------------------------------------------------------------

QTEST_MAIN(AApp_Context_Test)

#include "tst_App_Context.moc"