#include "Presentation/Exercises_VM.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(EXERCISES_VM_Log, "trainingapp.exercises.vm")

//-------------------------------------------------------------------------------------------------------------
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
//-------------------------------------------------------------------------------------------------------------
AList_Model *Avm_Exercises::exercise_list() const
{
	return Exercise_List;
}
//-------------------------------------------------------------------------------------------------------------
QStringList Avm_Exercises::Muscle_Groups() const
{
	return Known_Groups;
}
//-------------------------------------------------------------------------------------------------------------
QStringList Avm_Exercises::Equipments() const
{
	return Known_Equipments;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Exercises::Is_Loading() const
{
	return Loading_Flag;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Exercises::Is_From_Cache() const
{
	return From_Cache_Flag;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Exercises::Error_Key() const
{
	return Error_Key_Text;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Exercises::Can_Load_More() const
{
	return !Has_All_Rows;
}
//-------------------------------------------------------------------------------------------------------------
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
//-------------------------------------------------------------------------------------------------------------
bool Avm_Exercises::Start_Load(int in_offset)
{
	int request_id;

	if (Repository_Instance == 0)
		return false;

	// Кэш нужен при открытии и смене фильтров. При обновлении той же выборки
	// оставляем видимые строки: кэш может иметь другой порядок и локальные id.
	if (in_offset == 0 && (Selection_Changed || Exercise_List->Row_Count() == 0))
		Show_Cache();
	// Кэш мог сразу показать больше одной страницы. Первая полная страница
	// сервера не должна обрезать уже видимую глубину (в том числе после отказа).
	if (in_offset == 0)
		Refresh_Row_Count = qMax(Refresh_Row_Count, Exercise_List->Row_Count());

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
	Request_Offset = in_offset;
	Selection_Changed = false;
	Loading_Flag = true;
	emit stateChanged();

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Exercises::reload()
{
	Debounce_Timer->stop();
	Refresh_Row_Count = Selection_Changed ? 0 : Next_Offset;
	if (Selection_Changed)
		Next_Offset = 0;
	Accumulated_Rows.clear();
	Has_All_Rows = false;

	return Start_Load(0);
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Exercises::loadMore()
{
	if (Loading_Flag || !Can_Load_More())
		return false;

	return Start_Load(Next_Offset);
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises::setQuery(const QString &in_query)
{
	if (Query_Text == in_query)
		return;

	Query_Text = in_query;
	Selection_Changed = true;

	// Окно debounce перезапускается каждым символом: запрос уходит один —
	// по последнему слову.
	Debounce_Timer->start();
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Exercises::setMuscleGroups(const QStringList &in_groups)
{
	if (Muscle_Group_Filter == in_groups)
		return false;

	Muscle_Group_Filter = in_groups;
	Selection_Changed = true;

	return reload();
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Exercises::setEquipments(const QStringList &in_equipments)
{
	if (Equipment_Filter == in_equipments)
		return false;

	Equipment_Filter = in_equipments;
	Selection_Changed = true;

	return reload();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises::loadCatalogs()
{
	if (Repository_Instance == 0)
		return;

	// Справочники сначала из кэша: чипы фильтра появляются сразу, не дожидаясь
	// сетевых таймаутов. Ответ сервера перепишет их, когда придёт.
	if (Known_Groups.isEmpty())
	{
		QStringList groups;

		if (Repository_Instance->Load_Cached_Muscle_Groups(groups) && !groups.isEmpty())
		{
			Known_Groups = groups;
			emit catalogsChanged();
		}
	}

	if (Known_Equipments.isEmpty())
	{
		QStringList equipments;

		if (Repository_Instance->Load_Cached_Equipments(equipments) && !equipments.isEmpty())
		{
			Known_Equipments = equipments;
			emit catalogsChanged();
		}
	}

	Repository_Instance->Load_Muscle_Groups();
	Repository_Instance->Load_Equipments();
}
//-------------------------------------------------------------------------------------------------------------
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
	const bool has_cache = Repository_Instance->Load_Offline_List(cached) && !cached.isEmpty();

	if (!has_cache)
	{
		Repository_Instance->Mirror_Base_Catalog();
		cached = AExercises_Repository::Base_Exercises();

		if (cached.isEmpty())
			return;
	}

	QVector<QVariantMap> rows;

	for (int i = 0; i < cached.size(); ++i)
	{
		if (!Matches_Filters(cached.at(i)))
			continue;

		rows.append(Row_Data(cached.at(i)));
	}

	// Пустая выборка тоже заменяет список: старый фильтр не должен оставаться.
	Exercise_List->Update_Rows(rows, "id");
	From_Cache_Flag = true;
}
//-------------------------------------------------------------------------------------------------------------
// Фильтры кэша — те же, что уходят серверу: поиск по названию без учёта
// регистра, пересечение по группам мышц и снаряжению.
bool Avm_Exercises::Matches_Filters(const SExercises_Item &in_item) const
{
	const QString query = Query_Text.trimmed();

	if (!query.isEmpty() && !in_item.Name.contains(query, Qt::CaseInsensitive))
		return false;

	for (int i = 0; i < Muscle_Group_Filter.size(); ++i)
	{
		if (!in_item.Muscle_Groups.contains(Muscle_Group_Filter.at(i)))
			return false;
	}

	for (int i = 0; i < Equipment_Filter.size(); ++i)
	{
		if (!in_item.Equipment.contains(Equipment_Filter.at(i)))
			return false;
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises::on_Debounce_Timeout()
{
	reload();
}
//-------------------------------------------------------------------------------------------------------------
// Сброс поиска и фильтров без сетевого запроса. Каталог — общий с вкладкой
// «Упражнения»: её поиск и chips-фильтр просачивались в мини-окно выбора
// (и поиск окна — обратно на вкладку), из-за окно могло открыться пустым.
// Перечитывание делает вызывающий reload().
bool Avm_Exercises::resetFilters()
{
	const bool had_filters = !Query_Text.isEmpty() || !Muscle_Group_Filter.isEmpty() || !Equipment_Filter.isEmpty();

	Query_Text.clear();
	Muscle_Group_Filter.clear();
	Equipment_Filter.clear();
	Debounce_Timer->stop();
	Selection_Changed = Selection_Changed || had_filters;

	return had_filters;
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises::on_List_Finished(int in_request_id, const QVector<SExercises_Item> &in_items, bool in_from_cache,
                                     const AApi_Error &in_error)
{
	if (in_request_id != Current_Request_Id)
		return;

	Error_Key_Text = in_error.Is_Error() ? in_error.Key() : QString();

	if (!in_error.Is_Error())
	{
		QVector<QVariantMap> rows;
		rows.reserve(in_items.size());

		for (int i = 0; i < in_items.size(); ++i)
		{
			// Офлайн-ответ репозитория — весь каталог, без серверных фильтров.
			if (!in_from_cache || Matches_Filters(in_items.at(i)))
				rows.append(Row_Data(in_items.at(i)));
		}

		Has_All_Rows = in_from_cache || in_items.size() < Page_Size;

		if (in_from_cache)
		{
			Exercise_List->Update_Rows(rows, "id");
			Next_Offset = 0;
		}
		else if (Request_Offset == 0 || Refresh_Row_Count > 0)
		{
			// Сохраняем глубину только для той же последовательности id.
			// База офлайн и иной серверный порядок — уже другая выборка.
			if (Request_Offset == 0)
			{
				const int common_count = qMin(rows.size(), Exercise_List->Row_Count());
				for (int i = 0; i < common_count; ++i)
				{
					if (rows.at(i).value("id") != Exercise_List->get(i).value("id"))
					{
						Refresh_Row_Count = 0;
						break;
					}
				}
			}
			Accumulated_Rows.append(rows);
			// Читаем обычными страницами: глубина каталога не ограничена limit API.
			if (!Has_All_Rows && Accumulated_Rows.size() < Refresh_Row_Count && Start_Load(Accumulated_Rows.size()))
				return;

			Next_Offset = Accumulated_Rows.size();
			Exercise_List->Update_Rows(Accumulated_Rows, "id");
		}
		else
		{
			Exercise_List->Append_Rows(rows);
			Next_Offset = Request_Offset + in_items.size();
		}

		From_Cache_Flag = in_from_cache;
		Accumulated_Rows.clear();
		Refresh_Row_Count = 0;
	}
	else
	{
		// HTTP-отказ: список не очищаем. Транспортный сбой репозиторий
		// закрывает кэшем; здесь ошибку выводит подпись состояния.
		Accumulated_Rows.clear();
		Refresh_Row_Count = 0;
		// Ошибка дозагрузки/обновления не меняет видимые строки и offset:
		// повтор запрашивает ту же страницу, а не добавляет к ней весь кэш.
		if (Exercise_List->Row_Count() == 0)
		{
			From_Cache_Flag = true;
			Show_Cache();
		}
	}

	// Сигналы модели могут вызвать автодогрузку QML синхронно. Открываем её
	// только после согласования строк и Next_Offset.
	Loading_Flag = false;
	emit stateChanged();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises::on_Muscle_Groups_Finished(int in_request_id, const QStringList &in_groups, bool in_from_cache,
                                              const AApi_Error &in_error)
{
	Q_UNUSED(in_request_id);

	if (in_error.Is_Error())
		return;

	Known_Groups = in_groups;
	emit catalogsChanged();
}
//-------------------------------------------------------------------------------------------------------------
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
//-------------------------------------------------------------------------------------------------------------
