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
// Смена сессии (Drop/Adopt/Restore) открывает новое «поколение»: refresh и
// ответы, начатые прежней сессией, не трогают токены новой.
// Формат "Bearer <token>", одноразовость refresh_token и ключи 401
// (invalid_token/token_expired) подтверждены публикацией документации backend:
// docs/03-api-contract.md §2, docs/03-api-backend-published.md.
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
		int Generation = 0; // поколение сессии, в котором запрос создан
		bool Transmitted = false;
		bool Is_Retry_After_Refresh = false;
		SHttp_Request Request;
		QString Access_Token_Used; // токен последней отправки: 401 по старому токену — без нового refresh
		SHttp_Response Auth_Failure_Response;
	};

	void Start_Refresh();
	void Handle_Refresh_Finished(const SHttp_Response &in_response);
	void Flush_Queue_After_Refresh();
	void Fail_Queue(const char *in_reason);
	// Новое поколение сессии: незавершённый refresh забывается, очередь
	// получает auth failure, ответы прежних запросов не запускают refresh.
	void Begin_Generation(const char *in_reason);
	bool Send_Call(SPending_Call &in_call);
	void Finish_Call(SPending_Call &in_call, const SHttp_Response &in_response);
	static SHttp_Response Auth_Failure_Response(int in_request_id);
	// Отказ refresh, завершающий сессию: 4xx кроме временных 408/429. 5xx,
	// transport-сбой и невалидный 2xx — сбой, сессия сохраняется (docs/03, §6).
	static bool Is_Refresh_Rejected(const SHttp_Response &in_response);
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
	int Session_Generation = 0;
	QString Last_Error_Text;
};
//----------------------------------------------------------------------------
