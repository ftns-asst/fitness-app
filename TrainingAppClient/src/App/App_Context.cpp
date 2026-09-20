#include "App/App_Context.h"

#include "Data/Network/Http_Client.h"

#include <QSettings>
#include <QUrl>
#include <QVariant>

//----------------------------------------------------------------------------
// AsApp_Context
//----------------------------------------------------------------------------
AsApp_Context::AsApp_Context(QObject *in_parent) : QObject(in_parent)
{
}
//----------------------------------------------------------------------------
bool AsApp_Context::Initialize(const QStringList &in_arguments)
{
	QSettings settings;
	QByteArray environment_url;

	environment_url = qgetenv(Environment_Variable_Name().toUtf8().constData());

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

	if (HTTP_Client_Instance != 0)
	{
		delete HTTP_Client_Instance;
		HTTP_Client_Instance = 0;
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
		candidate = in_settings.value(Settings_Key()).toString().trimmed();

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
				candidate = Default_Api_Base_URL();
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
	HTTP_Client_Instance = new AsHttp_Client(QUrl(Api_Base_URL), this);

	if (HTTP_Client_Instance->Base_URL().isValid() == false)
	{
		Last_Error_Text = HTTP_Client_Instance->Last_Error();
		delete HTTP_Client_Instance;
		HTTP_Client_Instance = 0;
		return false;
	}

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
QString AsApp_Context::Default_Api_Base_URL()
{
	return "http://fitness.nought.ru/api/v1";
}
//----------------------------------------------------------------------------
QString AsApp_Context::Environment_Variable_Name()
{
	return "TRAINING_APP_API_URL";
}
//----------------------------------------------------------------------------
QString AsApp_Context::Settings_Key()
{
	return "network/apiUrl";
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
	int i;

	out_value.clear();
	out_error.clear();
	out_found = false;
	found_count = 0;

	for (i = 0; i < in_arguments.size(); ++i)
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