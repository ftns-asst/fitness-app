#pragma once

#include "Data/Exercises/Exercises_Repository.h"
#include "Presentation/List_Model.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QTimer>
#include <QVector>

//----------------------------------------------------------------------------
// ViewModel каталога упражнений (спринт U3, docs/08-exercises-sprint-plan.md).
//
// Отдельный объект, а не режим другой VM: каталог живёт своей жизнью вкладки
// «Упражнения» — поиск, фильтры и пагинация здесь, а не в QML (docs/03, §3.9:
// поиск и фильтры — серверные параметры, QML-фильтрация потеряла бы пагинацию).
//
// Поиск дебаунсится 300 мс: быстрый набор символов даёт один серверный запрос.
// Смена фильтров перезапускает выборку немедленно и с offset=0. Пагинация
// «загрузить ещё» определяется по длине страницы (сервер не отдаёт total):
// короткая страница — конец списка. Ответы устаревших запросов (после нового
// reload) игнорируются по идентификатору.
//
// Кэш показывается мгновенно (offline-first): перед сетевым запросом выборка
// заполняется из exercises_cache — окно выбора и вкладка каталога не ждут
// сетевых таймаутов, при недоступном сервере строки уже на экране. Сетевой
// ответ заменяет кэш, офлайн-ответ репозитория отдаёт те же строки.
//
// Роли строк — mock-контракт CatalogExerciseRow (title/subtitle): замена
// mock-ViewModel в U4 не переписывает делегаты (qml/Mock, §3 роадмапа).
//----------------------------------------------------------------------------
class Avm_Exercises : public QObject
{
	Q_OBJECT
	Q_PROPERTY(AList_Model *exercise_list READ exercise_list CONSTANT)
	Q_PROPERTY(QStringList muscleGroups READ Muscle_Groups NOTIFY catalogsChanged)
	Q_PROPERTY(QStringList equipments READ Equipments NOTIFY catalogsChanged)
	Q_PROPERTY(bool loading READ Is_Loading NOTIFY stateChanged)
	Q_PROPERTY(bool fromCache READ Is_From_Cache NOTIFY stateChanged)
	Q_PROPERTY(QString errorKey READ Error_Key NOTIFY stateChanged)
	Q_PROPERTY(bool canLoadMore READ Can_Load_More NOTIFY stateChanged)

public:
	Avm_Exercises(AExercises_Repository *in_repository, int in_page_size, QObject *in_parent = 0);

	AList_Model *exercise_list() const;
	QStringList Muscle_Groups() const;
	QStringList Equipments() const;
	bool Is_Loading() const;
	bool Is_From_Cache() const;
	QString Error_Key() const;
	bool Can_Load_More() const;

	// QML-мост (§2 CODE_STYLE: имена контракта — lowerCamelCase).
	Q_INVOKABLE bool reload();
	Q_INVOKABLE bool loadMore();
	Q_INVOKABLE void setQuery(const QString &in_query);
	Q_INVOKABLE bool setMuscleGroups(const QStringList &in_groups);
	Q_INVOKABLE bool setEquipments(const QStringList &in_equipments);
	// Сброс поиска и фильтров без сетевого запроса: мини-окно выбора делит
	// каталог с вкладкой «Упражнения» и не должно наследовать её фильтры.
	// Возвращает true, если состояние было сброшено (вызову нужен reload()).
	Q_INVOKABLE bool resetFilters();
	Q_INVOKABLE void loadCatalogs();

signals:
	void stateChanged();
	void catalogsChanged();

private slots:
	void on_Debounce_Timeout();
	void on_List_Finished(int in_request_id, const QVector<SExercises_Item> &in_items, bool in_from_cache,
	                      const AApi_Error &in_error);
	void on_Muscle_Groups_Finished(int in_request_id, const QStringList &in_groups, bool in_from_cache,
	                               const AApi_Error &in_error);
	void on_Equipments_Finished(int in_request_id, const QStringList &in_equipments, bool in_from_cache,
	                            const AApi_Error &in_error);

private:
	QVariantMap Row_Data(const SExercises_Item &in_item) const;
	bool Start_Load(int in_offset);
	// Мгновенный показ кэша: строки появляются сразу, не дожидаясь сетевых
	// таймаутов (офлайн-ответ репозитория мог прийти через десятки секунд,
	// и окно выбора выглядело пустым). Сетевой ответ заменяет кэш.
	void Show_Cache();
	bool Matches_Filters(const SExercises_Item &in_item) const;

	AExercises_Repository *Repository_Instance;
	QStringList Known_Groups;
	QStringList Known_Equipments;
	AList_Model *Exercise_List;
	QTimer *Debounce_Timer;
	QString Query_Text;
	QStringList Muscle_Group_Filter;
	QStringList Equipment_Filter;
	QVector<QVariantMap> Accumulated_Rows;
	int Page_Size;
	int Next_Offset = 0;
	int Current_Request_Id = 0;
	bool Has_All_Rows = false;
	bool Loading_Flag = false;
	bool From_Cache_Flag = false;
	QString Error_Key_Text;
};
//----------------------------------------------------------------------------
