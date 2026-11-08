#include "Data/Auth/Auth_Session.h"

#include <QJsonObject>
#include <QLoggingCategory>
#include <QMap>

Q_LOGGING_CATEGORY(AUTH_Session_Log, "trainingapp.auth.session")

//-------------------------------------------------------------------------------------------------------------
// AsAuth_Session
//-------------------------------------------------------------------------------------------------------------
const char *AsAuth_Session::Refresh_Path = "auth/refresh";
//-------------------------------------------------------------------------------------------------------------
AsAuth_Session::AsAuth_Session(AsHttp_Client *in_http_client, AsToken_Store *in_token_store, QObject *in_parent)
    : QObject(in_parent), HTTP_Client_Instance(in_http_client), Token_Store_Instance(in_token_store),
      Error_State(&AUTH_Session_Log())
{
	if (HTTP_Client_Instance != 0)
		connect(HTTP_Client_Instance, &AsHttp_Client::requestFinished, this, &AsAuth_Session::on_Http_Finished);
}
//-------------------------------------------------------------------------------------------------------------
int AsAuth_Session::Send_Authed(const SHttp_Request &in_request)
{
	SPending_Call call;

	Error_State.Clear_Last_Error();

	if (!Session_Active)
	{
		Error_State.Set_Last_Error("Нет активной сессии: требуется вход");
		return 0;
	}

	call.Logical_Request_Id = Next_Logical_Id;
	++Next_Logical_Id;
	call.Generation = Session_Generation;
	call.Request = in_request;

	if (Refresh_In_Progress)
	{
		// Текущий access-токен уже отвергнут сервером: отправим после refresh.
		Refresh_Queue.append(call);
		qCInfo(AUTH_Session_Log).noquote() << "queued" << call.Logical_Request_Id;
		return call.Logical_Request_Id;
	}

	if (!Send_Call(call))
		return 0;

	return call.Logical_Request_Id;
}
//-------------------------------------------------------------------------------------------------------------
bool AsAuth_Session::Restore_From_Store()
{
	SStored_Tokens stored;

	if (Token_Store_Instance == 0)
		return false;

	if (!Token_Store_Instance->Load_Tokens(stored))
		return false;

	Session_Tokens = stored;
	Session_Active = true;
	Begin_Generation("session restored");
	qCInfo(AUTH_Session_Log).noquote() << "session restored";

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AsAuth_Session::Adopt_Session(const SStored_Tokens &in_tokens)
{
	if (in_tokens.User_ID.isEmpty() || in_tokens.Access_Token.isEmpty() || in_tokens.Refresh_Token.isEmpty())
		return false;

	// Login/signup: репозиторий уже сохранил пару токенов, сессия активируется
	// в памяти без повторного обращения к store.
	Session_Tokens = in_tokens;
	Session_Active = true;
	Begin_Generation("session replaced");
	qCInfo(AUTH_Session_Log).noquote() << "session adopted";

	return true;
}
//-------------------------------------------------------------------------------------------------------------
void AsAuth_Session::Drop_Session()
{
	// Локальный logout: только память; постоянные токены очищает репозиторий.
	Session_Active = false;
	Session_Tokens = SStored_Tokens();
	Begin_Generation("session dropped");
	qCInfo(AUTH_Session_Log).noquote() << "session dropped";
}
//-------------------------------------------------------------------------------------------------------------
bool AsAuth_Session::Has_Session() const
{
	return Session_Active;
}
//-------------------------------------------------------------------------------------------------------------
QString AsAuth_Session::User_ID() const
{
	if (!Session_Active)
		return QString();

	return Session_Tokens.User_ID;
}
//-------------------------------------------------------------------------------------------------------------
QString AsAuth_Session::Last_Error() const
{
	return Error_State.Get_Last_Error();
}
//-------------------------------------------------------------------------------------------------------------
void AsAuth_Session::on_Http_Finished(int in_request_id, const SHttp_Response &in_response)
{
	SPending_Call call;

	if (Refresh_In_Progress && in_request_id == Refresh_Transport_Id)
	{
		Handle_Refresh_Finished(in_response);
		return;
	}

	if (!Active_Calls.contains(in_request_id))
		return;

	call = Active_Calls.take(in_request_id);

	// Первый auth failure ставит запрос в очередь refresh; повтор после
	// refresh при новом 401 отдаётся вызывающему без второго refresh.
	// Запрос прежней сессии (другое поколение) refresh не запускает.
	if (in_response.HTTP_Status_Code == 401 && !call.Is_Retry_After_Refresh && Session_Active &&
	    call.Generation == Session_Generation)
	{
		call.Auth_Failure_Response = in_response;
		call.Transmitted = true;

		if (!Refresh_In_Progress && call.Access_Token_Used != Session_Tokens.Access_Token)
		{
			// Пока запрос был в пути, refresh уже выполнен другим запросом:
			// одноразовый refresh-токен не тратим, повторяем с новым access.
			call.Is_Retry_After_Refresh = true;

			if (!Send_Call(call))
				Finish_Call(call, call.Auth_Failure_Response);

			return;
		}

		Refresh_Queue.append(call);

		if (!Refresh_In_Progress)
			Start_Refresh();

		return;
	}

	Finish_Call(call, in_response);
}
//-------------------------------------------------------------------------------------------------------------
void AsAuth_Session::Start_Refresh()
{
	QJsonObject body;

	Refresh_In_Progress = true;
	body.insert("refresh_token", Session_Tokens.Refresh_Token);
	Refresh_Transport_Id = HTTP_Client_Instance->Post_JSON(Refresh_Path, body, QMap<QByteArray, QByteArray>(), false);

	if (Refresh_Transport_Id == 0)
	{
		// Refresh не отправлен: сессию сохраняем, очередь отпускаем.
		Refresh_In_Progress = false;
		Error_State.Set_Last_Error(HTTP_Client_Instance->Last_Error());
		Fail_Queue("refresh not sent");
		return;
	}

	qCInfo(AUTH_Session_Log).noquote() << "refresh started" << Refresh_Transport_Id;
}
//-------------------------------------------------------------------------------------------------------------
void AsAuth_Session::Handle_Refresh_Finished(const SHttp_Response &in_response)
{
	QJsonObject tokens;

	Refresh_In_Progress = false;
	Refresh_Transport_Id = 0;

	tokens = in_response.JSON_Valid ? in_response.JSON.object().value("tokens").toObject() : QJsonObject();

	if (in_response.Success && !tokens.value("access_token").toString().isEmpty() &&
	    !tokens.value("refresh_token").toString().isEmpty())
	{
		// Ротация: новая пара атомарно заменяет старую.
		Session_Tokens.Access_Token = tokens.value("access_token").toString();
		Session_Tokens.Refresh_Token = tokens.value("refresh_token").toString();
		Session_Tokens.Access_Expires_At = QDateTime();
		Session_Tokens.Refresh_Expires_At = QDateTime();

		if (Token_Store_Instance == 0 ||
		    !Token_Store_Instance->Save_Tokens(Session_Tokens.User_ID, Session_Tokens.Access_Token,
		                                       Session_Tokens.Refresh_Token, QDateTime(), QDateTime()))
			qCWarning(AUTH_Session_Log).noquote() << "token save failed after refresh";

		qCInfo(AUTH_Session_Log).noquote() << "refresh ok";
		Flush_Queue_After_Refresh();
		return;
	}

	if (Is_Refresh_Rejected(in_response))
	{
		// Сервер подтвердил невалидность refresh-токена: сессия завершена.
		if (Token_Store_Instance != 0)
			Token_Store_Instance->Clear_Tokens();

		Session_Active = false;
		Session_Tokens = SStored_Tokens();
		qCInfo(AUTH_Session_Log).noquote() << "refresh rejected" << in_response.HTTP_Status_Code;
		Fail_Queue("refresh rejected");
		emit sessionExpired();
		return;
	}

	// Transport-сбой, 5xx и невалидный 2xx не уничтожают локальную сессию (docs/03, §6).
	qCWarning(AUTH_Session_Log).noquote()
	    << "refresh failure kept session" << in_response.HTTP_Status_Code << in_response.Network_Error_Code;
	Fail_Queue("refresh transport failure");
}
//-------------------------------------------------------------------------------------------------------------
void AsAuth_Session::Flush_Queue_After_Refresh()
{
	QList<SPending_Call> queue;
	SPending_Call call;
	SHttp_Response failure;
	int i;

	queue = Refresh_Queue;
	Refresh_Queue.clear();

	for (i = 0; i < queue.size(); ++i)
	{
		call = queue.at(i);
		call.Is_Retry_After_Refresh = true;
		call.Auth_Failure_Response = SHttp_Response();

		if (!Send_Call(call))
		{
			// Повтор не отправлен: отдаём auth-failure без повторных ретраев.
			failure = Auth_Failure_Response(call.Logical_Request_Id);
			Finish_Call(call, failure);
		}
	}
}
//-------------------------------------------------------------------------------------------------------------
void AsAuth_Session::Fail_Queue(const char *in_reason)
{
	QList<SPending_Call> queue;
	SPending_Call call;
	SHttp_Response failure;
	int i;

	queue = Refresh_Queue;
	Refresh_Queue.clear();

	if (queue.isEmpty())
		return;

	qCInfo(AUTH_Session_Log).noquote() << "queue released" << queue.size() << in_reason;

	for (i = 0; i < queue.size(); ++i)
	{
		call = queue.at(i);

		if (call.Transmitted)
			Finish_Call(call, call.Auth_Failure_Response);
		else
		{
			failure = Auth_Failure_Response(call.Logical_Request_Id);
			Finish_Call(call, failure);
		}
	}
}
//-------------------------------------------------------------------------------------------------------------
void AsAuth_Session::Begin_Generation(const char *in_reason)
{
	++Session_Generation;

	// Ответ на брошенный refresh придёт с Refresh_Transport_Id, которого уже
	// нет ни здесь, ни в Active_Calls, и будет проигнорирован.
	Refresh_In_Progress = false;
	Refresh_Transport_Id = 0;
	Fail_Queue(in_reason);
}
//-------------------------------------------------------------------------------------------------------------
bool AsAuth_Session::Send_Call(SPending_Call &in_call)
{
	SHttp_Request request;
	int transport_id;

	request = in_call.Request;
	request.Headers.insert("Authorization", Authorization_Value(Session_Tokens.Access_Token));
	in_call.Access_Token_Used = Session_Tokens.Access_Token;
	transport_id = HTTP_Client_Instance->Send(request);

	if (transport_id == 0)
	{
		Error_State.Set_Last_Error(HTTP_Client_Instance->Last_Error());
		qCWarning(AUTH_Session_Log).noquote() << "send failed" << in_call.Logical_Request_Id;
		return false;
	}

	in_call.Transmitted = true;
	Active_Calls.insert(transport_id, in_call);

	return true;
}
//-------------------------------------------------------------------------------------------------------------
void AsAuth_Session::Finish_Call(SPending_Call &in_call, const SHttp_Response &in_response)
{
	SHttp_Response response;

	response = in_response;
	response.Request_Id = in_call.Logical_Request_Id;

	emit requestFinished(in_call.Logical_Request_Id, response);
}
//-------------------------------------------------------------------------------------------------------------
SHttp_Response AsAuth_Session::Auth_Failure_Response(int in_request_id)
{
	SHttp_Response response;

	response.Request_Id = in_request_id;
	response.Success = false;
	response.HTTP_Status_Code = 401;
	response.Error = AApi_Error("invalid_token", "Session expired", 401, 0);

	return response;
}
//-------------------------------------------------------------------------------------------------------------
bool AsAuth_Session::Is_Refresh_Rejected(const SHttp_Response &in_response)
{
	if (in_response.HTTP_Status_Code < 400 || in_response.HTTP_Status_Code > 499)
		return false;

	return in_response.HTTP_Status_Code != 408 && in_response.HTTP_Status_Code != 429;
}
//-------------------------------------------------------------------------------------------------------------
QByteArray AsAuth_Session::Authorization_Value(const QString &in_access_token)
{
	// Формат "Bearer <token>" подтверждён публикацией документации backend:
	// docs/03-api-contract.md §2, docs/03-api-backend-published.md.
	return QByteArray("Bearer ") + in_access_token.toUtf8();
}
//-------------------------------------------------------------------------------------------------------------