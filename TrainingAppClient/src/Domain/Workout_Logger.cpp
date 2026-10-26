#include "Domain/Workout_Logger.h"

#include "Data/Program/Program_Repository.h"
#include "Data/Program/Workout_Repository.h"

#include <QDateTime>
#include <QLoggingCategory>
#include <QSet>
#include <QUuid>

Q_LOGGING_CATEGORY(WORKOUT_Logger_Log, "trainingapp.program.workout_logger")

//-------------------------------------------------------------------------------------------------------------
// AsWorkout_Logger
//-------------------------------------------------------------------------------------------------------------
AsWorkout_Logger::AsWorkout_Logger(AWorkout_Repository *in_workout_repository,
                                   AProgram_Repository *in_program_repository)
    : Repository_Instance(in_workout_repository), Program_Repository_Instance(in_program_repository),
      Error_State(&WORKOUT_Logger_Log())
{
}
//-------------------------------------------------------------------------------------------------------------
QString AsWorkout_Logger::Get_Last_Error() const
{
	return Error_State.Get_Last_Error();
}
//-------------------------------------------------------------------------------------------------------------
bool AsWorkout_Logger::Set_Last_Error(const QString &in_error)
{
	return Error_State.Set_Last_Error(in_error);
}
//-------------------------------------------------------------------------------------------------------------
void AsWorkout_Logger::Clear_Last_Error()
{
	Error_State.Clear_Last_Error();
}
//-------------------------------------------------------------------------------------------------------------
bool AsWorkout_Logger::Is_Valid_Date(const QString &in_date) const
{
	return QDate::fromString(in_date, Qt::ISODate).isValid();
}
//-------------------------------------------------------------------------------------------------------------
bool AsWorkout_Logger::Start_Session(const QString &in_user_id, const QString &in_date, const QString &in_program_id,
                                     const QString &in_program_day_id, const QString &in_note,
                                     SWorkout_Session &out_session)
{
	SProgram program;
	SWorkout_Session draft;

	Clear_Last_Error();
	out_session = SWorkout_Session();

	if (Repository_Instance == 0)
		return Set_Last_Error("Хранилище тренировок недоступно");

	if (in_user_id.isEmpty())
		return Set_Last_Error("Тренировке нужен владелец (userId)");

	if (!Is_Valid_Date(in_date))
		return Set_Last_Error(QString("Некорректная дата тренировки: %1").arg(in_date));

	// День уже открыт: возвращаем тот же черновик, а не создаём второй.
	if (Repository_Instance->Find_Draft(in_user_id, in_date, draft))
	{
		out_session = draft;

		return true;
	}

	if (!in_program_id.isEmpty() && !Resolve_Program_Links(in_program_id, in_program_day_id, program))
		return false;

	out_session.ID = QUuid::createUuid().toString(QUuid::WithoutBraces);
	out_session.User_Id = in_user_id;
	out_session.Program_Id = in_program_id;
	out_session.Program_Day_Id = in_program_day_id;
	out_session.Date = in_date;
	out_session.Started_At = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
	out_session.State = EWorkout_State::Draft;
	out_session.Program_Revision = program.ID.isEmpty() ? 0 : program.Revision;
	out_session.Note = in_note;
	out_session.Sync_State = ESync_State::Local;

	if (!Repository_Instance->Insert_Session(out_session))
	{
		out_session = SWorkout_Session();

		return false;
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
// День и ревизия программы читаются вместе с проверкой принадлежности: пустой
// program_day_id означал бы сессию без дня, а чужой день — фиктивную привязку.
bool AsWorkout_Logger::Resolve_Program_Links(const QString &in_program_id, const QString &in_program_day_id,
                                             SProgram &out_program)
{
	QVector<SProgram_Day> days;
	bool found = false;

	if (Program_Repository_Instance == 0)
		return Set_Last_Error("Программа недоступна: не задан репозиторий");

	if (!Program_Repository_Instance->Load_Program(in_program_id, out_program))
		return false;

	if (!Program_Repository_Instance->Load_Days(in_program_id, days))
		return false;

	for (int i = 0; i < days.size(); ++i)
	{
		if (days.at(i).ID == in_program_day_id)
			found = true;
	}

	if (!found)
		return Set_Last_Error(QString("День %1 не принадлежит программе %2").arg(in_program_day_id).arg(in_program_id));

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AsWorkout_Logger::Is_Own_Draft(const QString &in_user_id, const QString &in_session_id,
                                    SWorkout_Session &out_session)
{
	if (!Repository_Instance->Find_Session(in_session_id, out_session))
		return false;

	if (out_session.User_Id != in_user_id)
		return Set_Last_Error(QString("Тренировка %1 принадлежит другому пользователю").arg(in_session_id));

	if (out_session.State != EWorkout_State::Draft)
		return Set_Last_Error(QString("Тренировка %1 уже завершена").arg(in_session_id));

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AsWorkout_Logger::Is_Valid_Set(const SSet_Log &in_set)
{
	if (in_set.Session_Id.isEmpty())
		return Set_Last_Error("Подходу нужна тренировка");

	if (in_set.Exercise_Id.isEmpty())
		return Set_Last_Error("Подходу нужно упражнение");

	if (in_set.Set_Number < 1)
		return Set_Last_Error(QString("Номер подхода вне диапазона: %1").arg(in_set.Set_Number));

	if (in_set.Weight_Kg < 0)
		return Set_Last_Error(QString("Отрицательный вес в подходе %1").arg(in_set.Set_Number));

	if (in_set.Reps < 0)
		return Set_Last_Error(QString("Отрицательные повторы в подходе %1").arg(in_set.Set_Number));

	if (in_set.Has_Rpe && (in_set.Rpe < 5 || in_set.Rpe > 10))
		return Set_Last_Error(QString("RPE вне шкалы 5–10 в подходе %1").arg(in_set.Set_Number));

	return true;
}
//-------------------------------------------------------------------------------------------------------------
// Снимок берётся из текущего предписания: цель «100 кг × 5–8» фиксируется в
// факте, поэтому правка плана потом не перепишет выполненное (§3.8). День
// плана известен из сессии — подход его не хранит.
//-------------------------------------------------------------------------------------------------------------
bool AsWorkout_Logger::Apply_Plan_Snapshot(const QString &in_program_day_id, SSet_Log &in_out_set)
{
	QVector<SDay_Exercise> exercises;

	if (in_out_set.Day_Exercise_Id.isEmpty() || in_program_day_id.isEmpty() || Program_Repository_Instance == 0)
		return true;

	if (!Program_Repository_Instance->Load_Day_Exercises(in_program_day_id, exercises))
		return false;

	for (int i = 0; i < exercises.size(); ++i)
	{
		if (exercises.at(i).ID != in_out_set.Day_Exercise_Id)
			continue;

		in_out_set.Has_Target_Weight_Kg = exercises.at(i).Has_Target_Weight_Kg;
		in_out_set.Target_Weight_Kg = exercises.at(i).Target_Weight_Kg;
		in_out_set.Has_Target_Reps = true;
		in_out_set.Target_Reps = exercises.at(i).Reps_Min;

		return true;
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AsWorkout_Logger::Save_Set(const QString &in_user_id, const SSet_Log &in_set)
{
	SWorkout_Session session;
	SSet_Log stored = in_set;

	Clear_Last_Error();

	if (Repository_Instance == 0)
		return Set_Last_Error("Хранилище тренировок недоступно");

	if (!Is_Valid_Set(in_set))
		return false;

	if (!Is_Own_Draft(in_user_id, in_set.Session_Id, session))
		return false;

	if (!stored.Day_Exercise_Id.isEmpty() && !Apply_Plan_Snapshot(session.Program_Day_Id, stored))
		return false;

	return Repository_Instance->Save_Set(stored);
}
//-------------------------------------------------------------------------------------------------------------
bool AsWorkout_Logger::Finish_Session(const QString &in_user_id, const QString &in_session_id,
                                      SWorkout_Session &out_session)
{
	QDateTime started;
	QDateTime finished;
	SWorkout_Session session;

	Clear_Last_Error();

	if (Repository_Instance == 0)
		return Set_Last_Error("Хранилище тренировок недоступно");

	if (!Is_Own_Draft(in_user_id, in_session_id, session))
		return false;

	if (!Recalculate_Summary(in_session_id, session))
		return false;

	finished = QDateTime::currentDateTimeUtc();
	started = QDateTime::fromString(session.Started_At, Qt::ISODate);

	session.Completed_At = finished.toString(Qt::ISODate);

	if (started.isValid())
		session.Duration_Seconds = int(started.secsTo(finished));

	session.State = EWorkout_State::Completed;

	if (!Repository_Instance->Update_Session(session))
		return false;

	out_session = session;

	return true;
}
//-------------------------------------------------------------------------------------------------------------
// Итоги считаются тем же правилом, что и статистика: рабочие выполненные
// подходы дают тоннаж, все рабочие с RPE — средний (§5.2).
//-------------------------------------------------------------------------------------------------------------
bool AsWorkout_Logger::Recalculate_Summary(const QString &in_session_id, SWorkout_Session &in_out_session) const
{
	QVector<SSet_Log> sets;
	QSet<QString> exercises;
	double rpe_sum = 0;
	int rpe_count = 0;

	if (!Repository_Instance->Load_Sets(QStringList{in_session_id}, sets))
		return false;

	in_out_session.Set_Count = 0;
	in_out_session.Tonnage_Kg = 0;

	for (int i = 0; i < sets.size(); ++i)
	{
		const SSet_Log &set = sets.at(i);

		if (set.Is_Warmup || !set.Done)
			continue;

		in_out_session.Set_Count += 1;
		in_out_session.Tonnage_Kg += set.Weight_Kg * set.Reps;
		exercises.insert(set.Exercise_Id);

		if (!set.Has_Rpe)
			continue;

		rpe_sum += set.Rpe;
		++rpe_count;
	}

	in_out_session.Exercise_Count = exercises.size();
	in_out_session.Has_Avg_Rpe = false;
	in_out_session.Avg_Rpe = -1;

	if (rpe_count == 0)
		return true;

	in_out_session.Avg_Rpe = rpe_sum / rpe_count;
	in_out_session.Has_Avg_Rpe = true;

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AsWorkout_Logger::Get_Draft(const QString &in_user_id, const QString &in_date, SWorkout_Session &out_session)
{
	Clear_Last_Error();

	if (Repository_Instance == 0)
		return Set_Last_Error("Хранилище тренировок недоступно");

	if (in_user_id.isEmpty())
		return Set_Last_Error("Чтение черновика требует userId");

	if (!Is_Valid_Date(in_date))
		return Set_Last_Error(QString("Некорректная дата тренировки: %1").arg(in_date));

	return Repository_Instance->Find_Draft(in_user_id, in_date, out_session);
}
//-------------------------------------------------------------------------------------------------------------
