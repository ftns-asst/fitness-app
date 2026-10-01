#include "Data/Program/Workout_Repository.h"

#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

//----------------------------------------------------------------------------
// AWorkout_Repository
//----------------------------------------------------------------------------
AWorkout_Repository::AWorkout_Repository(AsSql_Database *in_database, QObject *in_parent)
    : AProgram_Repository_Base(in_database, in_parent)
{
}
//----------------------------------------------------------------------------
bool AWorkout_Repository::Load_Sessions(const QString &in_user_id, const QString &in_program_id,
                                        const QString &in_date_from, const QString &in_date_to,
                                        QVector<SWorkout_Session> &out_sessions)
{
	QSqlQuery query(Connection());
	SWorkout_Session session;
	QString sql;

	Clear_Last_Error();
	out_sessions.clear();

	if (in_user_id.isEmpty())
		return Set_Last_Error("Чтение сессий требует userId");

	if (Check_Open() == false)
		return false;

	sql = "SELECT id, user_id, program_id, program_day_id, planned_date, planned_week_index, date, started_at, "
	      "completed_at, duration_seconds, state, deviation, exercise_count, set_count, tonnage_kg, avg_rpe, "
	      "program_revision, correction_of_session_id, note, sync_state FROM workouts WHERE user_id = ?";

	if (in_program_id.isEmpty() == false)
		sql += " AND program_id = ?";

	if (in_date_from.isEmpty() == false)
		sql += " AND date >= ?";

	if (in_date_to.isEmpty() == false)
		sql += " AND date <= ?";

	sql += " ORDER BY date, id";

	query.prepare(sql);
	query.addBindValue(in_user_id);

	if (in_program_id.isEmpty() == false)
		query.addBindValue(in_program_id);

	if (in_date_from.isEmpty() == false)
		query.addBindValue(in_date_from);

	if (in_date_to.isEmpty() == false)
		query.addBindValue(in_date_to);

	if (query.exec() == false)
		return Set_Last_Error(QString("Не удалось прочитать сессии: %1").arg(query.lastError().text()));

	while (query.next())
	{
		session = SWorkout_Session();
		Read_Session_Row(query, 0, session);
		out_sessions.append(session);
	}

	return true;
}
//----------------------------------------------------------------------------
bool AWorkout_Repository::Read_Session_Row(const QSqlQuery &in_query, int in_first_column,
                                           SWorkout_Session &out_session)
{
	const int column = in_first_column;

	out_session.ID = in_query.value(column + 0).toString();
	out_session.User_Id = in_query.value(column + 1).toString();
	out_session.Program_Id = in_query.value(column + 2).toString();
	out_session.Program_Day_Id = in_query.value(column + 3).toString();
	out_session.Planned_Date = in_query.value(column + 4).toString();
	out_session.Planned_Week_Index = in_query.value(column + 5).toInt();
	out_session.Date = in_query.value(column + 6).toString();
	out_session.Started_At = in_query.value(column + 7).toString();
	out_session.Completed_At = in_query.value(column + 8).toString();
	out_session.Duration_Seconds = in_query.value(column + 9).toInt();

	From_String(in_query.value(column + 10).toString(), out_session.State);

	out_session.Has_Deviation = in_query.value(column + 11).toString().isEmpty() == false;

	if (out_session.Has_Deviation)
		From_String(in_query.value(column + 11).toString(), out_session.Deviation);

	out_session.Exercise_Count = in_query.value(column + 12).toInt();
	out_session.Set_Count = in_query.value(column + 13).toInt();
	out_session.Tonnage_Kg = in_query.value(column + 14).toDouble();
	// NULL в avg_rpe означает «не введено», а не 0 (§3.8).
	out_session.Has_Avg_Rpe = in_query.value(column + 15).isNull() == false;
	out_session.Avg_Rpe = in_query.value(column + 15).toDouble();
	out_session.Program_Revision = in_query.value(column + 16).toInt();
	out_session.Correction_Of_Session_Id = in_query.value(column + 17).toString();
	out_session.Note = in_query.value(column + 18).toString();
	From_String(in_query.value(column + 19).toString(), out_session.Sync_State);

	return true;
}

//----------------------------------------------------------------------------
bool AWorkout_Repository::Load_Sets(const QVector<QString> &in_session_ids, QVector<SSet_Log> &out_sets)
{
	QSqlQuery query(Connection());
	SSet_Log set_log;
	QStringList placeholders;

	Clear_Last_Error();
	out_sets.clear();

	// Пустой список — читаем все подходы: так факты программы собираются
	// одним вызовом без предварительного сбора идентификаторов.
	if (in_session_ids.isEmpty() == false)
	{
		for (int i = 0; i < in_session_ids.size(); ++i)
			placeholders.append("?");

		if (Check_Open() == false)
			return false;

		query.prepare(QString("SELECT id, session_id, day_exercise_id, exercise_id, set_number, is_warmup, weight_kg, "
		                      "reps, rpe, target_weight_kg, target_reps, done, note FROM sets WHERE session_id IN (%1) "
		                      "ORDER BY session_id, set_number")
		                  .arg(placeholders.join(", ")));

		for (int i = 0; i < in_session_ids.size(); ++i)
			query.addBindValue(in_session_ids.at(i));
	}
	else
	{
		if (Check_Open() == false)
			return false;

		query.prepare(
		    "SELECT id, session_id, day_exercise_id, exercise_id, set_number, is_warmup, weight_kg, reps, rpe, "
		    "target_weight_kg, target_reps, done, note FROM sets ORDER BY session_id, set_number");
	}

	if (query.exec() == false)
		return Set_Last_Error(QString("Не удалось прочитать подходы: %1").arg(query.lastError().text()));

	while (query.next())
	{
		set_log = SSet_Log();
		set_log.ID = query.value(0).toString();
		set_log.Session_Id = query.value(1).toString();
		set_log.Day_Exercise_Id = query.value(2).toString();
		set_log.Exercise_Id = query.value(3).toString();
		set_log.Set_Number = query.value(4).toInt();
		set_log.Is_Warmup = query.value(5).toInt() != 0;
		set_log.Weight_Kg = query.value(6).toDouble();
		set_log.Reps = query.value(7).toInt();
		// RPE не введён — пара «значение + Has_», а не 0 (§3.8).
		set_log.Has_Rpe = query.value(8).isNull() == false;
		set_log.Rpe = query.value(8).toDouble();
		set_log.Has_Target_Weight_Kg = query.value(9).isNull() == false;
		set_log.Target_Weight_Kg = query.value(9).toDouble();
		set_log.Has_Target_Reps = query.value(10).isNull() == false;
		set_log.Target_Reps = query.value(10).toInt();
		set_log.Done = query.value(11).toInt() != 0;
		set_log.Note = query.value(12).toString();
		out_sets.append(set_log);
	}

	return true;
}
//----------------------------------------------------------------------------
bool AWorkout_Repository::Load_Program_Facts(const QString &in_user_id, const QString &in_program_id, const QString &in_date_from, const QString &in_date_to, QVector<SWorkout_Session> &out_sessions, QVector<SSet_Log> &out_sets)
{
	QVector<QString> session_ids;

	// Сначала сессии программы, затем их подходы: соло-сессии получают
	// program_id = NULL и в факты программы не входят (§4 инвариант 18).
	if (Load_Sessions(in_user_id, in_program_id, in_date_from, in_date_to, out_sessions) == false)
		return false;

	for (int i = 0; i < out_sessions.size(); ++i)
		session_ids.append(out_sessions.at(i).ID);

	if (Load_Sets(session_ids, out_sets) == false)
		return false;

	return true;
}
//----------------------------------------------------------------------------