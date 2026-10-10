#include "Data/Exercises/Exercises_Api.h"

#include <QJsonArray>
#include <QJsonObject>
#include <QLoggingCategory>
#include <QUrlQuery>

Q_LOGGING_CATEGORY(EXERCISES_Api_Log, "trainingapp.exercises.api")

//----------------------------------------------------------------------------
AExercises_Api::AExercises_Api(AsHttp_Client *in_http_client, AsAuth_Session *in_session, QObject *in_parent)
    : QObject(in_parent), HTTP_Client_Instance(in_http_client), Session_Instance(in_session)
{
	qRegisterMetaType<SExercises_Item>("SExercises_Item");
	qRegisterMetaType<QVector<SExercises_Item>>("QVector<SExercises_Item>");

	if (HTTP_Client_Instance != 0)
		connect(HTTP_Client_Instance, &AsHttp_Client::requestFinished, this, &AExercises_Api::on_Client_Finished);

	if (Session_Instance != 0)
		connect(Session_Instance, &AsAuth_Session::requestFinished, this, &AExercises_Api::on_Session_Finished);
}
//----------------------------------------------------------------------------
QString AExercises_Api::Last_Error() const
{
	return Last_Error_Text;
}
//----------------------------------------------------------------------------
bool AExercises_Api::Valid_Type(const QString &in_type)
{
	return in_type == QString("basic") || in_type == QString("public") || in_type == QString("own") ||
	       in_type == QString("all");
}
//----------------------------------------------------------------------------
QString AExercises_Api::List_Path(const QString &in_type, const QStringList &in_muscle_groups,
                                  const QStringList &in_equipments, const QString &in_search, int in_limit,
                                  int in_offset)
{
	QUrlQuery query;

	// Multi-формат Swagger (collectionFormat: multi): фильтры повторяются
	// параметром; QUrlQuery сам повторяет addQueryItem и процент-кодирует
	// значения — кириллица поиска уходит кодом, а не сырыми байтами.
	query.addQueryItem("type", in_type);

	for (int i = 0; i < in_muscle_groups.size(); ++i)
		query.addQueryItem("muscle_group", in_muscle_groups.at(i));

	for (int i = 0; i < in_equipments.size(); ++i)
		query.addQueryItem("equipment", in_equipments.at(i));

	if (in_search.isEmpty() == false)
		query.addQueryItem("search", in_search);

	query.addQueryItem("limit", QString::number(in_limit));
	query.addQueryItem("offset", QString::number(in_offset));

	return QString("exercises?") + query.toString(QUrl::FullyEncoded);
}
//----------------------------------------------------------------------------
int AExercises_Api::Get_List(const QString &in_type, const QStringList &in_muscle_groups,
                             const QStringList &in_equipments, const QString &in_search, int in_limit, int in_offset)
{
	SHttp_Request request;

	Last_Error_Text.clear();

	// Обязательные и валидируемые параметры (docs/03, §3.9): невалидные не
	// отправляют запрос — вызывающий видит Last_Error и идентификатор 0.
	if (Valid_Type(in_type) == false)
	{
		Last_Error_Text = "Невалидный type списка упражнений";
		return 0;
	}

	if (in_limit < 1 || in_limit > 1000 || in_offset < 0 || in_offset > 100000)
	{
		Last_Error_Text = "Невалидная страница списка упражнений";
		return 0;
	}

	request.Method = "GET";
	request.Path = List_Path(in_type, in_muscle_groups, in_equipments, in_search, in_limit, in_offset);
	request.Retry_Enabled = true;

	return Send_Optional(request, Kind_List);
}
//----------------------------------------------------------------------------
int AExercises_Api::Get_Muscle_Groups()
{
	SHttp_Request request;

	Last_Error_Text.clear();
	request.Method = "GET";
	request.Path = "muscle-groups";
	request.Retry_Enabled = true;

	return Send_Optional(request, Kind_Muscle_Groups);
}
//----------------------------------------------------------------------------
int AExercises_Api::Get_Equipments()
{
	SHttp_Request request;

	Last_Error_Text.clear();
	request.Method = "GET";
	request.Path = "equipments";
	request.Retry_Enabled = true;

	return Send_Optional(request, Kind_Equipments);
}
//----------------------------------------------------------------------------
int AExercises_Api::Send_Optional(const SHttp_Request &in_request, int in_kind)
{
	SPending_Call pending;
	int inner_id;

	// Опциональная авторизация (docs/03, §3.9): с активной сессией токен
	// добавляется (и работает single-flight refresh), без неё запрос анонимный.
	if (Session_Instance != 0 && Session_Instance->Has_Session())
	{
		inner_id = Session_Instance->Send_Authed(in_request);
		pending.Source = Source_Session;
	}
	else
	{
		inner_id = HTTP_Client_Instance != 0 ? HTTP_Client_Instance->Send(in_request) : 0;
		pending.Source = Source_Client;
	}

	if (inner_id == 0)
		return 0;

	pending.Kind = in_kind;
	Pending_Calls.insert(Next_Request_Id, pending);
	qCInfo(EXERCISES_Api_Log).noquote() << "request started" << Next_Request_Id << "kind" << in_kind;

	return Next_Request_Id++;
}
//----------------------------------------------------------------------------
void AExercises_Api::on_Client_Finished(int in_request_id, const SHttp_Response &in_response)
{
	if (Pending_Calls.contains(in_request_id) == false)
		return;

	Finish_Call(in_request_id, in_response);
}
//----------------------------------------------------------------------------
void AExercises_Api::on_Session_Finished(int in_request_id, const SHttp_Response &in_response)
{
	if (Pending_Calls.contains(in_request_id) == false)
		return;

	Finish_Call(in_request_id, in_response);
}
//----------------------------------------------------------------------------
void AExercises_Api::Finish_Call(int in_request_id, const SHttp_Response &in_response)
{
	SPending_Call pending = Pending_Calls.take(in_request_id);
	AApi_Error error;

	switch (pending.Kind)
	{
	case Kind_Muscle_Groups:
	{
		QStringList groups;

		Parse_String_Array(in_response, groups, error);
		qCInfo(EXERCISES_Api_Log).noquote() << "muscle-groups finished" << in_request_id << "count" << groups.size();
		emit muscleGroupsFinished(in_request_id, groups, error);
		break;
	}
	case Kind_Equipments:
	{
		QStringList equipments;

		Parse_String_Array(in_response, equipments, error);
		qCInfo(EXERCISES_Api_Log).noquote() << "equipments finished" << in_request_id << "count" << equipments.size();
		emit equipmentsFinished(in_request_id, equipments, error);
		break;
	}
	default:
	{
		QVector<SExercises_Item> items;

		Parse_List(in_response, items, error);
		qCInfo(EXERCISES_Api_Log).noquote() << "list finished" << in_request_id << "count" << items.size();
		emit listFinished(in_request_id, items, error);
		break;
	}
	}
}
//----------------------------------------------------------------------------
void AExercises_Api::Parse_List(const SHttp_Response &in_response, QVector<SExercises_Item> &out_items,
                                AApi_Error &out_error)
{
	QJsonArray array;

	out_items.clear();
	out_error = AApi_Error();

	if (in_response.Success == false)
	{
		out_error = in_response.Error;
		return;
	}

	// Голый массив без обёртки (docs/03, §3.9): не массив — undefined_error.
	if (in_response.JSON_Valid == false || in_response.JSON.isArray() == false)
	{
		out_error = AApi_Error("undefined_error", "Incomplete exercises response", in_response.HTTP_Status_Code,
		                       in_response.Network_Error_Code);
		return;
	}

	array = in_response.JSON.array();

	for (int i = 0; i < array.size(); ++i)
	{
		QJsonObject row = array.at(i).toObject();
		SExercises_Item item;

		item.ID = row.value("id").toString();
		item.Name = row.value("name").toString();
		item.Description = row.value("description").toString();
		item.Owner_ID = row.value("owner_id").toString();
		item.Has_Owner = item.Owner_ID.isEmpty() == false;
		item.Created_At = row.value("created_at").toString();

		QJsonArray groups = row.value("muscle_groups").toArray();

		for (int g = 0; g < groups.size(); ++g)
			item.Muscle_Groups.append(groups.at(g).toString());

		QJsonArray equipment = row.value("equipment").toArray();

		for (int e = 0; e < equipment.size(); ++e)
			item.Equipment.append(equipment.at(e).toString());

		// Обязательные поля успешного ответа (docs/03, §3.9).
		if (item.ID.isEmpty() || item.Name.isEmpty())
		{
			out_items.clear();
			out_error = AApi_Error("undefined_error", "Incomplete exercises response", in_response.HTTP_Status_Code,
			                       in_response.Network_Error_Code);
			return;
		}

		out_items.append(item);
	}
}
//----------------------------------------------------------------------------
void AExercises_Api::Parse_String_Array(const SHttp_Response &in_response, QStringList &out_values,
                                        AApi_Error &out_error)
{
	QJsonArray array;

	out_values.clear();
	out_error = AApi_Error();

	if (in_response.Success == false)
	{
		out_error = in_response.Error;
		return;
	}

	if (in_response.JSON_Valid == false || in_response.JSON.isArray() == false)
	{
		out_error = AApi_Error("undefined_error", "Incomplete exercises response", in_response.HTTP_Status_Code,
		                       in_response.Network_Error_Code);
		return;
	}

	array = in_response.JSON.array();

	for (int i = 0; i < array.size(); ++i)
		out_values.append(array.at(i).toString());
}
//----------------------------------------------------------------------------
