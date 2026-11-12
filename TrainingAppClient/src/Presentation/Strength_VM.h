#pragma once

#include "Data/Exercises/Exercises_Repository.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Workout_Repository.h"
#include "Domain/Strength_Calculator.h"
#include "Presentation/List_Model.h"

//-------------------------------------------------------------------------------------------------------------
// Borrowed repositories; the three list models are owned by this QObject.
// reload always reads workouts/sets, never a second strength store or cache.
class Avm_Strength : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool hasData READ Has_Data NOTIFY changed)
	Q_PROPERTY(QString programId READ programId NOTIFY changed)
	Q_PROPERTY(QString programTitle READ programTitle NOTIFY changed)
	Q_PROPERTY(int repTolerance READ repTolerance NOTIFY changed)
	Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)
	Q_PROPERTY(QVariantMap programIndex READ programIndex NOTIFY changed)
	Q_PROPERTY(AList_Model *exercises READ exercises CONSTANT)
	Q_PROPERTY(AList_Model *splits READ splits CONSTANT)
	Q_PROPERTY(AList_Model *status_rows READ status_rows CONSTANT)
	Q_PROPERTY(AList_Model *status_plot READ status_plot CONSTANT)

public:
	Avm_Strength(AProgram_Repository *in_program_repository, AWorkout_Repository *in_workout_repository,
	             AExercises_Repository *in_exercises_repository = 0, QObject *in_parent = 0);
	// Empty program id selects only the user's active program, never another user's.
	Q_INVOKABLE bool reload(const QString &in_user_id, const QString &in_program_id = QString());
	bool Has_Data() const;
	QString programId() const;
	QString programTitle() const;
	int repTolerance() const;
	QString errorKey() const;
	QVariantMap programIndex() const;
	AList_Model *exercises() const;
	AList_Model *splits() const;
	AList_Model *status_rows() const;
	AList_Model *status_plot() const;

signals:
	void changed();

private:
	void Clear();
	bool Fail(const QString &in_error_key);
	QString Exercise_Title(const QString &in_id) const;
	QString Split_Title(const QString &in_id) const;
	QVariantMap Observation_Row(const SStrength_Observation &in_observation) const;
	QVariantMap Exercise_Row(const SStrength_Exercise &in_exercise) const;
	static QVariantMap Index_Row(const SProgress_Index &in_index);
	static void Add_Trend(QVariantMap &io_row, const SExercise_Trend &in_trend);
	static void Add_Workload(QVariantMap &io_row, const QString &in_prefix, const SStrength_Workload &in_workload);
	static QString Status_Text(const QString &in_status);
	static QString Reason_Text(const QString &in_reason);
	static QString Symbol(const QString &in_status);

	AProgram_Repository *Program_Repository_Instance;
	AWorkout_Repository *Workout_Repository_Instance;
	AExercises_Repository *Exercises_Repository_Instance;
	AList_Model *Exercises_Model;
	AList_Model *Splits_Model;
	AList_Model *Status_Rows_Model;
	AList_Model *Status_Plot_Model;
	SProgram Program;
	SStrength_Report Report;
	QHash<QString, QString> Exercise_Titles;
	QString Error_Key_Text;
};
//-------------------------------------------------------------------------------------------------------------
