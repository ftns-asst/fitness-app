#pragma once

#include "Data/Auth/Auth_Session.h"
#include "Data/Network/Api_Error.h"
#include "Data/Network/Http_Client.h"

#include <QHash>
#include <QMetaType>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

//----------------------------------------------------------------------------
// Упражнение из ответа `GET /exercises` (голый массив, docs/03, §3.9).
// description и owner_id — omitempty: отсутствие не ошибка, Has_Owner
// различает базовые (owner_id нет) и свои/публичные. is_public в ответе
// отсутствует (Swagger, observed 10.10.2026) — состояние публикации
// из объекта упражнения не читается.
//----------------------------------------------------------------------------
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

//----------------------------------------------------------------------------
// HTTP-адаптер тега exercises (docs/03, §3.9). GET /exercises — опциональная
// авторизация: с активной сессией запрос идёт через AsAuth_Session
// (Bearer + single-flight refresh), без — анонимно через AsHttp_Client.
// Справочники публичны — всегда анонимно. type задан Swagger-enum'ом,
// limit/offset — диапазонами: невалидные параметры не отправляют запрос.
// Ответ `POST /exercises` Swagger декларирует телом запроса — дефект
// спецификации; фактически это Exercise, парсится как Exercise.
//----------------------------------------------------------------------------
class AExercises_Api : public QObject
{
	Q_OBJECT

public:
	// Источник запроса (что ожидать в on_Source_Finished).
	enum ESource
	{
		Source_Session = 0,
		Source_Client
	};
	// Род dispatched запроса — что парсить в ответе.
	enum EKind
	{
		Kind_List = 0,
		Kind_Muscle_Groups,
		Kind_Equipments
	};

	AExercises_Api(AsHttp_Client *in_http_client, AsAuth_Session *in_session, QObject *in_parent = 0);

	int Get_List(const QString &in_type, const QStringList &in_muscle_groups, const QStringList &in_equipments,
	             const QString &in_search, int in_limit, int in_offset);
	int Get_Muscle_Groups();
	int Get_Equipments();
	QString Last_Error() const;

signals:
	void listFinished(int requestId, const QVector<SExercises_Item> &items, const AApi_Error &error);
	void muscleGroupsFinished(int requestId, const QStringList &groups, const AApi_Error &error);
	void equipmentsFinished(int requestId, const QStringList &equipments, const AApi_Error &error);

private slots:
	void on_Client_Finished(int in_request_id, const SHttp_Response &in_response);
	void on_Session_Finished(int in_request_id, const SHttp_Response &in_response);

private:
	struct SPending_Call
	{
		int Kind = Kind_List;
		int Source = Source_Client;
	};

	int Send_Optional(const SHttp_Request &in_request, int in_kind);
	void Finish_Call(int in_request_id, const SHttp_Response &in_response);
	static bool Valid_Type(const QString &in_type);
	static QString List_Path(const QString &in_type, const QStringList &in_muscle_groups,
	                         const QStringList &in_equipments, const QString &in_search, int in_limit, int in_offset);
	static void Parse_List(const SHttp_Response &in_response, QVector<SExercises_Item> &out_items,
	                       AApi_Error &out_error);
	static void Parse_String_Array(const SHttp_Response &in_response, QStringList &out_values, AApi_Error &out_error);

	AsHttp_Client *HTTP_Client_Instance;
	AsAuth_Session *Session_Instance;
	QHash<int, SPending_Call> Pending_Calls; // логический id → род запроса
	int Next_Request_Id = 1;
	QString Last_Error_Text;
};
//----------------------------------------------------------------------------

Q_DECLARE_METATYPE(SExercises_Item)
Q_DECLARE_METATYPE(QVector<SExercises_Item>)
