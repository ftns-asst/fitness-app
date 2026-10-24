#include "Domain/Program_Service.h"

#include <QHash>
#include <QLoggingCategory>
#include <QSet>
#include <QUuid>

#include <cmath>

Q_LOGGING_CATEGORY(PROGRAM_Service_Log, "trainingapp.program.service")

//----------------------------------------------------------------------------
AsProgram_Service::AsProgram_Service(IProgram_Repository *in_repository, QObject *in_parent)
    : QObject(in_parent), Repository_Instance(in_repository), Error_State(&PROGRAM_Service_Log())
{
}
//----------------------------------------------------------------------------
QString AsProgram_Service::Get_Last_Error() const
{
	return Error_State.Get_Last_Error();
}
//----------------------------------------------------------------------------
bool AsProgram_Service::Set_Last_Error(const QString &in_error)
{
	return Error_State.Set_Last_Error(in_error);
}
//----------------------------------------------------------------------------
void AsProgram_Service::Clear_Last_Error()
{
	Error_State.Clear_Last_Error();
}
//----------------------------------------------------------------------------
bool AsProgram_Service::Create_From_Scratch(const SProgram &in_program, const QVector<SGoal_Target> &in_targets,
                                            const QVector<SProgram_Day> &in_days,
                                            const QVector<SDay_Exercise> &in_exercises,
                                            const QVector<SWeek_Slot> &in_slots, SProgram &out_program)
{
	SProgram program = in_program;

	Clear_Last_Error();
	out_program = SProgram();

	if (!Repository_Instance)
		return Set_Last_Error("Репозиторий программы не задан");

	if (program.User_Id.trimmed().isEmpty() || program.Title.trimmed().isEmpty())
		return Set_Last_Error("Для создания программы нужны userId и title");

	if (program.Source_Kind != EProgram_Source_Kind::Scratch && program.Source_Kind != EProgram_Source_Kind::Matched)
		return Set_Last_Error("Создание с нуля поддерживает sourceKind scratch или matched");

	if (program.ID.isEmpty())
		program.ID = QUuid::createUuid().toString(QUuid::WithoutBraces);

	program.Title = program.Title.trimmed();
	program.Source_Template_Id.clear();
	program.State = EProgram_State::Draft;
	program.Revision = 1;
	program.Actual_Start_Date.clear();
	program.Created_At.clear();
	program.Updated_At.clear();
	program.Sync_State = ESync_State::Local;

	if (Repository_Instance->Insert_Program_Full(program, in_targets, in_days, in_exercises, in_slots) == false)
		return Set_Last_Error(Repository_Instance->Get_Last_Error());

	out_program = program;

	return true;
}
//----------------------------------------------------------------------------
bool AsProgram_Service::Create_From_Snapshot(const SProgram &in_program, const SProgram &in_template_snapshot,
                                             const QVector<SGoal_Target> &in_targets,
                                             const QVector<SProgram_Day> &in_days,
                                             const QVector<SDay_Exercise> &in_exercises,
                                             const QVector<SWeek_Slot> &in_slots, SProgram &out_program)
{
	SProgram program = in_template_snapshot;
	QVector<SGoal_Target> targets = in_targets;
	QVector<SProgram_Day> days = in_days;
	QVector<SDay_Exercise> exercises = in_exercises;
	QVector<SWeek_Slot> week_slots = in_slots;
	QHash<QString, QString> day_ids;
	QSet<QString> all_ids;

	Clear_Last_Error();
	out_program = SProgram();

	if (!Repository_Instance)
		return Set_Last_Error("Репозиторий программы не задан");

	if (in_program.User_Id.trimmed().isEmpty() || in_program.Title.trimmed().isEmpty())
		return Set_Last_Error("Для копирования программы нужны userId и title");

	if (in_template_snapshot.ID.isEmpty() || in_template_snapshot.Title.trimmed().isEmpty())
		return Set_Last_Error("Снимок шаблона должен содержать id и title");

	if (in_template_snapshot.Source_Kind != EProgram_Source_Kind::Template)
		return Set_Last_Error("Источник копии должен быть снимком шаблона");

	program.ID = QUuid::createUuid().toString(QUuid::WithoutBraces);
	program.User_Id = in_program.User_Id.trimmed();
	program.Title = in_program.Title.trimmed();
	program.Source_Kind = EProgram_Source_Kind::Copy;
	program.Source_Template_Id = in_template_snapshot.ID;
	program.State = EProgram_State::Draft;
	program.Revision = 1;
	program.Actual_Start_Date.clear();
	program.Created_At.clear();
	program.Updated_At.clear();
	program.Sync_State = ESync_State::Local;

	for (int i = 0; i < days.size(); ++i)
	{
		QString old_id = days.at(i).ID;
		QString new_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

		if (old_id.isEmpty() || day_ids.contains(old_id) || all_ids.contains(old_id))
			return Set_Last_Error("Снимок шаблона содержит пустой или повторный id дня");

		if (days.at(i).Program_Id != in_template_snapshot.ID)
			return Set_Last_Error(QString("День %1 не принадлежит снимку шаблона").arg(old_id));

		day_ids.insert(old_id, new_id);
		all_ids.insert(old_id);
		days[i].ID = new_id;
		days[i].Program_Id = program.ID;
	}

	if (targets.size() != in_targets.size())
		return Set_Last_Error("Не удалось подготовить цели шаблона");

	for (int i = 0; i < targets.size(); ++i)
	{
		if (targets.at(i).ID.isEmpty() || all_ids.contains(targets.at(i).ID))
			return Set_Last_Error("Снимок шаблона содержит пустой или повторный id сущности");

		if (targets.at(i).Program_Id != in_template_snapshot.ID)
			return Set_Last_Error(QString("Цель %1 не принадлежит снимку шаблона").arg(targets.at(i).ID));

		all_ids.insert(targets.at(i).ID);
		targets[i].ID = QUuid::createUuid().toString(QUuid::WithoutBraces);
		targets[i].Program_Id = program.ID;
	}

	for (int i = 0; i < exercises.size(); ++i)
	{
		if (exercises.at(i).ID.isEmpty() || all_ids.contains(exercises.at(i).ID))
			return Set_Last_Error("Снимок шаблона содержит пустой или повторный id сущности");

		if (days.isEmpty() || day_ids.contains(exercises.at(i).Program_Day_Id) == false)
			return Set_Last_Error(
			    QString("Предписание %1 ссылается на день вне снимка шаблона").arg(exercises.at(i).ID));

		all_ids.insert(exercises.at(i).ID);

		exercises[i].ID = QUuid::createUuid().toString(QUuid::WithoutBraces);
		exercises[i].Program_Day_Id = day_ids.value(exercises.at(i).Program_Day_Id);
	}

	for (int i = 0; i < week_slots.size(); ++i)
	{
		if (week_slots.at(i).ID.isEmpty() || all_ids.contains(week_slots.at(i).ID))
			return Set_Last_Error("Снимок шаблона содержит пустой или повторный id сущности");

		if (week_slots.at(i).Program_Id != in_template_snapshot.ID)
			return Set_Last_Error(QString("Слот %1 не принадлежит снимку шаблона").arg(week_slots.at(i).ID));

		all_ids.insert(week_slots.at(i).ID);

		if (week_slots.at(i).Program_Day_Id.isEmpty() == false)
		{
			if (day_ids.contains(week_slots.at(i).Program_Day_Id) == false)
				return Set_Last_Error(QString("Слот %1 ссылается на день вне снимка шаблона").arg(week_slots.at(i).ID));

			week_slots[i].Program_Day_Id = day_ids.value(week_slots.at(i).Program_Day_Id);
		}

		week_slots[i].ID = QUuid::createUuid().toString(QUuid::WithoutBraces);
		week_slots[i].Program_Id = program.ID;
	}

	if (Repository_Instance->Insert_Program_Full(program, targets, days, exercises, week_slots) == false)
		return Set_Last_Error(Repository_Instance->Get_Last_Error());

	out_program = program;
	return true;
}
//----------------------------------------------------------------------------
bool AsProgram_Service::Activate(const QString &in_program_id)
{
	Clear_Last_Error();

	if (!Repository_Instance)
		return Set_Last_Error("Репозиторий программы не задан");

	if (Repository_Instance->Change_State(in_program_id, EProgram_State::Active) == false)
		return Set_Last_Error(Repository_Instance->Get_Last_Error());

	return true;
}
//----------------------------------------------------------------------------
bool AsProgram_Service::Activate_Exclusive(const QString &in_program_id, QString &out_paused_program_id)
{
	Clear_Last_Error();
	out_paused_program_id.clear();

	if (!Repository_Instance)
		return Set_Last_Error("Репозиторий программы не задан");

	if (Repository_Instance->Activate_Exclusive(in_program_id, out_paused_program_id) == false)
		return Set_Last_Error(Repository_Instance->Get_Last_Error());

	return true;
}
//----------------------------------------------------------------------------
bool AsProgram_Service::Pause(const QString &in_program_id)
{
	Clear_Last_Error();

	if (!Repository_Instance)
		return Set_Last_Error("Репозиторий программы не задан");

	if (Repository_Instance->Change_State(in_program_id, EProgram_State::Paused) == false)
		return Set_Last_Error(Repository_Instance->Get_Last_Error());

	return true;
}
//----------------------------------------------------------------------------
bool AsProgram_Service::Resume(const QString &in_program_id)
{
	QString paused_program_id;

	// Возобновление — то же включение (§2.3): если активна другая программа,
	// она уходит в паузу. Через строгий Activate вызов упирался бы в частичный
	// уникальный индекс programs_one_active_per_user.
	return Activate_Exclusive(in_program_id, paused_program_id);
}
//----------------------------------------------------------------------------
bool AsProgram_Service::Complete(const QString &in_program_id)
{
	Clear_Last_Error();

	if (!Repository_Instance)
		return Set_Last_Error("Репозиторий программы не задан");

	if (Repository_Instance->Change_State(in_program_id, EProgram_State::Completed) == false)
		return Set_Last_Error(Repository_Instance->Get_Last_Error());

	return true;
}
//----------------------------------------------------------------------------
bool AsProgram_Service::Cancel(const QString &in_program_id)
{
	Clear_Last_Error();

	if (!Repository_Instance)
		return Set_Last_Error("Репозиторий программы не задан");

	if (Repository_Instance->Change_State(in_program_id, EProgram_State::Cancelled) == false)
		return Set_Last_Error(Repository_Instance->Get_Last_Error());

	return true;
}
//----------------------------------------------------------------------------
bool AsProgram_Service::Archive(const QString &in_program_id)
{
	Clear_Last_Error();

	if (!Repository_Instance)
		return Set_Last_Error("Репозиторий программы не задан");

	if (Repository_Instance->Change_State(in_program_id, EProgram_State::Archived) == false)
		return Set_Last_Error(Repository_Instance->Get_Last_Error());

	return true;
}
//----------------------------------------------------------------------------
bool AsProgram_Service::Recommend_Weight(const SDay_Exercise &in_exercise, double in_current_weight_kg,
                                         int in_completed_sets, int in_minimum_reps, double in_max_rpe,
                                         int in_week_index, double &out_recommended_weight_kg)
{
	const SProgression_Rule &rule = in_exercise.Progression;
	double recommended_weight = in_current_weight_kg;
	double increment = rule.Has_Increment_Kg ? rule.Increment_Kg : 2.5;
	int trigger_sets = rule.Has_Trigger_Sets ? rule.Trigger_Sets : in_exercise.Sets;
	int trigger_reps = rule.Has_Trigger_Reps_Min ? rule.Trigger_Reps_Min : in_exercise.Reps_Max;

	Clear_Last_Error();

	if (!in_exercise.Has_Progression)
		return Set_Last_Error("Для предписания не задано правило прогрессии");

	if (rule.Type == EProgression_Type::Manual)
		return Set_Last_Error("Ручное правило прогрессии не рассчитывает рекомендацию");

	if (rule.Type == EProgression_Type::Max)
		return Set_Last_Error("Неизвестный тип правила прогрессии");

	if (std::isfinite(in_current_weight_kg) == false || in_current_weight_kg <= 0 || in_week_index < 1 ||
	    in_completed_sets < 0 || in_minimum_reps < 0 || std::isfinite(in_max_rpe) == false || in_max_rpe < 0 ||
	    in_max_rpe > 10)
		return Set_Last_Error("Некорректные исходные данные рекомендации веса");

	if (std::isfinite(increment) == false || increment <= 0 || trigger_sets < 1 || trigger_reps < 0)
		return Set_Last_Error("Некорректный шаг или порог правила прогрессии");

	if (rule.Has_Max_Rpe && (std::isfinite(rule.Max_Rpe) == false || rule.Max_Rpe < 5 || rule.Max_Rpe > 10))
		return Set_Last_Error("Порог RPE правила прогрессии должен быть в диапазоне 5–10");

	if (rule.Has_Deload_Every_Weeks && rule.Deload_Every_Weeks < 1)
		return Set_Last_Error("Период разгрузки должен быть не меньше одной недели");

	if (rule.Has_Deload_Every_Weeks && in_week_index % rule.Deload_Every_Weeks == 0)
	{
		recommended_weight = in_current_weight_kg * 0.6;
	}
	else if (rule.Type == EProgression_Type::PercentPerWeek)
	{
		recommended_weight = in_current_weight_kg * std::pow(1.02, in_week_index);
	}
	else if (rule.Type == EProgression_Type::LinearWeight || rule.Type == EProgression_Type::DoubleProgression)
	{
		bool successful_sets = in_completed_sets >= trigger_sets && in_minimum_reps >= trigger_reps;
		bool rpe_allowed = rule.Has_Max_Rpe == false || in_max_rpe <= 0 || in_max_rpe <= rule.Max_Rpe;

		if (successful_sets && rpe_allowed)
			recommended_weight += increment;
	}
	else
	{
		return Set_Last_Error("Неизвестный тип правила прогрессии");
	}

	if (std::isfinite(recommended_weight) == false || recommended_weight <= 0)
		return Set_Last_Error("Правило прогрессии рассчитало некорректный вес");

	out_recommended_weight_kg = recommended_weight;
	return true;
}
//----------------------------------------------------------------------------
