#pragma once

#include "Data/Auth/Auth_Session.h"
#include "Data/Network/Api_Error.h"
#include "Data/Network/Http_Client.h"
#include "Domain/Error_State.h"

#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

//-------------------------------------------------------------------------------------------------------------
// Упражнение из ответа `GET /exercises` (голый массив, docs/03, §3.9).
// description и owner_id — omitempty: отсутствие не ошибка, Has_Owner
// различает базовые (owner_id нет) и свои/публичные. is_public в ответе
// отсутствует (Swagger, observed 10.10.2026) — состояние публикации
// из объекта упражнения не читается.
//-------------------------------------------------------------------------------------------------------------
struct SExercises_Item
{
	QString ID;
	QString Name;
	QString Description;
	QString Owner_ID;
	bool Has_Owner = false;
	QStringList Muscle_Groups;
	QStringList Equipment;
	QString Created_At;
};

//-------------------------------------------------------------------------------------------------------------
// HTTP-адаптер тега exercises (docs/03, §3.9). GET /exercises — опциональная
// авторизация: с активной сессией запрос идёт через AsAuth_Session
// (Bearer + single-flight refresh), без — анонимно через AsHttp_Client.
// Справочники публичны — всегда анонимно. type задан Swagger-enum'ом,
// limit/offset — диапазонами: невалидные параметры не отправляют запрос.
// Ответ `POST /exercises` Swagger декларирует телом запроса — дефект
// спецификации; фактически это Exercise, парсится как Exercise.
//-------------------------------------------------------------------------------------------------------------
class AExercises_Api : public QObject
{
	Q_OBJECT

public:
	// Род dispatched запроса — что парсить в ответе.
	enum class ECall_Kind : unsigned char
	{
		List,
		Muscle_Groups,
		Equipments,
		Create,
		Update,
		Delete,
		One,
		Max
	};

	AExercises_Api(AsHttp_Client *in_http_client, AsAuth_Session *in_session, QObject *in_parent = 0);

	int Get_List(const QString &in_type, const QStringList &in_muscle_groups, const QStringList &in_equipments,
	             const QString &in_search, int in_limit, int in_offset);
	int Get_Muscle_Groups();
	int Get_Equipments();
	// CRUD своего упражнения и приватное чтение (спринты U5–U6, docs/03, §3.9).
	// Возвращают 0 при невалидных локальных данных — запрос не отправляется.
	// POST/PATCH/DELETE неидемпотентны — retry выключен; GET /exercises/{id}
	// идемпотентен — retry включён. Ответ `POST` Swagger декларирует телом
	// запроса (дефект спецификации) — фактически это Exercise, парсится как он;
	// `DELETE` отвечает 200 без тела.
	int Create_Exercise(const QString &in_name, bool in_is_public, const QString &in_description,
	                    const QStringList &in_muscle_groups, const QStringList &in_equipments);
	int Update_Exercise(const QString &in_id, const QString &in_name, bool in_is_public, const QString &in_description,
	                    const QStringList &in_muscle_groups, const QStringList &in_equipments);
	int Delete_Exercise(const QString &in_id);
	// Детальное упражнение (спринт U6, docs/03, §3.9): приватный GET
	// /exercises/{id}. 403 — чужое (в том числе публичное чужое: его можно
	// получить только списком type=public), 404 — не найдено, 400 — невалидный
	// UUID.
	int Get_One(const QString &in_id);
	QString Last_Error() const;

signals:
	void listFinished(int requestId, const QVector<SExercises_Item> &items, const AApi_Error &error);
	void muscleGroupsFinished(int requestId, const QStringList &groups, const AApi_Error &error);
	void equipmentsFinished(int requestId, const QStringList &equipments, const AApi_Error &error);
	void createFinished(int requestId, const SExercises_Item &item, const AApi_Error &error);
	void updateFinished(int requestId, const SExercises_Item &item, const AApi_Error &error);
	void deleteFinished(int requestId, const AApi_Error &error);
	void oneFinished(int requestId, const SExercises_Item &item, const AApi_Error &error);

private slots:
	void on_Client_Finished(int in_request_id, const SHttp_Response &in_response);
	void on_Session_Finished(int in_request_id, const SHttp_Response &in_response);

private:
	struct SPending_Call
	{
		ECall_Kind Kind = ECall_Kind::List;
	};

	int Send_Optional(const SHttp_Request &in_request, ECall_Kind in_kind);
	// Регистрирует отправленный запрос: логический id выдаётся вызывающему,
	// inner id источника (сессия или клиент) сопоставляется с ним.
	int Register_Call(QHash<int, int> &in_out_inner_map, int in_inner_id, ECall_Kind in_kind);
	// Мутации и приватное чтение требуют активную сессию: без неё запрос не уходит.
	int Send_Authed_Mutation(const SHttp_Request &in_request, ECall_Kind in_kind);
	void Finish_Call(int in_request_id, const SHttp_Response &in_response);
	static bool Valid_Type(const QString &in_type);
	// Путь `exercises/{id}`: id процент-кодируется, чтобы `/`, `?`, `#` из
	// значения не меняли endpoint.
	static QString Item_Path(const QString &in_id);
	static QJsonObject Mutation_Body(const QString &in_id, const QString &in_name, bool in_is_public,
	                                 const QString &in_description, const QStringList &in_muscle_groups,
	                                 const QStringList &in_equipments);
	static bool Parse_Item(const QJsonObject &in_row, SExercises_Item &out_item);
	static void Parse_Single(const SHttp_Response &in_response, SExercises_Item &out_item, AApi_Error &out_error);
	static QString List_Path(const QString &in_type, const QStringList &in_muscle_groups,
	                         const QStringList &in_equipments, const QString &in_search, int in_limit, int in_offset);
	static void Parse_List(const SHttp_Response &in_response, QVector<SExercises_Item> &out_items,
	                       AApi_Error &out_error);
	static void Parse_String_Array(const SHttp_Response &in_response, QStringList &out_values, AApi_Error &out_error);

	AsHttp_Client *HTTP_Client_Instance;
	AsAuth_Session *Session_Instance;
	QHash<int, SPending_Call> Pending_Calls; // логический id → род запроса
	// inner id → логический id. Идентификаторы сессии и клиента — разные
	// пространства (общий AsHttp_Client обслуживает и другие API), поэтому
	// у каждого источника своя таблица.
	QHash<int, int> Session_Calls;
	QHash<int, int> Client_Calls;
	int Next_Request_Id = 1;
	AError_State Error_State;
};
//-------------------------------------------------------------------------------------------------------------

Q_DECLARE_METATYPE(SExercises_Item)
Q_DECLARE_METATYPE(QVector<SExercises_Item>)
