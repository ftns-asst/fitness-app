#pragma once

#include "Data/Exercises/Exercises_Repository.h"

#include <QObject>
#include <QString>
#include <QStringList>

//-------------------------------------------------------------------------------------------------------------
// ViewModel детальной карточки упражнения (спринт U6, docs/08-exercises-sprint-plan.md).
//
// Приватное чтение GET /exercises/{id}: 403 — чужое упражнение (в том числе
// публичное чужое — его видно списком, но детально открыть нельзя), 404 —
// не найдено. Офлайн карточка отдаётся из кэша репозитория с маркером
// from_cache — «своё» и недавно просмотренное читаются без сети.
//
// is_own определяется владельцем: правка и удаление доступны только автору.
// Экран не знает про серверные ключи: 403 нормализуется в понятный QML-ключ
// forbidden_exercise (ApiErrorText), остальные проходят как есть.
//-------------------------------------------------------------------------------------------------------------
class Avm_Exercises_One : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QString exerciseId READ Exercise_Id NOTIFY exerciseChanged)
	Q_PROPERTY(QString name READ Name NOTIFY exerciseChanged)
	Q_PROPERTY(QString description READ Description NOTIFY exerciseChanged)
	Q_PROPERTY(QStringList muscleGroups READ Muscle_Groups NOTIFY exerciseChanged)
	Q_PROPERTY(QStringList equipments READ Equipments NOTIFY exerciseChanged)
	Q_PROPERTY(QString photoUrl READ Photo_Url NOTIFY exerciseChanged)
	Q_PROPERTY(QString createdAt READ Created_At NOTIFY exerciseChanged)
	Q_PROPERTY(bool isOwn READ Is_Own NOTIFY exerciseChanged)
	Q_PROPERTY(bool hasExercise READ Has_Exercise NOTIFY exerciseChanged)
	Q_PROPERTY(bool fromCache READ Is_From_Cache NOTIFY stateChanged)
	Q_PROPERTY(bool loading READ Is_Loading NOTIFY stateChanged)
	Q_PROPERTY(QString errorKey READ Error_Key NOTIFY stateChanged)

public:
	explicit Avm_Exercises_One(AExercises_Repository *in_repository, QObject *in_parent = 0);

	QString Exercise_Id() const;
	QString Name() const;
	QString Description() const;
	QStringList Muscle_Groups() const;
	QStringList Equipments() const;
	// Локальное фото техники (схема v9): file://-URL для Image или пустая
	// строка — фото нет, карточка показывает плейсхолдер.
	QString Photo_Url() const;
	QString Created_At() const;
	bool Is_Own() const;
	bool Has_Exercise() const;
	bool Is_From_Cache() const;
	bool Is_Loading() const;
	QString Error_Key() const;

	// QML-мост (§2 CODE_STYLE: имена контракта — lowerCamelCase).
	Q_INVOKABLE void load(const QString &in_exercise_id);
	Q_INVOKABLE void reset();
	// Первый кэшированный id: карточка для аудита вёрстки (--screen
	// exercise-detail без идентификатора) показывает реальное содержимое.
	Q_INVOKABLE bool loadFirstCached();

signals:
	void exerciseChanged();
	void stateChanged();

private slots:
	void on_One_Finished(int in_request_id, const QString &in_exercise_id, const SExercises_Item &in_item,
	                     bool in_from_cache, const AApi_Error &in_error);

private:
	void Apply_Item(const SExercises_Item &in_item, bool in_from_cache);
	void Clear_Exercise();

	AExercises_Repository *Repository_Instance;
	QString Exercise_Id_Text;
	QString Name_Text;
	QString Description_Text;
	QString Created_At_Text;
	QString Error_Key_Text;
	QStringList Known_Groups;
	QStringList Known_Equipments;
	// Локальное фото: file://-URL либо пустая строка.
	QString Photo_Url_Text;
	bool Is_Own_Flag = false;
	bool From_Cache_Flag = false;
	bool Loading_Flag = false;
	int Current_Request_Id = 0;
};
//-------------------------------------------------------------------------------------------------------------
