#pragma once

#include "Data/Network/Api_Error.h"

#include <QByteArray>
#include <QHash>
#include <QJsonDocument>
#include <QJsonObject>
#include <QMap>
#include <QNetworkAccessManager>
#include <QObject>
#include <QString>
#include <QUrl>

class AHttp_Operation;
class QNetworkReply;

//----------------------------------------------------------------------------
// Единый DTO запроса transport-слоя. Path всегда относителен к API base URL.
// Retry_Enabled позволяет API-адаптеру запретить повтор неидемпотентной операции.
//----------------------------------------------------------------------------
struct SHttp_Request
{
	QByteArray Method = "GET";
	QString Path;
	QMap<QByteArray, QByteArray> Headers;
	QByteArray Body;
	bool Retry_Enabled = false;
};

//----------------------------------------------------------------------------
// Единый DTO ответа transport-слоя. JSON_Valid сообщает только результат
// синтаксического разбора; обязательные поля endpoint проверяет его API-адаптер.
//----------------------------------------------------------------------------
struct SHttp_Response
{
	int Request_Id = 0;
	bool Success = false;
	int HTTP_Status_Code = 0;
	int Network_Error_Code = 0;
	int Attempt_Count = 0;
	QMap<QByteArray, QByteArray> Headers;
	QByteArray Body;
	QJsonDocument JSON;
	bool JSON_Valid = false;
	AApi_Error Error;
};

//----------------------------------------------------------------------------
// Асинхронный HTTP-клиент приложения. Владеет одним QNetworkAccessManager,
// ограничивает время запроса и повторяет network/5xx ошибки с exponential backoff.
//----------------------------------------------------------------------------
class AsHttp_Client : public QObject
{
	Q_OBJECT

public:
	explicit AsHttp_Client(const QUrl &in_base_url, QObject *in_parent = 0);

	bool Set_Base_URL(const QUrl &in_base_url);
	QUrl Base_URL() const;
	QString Last_Error() const;

	int Send(const SHttp_Request &in_request);
	int Get(const QString &in_path, const QMap<QByteArray, QByteArray> &in_headers = QMap<QByteArray, QByteArray>());
	int Post_JSON(const QString &in_path, const QJsonObject &in_body,
	              const QMap<QByteArray, QByteArray> &in_headers = QMap<QByteArray, QByteArray>(),
	              bool in_retry_enabled = false);

	void Set_Timeout_Ms(int in_timeout_ms);
	int Timeout_Ms() const;
	void Set_Max_Retry_Count(int in_count);
	int Max_Retry_Count() const;
	void Set_Initial_Retry_Delay_Ms(int in_delay_ms);
	int Initial_Retry_Delay_Ms() const;

signals:
	void requestFinished(int requestId, const SHttp_Response &response);

private slots:
	void onReplyFinished();

private:
	bool Validate_Request(const SHttp_Request &in_request, QUrl &out_url);
	void Start_Request(AHttp_Operation *in_operation);
	void Finish_Request(AHttp_Operation *in_operation, QNetworkReply *in_reply, int in_http_status,
	                    int in_network_error, const QByteArray &in_body);
	bool Should_Retry(const AHttp_Operation *in_operation, int in_http_status, int in_network_error) const;
	int Retry_Delay_Ms(int in_attempt_count) const;
	static QString Safe_Path(const QUrl &in_url);

	QNetworkAccessManager Network_Manager;
	QHash<QNetworkReply *, AHttp_Operation *> Active_Replies;
	QUrl API_Base_URL;
	QString Last_Error_Text;
	int Next_Request_Id = 1;
	int Request_Timeout_Ms = 10000;
	int Maximum_Retry_Count = 2;
	int Initial_Retry_Delay = 250;
};
//----------------------------------------------------------------------------

Q_DECLARE_METATYPE(SHttp_Request)
Q_DECLARE_METATYPE(SHttp_Response)
