#include "Data/Network/Api_Error.h"

#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonParseError>
#include <QLoggingCategory>
#include <QNetworkReply>

Q_LOGGING_CATEGORY(API_Error_Log, "trainingapp.api.error")

//----------------------------------------------------------------------------
// AApi_Error
//----------------------------------------------------------------------------
AApi_Error::AApi_Error()
{
}
//----------------------------------------------------------------------------
AApi_Error::AApi_Error(const QString &in_key, const QString &in_message, int in_http_status_code,
                       int in_network_error_code)
    : Error_Key(in_key), Error_Message(in_message), HTTP_Status(in_http_status_code),
      Network_Error(in_network_error_code)
{
}
//----------------------------------------------------------------------------
QString AApi_Error::Key() const
{
	return Error_Key;
}
//----------------------------------------------------------------------------
QString AApi_Error::Message() const
{
	return Error_Message;
}
//----------------------------------------------------------------------------
int AApi_Error::HTTP_Status_Code() const
{
	return HTTP_Status;
}
//----------------------------------------------------------------------------
int AApi_Error::Network_Error_Code() const
{
	return Network_Error;
}
//----------------------------------------------------------------------------
bool AApi_Error::Is_Error() const
{
	return Error_Key.isEmpty() == false;
}
//----------------------------------------------------------------------------
AApi_Error AApi_Error::From_Response(int in_http_status_code, int in_network_error_code,
                                     const QString &in_network_error_text, const QByteArray &in_body)
{
	QJsonParseError parse_error;
	QJsonDocument document;
	QJsonObject object;
	QString key;
	QString message;

	if (in_body.isEmpty() == false)
	{
		document = QJsonDocument::fromJson(in_body, &parse_error);

		if (parse_error.error == QJsonParseError::NoError && document.isObject())
		{
			object = document.object();

			if (object.value("key").isString())
				key = object.value("key").toString().trimmed();

			if (object.value("message").isString())
				message = object.value("message").toString();
		}
	}

	if (key.isEmpty())
		key = Fallback_Key(in_http_status_code, in_network_error_code);

	if (message.isEmpty() && in_http_status_code <= 0)
		message = in_network_error_text;

	if (key == "undefined_error")
	{
		// Backend message/body намеренно не пишется: он может содержать PII.
		qCWarning(API_Error_Log).noquote()
		    << "undefined_error status" << in_http_status_code << "network" << in_network_error_code;
	}

	return AApi_Error(key, message, in_http_status_code, in_network_error_code);
}
//----------------------------------------------------------------------------
QString AApi_Error::Fallback_Key(int in_http_status_code, int in_network_error_code)
{
	if (in_http_status_code <= 0)
	{
		if (in_network_error_code == QNetworkReply::TimeoutError)
			return "timeout";

		return "network_error";
	}

	switch (in_http_status_code)
	{
	case 400:
		return "validation_error";
	case 401:
		return "invalid_token";
	case 404:
		return "not_found";
	case 408:
		return "timeout";
	case 429:
		return "rate_limited";
	default:
		break;
	}

	if (in_http_status_code >= 500 && in_http_status_code <= 599)
		return "server_error";

	return "undefined_error";
}
//----------------------------------------------------------------------------
