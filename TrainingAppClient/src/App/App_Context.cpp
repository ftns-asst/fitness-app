#include "App/App_Context.h"

#include "Data/Auth/Auth_Repository.h"
#include "Data/Auth/Auth_Session.h"
#include "Data/Auth/Token_Store.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Network/Http_Client.h"
#include "Data/Users/Users_Repository.h"
#include "Presentation/Auth_VM.h"

#include <QSettings>
#include <QUrl>
#include <QVariant>

//----------------------------------------------------------------------------
// AsApp_Context
//----------------------------------------------------------------------------
const QString AsApp_Context::Default_Api_Base_URL = "http://fitness.nought.ru/api/v1";
const QString AsApp_Context::Environment_Variable_Name = "TRAINING_APP_API_URL";
const QString AsApp_Context::Settings_Key = "network/apiUrl";
//----------------------------------------------------------------------------
AsApp_Context::AsApp_Context(QObject *in_parent) : QObject(in_parent)
{
}
//----------------------------------------------------------------------------
bool AsApp_Context::Initialize(const QStringList &in_arguments)
{
	QSettings settings;
	QByteArray environment_url;

	environment_url = qgetenv(Environment_Variable_Name.toUtf8().constData());

	return Initialize(in_arguments, settings, environment_url);
}
//----------------------------------------------------------------------------
bool AsApp_Context::Initialize(const QStringList &in_arguments, QSettings &in_settings,
                               const QByteArray &in_environment_url)
{
	QString candidate;
	QString normalized_url;
	QString source;
	QString error;
	bool command_line_found;
	bool success;

	Api_Base_URL.clear();
	Api_URL_Source.clear();
	Initialized = false;
	Last_Error_Text.clear();

	if (Users_Repository_Instance != 0)
	{
		delete Users_Repository_Instance;
		Users_Repository_Instance = 0;
	}

	if (Auth_VM_Instance != 0)
	{
		delete Auth_VM_Instance;
		Auth_VM_Instance = 0;
	}

	if (Auth_Repository_Instance != 0)
	{
		delete Auth_Repository_Instance;
		Auth_Repository_Instance = 0;
	}

	if (Auth_Session_Instance != 0)
	{
		delete Auth_Session_Instance;
		Auth_Session_Instance = 0;
	}

	if (Token_Store_Instance != 0)
	{
		delete Token_Store_Instance;
		Token_Store_Instance = 0;
	}

	if (HTTP_Client_Instance != 0)
	{
		delete HTTP_Client_Instance;
		HTTP_Client_Instance = 0;
	}

	if (SQL_Database_Instance != 0)
	{
		delete SQL_Database_Instance;
		SQL_Database_Instance = 0;
	}

	candidate.clear();
	source.clear();
	command_line_found = false;

	success = Read_Command_Line_URL(in_arguments, candidate, command_line_found, error);

	if (success == false)
	{
		Last_Error_Text = error;
		return false;
	}

	if (command_line_found)
	{
		source = "commandLine";
	}
	else
	{
		candidate = in_settings.value(Settings_Key).toString().trimmed();

		if (candidate.isEmpty() == false)
		{
			source = "settings";
		}
		else
		{
			candidate = QString::fromUtf8(in_environment_url).trimmed();

			if (candidate.isEmpty() == false)
			{
				source = "environment";
			}
			else
			{
				candidate = Default_Api_Base_URL;
				source = "default";
			}
		}
	}

	success = Normalize_Api_Base_URL(candidate, normalized_url, error);

	if (success == false)
	{
		Last_Error_Text = QString("Некорректный API URL из источника %1: %2").arg(source, error);
		return false;
	}

	Api_Base_URL = normalized_url;
	Api_URL_Source = source;
	SQL_Database_Instance = new AsSql_Database(this);

	if (SQL_Database_Instance->Open() == false)
	{
		Last_Error_Text = SQL_Database_Instance->Get_Last_Error();
		delete SQL_Database_Instance;
		SQL_Database_Instance = 0;
		return false;
	}

	HTTP_Client_Instance = new AsHttp_Client(QUrl(Api_Base_URL), this);

	if (HTTP_Client_Instance->Base_URL().isValid() == false)
	{
		Last_Error_Text = HTTP_Client_Instance->Last_Error();
		delete HTTP_Client_Instance;
		HTTP_Client_Instance = 0;
		return false;
	}

	// Composition root: token store поверх приватной SQLite, auth-сессия
	// поверх HTTP-клиента и token store (issue 7).
	Token_Store_Instance = new AsToken_Store(SQL_Database_Instance, this);
	Auth_Session_Instance = new AsAuth_Session(HTTP_Client_Instance, Token_Store_Instance, this);

	// Auth-интеграция (issue 8): репозиторий и ViewModel поверх тех же
	// зависимостей; QML остаётся на mock до снятия backend-блокеров.
	Auth_Repository_Instance = new AAuth_Repository(HTTP_Client_Instance, SQL_Database_Instance, Token_Store_Instance,
	                                                Auth_Session_Instance, this);
	Auth_VM_Instance = new Avm_Auth(Auth_Repository_Instance, this);

	// Профиль с сервера (issue 9): репозиторий поверх auth-сессии и SQLite.
	Users_Repository_Instance = new AUsers_Repository(Auth_Session_Instance, SQL_Database_Instance, this);

	Initialized = true;

	qInfo("App context: API base URL source=%s url=%s", qUtf8Printable(Api_URL_Source), qUtf8Printable(Api_Base_URL));

	return true;
}
//----------------------------------------------------------------------------
QString AsApp_Context::apiBaseUrl() const
{
	return Api_Base_URL;
}
//----------------------------------------------------------------------------
QString AsApp_Context::apiUrlSource() const
{
	return Api_URL_Source;
}
//----------------------------------------------------------------------------
bool AsApp_Context::initialized() const
{
	return Initialized;
}
//----------------------------------------------------------------------------
QString AsApp_Context::Last_Error() const
{
	return Last_Error_Text;
}
//----------------------------------------------------------------------------
AsHttp_Client *AsApp_Context::HTTP_Client() const
{
	return HTTP_Client_Instance;
}
//----------------------------------------------------------------------------
AsSql_Database *AsApp_Context::SQL_Database() const
{
	return SQL_Database_Instance;
}
//----------------------------------------------------------------------------
AsToken_Store *AsApp_Context::Token_Store() const
{
	return Token_Store_Instance;
}
//----------------------------------------------------------------------------
AsAuth_Session *AsApp_Context::Auth_Session() const
{
	return Auth_Session_Instance;
}
//----------------------------------------------------------------------------
AAuth_Repository *AsApp_Context::Auth_Repository() const
{
	return Auth_Repository_Instance;
}
//----------------------------------------------------------------------------
Avm_Auth *AsApp_Context::Auth_VM() const
{
	return Auth_VM_Instance;
}
//----------------------------------------------------------------------------
AUsers_Repository *AsApp_Context::Users_Repository() const
{
	return Users_Repository_Instance;
}
//----------------------------------------------------------------------------
bool AsApp_Context::Normalize_Api_Base_URL(const QString &in_value, QString &out_value, QString &out_error)
{
	QUrl url;
	QString scheme;

	out_value.clear();
	out_error.clear();
	url = QUrl(in_value.trimmed(), QUrl::StrictMode);
	scheme = url.scheme().toLower();

	if (url.isValid() == false || url.isRelative() || url.host().isEmpty())
	{
		out_error = "ожидается абсолютный URL с именем хоста";
		return false;
	}

	if (scheme != "http" && scheme != "https")
	{
		out_error = "разрешены только схемы http и https";
		return false;
	}

	if (url.userInfo().isEmpty() == false)
	{
		out_error = "credentials запрещены в base URL";
		return false;
	}

	if (url.hasQuery() || url.hasFragment())
	{
		out_error = "query и fragment запрещены в base URL";
		return false;
	}

	url.setScheme(scheme);
	url = url.adjusted(QUrl::StripTrailingSlash);
	out_value = url.toString(QUrl::FullyEncoded);

	return true;
}
//----------------------------------------------------------------------------
bool AsApp_Context::Read_Command_Line_URL(const QStringList &in_arguments, QString &out_value, bool &out_found,
                                          QString &out_error)
{
	int found_count;

	out_value.clear();
	out_error.clear();
	out_found = false;
	found_count = 0;

	for (int i = 0; i < in_arguments.size(); ++i)
	{
		if (in_arguments.at(i) != "--api-url")
			continue;

		++found_count;

		if (i + 1 >= in_arguments.size() || in_arguments.at(i + 1).startsWith("--"))
		{
			out_error = "После --api-url требуется значение";
			return false;
		}

		out_value = in_arguments.at(i + 1).trimmed();
		++i;
	}

	if (found_count > 1)
	{
		out_error = "Параметр --api-url указан более одного раза";
		return false;
	}

	if (found_count == 1 && out_value.isEmpty())
	{
		out_error = "Значение --api-url не должно быть пустым";
		return false;
	}

	out_found = found_count == 1;

	return true;
}
//----------------------------------------------------------------------------