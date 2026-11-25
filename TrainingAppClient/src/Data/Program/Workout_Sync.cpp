#include "Data/Program/Workout_Sync.h"

#include "Data/Auth/Auth_Session.h"
#include "Data/Network/Network_Monitor.h"
#include "Data/Program/Workout_Repository.h"

#include <QDateTime>
#include <QJsonArray>
#include <QSqlQuery>
#include <QUuid>
#include <algorithm>

namespace
{
constexpr int History_Limit = 50;
constexpr auto Blocked_Until = "9999-01-01T00:00:00.000Z";
} // namespace

//-------------------------------------------------------------------------------------------------------------
AWorkout_Sync::AWorkout_Sync(AsSql_Database *in_database, AWorkout_Repository *in_workouts, AsAuth_Session *in_session,
                             AsNetwork_Monitor *in_network, QObject *in_parent)
    : QObject(in_parent), Database(in_database), Workouts(in_workouts), Session(in_session), Network(in_network)
{
	Retry_Timer.setSingleShot(true);
	connect(&Retry_Timer, &QTimer::timeout, this, &AWorkout_Sync::Flush);
	connect(Session, &AsAuth_Session::sessionChanged, this, &AWorkout_Sync::Reset_Account);
	connect(Session, &AsAuth_Session::requestFinished, this, &AWorkout_Sync::Receive);
	connect(Network, &AsNetwork_Monitor::connectionRestored, this,
	        [this]
	        {
		        Flush();
		        refreshHistory();
	        });
	Reset_Account();
}
//-------------------------------------------------------------------------------------------------------------
QString AWorkout_Sync::userId() const
{
	return Owner;
}
//-------------------------------------------------------------------------------------------------------------
QVariantList AWorkout_Sync::history() const
{
	return History;
}
//-------------------------------------------------------------------------------------------------------------
QVariantMap AWorkout_Sync::detail() const
{
	return Detail;
}
//-------------------------------------------------------------------------------------------------------------
bool AWorkout_Sync::loading() const
{
	return History_Request > 0 || Detail_Request > 0;
}
//-------------------------------------------------------------------------------------------------------------
bool AWorkout_Sync::hasMore() const
{
	return Has_More;
}
//-------------------------------------------------------------------------------------------------------------
QString AWorkout_Sync::errorKey() const
{
	return Error_Key;
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Sync::refreshHistory()
{
	Rebuild_History();
	Send_History(0);
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Sync::loadMoreHistory()
{
	if (Has_More)
		Send_History(History_Offset);
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Sync::openWorkout(const QString &in_id)
{
	Detail.clear();
	Detail_Id = QUuid(in_id).isNull() ? QString() : QUuid(in_id).toString(QUuid::WithoutBraces);
	Detail_Request = 0;
	Error_Key.clear();
	if (!Owner.isEmpty() && !Detail_Id.isEmpty())
	{
		Load_Local_Detail(Detail_Id);
		if (Detail.isEmpty() && !Network->offline())
		{
			SHttp_Request request;
			request.Path = "/workouts/" + Detail_Id;
			request.Retry_Enabled = true;
			Detail_Request = Session->Send_Authed(request);
		}
		if (Detail.isEmpty() && Detail_Request == 0)
			Error_Key = "network_unavailable";
	}
	emit changed();
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Sync::Flush()
{
	Rebuild_History();
	if (Owner.isEmpty() || Upload_Request > 0 || Network->offline())
		return;

	QSqlQuery query(Database->Sql_Connection());
	query.prepare("SELECT o.id, o.entity_id, o.payload_json, o.attempt_count, o.next_attempt_at "
	              "FROM sync_outbox o JOIN workouts w ON w.id = o.entity_id "
	              "WHERE o.entity_type = 'workout' AND w.user_id = ? AND w.program_id IS NULL "
	              "AND w.state = 'completed' AND (o.next_attempt_at IS NULL OR o.next_attempt_at <> ?) "
	              "ORDER BY o.next_attempt_at, o.id LIMIT 1");
	query.addBindValue(Owner);
	query.addBindValue(Blocked_Until);
	if (!query.exec() || !query.next())
		return;
	const qint64 delay =
	    QDateTime::currentDateTimeUtc().msecsTo(QDateTime::fromString(query.value(4).toString(), Qt::ISODate));
	if (delay > 0)
	{
		Retry_Timer.start(int(qMin<qint64>(delay, 300000)));
		return;
	}

	Sending_Id = query.value(1).toString();
	Queue_Id = query.value(0).toLongLong();
	Attempts = query.value(3).toInt();
	SHttp_Request request;
	request.Method = "PUT";
	request.Path = "/workouts/" + Sending_Id;
	request.Body = query.value(2).toString().toUtf8();
	request.Headers.insert("Content-Type", "application/json");
	Upload_Request = Session->Send_Authed(request);
	if (Upload_Request == 0)
		Retry_Timer.start(5000);
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Sync::Reset_Account()
{
	Retry_Timer.stop();
	Owner = QUuid(Session->User_ID()).isNull() ? QString() : Session->User_ID();
	Upload_Request = 0;
	History_Request = 0;
	Detail_Request = 0;
	Detail.clear();
	Detail_Id.clear();
	Error_Key.clear();
	Cloud.clear();
	History.clear();
	History_Offset = 0;
	Has_More = false;
	emit accountChanged();
	refreshHistory();
	Flush();
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Sync::Send_History(int in_offset)
{
	if (Owner.isEmpty() || History_Request > 0)
		return;
	if (Network->offline())
	{
		Error_Key = "network_unavailable";
		emit changed();
		return;
	}

	SHttp_Request request;
	request.Path = QString("/workouts?limit=%1&offset=%2").arg(History_Limit).arg(in_offset);
	request.Retry_Enabled = true;
	Pending_Offset = in_offset;
	Error_Key.clear();
	History_Request = Session->Send_Authed(request);
	emit changed();
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Sync::Receive(int in_id, const SHttp_Response &in_response)
{
	if (Owner.isEmpty() || Owner != Session->User_ID())
		return;
	if (in_id == Upload_Request)
	{
		Upload_Request = 0;
		Finish_Send(in_response);
		return;
	}
	if (in_id == History_Request)
	{
		History_Request = 0;
		if (!in_response.Success || !in_response.JSON.isArray())
			Error_Key = in_response.Success ? QString("undefined_error") : in_response.Error.Key();
		else
		{
			if (Pending_Offset == 0)
				Cloud.clear();
			const QJsonArray rows = in_response.JSON.array();
			for (const QJsonValue &value : rows)
			{
				const QJsonObject row = value.toObject();
				const QUuid id(row.value("id").toString());
				if (!id.isNull() && row.value("program_id").toString().isEmpty())
					Cloud.insert(id.toString(QUuid::WithoutBraces), row.toVariantMap());
			}
			History_Offset = Pending_Offset + int(rows.size());
			Has_More = rows.size() == History_Limit;
		}
		Rebuild_History();
	}
	else if (in_id == Detail_Request)
	{
		Detail_Request = 0;
		const QJsonObject row = in_response.JSON.object();
		if (!in_response.Success || row.value("id").toString() != Detail_Id || !row.value("exercises").isArray())
			Error_Key = in_response.Success ? QString("undefined_error") : in_response.Error.Key();
		else
			Detail = row.toVariantMap();
		emit changed();
	}
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Sync::Finish_Send(const SHttp_Response &in_response)
{
	QSqlDatabase connection = Database->Sql_Connection();
	const bool acknowledged = in_response.Success && in_response.JSON.object().value("id").toString() == Sending_Id;
	if (acknowledged && connection.transaction())
	{
		QSqlQuery mark(connection);
		mark.prepare("UPDATE workouts SET sync_state = 'synced' WHERE id = ? AND user_id = ?");
		mark.addBindValue(Sending_Id);
		mark.addBindValue(Owner);
		QSqlQuery remove(connection);
		remove.prepare("DELETE FROM sync_outbox WHERE id = ? AND entity_type = 'workout' AND entity_id = ?");
		remove.addBindValue(Queue_Id);
		remove.addBindValue(Sending_Id);
		if (mark.exec() && remove.exec() && connection.commit())
		{
			Flush();
			return;
		}
		connection.rollback();
	}
	const int status = in_response.HTTP_Status_Code;
	const bool blocked = status >= 400 && status < 500 && status != 401 && status != 408 && status != 429;
	const int delay = qMin(300000, (status == 429 ? 60000 : 5000) << qMin(Attempts, 6));
	QSqlQuery defer(connection);
	defer.prepare("UPDATE sync_outbox SET attempt_count = attempt_count + 1, next_attempt_at = ?, last_error = ? "
	              "WHERE id = ? AND entity_type = 'workout' AND entity_id = ?");
	defer.addBindValue(blocked ? QString(Blocked_Until)
	                           : QDateTime::currentDateTimeUtc().addMSecs(delay).toString(Qt::ISODateWithMs));
	defer.addBindValue(status == 409         ? QString("workout_conflict")
	                   : in_response.Success ? QString("undefined_error")
	                                         : in_response.Error.Key());
	defer.addBindValue(Queue_Id);
	defer.addBindValue(Sending_Id);
	if (defer.exec())
		Flush();
	else
		Retry_Timer.start(5000);
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Sync::Rebuild_History()
{
	QHash<QString, QVariantMap> rows = Cloud;
	if (!Owner.isEmpty())
	{
		QSqlQuery query(Database->Sql_Connection());
		query.prepare("SELECT w.id, w.completed_at, w.set_count, w.sync_state, o.next_attempt_at, o.id, o.last_error "
		              "FROM workouts w LEFT JOIN sync_outbox o ON o.entity_id = w.id AND o.entity_type = 'workout' "
		              "WHERE w.user_id = ? AND w.program_id IS NULL AND w.state = 'completed'");
		query.addBindValue(Owner);
		if (query.exec())
			while (query.next())
			{
				const QString id = query.value(0).toString();
				const bool blocked = query.value(4).toString() == Blocked_Until;
				const QString sync = blocked                    ? query.value(6).toString()
				                     : !query.value(5).isNull() ? QString("pending")
				                                                : query.value(3).toString();
				rows.insert(
				    id, {{"id", id}, {"completed_at", query.value(1)}, {"setCount", query.value(2)}, {"sync", sync}});
			}
	}
	History.clear();
	for (const QVariantMap &row : rows)
		History.append(row);
	std::sort(History.begin(), History.end(),
	          [](const QVariant &in_a, const QVariant &in_b)
	          {
		          const QVariantMap a = in_a.toMap();
		          const QVariantMap b = in_b.toMap();
		          const QDateTime time_a = QDateTime::fromString(a.value("completed_at").toString(), Qt::ISODate);
		          const QDateTime time_b = QDateTime::fromString(b.value("completed_at").toString(), Qt::ISODate);
		          return time_a == time_b ? a.value("id").toString() < b.value("id").toString() : time_a > time_b;
	          });
	emit changed();
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Sync::Load_Local_Detail(const QString &in_id)
{
	SWorkout_Session workout;
	if (!Workouts->Find_Session(in_id, workout) || workout.User_Id != Owner || !workout.Program_Id.isEmpty() ||
	    workout.State != EWorkout_State::Completed)
		return;
	QSqlQuery query(Database->Sql_Connection());
	query.prepare("SELECT s.exercise_id, e.title, s.reps, s.weight_kg, s.note FROM sets s "
	              "JOIN exercises e ON e.id = s.exercise_id WHERE s.session_id = ? AND s.done = 1 "
	              "ORDER BY s.exercise_id, s.set_number");
	query.addBindValue(in_id);
	if (!query.exec())
		return;
	QVariantList exercises;
	QVariantList facts;
	QString previous_id;
	QString title;
	while (query.next())
	{
		const QString current_id = query.value(0).toString();
		if (current_id != previous_id)
		{
			if (!previous_id.isEmpty())
				exercises.append(QVariantMap{{"exercise_id", previous_id}, {"exercise_name", title}, {"sets", facts}});
			facts.clear();
			previous_id = current_id;
			title = query.value(1).toString();
		}
		facts.append(QVariantMap{{"reps", query.value(2)}, {"weight_kg", query.value(3)}, {"note", query.value(4)}});
	}
	if (!previous_id.isEmpty())
		exercises.append(QVariantMap{{"exercise_id", previous_id}, {"exercise_name", title}, {"sets", facts}});
	Detail = {{"id", in_id}, {"completed_at", workout.Completed_At}, {"exercises", exercises}};
}
//-------------------------------------------------------------------------------------------------------------
