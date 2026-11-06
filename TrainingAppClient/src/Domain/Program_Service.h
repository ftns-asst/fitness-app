#pragma once

#include "Domain/Error_State.h"
#include "Domain/Program_Model.h"

#include <QObject>
#include <QString>
#include <QVector>

using namespace Program_Model;

//-------------------------------------------------------------------------------------------------------------
// Минимальный доменный порт для операций создания и переходов состояния.
// Реализация SQLite находится в Data; бизнес-сервис не зависит от неё.
//-------------------------------------------------------------------------------------------------------------
class AProgram_Repository_Port
{
public:
	virtual ~AProgram_Repository_Port()
	{
	}
	virtual bool Insert_Program_Full(const SProgram &in_program, const QVector<Program_Model::SGoal_Target> &in_targets,
	                                 const QVector<SProgram_Day> &in_days, const QVector<SDay_Exercise> &in_exercises,
	                                 const QVector<SWeek_Slot> &in_slots) = 0;
	virtual bool Change_State(const QString &in_program_id, EProgram_State in_new_state) = 0;
	// Включение программы с соблюдением §2.3 («активная у пользователя одна»):
	// прежняя активная переводится в паузу, её идентификатор отдаётся наружу.
	virtual bool Activate_Exclusive(const QString &in_program_id, QString &out_paused_program_id) = 0;
	virtual QString Get_Last_Error() const = 0;
};

//-------------------------------------------------------------------------------------------------------------
// Use case «создать личную программу» из шаблона/с нуля и выполнить переходы
// состояния. Repository port не зависит от конкретного SQL-хранилища.
//-------------------------------------------------------------------------------------------------------------
// Оркестрация жизненного цикла личной программы и расчёт рекомендации веса.
// Бизнес-сервис зависит от абстракции хранения программы; шаблон и его
// структуры передаются как снимок, чтобы создание копии не зависело от
// каталожной схемы.
//-------------------------------------------------------------------------------------------------------------
class AsProgram_Service : public QObject
{
	Q_OBJECT

public:
	explicit AsProgram_Service(AProgram_Repository_Port *in_repository, QObject *in_parent = 0);

	QString Get_Last_Error() const;

	bool Create_From_Scratch(const SProgram &in_program, const QVector<SGoal_Target> &in_targets,
	                         const QVector<SProgram_Day> &in_days, const QVector<SDay_Exercise> &in_exercises,
	                         const QVector<SWeek_Slot> &in_slots, SProgram &out_program);
	bool Create_From_Snapshot(const SProgram &in_program, const SProgram &in_template_snapshot,
	                          const QVector<SGoal_Target> &in_targets, const QVector<SProgram_Day> &in_days,
	                          const QVector<SDay_Exercise> &in_exercises, const QVector<SWeek_Slot> &in_slots,
	                          SProgram &out_program);

	bool Activate(const QString &in_program_id);
	// Включение программы по §2.3: прежняя активная программа пользователя
	// уходит в паузу. Идентификатор приостановленной отдаётся наружу — экран
	// сообщает об этом пользователю, молчаливая смена чужого состояния
	// выглядела бы как потеря программы.
	bool Activate_Exclusive(const QString &in_program_id, QString &out_paused_program_id);
	bool Pause(const QString &in_program_id);
	// Возобновление — включение по тем же правилам, что Activate_Exclusive:
	// активной может быть одна программа (§2.3).
	bool Resume(const QString &in_program_id);
	bool Complete(const QString &in_program_id);
	bool Cancel(const QString &in_program_id);
	bool Archive(const QString &in_program_id);

	// Returns false when a recommendation cannot safely be calculated (for
	// example, manual progression or a deload week with no current weight).
	bool Recommend_Weight(const SDay_Exercise &in_exercise, double in_current_weight_kg, int in_completed_sets,
	                      int in_minimum_reps, double in_max_rpe, int in_week_index, double &out_recommended_weight_kg);

private:
	// Хранилище ошибки (рефакторинг ошибок): состояние и логирование в
	// PROGRAM_Service_Log живут в AError_State.
	bool Set_Last_Error(const QString &in_error);
	void Clear_Last_Error();

	AProgram_Repository_Port *Repository_Instance;
	AError_State Error_State;
};
//-------------------------------------------------------------------------------------------------------------