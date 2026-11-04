#include "Presentation/Exercises_VM.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(EXERCISES_VM_Log, "trainingapp.exercises.vm")

//----------------------------------------------------------------------------
Avm_Exercises::Avm_Exercises(AExercises_Repository *in_repository, int in_page_size, QObject *in_parent)
    : QObject(in_parent), Repository_Instance(in_repository), Exercise_List(new AList_Model(this)),
      Page_Size(in_page_size)
{
	// Роли списка — контракт делегатов каталога (вкладка «Упражнения» и окно
	// выбора в дне программы): id и muscleGroups нужны окну (required-свойства
	// делегата), без них строка не создаётся и список выглядит пустым.
	Exercise_List->Set_Roles(QStringList({"id", "title", "subtitle", "muscleGroups", "photoUrl"}));

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
QVariantMap Avm_Exercises::Row_Data(const SExercises_Item &in_item) const
{
	QVariantMap row;

	// Mock-контракт CatalogExerciseRow: title + muscleGroups (список ключей
	// справочника). Перевод ключей — зона QML (ExerciseTexts), C++ словарём
	// локализации не владеет.
	row.insert("id", in_item.ID);
	row.insert("title", in_item.Name);
	row.insert("muscleGroups", QVariant::fromValue(in_item.Muscle_Groups));
	row.insert("subtitle", in_item.Muscle_Groups.join(", "));

	// Локальное фото техники (схема v9): плитка каталога показывает его
	// вместо плейсхолдера; пустая строка — фото нет.
	QString photo_file;

	if (Repository_Instance != 0 && Repository_Instance->Photo_File(in_item.ID, photo_file))
		row.insert("photoUrl", QUrl::fromLocalFile(photo_file).toString());
	else
		row.insert("photoUrl", QString(""));

	return row;
}
//----------------------------------------------------------------------------
bool Avm_Exercises::Start_Load(int in_offset)
{
	int request_id;

	if (Repository_Instance == 0 || (Loading_Flag && in_offset > 0))
		return false;

	// Отложенный показ кэша: строки каталога появляются сразу, сетевой ответ
	// заменяет их, когда придёт (офлайн — это же строки и останутся).
	if (in_offset == 0)
		Show_Cache();

	// Отмена не нужна: устаревший ответ игнорируется по идентификатору, а
	// загрузка страницы (offset > 0) не мешает новому reload — их ответы
	// расходятся по Current_Request_Id.
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
	if (Repository_Instance == 0)
		return;

	// Справочники сначала из кэша: чипы фильтра появляются сразу, не дожидаясь
	// сетевых таймаутов. Ответ сервера перепишет их, когда придёт.
	if (Known_Groups.isEmpty())
	{
		QStringList groups;

		if (Repository_Instance->Load_Cached_Muscle_Groups(groups) && groups.isEmpty() == false)
		{
			Known_Groups = groups;
			emit catalogsChanged();
		}
	}

	if (Known_Equipments.isEmpty())
	{
		QStringList equipments;

		if (Repository_Instance->Load_Cached_Equipments(equipments) && equipments.isEmpty() == false)
		{
			Known_Equipments = equipments;
			emit catalogsChanged();
		}
	}

	Repository_Instance->Load_Muscle_Groups();
	Repository_Instance->Load_Equipments();
}
//----------------------------------------------------------------------------
// Кэш показывается мгновенно: репозиторий отвечает сетевым таймаутом (10 с ×
// попытки), и без этого окно выбора десятки секунд выглядело пустым — «не
// появляются упражнения». Сетевой ответ заменяет строки, офлайн-ответ
// репозитория отдаёт тот же кэш. Пустой кэш показывается базовым каталогом
// ТОЛЬКО на экране: кэш (источник истины — сервер) не засевается до
// подтверждённого сбоя сети, иначе примитивы разбавили бы серверные строки.
// Зеркало FK готовится сразу — выбор базового упражнения до офлайн-ветки
// иначе упёрся бы в FOREIGN KEY.
void Avm_Exercises::Show_Cache()
{
	QVector<SExercises_Item> cached;

	if (Repository_Instance == 0)
		return;

	// Офлайн-список = серверный кэш + локальный каталог (без дублей по id).
	// Локальный каталог обязателен: упражнения, стоящие в днях программы
	// (базовые примитивы и свои), иначе не находились бы в окне выбора.
	const bool has_cache = Repository_Instance->Load_Offline_List(cached) && cached.isEmpty() == false;

	if (has_cache == false)
	{
		Repository_Instance->Mirror_Base_Catalog();
		cached = AExercises_Repository::Base_Exercises();

		if (cached.isEmpty())
			return;
	}

	QVector<QVariantMap> rows;

	for (int i = 0; i < cached.size(); ++i)
	{
		if (Matches_Filters(cached.at(i)) == false)
			continue;

		rows.append(Row_Data(cached.at(i)));
	}

	// Пустую выборку фильтра показом не затираем: пусть остаётся то, что было
	// (офлайн-ветка репозитория всё равно перечитает кэш целиком).
	if (rows.isEmpty() == false)
		Exercise_List->Set_Rows(rows);
}
//----------------------------------------------------------------------------
// Фильтры кэша — те же, что уходят серверу: поиск по названию без учёта
// регистра, пересечение по группам мышц и снаряжению.
bool Avm_Exercises::Matches_Filters(const SExercises_Item &in_item) const
{
	const QString query = Query_Text.trimmed();

	if (query.isEmpty() == false && in_item.Name.contains(query, Qt::CaseInsensitive) == false)
		return false;

	for (int i = 0; i < Muscle_Group_Filter.size(); ++i)
	{
		if (in_item.Muscle_Groups.contains(Muscle_Group_Filter.at(i)) == false)
			return false;
	}

	for (int i = 0; i < Equipment_Filter.size(); ++i)
	{
		if (in_item.Equipment.contains(Equipment_Filter.at(i)) == false)
			return false;
	}

	return true;
}
//----------------------------------------------------------------------------
void Avm_Exercises::on_Debounce_Timeout()
{
	reload();
}
//----------------------------------------------------------------------------
// Сброс поиска и фильтров без сетевого запроса. Каталог — общий с вкладкой
// «Упражнения»: её поиск и chips-фильтр просачивались в мини-окно выбора
// (и поиск окна — обратно на вкладку), из-за окно могло открыться пустым.
// Перечитывание делает вызывающий reload().
bool Avm_Exercises::resetFilters()
{
	const bool had_filters =
	    Query_Text.isEmpty() == false || Muscle_Group_Filter.isEmpty() == false || Equipment_Filter.isEmpty() == false;

	Query_Text.clear();
	Muscle_Group_Filter.clear();
	Equipment_Filter.clear();

	return had_filters;
}
//----------------------------------------------------------------------------
void Avm_Exercises::on_List_Finished(int in_request_id, const QVector<SExercises_Item> &in_items, bool in_from_cache,
                                     const AApi_Error &in_error)
{
	if (in_request_id != Current_Request_Id)
		return;

	Loading_Flag = false;
	Error_Key_Text = in_error.Is_Error() ? in_error.Key() : QString();

	if (in_error.Is_Error() == false)
	{
		From_Cache_Flag = in_from_cache;

		// Дозагрузка дописывает страницы к накопленным; перезагрузка заменяет.
		for (int i = 0; i < in_items.size(); ++i)
			Accumulated_Rows.append(Row_Data(in_items.at(i)));

		Next_Offset = Accumulated_Rows.size();
		Has_All_Rows = in_items.size() < Page_Size;
		Exercise_List->Set_Rows(Accumulated_Rows);
	}
	else
	{
		// Сервер ответил ошибкой (транспортный сбой репозиторий уже закрыл
		// кэшем — сюда попадают HTTP-отказы). Список не очищаем: после
		// долгой «Загрузки…» окно выбора и вкладка каталога оставались бы
		// пустыми, хотя кэш и базовый каталог никуда не делись. Показываем
		// кэш/базу, ошибку выводит подпись состояния.
		Accumulated_Rows.clear();
		Next_Offset = 0;
		Has_All_Rows = false;
		From_Cache_Flag = true;
		Show_Cache();
	}

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
