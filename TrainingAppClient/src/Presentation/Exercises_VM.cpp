#include "Presentation/Exercises_VM.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(EXERCISES_VM_Log, "trainingapp.exercises.vm")

//----------------------------------------------------------------------------
Avm_Exercises::Avm_Exercises(AExercises_Repository *in_repository, int in_page_size, QObject *in_parent)
    : QObject(in_parent), Repository_Instance(in_repository), Exercise_List(new AList_Model(this)),
      Page_Size(in_page_size)
{
	Exercise_List->Set_Roles(QStringList({"title", "subtitle"}));

	// Поиск — debounce: быстрый набор даёт один запрос последним словом.
	Debounce_Timer = new QTimer(this);
	Debounce_Timer->setSingleShot(true);
	Debounce_Timer->setInterval(300);
	connect(Debounce_Timer, &QTimer::timeout, this, &Avm_Exercises::on_Debounce_Timeout);

	if (Repository_Instance != 0)
	{
		connect(Repository_Instance, &AExercises_Repository::listFinished, this, &Avm_Exercises::on_List_Finished);
		connect(Repository_Instance, &AExercises_Repository::muscleGroupsFinished, this,
		        &Avm_Exercises::on_Muscle_Groups_Finished);
		connect(Repository_Instance, &AExercises_Repository::equipmentsFinished, this,
		        &Avm_Exercises::on_Equipments_Finished);
	}
}
//----------------------------------------------------------------------------
AList_Model *Avm_Exercises::exercise_list() const
{
	return Exercise_List;
}
//----------------------------------------------------------------------------
	QStringList Avm_Exercises::Muscle_Groups() const
{
	return Known_Groups;
}
//----------------------------------------------------------------------------
QStringList Avm_Exercises::Equipments() const
{
	return Known_Equipments;
}
//----------------------------------------------------------------------------
bool Avm_Exercises::Is_Loading() const
{
	return Loading_Flag;
}
//----------------------------------------------------------------------------
bool Avm_Exercises::Is_From_Cache() const
{
	return From_Cache_Flag;
}
//----------------------------------------------------------------------------
QString Avm_Exercises::Error_Key() const
{
	return Error_Key_Text;
}
//----------------------------------------------------------------------------
bool Avm_Exercises::Can_Load_More() const
{
	return Has_All_Rows == false;
}
//----------------------------------------------------------------------------
QVariantMap Avm_Exercises::Row_Data(const SExercises_Item &in_item)
{
	QVariantMap row;

	// Mock-контракт CatalogExerciseRow: title + muscleGroups (список ключей
	// справочника). Перевод ключей — зона QML (ExerciseTexts), C++ словарём
	// локализации не владеет.
	row.insert("id", in_item.ID);
	row.insert("title", in_item.Name);
	row.insert("muscleGroups", QVariant::fromValue(in_item.Muscle_Groups));
	row.insert("subtitle", in_item.Muscle_Groups.join(", "));

	return row;
}
//----------------------------------------------------------------------------
bool Avm_Exercises::Start_Load(int in_offset)
{
	int request_id;

	if (Repository_Instance == 0 || Loading_Flag)
		return false;

	request_id =
	    Repository_Instance->Load_List("all", Muscle_Group_Filter, Equipment_Filter, Query_Text, Page_Size, in_offset);

	if (request_id == 0)
		return false;

	// Ответы устаревших запросов игнорируются по идентификатору: reload на
	// фоне летящего loadMore не должен склеивать страницы разных выборок.
	Current_Request_Id = request_id;
	Loading_Flag = true;
	emit stateChanged();

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Exercises::reload()
{
	Accumulated_Rows.clear();
	Has_All_Rows = false;

	return Start_Load(0);
}
//----------------------------------------------------------------------------
bool Avm_Exercises::loadMore()
{
	if (Can_Load_More() == false)
		return false;

	return Start_Load(Next_Offset);
}
//----------------------------------------------------------------------------
void Avm_Exercises::setQuery(const QString &in_query)
{
	if (Query_Text == in_query)
		return;

	Query_Text = in_query;

	// Окно debounce перезапускается каждым символом: запрос уходит один —
	// по последнему слову.
	Debounce_Timer->start();
}
//----------------------------------------------------------------------------
bool Avm_Exercises::setMuscleGroups(const QStringList &in_groups)
{
	if (Muscle_Group_Filter == in_groups)
		return false;

	Muscle_Group_Filter = in_groups;

	return reload();
}
//----------------------------------------------------------------------------
bool Avm_Exercises::setEquipments(const QStringList &in_equipments)
{
	if (Equipment_Filter == in_equipments)
		return false;

	Equipment_Filter = in_equipments;

	return reload();
}
//----------------------------------------------------------------------------
void Avm_Exercises::loadCatalogs()
{
	if (Repository_Instance != 0)
	{
		Repository_Instance->Load_Muscle_Groups();
		Repository_Instance->Load_Equipments();
	}
}
//----------------------------------------------------------------------------
void Avm_Exercises::on_Debounce_Timeout()
{
	reload();
}
//----------------------------------------------------------------------------
void Avm_Exercises::on_List_Finished(int in_request_id, const QVector<SExercises_Item> &in_items, bool in_from_cache,
                                     const AApi_Error &in_error)
{
	QVector<QVariantMap> rows;

	if (in_request_id != Current_Request_Id)
		return;

	Loading_Flag = false;
	From_Cache_Flag = in_from_cache;
	Error_Key_Text = in_error.Is_Error() ? in_error.Key() : QString();

	if (in_error.Is_Error() == false)
	{
		// Дозагрузка дописывает страницы к накопленным; перезагрузка заменяет.
		for (int i = 0; i < in_items.size(); ++i)
			Accumulated_Rows.append(Row_Data(in_items.at(i)));

		rows = Accumulated_Rows;
		Next_Offset = Accumulated_Rows.size();
		Has_All_Rows = in_items.size() < Page_Size;
	}

	Exercise_List->Set_Rows(rows);
	emit stateChanged();
}
//----------------------------------------------------------------------------
void Avm_Exercises::on_Muscle_Groups_Finished(int in_request_id, const QStringList &in_groups, bool in_from_cache,
                                              const AApi_Error &in_error)
{
	Q_UNUSED(in_request_id);

	if (in_error.Is_Error())
		return;

	Known_Groups = in_groups;
	emit catalogsChanged();
}
//----------------------------------------------------------------------------
void Avm_Exercises::on_Equipments_Finished(int in_request_id, const QStringList &in_equipments, bool in_from_cache,
                                           const AApi_Error &in_error)
{
	Q_UNUSED(in_request_id);
	Q_UNUSED(in_from_cache);

	if (in_error.Is_Error())
		return;

	Known_Equipments = in_equipments;
	emit catalogsChanged();
}
//----------------------------------------------------------------------------
