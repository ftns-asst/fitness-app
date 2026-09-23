#include "Data/Network/Http_Client.h"

#include <QJsonParseError>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QNetworkRequest>
#include <QTimer>
#include <QVariant>

Q_LOGGING_CATEGORY(HTTP_Log, "trainingapp.http")

//----------------------------------------------------------------------------
// Состояние одной логической операции, общее для всех её retry attempts.
// Владелец — AsHttp_Client через QObject parent.
//----------------------------------------------------------------------------
class AHttp_Operation : public QObject
{
public:
	explicit AHttp_Operation(QObject *in_parent = 0) : QObject(in_parent)
	{
	}

	SHttp_Request Request;
	QUrl URL;
	int Request_Id = 0;
	int Attempt_Count = 0;
};

//----------------------------------------------------------------------------
// AsHttp_Client
//----------------------------------------------------------------------------
AsHttp_Client::AsHttp_Client(const QUrl &in_base_url, QObject *in_parent) : QObject(in_parent), Network_Manager()
{
	Set_Base_URL(in_base_url);
	qRegisterMetaType<SHttp_Response>("SHttp_Response");
}
//----------------------------------------------------------------------------
bool AsHttp_Client::Set_Base_URL(const QUrl &in_base_url)
{
	QUrl normalized_url;
	QString scheme;

	Last_Error_Text.clear();
	API_Base_URL.clear();
	normalized_url = in_base_url;
	scheme = normalized_url.scheme().toLower();

	if (normalized_url.isValid() == false || normalized_url.isRelative() || normalized_url.host().isEmpty())
	{
		Last_Error_Text = "API base URL должен быть абсолютным и содержать host";
		return false;
	}

	if (scheme != "http" && scheme != "https")
	{
		Last_Error_Text = "API base URL поддерживает только http и https";
		return false;
	}

	if (normalized_url.userInfo().isEmpty() == false || normalized_url.hasQuery() || normalized_url.hasFragment())
	{
		Last_Error_Text = "API base URL не должен содержать credentials, query или fragment";
		return false;
	}

	normalized_url.setScheme(scheme);
	normalized_url = normalized_url.adjusted(QUrl::StripTrailingSlash);
	API_Base_URL = normalized_url;

	return true;
}
//----------------------------------------------------------------------------
QUrl AsHttp_Client::Base_URL() const
{
	return API_Base_URL;
}
//----------------------------------------------------------------------------
QString AsHttp_Client::Last_Error() const
{
	return Last_Error_Text;
}
//----------------------------------------------------------------------------
int AsHttp_Client::Send(const SHttp_Request &in_request)
{
	AHttp_Operation *operation;
	QUrl url;
	int request_id;

	if (Validate_Request(in_request, url) == false)
		return 0;

	request_id = Next_Request_Id;
	++Next_Request_Id;
	operation = new AHttp_Operation(this);
	operation->Request = in_request;
	operation->URL = url;
	operation->Request_Id = request_id;
	Start_Request(operation);

	return request_id;
}
//----------------------------------------------------------------------------
int AsHttp_Client::Get(const QString &in_path, const QMap<QByteArray, QByteArray> &in_headers)
{
	SHttp_Request request;

	request.Method = "GET";
	request.Path = in_path;
	request.Headers = in_headers;
	request.Retry_Enabled = true;

	return Send(request);
}
//----------------------------------------------------------------------------
int AsHttp_Client::Post_JSON(const QString &in_path, const QJsonObject &in_body,
                             const QMap<QByteArray, QByteArray> &in_headers, bool in_retry_enabled)
{
	SHttp_Request request;

	request.Method = "POST";
	request.Path = in_path;
	request.Headers = in_headers;
	request.Headers.insert("Content-Type", "application/json");
	request.Body = QJsonDocument(in_body).toJson(QJsonDocument::Compact);
	request.Retry_Enabled = in_retry_enabled;

	return Send(request);
}
//----------------------------------------------------------------------------
void AsHttp_Client::Set_Timeout_Ms(int in_timeout_ms)
{
	if (in_timeout_ms > 0)
		Request_Timeout_Ms = in_timeout_ms;
}
//----------------------------------------------------------------------------
int AsHttp_Client::Timeout_Ms() const
{
	return Request_Timeout_Ms;
}
//----------------------------------------------------------------------------
void AsHttp_Client::Set_Max_Retry_Count(int in_count)
{
	if (in_count >= 0)
		Maximum_Retry_Count = in_count;
}
//----------------------------------------------------------------------------
int AsHttp_Client::Max_Retry_Count() const
{
	return Maximum_Retry_Count;
}
//----------------------------------------------------------------------------
void AsHttp_Client::Set_Initial_Retry_Delay_Ms(int in_delay_ms)
{
	if (in_delay_ms >= 0)
		Initial_Retry_Delay = in_delay_ms;
}
//----------------------------------------------------------------------------
int AsHttp_Client::Initial_Retry_Delay_Ms() const
{
	return Initial_Retry_Delay;
}
//----------------------------------------------------------------------------
void AsHttp_Client::onReplyFinished()
{
	QNetworkReply *reply;
	AHttp_Operation *operation;
	QByteArray body;
	QVariant status_attribute;
	int http_status;
	int network_error;

	reply = qobject_cast<QNetworkReply *>(sender());

	if (reply == 0 || Active_Replies.contains(reply) == false)
		return;

	operation = Active_Replies.take(reply);
	body = reply->readAll();
	status_attribute = reply->attribute(QNetworkRequest::HttpStatusCodeAttribute);
	http_status = status_attribute.isValid() ? status_attribute.toInt() : 0;
	network_error = (int)reply->error();
	Finish_Request(operation, reply, http_status, network_error, body);
}
//----------------------------------------------------------------------------
bool AsHttp_Client::Validate_Request(const SHttp_Request &in_request, QUrl &out_url)
{
	QUrl relative_url;
	QByteArray method;
	QString path;

	Last_Error_Text.clear();
	out_url.clear();
	method = in_request.Method.trimmed().toUpper();
	path = in_request.Path.trimmed();
	relative_url = QUrl(path, QUrl::StrictMode);

	if (API_Base_URL.isValid() == false || API_Base_URL.isEmpty())
	{
		Last_Error_Text = "HTTP client не имеет валидного API base URL";
		return false;
	}

	if (method.isEmpty() || path.isEmpty())
	{
		Last_Error_Text = "HTTP method и path обязательны";
		return false;
	}

	for (const char symbol : method)
	{
		if (symbol < 'A' || symbol > 'Z')
		{
			Last_Error_Text = "HTTP method должен содержать только A-Z";
			return false;
		}
	}

	if (relative_url.isValid() == false || relative_url.isRelative() == false || relative_url.hasFragment())
	{
		Last_Error_Text = "HTTP path должен быть relative URL без fragment";
		return false;
	}

	if (relative_url.path().split('/', Qt::SkipEmptyParts).contains(".."))
	{
		Last_Error_Text = "HTTP path не должен выходить из API base path";
		return false;
	}

	while (path.startsWith('/'))
		path.remove(0, 1);

	out_url = QUrl(API_Base_URL.toString(QUrl::FullyEncoded) + "/" + path, QUrl::StrictMode);

	if (out_url.isValid() == false || out_url.host() != API_Base_URL.host())
	{
		Last_Error_Text = "Не удалось построить безопасный endpoint URL";
		out_url.clear();
		return false;
	}

	return true;
}
//----------------------------------------------------------------------------
void AsHttp_Client::Start_Request(AHttp_Operation *in_operation)
{
	QNetworkRequest network_request;
	QNetworkReply *reply;
	QMap<QByteArray, QByteArray>::const_iterator header;
	QByteArray method;

	++in_operation->Attempt_Count;
	method = in_operation->Request.Method.trimmed().toUpper();
	network_request.setUrl(in_operation->URL);
	network_request.setTransferTimeout(Request_Timeout_Ms);
	network_request.setAttribute(QNetworkRequest::RedirectPolicyAttribute, QNetworkRequest::NoLessSafeRedirectPolicy);
	network_request.setRawHeader("Accept", "application/json");
	header = in_operation->Request.Headers.constBegin();

	while (header != in_operation->Request.Headers.constEnd())
	{
		network_request.setRawHeader(header.key(), header.value());
		++header;
	}

	reply = Network_Manager.sendCustomRequest(network_request, method, in_operation->Request.Body);
	Active_Replies.insert(reply, in_operation);
	connect(reply, &QNetworkReply::finished, this, &AsHttp_Client::onReplyFinished);

	qCInfo(HTTP_Log).noquote() << "request" << in_operation->Request_Id << method << Safe_Path(in_operation->URL)
	                           << "attempt" << in_operation->Attempt_Count;
}
//----------------------------------------------------------------------------
void AsHttp_Client::Finish_Request(AHttp_Operation *in_operation, QNetworkReply *in_reply, int in_http_status,
                                   int in_network_error, const QByteArray &in_body)
{
	SHttp_Response response;
	QJsonParseError parse_error;
	QList<QByteArray> header_names;
	int delay_ms;

	qCInfo(HTTP_Log).noquote() << "response" << in_operation->Request_Id << "status" << in_http_status << "network"
	                           << in_network_error << "attempt" << in_operation->Attempt_Count;

	if (Should_Retry(in_operation, in_http_status, in_network_error))
	{
		delay_ms = Retry_Delay_Ms(in_operation->Attempt_Count);
		qCInfo(HTTP_Log).noquote() << "retry" << in_operation->Request_Id << "after-ms" << delay_ms;
		in_reply->deleteLater();
		QTimer::singleShot(delay_ms, in_operation, [this, in_operation]() { Start_Request(in_operation); });
		return;
	}

	response.Request_Id = in_operation->Request_Id;
	response.HTTP_Status_Code = in_http_status;
	response.Network_Error_Code = in_network_error;
	response.Attempt_Count = in_operation->Attempt_Count;
	response.Body = in_body;
	header_names = in_reply->rawHeaderList();

	for (const QByteArray &header_name : header_names)
		response.Headers.insert(header_name.toLower(), in_reply->rawHeader(header_name));

	if (in_body.isEmpty() == false)
	{
		response.JSON = QJsonDocument::fromJson(in_body, &parse_error);
		response.JSON_Valid = parse_error.error == QJsonParseError::NoError;
	}

	response.Success = in_http_status >= 200 && in_http_status <= 299 && in_network_error == QNetworkReply::NoError;

	if (response.Success == false)
		response.Error = AApi_Error::From_Response(in_http_status, in_network_error, in_reply->errorString(), in_body);

	emit requestFinished(response.Request_Id, response);
	in_reply->deleteLater();
	in_operation->deleteLater();
}
//----------------------------------------------------------------------------
bool AsHttp_Client::Should_Retry(const AHttp_Operation *in_operation, int in_http_status, int in_network_error) const
{
	if (in_operation->Request.Retry_Enabled == false || in_operation->Attempt_Count > Maximum_Retry_Count)
		return false;

	if (in_http_status >= 500 && in_http_status <= 599)
		return true;

	return in_http_status <= 0 && in_network_error != QNetworkReply::NoError;
}
//----------------------------------------------------------------------------
int AsHttp_Client::Retry_Delay_Ms(int in_attempt_count) const
{
	int delay_ms;
	int i;

	delay_ms = Initial_Retry_Delay;

	for (i = 1; i < in_attempt_count; ++i)
	{
		if (delay_ms > 30000 / 2)
			return 30000;

		delay_ms *= 2;
	}

	return delay_ms;
}
//----------------------------------------------------------------------------
QString AsHttp_Client::Safe_Path(const QUrl &in_url)
{
	return in_url.path(QUrl::FullyEncoded);
}
//----------------------------------------------------------------------------
