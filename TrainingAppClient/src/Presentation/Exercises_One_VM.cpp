#include "Presentation/Exercises_One_VM.h"

#include <QLoggingCategory>
#include <QUrl>

Q_LOGGING_CATEGORY(EXERCISES_One_Log, "trainingapp.exercises.one")

//-------------------------------------------------------------------------------------------------------------
Avm_Exercises_One::Avm_Exercises_One(AExercises_Repository *in_repository, QObject *in_parent)
    : QObject(in_parent), Repository_Instance(in_repository)
{
	if (Repository_Instance != 0)
		connect(Repository_Instance, &AExercises_Repository::oneFinished, this, &Avm_Exercises_One::on_One_Finished);
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Exercises_One::Exercise_Id() const
{
	return Exercise_Id_Text;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Exercises_One::Name() const
{
	return Name_Text;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Exercises_One::Description() const
{
	return Description_Text;
}
//-------------------------------------------------------------------------------------------------------------
QStringList Avm_Exercises_One::Muscle_Groups() const
{
	return Known_Groups;
}
//-------------------------------------------------------------------------------------------------------------
QStringList Avm_Exercises_One::Equipments() const
{
	return Known_Equipments;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Exercises_One::Photo_Url() const
{
	return Photo_Url_Text;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Exercises_One::Created_At() const
{
	return Created_At_Text;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Exercises_One::Is_Own() const
{
	return Is_Own_Flag;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Exercises_One::Has_Exercise() const
{
	return !Exercise_Id_Text.isEmpty() && !Name_Text.isEmpty();
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Exercises_One::Is_From_Cache() const
{
	return From_Cache_Flag;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Exercises_One::Is_Loading() const
{
	return Loading_Flag;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Exercises_One::Error_Key() const
{
	return Error_Key_Text;
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises_One::Apply_Item(const SExercises_Item &in_item, bool in_from_cache)
{
	Exercise_Id_Text = in_item.ID;
	Name_Text = in_item.Name;
	Description_Text = in_item.Description;
	Created_At_Text = in_item.Created_At;
	Known_Groups = in_item.Muscle_Groups;
	Known_Equipments = in_item.Equipment;
	From_Cache_Flag = in_from_cache;
	// Правка и удаление — только автору: чужое публичное видно списком, но
	// детальная карточка остаётся режимом «только чтение» (docs/08, §4 U6).
	Is_Own_Flag = Repository_Instance != 0 && Repository_Instance->Is_Own_Owner(in_item.Owner_ID);

	// Локальное фото (схема v9) — клиентское: есть у своего, нет у серверного
	// до загрузки. Пустая строка — плейсхолдер карточки.
	QString photo_file;

	if (Repository_Instance != 0 && !Repository_Instance->Photo_File(in_item.ID, photo_file))
		photo_file.clear();

	Photo_Url_Text = photo_file.isEmpty() ? QString("") : QUrl::fromLocalFile(photo_file).toString();

	emit exerciseChanged();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises_One::Clear_Exercise()
{
	Name_Text.clear();
	Description_Text.clear();
	Created_At_Text.clear();
	Known_Groups.clear();
	Known_Equipments.clear();
	Is_Own_Flag = false;
	Photo_Url_Text.clear();

	emit exerciseChanged();
}
//-------------------------------------------------------------------------------------------------------------
// Загрузка карточки. Мгновенный показ кэша: строка появляется сразу, сетевой
// ответ заменяет её, офлайн — она и остаётся (offline-first, как в каталоге).
void Avm_Exercises_One::load(const QString &in_exercise_id)
{
	Current_Request_Id = 0;
	Error_Key_Text.clear();
	Exercise_Id_Text = in_exercise_id.trimmed();
	From_Cache_Flag = false;
	Clear_Exercise();
	emit stateChanged();

	if (Exercise_Id_Text.isEmpty() || Repository_Instance == 0)
		return;

	// Кэш показывается мгновенно: без сети карточка своя читается из кэша.
	SExercises_Item cached;

	if (Repository_Instance->Load_Cached_One(Exercise_Id_Text, cached) && !cached.Name.isEmpty())
		Apply_Item(cached, true);

	const int request_id = Repository_Instance->Load_One(Exercise_Id_Text);

	if (request_id == 0)
	{
		// Без активной сессии приватный GET не уходит (docs/03, §3.9).
		// Показанный кэш остаётся; без кэша — понятный отказ вместо «вечной
		// загрузки».
		if (!Has_Exercise())
			Error_Key_Text = "undefined_error";

		emit stateChanged();
		return;
	}

	Current_Request_Id = request_id;
	Loading_Flag = true;
	emit stateChanged();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises_One::reset()
{
	Current_Request_Id = 0;
	Error_Key_Text.clear();
	Exercise_Id_Text.clear();
	From_Cache_Flag = false;
	Loading_Flag = false;
	Clear_Exercise();
	emit stateChanged();
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Exercises_One::loadFirstCached()
{
	QString exercise_id;

	if (Repository_Instance == 0 || !Repository_Instance->Load_Cached_First_Id(exercise_id))
		return false;

	load(exercise_id);

	return Has_Exercise();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises_One::on_One_Finished(int in_request_id, const QString &in_exercise_id,
                                        const SExercises_Item &in_item, bool in_from_cache, const AApi_Error &in_error)
{
	// Устаревший ответ (после повторного load) игнорируется по идентификатору.
	if (in_request_id != Current_Request_Id)
		return;

	Current_Request_Id = 0;
	Loading_Flag = false;

	if (in_error.Is_Error())
	{
		// 403 — чужое упражнение: клиентский ключ вместо «Не удалось выполнить
		// операцию» (docs/08, §4 U6: отказ с понятным текстом). Остальные
		// ключи проходят как есть (404 → not_found, 400 → validation_error).
		Error_Key_Text = in_error.HTTP_Status_Code() == 403 ? QString("forbidden_exercise") : in_error.Key();
		Clear_Exercise();
	}
	else if (!in_item.ID.isEmpty())
	{
		Error_Key_Text.clear();
		Apply_Item(in_item, in_from_cache);
	}
	else
	{
		Error_Key_Text = "undefined_error";
	}

	emit stateChanged();
}
//-------------------------------------------------------------------------------------------------------------
