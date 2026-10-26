#pragma once

#include "Data/Exercises/Exercises_Repository.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QUrl>

class QTimer;

//-------------------------------------------------------------------------------------------------------------
// ViewModel формы своего упражнения (спринт U5, docs/08-exercises-sprint-plan.md).
//
// Одна форма на создание и правку: loadExisting(id) переключает в режим
// правки (поля читаются из кэша репозитория), пустая форма — создание.
//
// Состояние «Публичное» в правке не читается из объекта упражнения (в ответе
// API поля is_public нет, docs/03, §3.9): оно определяется вхождением в
// публичный набор — при loadExisting отправляется Load_Public_List, и
// Is_Public поднимается, когда id найден в списке. Для нового упражнения —
// выключено.
//
// Валидация до отправки: имя непустое после trim; выбранные группы мышц и
// инвентарь обязаны быть значениями справочников (unknown value — отказ, а не
// серверный 400).
//-------------------------------------------------------------------------------------------------------------
class Avm_Exercises_Editor : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString name READ Name WRITE set_Name NOTIFY fieldsChanged)
	Q_PROPERTY(QString description READ Description WRITE set_Description NOTIFY fieldsChanged)
	Q_PROPERTY(bool isPublic READ Is_Public WRITE set_Is_Public NOTIFY fieldsChanged)
	Q_PROPERTY(bool selected READ Is_Selected NOTIFY fieldsChanged)
	Q_PROPERTY(QStringList availableMuscleGroups READ Available_Muscle_Groups NOTIFY catalogsChanged)
	Q_PROPERTY(QStringList availableEquipments READ Available_Equipments NOTIFY catalogsChanged)
	Q_PROPERTY(QStringList selectedMuscleGroups READ Selected_Muscle_Groups NOTIFY fieldsChanged)
	Q_PROPERTY(QStringList selectedEquipments READ Selected_Equipments NOTIFY fieldsChanged)
	Q_PROPERTY(QString errorKey READ Error_Key NOTIFY stateChanged)
	Q_PROPERTY(bool busy READ Is_Busy NOTIFY stateChanged)
	// Фото техники (референс 2089): превью file://-URL — новое фото видно
	// сразу, до сохранения; пустая строка — фото нет.
	Q_PROPERTY(QString photoUrl READ Photo_Url NOTIFY photoChanged)
	Q_PROPERTY(bool hasPhoto READ Has_Photo NOTIFY photoChanged)

public:
	explicit Avm_Exercises_Editor(AExercises_Repository *in_repository, QObject *in_parent = 0);

	QString Name() const;
	QString Description() const;
	bool Is_Public() const;
	bool Is_Selected() const;
	QStringList Available_Muscle_Groups() const;
	QStringList Available_Equipments() const;
	QStringList Selected_Muscle_Groups() const;
	QStringList Selected_Equipments() const;
	QString Error_Key() const;
	bool Is_Busy() const;
	QString Photo_Url() const;
	bool Has_Photo() const;

	void set_Name(const QString &in_name);
	void set_Description(const QString &in_description);
	void set_Is_Public(bool in_is_public);

	// QML-мост (§2 CODE_STYLE).
	Q_INVOKABLE bool loadExisting(const QString &in_exercise_id);
	Q_INVOKABLE void reset();
	Q_INVOKABLE void loadCatalogs();
	Q_INVOKABLE bool toggleMuscleGroup(const QString &in_group);
	Q_INVOKABLE bool toggleEquipment(const QString &in_equipment);
	Q_INVOKABLE bool submit();
	Q_INVOKABLE bool remove();
	// Фото техники: выбранное копируется в app data (стейджинг), превью —
	// сразу; коммит — после сохранения упражнения (id узнаётся из ответа).
	Q_INVOKABLE bool attachPhoto(const QUrl &in_source);
	// Снятие фото: стейджинг отменяется сразу, прежнее удаляется сохранением.
	Q_INVOKABLE void removePhoto();

signals:
	void fieldsChanged();
	void catalogsChanged();
	void stateChanged();
	void photoChanged();
	// Успех: создание — упражнение готово и лежит в кэше; удаление — строка
	// ушла из кэша. Экран закрывается, каталог перезагружается.
	void saved(const QString &exerciseId);
	void deleted();

private slots:
	void on_Create_Finished(int in_request_id, const SExercises_Item &in_item, const AApi_Error &in_error);
	void on_Update_Finished(int in_request_id, const SExercises_Item &in_item, const AApi_Error &in_error);
	void on_Delete_Finished(int in_request_id, const AApi_Error &in_error);
	void on_Public_List_Finished(int in_request_id, const QVector<SExercises_Item> &in_items, bool in_from_cache,
	                             const AApi_Error &in_error);
	void on_Muscle_Groups_Finished(int in_request_id, const QStringList &in_groups, const AApi_Error &in_error);
	void on_Equipments_Finished(int in_request_id, const QStringList &in_equipments, const AApi_Error &in_error);

private:
	// Проверка значений по справочникам из кэша; unknown_value — отказ.
	bool Values_Known(const QStringList &in_values, const QStringList &in_known, QString &out_error_key) const;
	void Set_Error(const QString &in_key);
	// Фото подтверждается сохранением: стейджинг коммитится в <id>.<ext>,
	// снятое прежнее удаляется. Вызывается из успехов create/update.
	void Commit_Photo_For(const QString &in_exercise_id);
	// Локальное сохранение (без входа или без сети): репозиторий пишет кэш и зеркало.
	bool Save_Locally();
	void Clear_Photo_State();

	AExercises_Repository *Repository_Instance;
	QString Name_Text;
	QString Description_Text;
	QStringList Selected_Groups;
	QStringList Selected_Equipment_List;
	QStringList Known_Groups;
	QStringList Known_Equipments;
	QString Editing_Id;
	QString Error_Key_Text;
	// Локальные фото (схема v9): путь существующего файла (правка) и
	// стейджинг нового (создание) — абсолютные пути app data.
	QString Existing_Photo_File;
	QString Staged_Photo_File;
	bool Photo_Removed_Flag = false;
	bool Is_Public_Flag = false;
	bool Busy_Flag = false;
	int Create_Request_Id = 0;
	int Update_Request_Id = 0;
	int Delete_Request_Id = 0;
	int Public_Request_Id = 0;
};
//-------------------------------------------------------------------------------------------------------------
