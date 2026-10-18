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
	// Детальное упражнение (спринт U6, docs/03, §3.9): приватный GET
	// /exercises/{id}. 403 — чужое, 404 — не найдено; transport-сбой отдаёт
	// кэш с маркером from_cache, детальная карточка обязана открываться офлайн.
	int Load_One(const QString &in_id);

	// Офлайн-чтение кэша: true — строки прочитаны (пустой кэш тоже true).
	bool Load_Cached_List(QVector<SExercises_Item> &out_items);
	// Одно упражнение из кэша по id: мгновенный показ карточки без сети и
	// офлайн-фолбэк Load_One. false — строки нет или база недоступна.
	bool Load_Cached_One(const QString &in_exercise_id, SExercises_Item &out_item);
	// Локальный каталог упражнений (`exercises`) целиком: базовые примитивы и
	// строки, которые уже используются в программах. Офлайн-список каталога
	// объединяет его с серверным кэшем: без этого упражнение, стоящее в дне
	// программы, не находилось в окне выбора («в программе есть, выбрать нельзя»).
	bool Load_Local_Catalog(QVector<SExercises_Item> &out_items);
	// Офлайн-список каталога: кэш + локальный каталог, без дублей по id.
	// Единый источник для мгновенного показа и офлайн-ответа репозитория —
	// иначе список «съёживался» после сетевого таймаута.
	bool Load_Offline_List(QVector<SExercises_Item> &out_items);
	// Первый по алфавиту кэшированный id: аудит вёрстки карточки (--screen
	// exercise-detail без идентификатора) показывает реальную карточку.
	bool Load_Cached_First_Id(QString &out_exercise_id);
	// — Локальные фото техники (спринт U5/U6, схема v9) —
	// Фото — клиентское дополнение к серверному упражнению: файл лежит в app
	// data (images/exercises), в базе — только имя по id. Серверное API фото
	// не знает (docs/03, §3.9); синхронизация фото между устройствами — этап
	// M3, сейчас фото локально и офлайн.
	// Каталог фото в app data.
	static QString Photos_Directory();
	// Копия выбранного файла под временным именем: id упражнения при создании
	// неизвестен до ответа сервера, поэтому фото сначала стейджится и
	// коммитится после сохранения. false + out_error — файл не читается.
	bool Stage_Photo(const QUrl &in_source, QString &out_staged_file, QString &out_error);
	// Стейджинг становится фото упражнения: файл переименовывается в
	// <id>.<ext>, строка пишется в exercise_photos, прежнее фото убирается.
	bool Commit_Photo(const QString &in_exercise_id, const QString &in_staged_file);
	// Абсолютный путь к файлу фото упражнения; false — фото нет.
	bool Photo_File(const QString &in_exercise_id, QString &out_file);
	// Удаление фото упражнения: и файл, и строка.
	bool Delete_Photo(const QString &in_exercise_id);
	// Отмена стейджинга (закрытие формы без сохранения): убирается только
	// файл внутри каталога фото, произвольный путь не трогается.
	bool Cancel_Staged_Photo(const QString &in_staged_file);
	// Владелец ли сессии: owner_id совпадает с пользователем токена. Детальный
	// экран открывает правку только для своих (403 чужого — норма API).
	bool Is_Own_Owner(const QString &in_owner_id);
	// Есть ли активная сессия: без неё сервер недоступен для мутаций, и своё
	// упражнение сохраняется локально (Save_Local_Exercise).
	bool Has_Session() const;
	// Локальное сохранение своего упражнения (без сессии или при transport-сбое):
	// пишет в exercises_cache и зеркало exercises. Пустой io_exercise_id —
	// создание (uuid генерируется), иначе правка. Владелец — пользователь
	// сессии, а без сессии — маркер Local_Owner_Id.
	bool Save_Local_Exercise(QString &io_exercise_id, const QString &in_name, const QString &in_description,
	                         const QStringList &in_muscle_groups, const QStringList &in_equipments);
	// Локальное удаление своего упражнения: кэш и зеркало (если нет ссылок FK).
	bool Delete_Local_Exercise(const QString &in_exercise_id);
	// Владелец локально созданных упражнений без сессии.
	static QString Local_Owner_Id();
	bool Load_Cached_Muscle_Groups(QStringList &out_groups);
	bool Load_Cached_Equipments(QStringList &out_equipments);
	// Своё ли кэшированное упражнение (owner_id совпадает с сессией).
	bool Is_Cached_Own(const QString &in_exercise_id);
	// Жёсткое удаление из кэша после мягкого DELETE сервера (ответ без тела —
	// id знает вызывающий, поэтому вызов наружу).
	bool Delete_Cached(const QString &in_exercise_id);
	// Наполнение справочников (тесты и первый запуск UI без сети).
	bool Save_Catalogs(const QStringList &in_groups, const QStringList &in_equipments);
	// Базовый каталог для первого запуска без сети: пустые выбор и фильтры
	// делали экран «не из чего выбрать». Засевается только пустой кэш —
	// сервер остаётся источником истины.
	bool Ensure_Offline_Base();
	// Базовые примитивы каталога (ключи справочников совпадают с ExerciseTexts —
	// перевод на русский делает QML-синглтон, C++ словарём локализации не владеет).
	// Публичные: мгновенный показ без сети отображает их до ответа сервера.
	static QStringList Base_Muscle_Groups();
	static QStringList Base_Equipments();
	static QVector<SExercises_Item> Base_Exercises();
	// Зеркало базовых упражнений в локальный каталог (FK дня программы):
	// вызывается офлайн-веткой и мгновенным показом до ответа сети, иначе
	// выбор базового упражнения упёрся бы в FOREIGN KEY.
	void Mirror_Base_Catalog();
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
	// Детальная карточка (U6): exerciseId дублируется отдельно — в ошибочных
	// ответах (403/404) item пуст, а вызывающий должен знать, к чему отказ.
	void oneFinished(int requestId, const QString &exerciseId, const SExercises_Item &item, bool from_cache,
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
	void on_One_Finished(int in_request_id, const SExercises_Item &in_item, const AApi_Error &in_error);

private:
	// Kind_List — каталог; Kind_Public_List — запрос публичного набора
	// (is_public-проверка редактора): listFinished слушают два обработчика,
	// род различает их по идентификатору запроса.
	enum EPending_Kind
	{
		Kind_List = 0,
		Kind_Public_List,
		Kind_One
	};
	struct SPending_Call
	{
		// Kind_List — каталог; Kind_Public_List — запрос публичного набора
		// (is_public-проверка редактора): сигналы listFinished слушают оба
		// обработчика, род различает их по идентификатору. Kind_One хранит
		// запрошенный id: в ошибочном ответе (403/404) item пуст.
		int Kind = Kind_List;
		QString Exercise_Id;
	};

	// Null-безопасное соединение: без базы отдаёт пустое QSqlDatabase, и
	// запрос откатится проверкой Is_Open() ниже, а не разыменованием 0.
	QSqlDatabase Connection() const;
	static bool Is_Transport_Error(const AApi_Error &in_error);
	// Имя файла фото упражнения из exercise_photos; false — строки нет.
	bool Photo_File_Name(const QString &in_exercise_id, QString &out_file_name);
	// Уборка незакоммиченных стейджингов: стейджинг единственный, новое фото
	// отменяет прежнее.
	void Clear_Staged_Photos();
	// Зеркало базового упражнения в локальный каталог с is_custom = 0:
	// FK program_day_exercises.exercise_id → exercises.id требует строки и для
	// базовых (свои зеркалит Persist_List → Mirror_Local_Exercise).
	void Mirror_Base_Exercise(const SExercises_Item &in_item);
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
