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
// Тело POST/PATCH: name, is_public, description (omitempty — пустой не
// отправляем), muscle_groups, equipment; PATCH добавляет id (обязан совпадать
// с путём, docs/03, §3.9).
//----------------------------------------------------------------------------
QJsonObject Exercises_Mutation_Body(const QString &in_id, const QString &in_name, bool in_is_public,
                                    const QString &in_description, const QStringList &in_muscle_groups,
                                    const QStringList &in_equipments)
{
	QJsonObject body;
	QJsonArray groups;
	QJsonArray equipments;

	body.insert("name", in_name);
	body.insert("is_public", in_is_public);

	for (int i = 0; i < in_muscle_groups.size(); ++i)
		groups.append(in_muscle_groups.at(i));

	for (int i = 0; i < in_equipments.size(); ++i)
		equipments.append(in_equipments.at(i));

	body.insert("muscle_groups", groups);
	body.insert("equipment", equipments);

	if (in_description.isEmpty() == false)
		body.insert("description", in_description);

	if (in_id.isEmpty() == false)
		body.insert("id", in_id);

	return body;
}
//----------------------------------------------------------------------------
int AExercises_Api::Create_Exercise(const QString &in_name, bool in_is_public, const QString &in_description,
                                    const QStringList &in_muscle_groups, const QStringList &in_equipments)
{
	SHttp_Request request;

	Last_Error_Text.clear();

	// Имя — единственное содержательно обязательное поле: сервер в Swagger
	// не помечает required, но пустое имя не имеет смысла (docs/03, §3.9).
	if (in_name.trimmed().isEmpty())
	{
		Last_Error_Text = "Название упражнения не может быть пустым";
		return 0;
	}

	request.Method = "POST";
	request.Path = "exercises";
	request.Body = QJsonDocument(Exercises_Mutation_Body(QString(), in_name.trimmed(), in_is_public, in_description,
	                                                     in_muscle_groups, in_equipments))
	                   .toJson(QJsonDocument::Compact);

	return Send_Authed_Mutation(request, Kind_Create);
}
//----------------------------------------------------------------------------
int AExercises_Api::Update_Exercise(const QString &in_id, const QString &in_name, bool in_is_public,
                                    const QString &in_description, const QStringList &in_muscle_groups,
                                    const QStringList &in_equipments)
{
	SHttp_Request request;

	Last_Error_Text.clear();

	if (in_id.trimmed().isEmpty() || in_name.trimmed().isEmpty())
	{
		Last_Error_Text = "Название упражнения не может быть пустым";
		return 0;
	}

	request.Method = "PATCH";
	request.Path = QString("exercises/%1").arg(in_id.trimmed());
	request.Body = QJsonDocument(Exercises_Mutation_Body(in_id.trimmed(), in_name.trimmed(), in_is_public,
	                                                     in_description, in_muscle_groups, in_equipments))
	                   .toJson(QJsonDocument::Compact);

	return Send_Authed_Mutation(request, Kind_Update);
}
//----------------------------------------------------------------------------
int AExercises_Api::Delete_Exercise(const QString &in_id)
{
	SHttp_Request request;

	Last_Error_Text.clear();

	if (in_id.trimmed().isEmpty())
	{
		Last_Error_Text = "Не указано упражнение для удаления";
		return 0;
	}

	request.Method = "DELETE";
	request.Path = QString("exercises/%1").arg(in_id.trimmed());

	return Send_Authed_Mutation(request, Kind_Delete);
}
//----------------------------------------------------------------------------
int AExercises_Api::Get_One(const QString &in_id)
{
	SHttp_Request request;

	Last_Error_Text.clear();

	if (in_id.trimmed().isEmpty())
	{
		Last_Error_Text = "Не указано упражнение для чтения";
		return 0;
	}

	// GET /exercises/{id} — приватный эндпоинт (docs/03, §3.9): без активной
	// сессии запрос не отправляется, «своё» без токена не читается.
	request.Method = "GET";
	request.Path = QString("exercises/%1").arg(in_id.trimmed());
	request.Retry_Enabled = true;

	return Send_Authed_Mutation(request, Kind_One);
}
//----------------------------------------------------------------------------
int AExercises_Api::Send_Authed_Mutation(const SHttp_Request &in_request, int in_kind)
{
	SPending_Call pending;
	int inner_id;

	if (Session_Instance == 0 || Session_Instance->Has_Session() == false)
	{
		Last_Error_Text = "Нет активной сессии: требуется вход";
		return 0;
	}

	inner_id = Session_Instance->Send_Authed(in_request);

	if (inner_id == 0)
		return 0;

	pending.Kind = in_kind;
	pending.Source = Source_Session;
	Pending_Calls.insert(Next_Request_Id, pending);
	qCInfo(EXERCISES_Api_Log).noquote() << "mutation started" << Next_Request_Id << "kind" << in_kind;

	return Next_Request_Id++;
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
	case Kind_Create:
	{
		SExercises_Item item;

		Parse_Single(in_response, item, error);
		qCInfo(EXERCISES_Api_Log).noquote() << "create finished" << in_request_id << "id" << item.ID;
		emit createFinished(in_request_id, item, error);
		break;
	}
	case Kind_Update:
	{
		SExercises_Item item;

		Parse_Single(in_response, item, error);
		qCInfo(EXERCISES_Api_Log).noquote() << "update finished" << in_request_id << "id" << item.ID;
		emit updateFinished(in_request_id, item, error);
		break;
	}
	case Kind_Delete:
	{
		qCInfo(EXERCISES_Api_Log).noquote() << "delete finished" << in_request_id;
		emit deleteFinished(in_request_id, in_response.Error);
		break;
	}
	case Kind_One:
	{
		SExercises_Item item;

		Parse_Single(in_response, item, error);
		qCInfo(EXERCISES_Api_Log).noquote() << "one finished" << in_request_id << "id" << item.ID;
		emit oneFinished(in_request_id, item, error);
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
bool AExercises_Api::Parse_Item(const QJsonObject &in_row, SExercises_Item &out_item)
{
	QJsonArray groups;
	QJsonArray equipment;
	int g;
	int e;

	out_item = SExercises_Item();

	out_item.ID = in_row.value("id").toString();
	out_item.Name = in_row.value("name").toString();

	// omitempty-поля отсутствуют в JSON целиком: toString() вернул бы
	// null-QString, а bind null упёрся бы в NOT NULL DEFAULT '' кэша —
	// DEFAULT работает только для опущенной колонки, не для NULL.
	out_item.Description =
	    in_row.value("description").isString() ? in_row.value("description").toString() : QString("");
	out_item.Owner_ID = in_row.value("owner_id").isString() ? in_row.value("owner_id").toString() : QString("");
	out_item.Has_Owner = out_item.Owner_ID.isEmpty() == false;
	out_item.Created_At = in_row.value("created_at").isString() ? in_row.value("created_at").toString() : QString("");

	groups = in_row.value("muscle_groups").toArray();

	for (g = 0; g < groups.size(); ++g)
		out_item.Muscle_Groups.append(groups.at(g).toString());

	equipment = in_row.value("equipment").toArray();

	for (e = 0; e < equipment.size(); ++e)
		out_item.Equipment.append(equipment.at(e).toString());

	// Обязательные поля успешного ответа (docs/03, §3.9).
	return out_item.ID.isEmpty() == false && out_item.Name.isEmpty() == false;
}
//----------------------------------------------------------------------------
// Один Exercise (ответ POST/PATCH; схема ответа POST в Swagger декларирует
// тело запроса — дефект спецификации, docs/03, §3.9). Не объект или без
// обязательных полей — undefined_error.
//----------------------------------------------------------------------------
void AExercises_Api::Parse_Single(const SHttp_Response &in_response, SExercises_Item &out_item, AApi_Error &out_error)
{
	out_item = SExercises_Item();
	out_error = AApi_Error();

	if (in_response.Success == false)
	{
		out_error = in_response.Error;
		return;
	}

	if (in_response.JSON_Valid == false || in_response.JSON.isObject() == false ||
	    Parse_Item(in_response.JSON.object(), out_item) == false)
	{
		out_item = SExercises_Item();
		out_error = AApi_Error("undefined_error", "Incomplete exercise response", in_response.HTTP_Status_Code,
		                       in_response.Network_Error_Code);
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
		// omitempty-поля отсутствуют в JSON целиком: toString() вернул бы
		// null-QString, а bind null упёрся в NOT NULL DEFAULT '' кэша —
		// DEFAULT работает только для опущенной колонки, не для NULL.
		item.Description = row.value("description").isString() ? row.value("description").toString() : QString("");
		item.Owner_ID = row.value("owner_id").isString() ? row.value("owner_id").toString() : QString("");
		item.Has_Owner = item.Owner_ID.isEmpty() == false;
		item.Created_At = row.value("created_at").isString() ? row.value("created_at").toString() : QString("");

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
