#include "Presentation/Exercises_One_VM.h"

#include <QDir>
#include <QLoggingCategory>
#include <QNetworkReply>
#include <QTemporaryFile>
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
	if (!Photo_Url_Text.isEmpty())
		return Photo_Url_Text;
	return Media.Type == "image" ? Media.Presentation_Url() : QString();
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Exercises_One::Video_Url() const
{
	return Video_File != 0 && Video_Request == 0 && !Video_Failed && Video_Source == Media.Url &&
	               Video_Expires_At > QDateTime::currentDateTimeUtc()
	           ? QUrl::fromLocalFile(Video_File->fileName()).toString()
	           : QString();
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Exercises_One::Has_Video() const
{
	return Media.Type == "video";
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Exercises_One::Video_Loading() const
{
	return Video_Request != 0;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Exercises_One::Video_Download_Failed() const
{
	return Video_Failed;
}
//-------------------------------------------------------------------------------------------------------------
qint64 Avm_Exercises_One::Media_Expires_At() const
{
	return Media.Expires_At.isValid() ? Media.Expires_At.toMSecsSinceEpoch() : 0;
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
	Media = Repository_Instance != 0 ? Repository_Instance->Media_For(in_item.ID) : SExercise_Media();
	if (Media.Url != Video_Source)
		Clear_Video();

	emit exerciseChanged();
	emit videoChanged();
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
	Media = {};

	emit exerciseChanged();
	emit videoChanged();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises_One::Clear_Video()
{
	if (Video_Request != 0)
	{
		disconnect(Video_Request, 0, this, 0);
		Video_Request->abort();
		Video_Request->deleteLater();
		Video_Request = 0;
	}
	if (Video_File != 0)
	{
		Video_File->deleteLater();
		Video_File = 0;
	}
	Video_Source.clear();
	Video_Expires_At = {};
	Video_Failed = false;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Exercises_One::Save_Video_Data()
{
	const QByteArray chunk = Video_Request->readAll();
	return Video_File->write(chunk) == chunk.size();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises_One::loadVideo()
{
	if (!Has_Video() || Video_Request != 0 || !Video_Url().isEmpty())
		return;
	if (!Media.Is_Fresh())
	{
		refreshMedia();
		return;
	}
	Clear_Video();
	Video_Source = Media.Url;
	Video_Expires_At = Media.Expires_At;
	Video_File = new QTemporaryFile(QDir::temp().filePath("training-exercise-XXXXXX"), this);
	if (!Video_File->open())
	{
		Video_Failed = true;
		emit videoChanged();
		return;
	}

	QNetworkRequest request(Media.Url);
	request.setTransferTimeout(15000);
	Video_Request = Video_Network.get(request);
	Video_Request->setReadBufferSize(256 * 1024);
	connect(Video_Request, &QIODevice::readyRead, this,
	        [this]
	        {
		        if (!Save_Video_Data())
			        Video_Request->abort();
	        });
	connect(Video_Request, &QNetworkReply::finished, this,
	        [this]
	        {
		        const bool success = Video_Request->error() == QNetworkReply::NoError && Save_Video_Data() &&
		                             Video_File->size() > 0 && Video_File->flush();
		        Video_Request->deleteLater();
		        Video_Request = 0;
		        Video_File->close();
		        if (!success)
		        {
			        Clear_Video();
			        Video_Failed = true;
		        }
		        emit videoChanged();
	        });
	emit videoChanged();
}
//-------------------------------------------------------------------------------------------------------------
// Загрузка карточки. Мгновенный показ кэша: строка появляется сразу, сетевой
// ответ заменяет её, офлайн — она и остаётся (offline-first, как в каталоге).
void Avm_Exercises_One::load(const QString &in_exercise_id)
{
	if (Exercise_Id_Text != in_exercise_id.trimmed())
		Clear_Video();
	Current_Request_Id = 0;
	Refreshing_Media_Flag = false;
	Loading_Flag = false;
	Error_Key_Text.clear();
	Exercise_Id_Text = in_exercise_id.trimmed();
	From_Cache_Flag = false;
	Clear_Exercise();
	emit stateChanged();

	if (Exercise_Id_Text.isEmpty() || Repository_Instance == 0)
		return;

	// Упражнения встроенного шаблона доступны без серверного идентификатора.
	// Локальная карточка не отправляет бессмысленный GET /exercises/builtin-… .
	for (const SExercises_Item &item : AExercises_Repository::Base_Exercises())
		if (item.ID == Exercise_Id_Text)
		{
			Apply_Item(item, true);
			Loading_Flag = false;
			emit stateChanged();
			return;
		}

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
void Avm_Exercises_One::refreshMedia()
{
	if (Current_Request_Id != 0 || Repository_Instance == 0 || !Has_Exercise() || Media.Is_Fresh())
		return;
	Media = {};
	Clear_Video();
	emit exerciseChanged();
	emit videoChanged();
	Current_Request_Id = Repository_Instance->Load_One(Exercise_Id_Text);
	Refreshing_Media_Flag = Current_Request_Id != 0;
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Exercises_One::reset()
{
	Current_Request_Id = 0;
	Refreshing_Media_Flag = false;
	Error_Key_Text.clear();
	Exercise_Id_Text.clear();
	From_Cache_Flag = false;
	Loading_Flag = false;
	Clear_Video();
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
	if (Refreshing_Media_Flag)
	{
		Refreshing_Media_Flag = false;
		if (!in_error.Is_Error() && !in_item.ID.isEmpty())
			Apply_Item(in_item, in_from_cache);
		return;
	}

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
