#pragma once

#include "Data/Auth/Auth_Session.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Exercises/Exercises_Api.h"
#include "Data/Network/Api_Error.h"

#include <QHash>
#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

//----------------------------------------------------------------------------
// Оркестрация каталога упражнений (спринт U2, docs/08-exercises-sprint-plan.md):
// сеть — источник истины, успех переписывает exercises_cache upsert'ом;
// transport-сбой (офлайн) вместо ошибки отдаёт кэш с маркером from_cache —
// каталог обязан открываться без сети. «Своё» распознаётся по владельцу
// сессии (owner_id == User_ID) и не вытесняется обновлениями базовых:
// наборы списков не пересекаются, вытеснение разомнуло бы кэш при пагинации.
//----------------------------------------------------------------------------
class AExercises_Repository : public QObject
{
	Q_OBJECT

public:
	AExercises_Repository(AsHttp_Client *in_http_client, AsAuth_Session *in_session, AsSql_Database *in_database,
	                      QObject *in_parent = 0);

	int Load_List(const QString &in_type, const QStringList &in_muscle_groups, const QStringList &in_equipments,
	              const QString &in_search, int in_limit, int in_offset);
	int Load_Muscle_Groups();
	int Load_Equipments();
	// CRUD своего упражнения (спринт U5). Успех persist'ит кэш (create/update)
	// или убирает строку (delete); transport-сбой — ошибка наружу: писать своё
	// офлайн нельзя, кэш мог бы обещать сохранность, которой нет.
	int Create_Exercise(const QString &in_name, bool in_is_public, const QString &in_description,
	                    const QStringList &in_muscle_groups, const QStringList &in_equipments);
	int Update_Exercise(const QString &in_id, const QString &in_name, bool in_is_public, const QString &in_description,
	                    const QStringList &in_muscle_groups, const QStringList &in_equipments);
	int Delete_Exercise(const QString &in_id);
	// Загрузка публичного набора (для проверки is_public своего упражнения —
	// в ответе API поле отсутствует, docs/03, §3.9).
	int Load_Public_List();

	// Офлайн-чтение кэша: true — строки прочитаны (пустой кэш тоже true).
	bool Load_Cached_List(QVector<SExercises_Item> &out_items);
	bool Load_Cached_Muscle_Groups(QStringList &out_groups);
	bool Load_Cached_Equipments(QStringList &out_equipments);
	// Своё ли кэшированное упражнение (owner_id совпадает с сессией).
	bool Is_Cached_Own(const QString &in_exercise_id);
	// Жёсткое удаление из кэша после мягкого DELETE сервера (ответ без тела —
	// id знает вызывающий, поэтому вызов наружу).
	bool Delete_Cached(const QString &in_exercise_id);
	// Наполнение справочников (тесты и первый запуск UI без сети).
	bool Save_Catalogs(const QStringList &in_groups, const QStringList &in_equipments);
	QString Last_Error() const;

signals:
	void listFinished(int requestId, const QVector<SExercises_Item> &items, bool from_cache, const AApi_Error &error);
	void muscleGroupsFinished(int requestId, const QStringList &groups, bool from_cache, const AApi_Error &error);
	void equipmentsFinished(int requestId, const QStringList &equipments, bool from_cache, const AApi_Error &error);
	void createFinished(int requestId, const SExercises_Item &item, const AApi_Error &error);
	void updateFinished(int requestId, const SExercises_Item &item, const AApi_Error &error);
	void deleteFinished(int requestId, const AApi_Error &error);
	void publicListFinished(int requestId, const QVector<SExercises_Item> &items, bool from_cache,
	                        const AApi_Error &error);

private slots:
	void on_List_Finished(int in_request_id, const QVector<SExercises_Item> &in_items, const AApi_Error &in_error);
	void on_Muscle_Groups_Finished(int in_request_id, const QStringList &in_groups, const AApi_Error &in_error);
	void on_Equipments_Finished(int in_request_id, const QStringList &in_equipments, const AApi_Error &in_error);
	void on_Create_Finished(int in_request_id, const SExercises_Item &in_item, const AApi_Error &in_error);
	void on_Update_Finished(int in_request_id, const SExercises_Item &in_item, const AApi_Error &in_error);
	void on_Delete_Finished(int in_request_id, const AApi_Error &in_error);
	void on_Public_List_Finished(int in_request_id, const QVector<SExercises_Item> &in_items,
	                             const AApi_Error &in_error);

private:
	// Kind_List — каталог; Kind_Public_List — запрос публичного набора
	// (is_public-проверка редактора): listFinished слушают два обработчика,
	// род различает их по идентификатору запроса.
	enum EPending_Kind
	{
		Kind_List = 0,
		Kind_Public_List
	};
	struct SPending_Call
	{
		// Kind_List — каталог; Kind_Public_List — запрос публичного набора
		// (is_public-проверка редактора): сигналы listFinished слушают оба
		// обработчика, род различает их по идентификатору.
		int Kind = Kind_List;
	};

	static bool Is_Transport_Error(const AApi_Error &in_error);
	static QString Joined_List(const QStringList &in_values);
	int Send(const char *in_kind, int in_request_id);
	bool Persist_List(const QVector<SExercises_Item> &in_items);
	bool Persist_String_List(const char *in_table, const QStringList &in_values);
	void Mirror_Local_Exercise(const SExercises_Item &in_item);

	AsSql_Database *Database_Instance;
	AsAuth_Session *Session_Instance;
	AExercises_Api *Exercises_Api_Instance;
	QHash<int, SPending_Call> Pending_Calls; // request id → вид запроса
	QString Last_Error_Text;
};
//----------------------------------------------------------------------------
