#pragma once

#include "Data/Auth/Token_Store.h"
#include "Data/Network/Http_Client.h"

#include <QByteArray>
#include <QHash>
#include <QList>
#include <QObject>
#include <QString>

class QJsonObject;

//----------------------------------------------------------------------------
// Auth-сессия приложения поверх AsHttp_Client и AsToken_Store. Добавляет
// Authorization к приватным запросам и выполняет single-flight refresh: при
// первом auth failure отправляется один POST /auth/refresh, параллельные
// запросы ждут в очереди и повторяются по одному разу с новой парой токенов.
// HTTP-подтверждённая неудача refresh очищает токены, завершает сессию и
// сигналит sessionExpired; transport-сбой сессию сохраняет (docs/03, §6).
// Формат "Bearer <token>" — assumed, точный формат запрошен у backend.
//----------------------------------------------------------------------------
class AsAuth_Session : public QObject
{
	Q_OBJECT

public:
	explicit AsAuth_Session(AsHttp_Client *in_http_client, AsToken_Store *in_token_store, QObject *in_parent = 0);

	int Send_Authed(const SHttp_Request &in_request);
	bool Restore_From_Store();
	bool Adopt_Session(const SStored_Tokens &in_tokens);
	void Drop_Session();
	bool Has_Session() const;
	QString User_ID() const;
	QString Last_Error() const;

	static const char *Refresh_Path;

signals:
	void requestFinished(int requestId, const SHttp_Response &response);
	void sessionExpired();

private slots:
	void on_Http_Finished(int in_request_id, const SHttp_Response &in_response);

private:
	// Логическая операция вызывающего: исходный запрос, выданный ему
	// идентификатор и полученный 401-ответ, если refresh ещё не завершён.
	struct SPending_Call
	{
		int Logical_Request_Id = 0;
		bool Transmitted = false;
		bool Is_Retry_After_Refresh = false;
		SHttp_Request Request;
		SHttp_Response Auth_Failure_Response;
	};

	void Start_Refresh();
	void Handle_Refresh_Finished(const SHttp_Response &in_response);
	void Flush_Queue_After_Refresh();
	void Fail_Queue(const char *in_reason);
	bool Send_Call(SPending_Call &in_call);
	void Finish_Call(SPending_Call &in_call, const SHttp_Response &in_response);
	static SHttp_Response Auth_Failure_Response(int in_request_id);
	static QByteArray Authorization_Value(const QString &in_access_token);

	AsHttp_Client *HTTP_Client_Instance;
	AsToken_Store *Token_Store_Instance;
	QHash<int, SPending_Call> Active_Calls; // ключ — transport id AsHttp_Client
	QList<SPending_Call> Refresh_Queue;
	SStored_Tokens Session_Tokens;
	bool Session_Active = false;
	bool Refresh_In_Progress = false;
	int Refresh_Transport_Id = 0;
	int Next_Logical_Id = 1;
	QString Last_Error_Text;
};
//----------------------------------------------------------------------------
