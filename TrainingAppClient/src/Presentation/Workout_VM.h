#pragma once

#include "Domain/Error_State.h"
#include "Domain/Program_Model.h"

#include <QDateTime>
#include <QObject>
#include <QVariantList>

class AsWorkout_Logger;
class AWorkout_Repository;
class AProgram_Repository;

//-------------------------------------------------------------------------------------------------------------
// План фиксируется при старте, факты — после каждого подхода. Отдых хранит
// абсолютное время окончания, поэтому приостановка приложения не останавливает часы.
class Avm_Workout : public QObject
{
	Q_OBJECT
	Q_PROPERTY(QVariantList exercises READ exercises NOTIFY changed)
	Q_PROPERTY(QString sessionId READ sessionId NOTIFY changed)
	Q_PROPERTY(QString title READ title NOTIFY changed)
	Q_PROPERTY(QString errorDetail READ errorDetail NOTIFY changed)
	Q_PROPERTY(bool hasPending READ hasPending NOTIFY changed)
	Q_PROPERTY(bool completed READ completed NOTIFY changed)
	Q_PROPERTY(int completedSets READ completedSets NOTIFY changed)
	Q_PROPERTY(int totalSets READ totalSets NOTIFY changed)
	Q_PROPERTY(int restRemaining READ restRemaining NOTIFY clockChanged)
	Q_PROPERTY(double tonnageKg READ tonnageKg NOTIFY changed)
	Q_PROPERTY(int durationSeconds READ durationSeconds NOTIFY clockChanged)
	Q_PROPERTY(int completedExercises READ completedExercises NOTIFY changed)

public:
	Avm_Workout(AsWorkout_Logger *in_logger, AWorkout_Repository *in_workouts, AProgram_Repository *in_programs,
	            QObject *in_parent = 0);
	Q_INVOKABLE bool startOrResume(const QString &in_user_id, const QString &in_program_id, const QString &in_date);
	Q_INVOKABLE bool checkPending(const QString &in_user_id);
	Q_INVOKABLE bool saveSet(int in_exercise_index, int in_set_number, double in_weight_kg, int in_reps,
	                         double in_rpe = -1, const QString &in_note = QString());
	Q_INVOKABLE bool finish();
	Q_INVOKABLE bool skipRest();
	Q_INVOKABLE bool adjustRest(int in_seconds);
	Q_INVOKABLE void refreshClock();
	void Set_Now(const QDateTime &in_now);
	QVariantList exercises() const;
	QString sessionId() const;
	QString title() const;
	QString errorDetail() const;
	bool hasPending() const;
	bool completed() const;
	int completedSets() const;
	int totalSets() const;
	int restRemaining() const;
	double tonnageKg() const;
	int durationSeconds() const;
	int completedExercises() const;

signals:
	void changed();
	void clockChanged();
	void sessionFinished(const QString &programId);

private:
	bool Fail(const QString &in_detail);
	bool Build_Plan(const QString &in_user_id, const QString &in_program_id, const QString &in_date,
	                const QString &in_day_id, QString &out_day_id);
	bool Load_Facts();
	bool Save_Runtime();
	bool Find_Pending(const QString &in_user_id, Program_Model::SWorkout_Session &out_session);
	QDateTime Now() const;
	QString Plan_Json() const;

	// Зависимости принадлежат AppContext, время жизни превышает время жизни VM.
	AsWorkout_Logger *Logger_Instance;
	AWorkout_Repository *Workout_Repository_Instance;
	AProgram_Repository *Program_Repository_Instance;
	Program_Model::SWorkout_Session Session;
	QVector<Program_Model::SSet_Log> Sets;
	QVariantList Plan;
	QString Title_Text;
	QString User_Id;
	bool Has_Pending = false;
	qint64 Rest_Until_Ms = 0;
	QDateTime Pinned_Now;
	AError_State Error_State;
};
//-------------------------------------------------------------------------------------------------------------
