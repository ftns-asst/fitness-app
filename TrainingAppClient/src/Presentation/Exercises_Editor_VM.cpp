#include "Presentation/Exercises_Editor_VM.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(EXERCISES_Editor_Log, "trainingapp.exercises.editor")

//----------------------------------------------------------------------------
Avm_Exercises_Editor::Avm_Exercises_Editor(AExercises_Repository *in_repository, QObject *in_parent)
    : QObject(in_parent), Repository_Instance(in_repository)
{
	if (Repository_Instance != 0)
	{
		connect(Repository_Instance, &AExercises_Repository::createFinished, this,
		        &Avm_Exercises_Editor::on_Create_Finished);
		connect(Repository_Instance, &AExercises_Repository::updateFinished, this,
		        &Avm_Exercises_Editor::on_Update_Finished);
		connect(Repository_Instance, &AExercises_Repository::deleteFinished, this,
		        &Avm_Exercises_Editor::on_Delete_Finished);
		connect(Repository_Instance, &AExercises_Repository::publicListFinished, this,
		        &Avm_Exercises_Editor::on_Public_List_Finished);
	}
}
//----------------------------------------------------------------------------
QString Avm_Exercises_Editor::Name() const
{
	return Name_Text;
}
//----------------------------------------------------------------------------
QString Avm_Exercises_Editor::Description() const
{
	return Description_Text;
}
//----------------------------------------------------------------------------
bool Avm_Exercises_Editor::Is_Public() const
{
	return Is_Public_Flag;
}
//----------------------------------------------------------------------------
bool Avm_Exercises_Editor::Is_Selected() const
{
	return Editing_Id.isEmpty() == false;
}
//----------------------------------------------------------------------------
QStringList Avm_Exercises_Editor::Available_Muscle_Groups() const
{
	return Known_Groups;
}
//----------------------------------------------------------------------------
QStringList Avm_Exercises_Editor::Available_Equipments() const
{
	return Known_Equipments;
}
//----------------------------------------------------------------------------
QStringList Avm_Exercises_Editor::Selected_Muscle_Groups() const
{
	return Selected_Groups;
}
//----------------------------------------------------------------------------
QStringList Avm_Exercises_Editor::Selected_Equipments() const
{
	return Selected_Equipment_List;
}
//----------------------------------------------------------------------------
QString Avm_Exercises_Editor::Error_Key() const
{
	return Error_Key_Text;
}
//----------------------------------------------------------------------------
bool Avm_Exercises_Editor::Is_Busy() const
{
	return Busy_Flag;
}
//----------------------------------------------------------------------------
// Превью: новое фото видно сразу (стейджинг), снятое прежнее — исчезает.
// file://-URL: Image в QML принимает и локальный путь, но URL не зависит от
// не-ASCII путей на Windows.
QString Avm_Exercises_Editor::Photo_Url() const
{
	if (Staged_Photo_File.isEmpty() == false)
		return QUrl::fromLocalFile(Staged_Photo_File).toString();

	if (Photo_Removed_Flag == false && Existing_Photo_File.isEmpty() == false)
		return QUrl::fromLocalFile(Existing_Photo_File).toString();

	return QString("");
}
//----------------------------------------------------------------------------
bool Avm_Exercises_Editor::Has_Photo() const
{
	return Staged_Photo_File.isEmpty() == false ||
	       (Photo_Removed_Flag == false && Existing_Photo_File.isEmpty() == false);
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor::set_Name(const QString &in_name)
{
	if (Name_Text == in_name)
		return;

	Name_Text = in_name;
	emit fieldsChanged();
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor::set_Description(const QString &in_description)
{
	if (Description_Text == in_description)
		return;

	Description_Text = in_description;
	emit fieldsChanged();
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor::set_Is_Public(bool in_is_public)
{
	if (Is_Public_Flag == in_is_public)
		return;

	Is_Public_Flag = in_is_public;
	emit fieldsChanged();
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor::reset()
{
	// Стейджинг не переживает сброс: незакоммиченное фото — мусор.
	if (Staged_Photo_File.isEmpty() == false && Repository_Instance != 0)
		Repository_Instance->Cancel_Staged_Photo(Staged_Photo_File);

	Clear_Photo_State();
	Name_Text = QString("");
	Description_Text = QString("");
	Selected_Groups.clear();
	Selected_Equipment_List.clear();
	Editing_Id.clear();
	Is_Public_Flag = false;
	Error_Key_Text = QString("");
	emit fieldsChanged();
	emit stateChanged();
}
//----------------------------------------------------------------------------
bool Avm_Exercises_Editor::loadExisting(const QString &in_exercise_id)
{
	QVector<SExercises_Item> cached;
	int i;

	// Поля читаются из кэша репозитория: правка доступна офлайн.
	if (Repository_Instance == 0 || Repository_Instance->Load_Cached_List(cached) == false)
		return false;

	for (i = 0; i < cached.size(); ++i)
	{
		if (cached.at(i).ID != in_exercise_id)
			continue;

		Name_Text = cached.at(i).Name;
		Description_Text = cached.at(i).Description;
		Selected_Groups = cached.at(i).Muscle_Groups;
		Selected_Equipment_List = cached.at(i).Equipment;
		Editing_Id = cached.at(i).ID;
		Is_Public_Flag = false;

		// Фото правки читается локально (схема v9): превью видно сразу.
		if (Repository_Instance->Photo_File(Editing_Id, Existing_Photo_File) == false)
			Existing_Photo_File.clear();

		Photo_Removed_Flag = false;

		// Состояние публикации вне ответа API: определяется списком public.
		Public_Request_Id = Repository_Instance->Load_Public_List();

		emit fieldsChanged();
		emit photoChanged();
		emit stateChanged();

		return true;
	}

	Set_Error("not_found");

	return false;
}
//----------------------------------------------------------------------------
bool Avm_Exercises_Editor::toggleMuscleGroup(const QString &in_group)
{
	int index = Selected_Groups.indexOf(in_group);

	if (index >= 0)
		Selected_Groups.removeAt(index);
	else
		Selected_Groups.append(in_group);

	emit fieldsChanged();

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Exercises_Editor::toggleEquipment(const QString &in_equipment)
{
	int index = Selected_Equipment_List.indexOf(in_equipment);

	if (index >= 0)
		Selected_Equipment_List.removeAt(index);
	else
		Selected_Equipment_List.append(in_equipment);

	emit fieldsChanged();

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Exercises_Editor::Values_Known(const QStringList &in_values, const QStringList &in_known,
                                        QString &out_error_key) const
{
	for (int i = 0; i < in_values.size(); ++i)
	{
		if (in_known.indexOf(in_values.at(i)) < 0)
		{
			out_error_key = "invalid_value";
			return false;
		}
	}

	return true;
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor::Set_Error(const QString &in_key)
{
	Error_Key_Text = in_key;
	emit stateChanged();
}
//----------------------------------------------------------------------------
bool Avm_Exercises_Editor::submit()
{
	QString unknown_key;

	Error_Key_Text.clear();

	// Локальная валидация до отправки: пустое имя и значения вне справочников
	// не доходят до сервера (docs/08, §4 U5).
	if (Name_Text.trimmed().isEmpty())
	{
		Set_Error("invalid_name");
		return false;
	}

	if (Repository_Instance == 0)
	{
		Set_Error("undefined_error");
		return false;
	}

	if (Repository_Instance->Load_Cached_Muscle_Groups(Known_Groups) == false ||
	    Repository_Instance->Load_Cached_Equipments(Known_Equipments) == false)
	{
		Set_Error("undefined_error");
		return false;
	}

	if (Values_Known(Selected_Groups, Known_Groups, unknown_key) == false ||
	    Values_Known(Selected_Equipment_List, Known_Equipments, unknown_key) == false)
	{
		Set_Error(unknown_key);
		return false;
	}

	// Без входа сервер мутации не принимает: своё упражнение живёт локально
	// (кэш + зеркало FK), иначе форма отвечала «нужен вход» и создать его
	// было невозможно.
	if (Repository_Instance->Has_Session() == false)
		return Save_Locally();

	Busy_Flag = true;
	emit stateChanged();

	if (Is_Selected())
		Update_Request_Id =
		    Repository_Instance->Update_Exercise(Editing_Id, Name_Text.trimmed(), Is_Public_Flag,
		                                         Description_Text.trimmed(), Selected_Groups, Selected_Equipment_List);
	else
		Create_Request_Id = Repository_Instance->Create_Exercise(
		    Name_Text.trimmed(), Is_Public_Flag, Description_Text.trimmed(), Selected_Groups, Selected_Equipment_List);

	if ((Is_Selected() ? Update_Request_Id : Create_Request_Id) == 0)
	{
		Busy_Flag = false;
		Set_Error("undefined_error");
		return false;
	}

	emit stateChanged();

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Exercises_Editor::Save_Locally()
{
	QString exercise_id = Editing_Id;

	Busy_Flag = false;

	if (Repository_Instance == 0 ||
	    Repository_Instance->Save_Local_Exercise(exercise_id, Name_Text, Description_Text.trimmed(), Selected_Groups,
	                                             Selected_Equipment_List) == false)
	{
		Set_Error("undefined_error");
		return false;
	}

	Editing_Id = exercise_id;
	Commit_Photo_For(exercise_id);
	emit saved(exercise_id);
	emit stateChanged();

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Exercises_Editor::remove()
{
	if (Is_Selected() == false)
		return false;

	if (Repository_Instance != 0 && Repository_Instance->Has_Session() == false)
	{
		QString closed_id = Editing_Id;

		Repository_Instance->Delete_Local_Exercise(closed_id);
		Repository_Instance->Delete_Photo(closed_id);
		reset();
		emit deleted();
		emit saved(closed_id);

		return true;
	}

	Busy_Flag = true;
	emit stateChanged();

	Delete_Request_Id = Repository_Instance->Delete_Exercise(Editing_Id);

	if (Delete_Request_Id == 0)
	{
		Busy_Flag = false;
		Set_Error("undefined_error");
		return false;
	}

	return true;
}
//----------------------------------------------------------------------------
// Справочники чипов: сначала кэш (мгновенная отрисовка), затем свежий
// запрос — Unknown-ключи не «всплывают» при росте справочников.
void Avm_Exercises_Editor::loadCatalogs()
{
	if (Repository_Instance == 0)
		return;

	if (Repository_Instance->Load_Cached_Muscle_Groups(Known_Groups) && Known_Groups.isEmpty() == false)
		emit catalogsChanged();

	if (Repository_Instance->Load_Cached_Equipments(Known_Equipments) && Known_Equipments.isEmpty() == false)
		emit catalogsChanged();

	Repository_Instance->Load_Muscle_Groups();
	Repository_Instance->Load_Equipments();
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor::on_Muscle_Groups_Finished(int in_request_id, const QStringList &in_groups,
                                                     const AApi_Error &in_error)
{
	Q_UNUSED(in_request_id);

	if (in_error.Is_Error())
		return;

	Known_Groups = in_groups;
	emit catalogsChanged();
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor::on_Equipments_Finished(int in_request_id, const QStringList &in_equipments,
                                                  const AApi_Error &in_error)
{
	Q_UNUSED(in_request_id);

	if (in_error.Is_Error())
		return;

	Known_Equipments = in_equipments;
	emit catalogsChanged();
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor::on_Create_Finished(int in_request_id, const SExercises_Item &in_item,
                                              const AApi_Error &in_error)
{
	if (in_request_id != Create_Request_Id)
		return;

	Create_Request_Id = 0;
	Busy_Flag = false;

	if (in_error.Is_Error())
	{
		// Нет сети — не отказ: упражнение сохраняется локально.
		if (in_error.HTTP_Status_Code() == 0)
		{
			Save_Locally();
			return;
		}

		// Форма сохраняет поля при отказе: пользователь правит и шлёт снова.
		Set_Error(in_error.Key());
		return;
	}

	Editing_Id = in_item.ID;
	Commit_Photo_For(in_item.ID);
	emit saved(in_item.ID);
	emit stateChanged();
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor::on_Update_Finished(int in_request_id, const SExercises_Item &in_item,
                                              const AApi_Error &in_error)
{
	if (in_request_id != Update_Request_Id)
		return;

	Update_Request_Id = 0;
	Busy_Flag = false;

	if (in_error.Is_Error())
	{
		if (in_error.HTTP_Status_Code() == 0)
		{
			Save_Locally();
			return;
		}

		Set_Error(in_error.Key());
		return;
	}

	Commit_Photo_For(Editing_Id);
	emit saved(in_item.ID);
	emit stateChanged();
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor::on_Delete_Finished(int in_request_id, const AApi_Error &in_error)
{
	if (in_request_id != Delete_Request_Id)
		return;

	Delete_Request_Id = 0;
	Busy_Flag = false;

	if (in_error.Is_Error())
	{
		Set_Error(in_error.Key());
		return;
	}

	// Кэш чистится здесь: в ответе DELETE тела нет, репозиторий id не знает.
	if (Repository_Instance != 0)
	{
		Repository_Instance->Delete_Cached(Editing_Id);

		// Фото — тоже клиентское: упражнение удалено, фото не должно
		// переживать его (строка и файл).
		Repository_Instance->Delete_Photo(Editing_Id);
	}

	QString closed_id = Editing_Id;

	reset();
	emit deleted();
	emit saved(closed_id);
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor::on_Public_List_Finished(int in_request_id, const QVector<SExercises_Item> &in_items,
                                                   bool in_from_cache, const AApi_Error &in_error)
{
	Q_UNUSED(in_from_cache);

	if (in_request_id != Public_Request_Id || Is_Selected() == false)
		return;

	Public_Request_Id = 0;

	if (in_error.Is_Error())
		return;

	// Вхождение в публичный набор — единственный признак is_public
	// (в объекте упражнения поля нет, docs/03, §3.9).
	for (int i = 0; i < in_items.size(); ++i)
	{
		if (in_items.at(i).ID == Editing_Id)
		{
			Is_Public_Flag = true;
			emit fieldsChanged();

			return;
		}
	}
}
//----------------------------------------------------------------------------
// Выбор фото: копия в app data под временным именем — id упражнения при
// создании неизвестен до ответа сервера, поэтому коммит отложен. Превью
// показывается сразу, до сохранения.
bool Avm_Exercises_Editor::attachPhoto(const QUrl &in_source)
{
	QString staged_file;
	QString error;

	if (Repository_Instance == 0 || Repository_Instance->Stage_Photo(in_source, staged_file, error) == false)
	{
		Set_Error(error.isEmpty() ? QString("photo_failed") : error);

		return false;
	}

	Staged_Photo_File = staged_file;

	// Новое фото отменяет снятие прежнего: «убрал, передумал, загрузил новое».
	Photo_Removed_Flag = false;
	Set_Error(QString(""));

	emit photoChanged();

	return true;
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor::removePhoto()
{
	// Стейджинг отменяется сразу: файл больше не нужен ни превью, ни сохранению.
	if (Staged_Photo_File.isEmpty() == false)
	{
		if (Repository_Instance != 0)
			Repository_Instance->Cancel_Staged_Photo(Staged_Photo_File);

		Staged_Photo_File.clear();
	}
	else if (Existing_Photo_File.isEmpty() == false)
	{
		// Прежнее фото удаляется сохранением: снимать до него нельзя — отмена
		// формы оставила бы фото, а карточка уже без него.
		Photo_Removed_Flag = true;
	}

	emit photoChanged();
}
//----------------------------------------------------------------------------
// Сохранение подтверждено сервером: стейджинг коммитится, снятое прежнее
// удаляется. Отказ коммита не рушит сохранение: упражнение уже на сервере,
// фото можно выбрать снова — сообщение в лог, а не ошибка формы.
void Avm_Exercises_Editor::Commit_Photo_For(const QString &in_exercise_id)
{
	if (Repository_Instance == 0)
		return;

	if (Staged_Photo_File.isEmpty() == false)
	{
		if (Repository_Instance->Commit_Photo(in_exercise_id, Staged_Photo_File) == false)
			qCWarning(EXERCISES_Editor_Log).nospace() << "photo commit failed: " << in_exercise_id;

		Staged_Photo_File.clear();
	}
	else if (Photo_Removed_Flag)
	{
		if (Repository_Instance->Delete_Photo(in_exercise_id) == false)
			qCWarning(EXERCISES_Editor_Log).nospace() << "photo removal failed: " << in_exercise_id;
	}

	Photo_Removed_Flag = false;

	QString committed;

	if (Repository_Instance->Photo_File(in_exercise_id, committed) == false)
		committed.clear();

	Existing_Photo_File = committed;

	emit photoChanged();
}
//----------------------------------------------------------------------------
void Avm_Exercises_Editor::Clear_Photo_State()
{
	Staged_Photo_File.clear();
	Existing_Photo_File.clear();
	Photo_Removed_Flag = false;
}
