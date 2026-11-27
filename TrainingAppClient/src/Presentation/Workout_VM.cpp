#include "Presentation/Workout_VM.h"

#include "Data/Program/Program_Repository.h"
#include "Data/Program/Workout_Repository.h"
#include "Domain/Workout_Logger.h"

#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QUuid>
#include <QtMath>

using namespace Program_Model;

namespace
{
QString Plan_Snapshot(const SWorkout_Session &in_session, const QString &in_title, const QVariantList &in_plan)
{
	QJsonObject snapshot{
	    {"dayId", in_session.Snapshot_Day_Id.isEmpty() ? in_session.Program_Day_Id : in_session.Snapshot_Day_Id},
	    {"title", in_title},
	    {"exercises", QJsonArray::fromVariantList(in_plan)}};
	if (!in_session.Server_Program_Id.isEmpty())
		snapshot.insert("serverProgram", QJsonObject{{"program_id", in_session.Server_Program_Id},
		                                             {"week_number", in_session.Server_Week_Number},
		                                             {"day_of_week", in_session.Server_Day_Of_Week}});
	return QString::fromUtf8(QJsonDocument(snapshot).toJson(QJsonDocument::Compact));
}
} // namespace

//-------------------------------------------------------------------------------------------------------------
Avm_Workout::Avm_Workout(AsWorkout_Logger *in_logger, AWorkout_Repository *in_workouts,
                         AProgram_Repository *in_programs, QObject *in_parent)
    : QObject(in_parent), Logger_Instance(in_logger), Workout_Repository_Instance(in_workouts),
      Program_Repository_Instance(in_programs)
{
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Workout::Fail(const QString &in_detail)
{
	Error_State.Set_Last_Error(in_detail);
	emit changed();
	return false;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Workout::Find_Pending(const QString &in_user_id, SWorkout_Session &out_session)
{
	out_session = SWorkout_Session();
	QVector<SWorkout_Session> sessions;
	if (Workout_Repository_Instance == 0 || in_user_id.isEmpty())
		return Fail("Не удалось определить пользователя тренировки");
	if (!Workout_Repository_Instance->Load_Sessions(in_user_id, QString(), QString(), QString(), sessions))
		return Fail(Workout_Repository_Instance->Get_Last_Error());
	for (const SWorkout_Session &session : sessions)
		if (session.State == EWorkout_State::Draft &&
		    (out_session.ID.isEmpty() || session.Started_At > out_session.Started_At))
			out_session = session;
	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Workout::checkPending(const QString &in_user_id)
{
	Error_State.Clear_Last_Error();
	if (!User_Id.isEmpty() && User_Id != in_user_id)
	{
		Session = SWorkout_Session();
		Sets.clear();
		Plan.clear();
		User_Id.clear();
		Title_Text.clear();
		Rest_Until_Ms = 0;
		emit clockChanged();
	}
	Has_Pending = false;
	SWorkout_Session session;
	if (!Find_Pending(in_user_id, session))
		return false;
	Has_Pending = !session.ID.isEmpty();
	emit changed();
	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Workout::Build_Plan(const QString &in_user_id, const QString &in_program_id, const QString &in_date,
                             const QString &in_day_id, QString &out_day_id)
{
	SProgram program;
	QVector<SGoal_Target> targets;
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> prescriptions;
	QVector<SWeek_Slot> week_slots;
	if (Program_Repository_Instance == 0 || !Program_Repository_Instance->Load_Program_Full(
	                                            in_program_id, program, targets, days, prescriptions, week_slots))
		return Fail("Не удалось прочитать программу тренировки");
	if (program.User_Id != in_user_id)
		return Fail("Программа принадлежит другому пользователю");
	const QDate date = QDate::fromString(in_date, Qt::ISODate);
	const QDate start = QDate::fromString(
	    program.Actual_Start_Date.isEmpty() ? program.Term.Start_Date : program.Actual_Start_Date, Qt::ISODate);
	if (!date.isValid() || date.toString(Qt::ISODate) != in_date || !start.isValid())
		return Fail("Некорректная дата тренировки");
	out_day_id = in_day_id;
	if (out_day_id.isEmpty())
	{
		if (program.State != EProgram_State::Active || date < start)
			return Fail("Сначала запустите программу");
		int length = 0;
		for (const SWeek_Slot &slot : week_slots)
			length = qMax(length, slot.Rotation_Index);
		int position = date.dayOfWeek();
		if (program.Schedule_Kind == ESchedule_Kind::Rotation)
			position = length > 0 ? int(start.daysTo(date) % length) + 1 : 0;
		for (const SWeek_Slot &slot : week_slots)
			if ((program.Schedule_Kind == ESchedule_Kind::Rotation ? slot.Rotation_Index : slot.Day_Of_Week) ==
			        position &&
			    slot.Kind != ESlot_Kind::Rest)
				out_day_id = slot.Program_Day_Id;
	}
	if (out_day_id.isEmpty())
		return Fail("На эту дату тренировка не запланирована");
	Title_Text.clear();
	for (const SProgram_Day &day : days)
		if (day.ID == out_day_id && day.Kind != EDay_Kind::Rest)
			Title_Text = day.Title;
	if (Title_Text.isEmpty())
		return Fail("Тренировочный день не найден");
	QVector<QString> ids;
	for (const SDay_Exercise &exercise : prescriptions)
		if (exercise.Program_Day_Id == out_day_id)
			ids.append(exercise.Exercise_Id);
	QHash<QString, QString> titles;
	if (!Program_Repository_Instance->Load_Exercise_Titles(ids, titles))
		return Fail(Program_Repository_Instance->Get_Last_Error());
	Plan.clear();
	for (const SDay_Exercise &exercise : prescriptions)
		if (exercise.Program_Day_Id == out_day_id)
			Plan.append(QVariantMap{{"id", exercise.ID},
			                        {"exerciseId", exercise.Exercise_Id},
			                        {"title", titles.value(exercise.Exercise_Id, exercise.Exercise_Id)},
			                        {"sets", exercise.Sets},
			                        {"repsMin", exercise.Reps_Min},
			                        {"repsMax", exercise.Reps_Max},
			                        {"restSeconds", exercise.Rest_Seconds},
			                        {"weightKg", exercise.Has_Target_Weight_Kg ? exercise.Target_Weight_Kg : 0},
			                        {"hasWeight", exercise.Has_Target_Weight_Kg}});
	QVector<SWorkout_Session> history;
	QVector<QString> history_ids;
	QVector<SSet_Log> history_sets;
	if (!Workout_Repository_Instance->Load_Sessions(in_user_id, in_program_id, QString(), in_date, history))
		return Fail(Workout_Repository_Instance->Get_Last_Error());
	for (const SWorkout_Session &session : history)
		if (session.State == EWorkout_State::Completed)
			history_ids.append(session.ID);
	if (!history_ids.isEmpty() && !Workout_Repository_Instance->Load_Sets(history_ids, history_sets))
		return Fail(Workout_Repository_Instance->Get_Last_Error());
	for (QVariant &value : Plan)
	{
		QVariantMap row = value.toMap();
		QString latest_id;
		QString latest_time;
		for (const SWorkout_Session &session : history)
			if (session.State == EWorkout_State::Completed && session.Date + session.Started_At > latest_time)
				for (const SSet_Log &set : history_sets)
					if (set.Session_Id == session.ID && set.Exercise_Id == row.value("exerciseId").toString() &&
					    set.Done && !set.Is_Warmup)
					{
						latest_id = session.ID;
						latest_time = session.Date + session.Started_At;
						break;
					}
		QStringList previous;
		for (const SSet_Log &set : history_sets)
			if (set.Session_Id == latest_id && set.Exercise_Id == row.value("exerciseId").toString() && set.Done &&
			    !set.Is_Warmup)
				previous.append(QString("%1 кг × %2").arg(set.Weight_Kg).arg(set.Reps));
		row["previous"] = previous.join(" · ");
		row["previousDate"] = latest_time.left(10);
		value = row;
	}
	return !Plan.isEmpty() || Fail("В тренировке пока нет упражнений");
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Workout::startOrResume(const QString &in_user_id, const QString &in_program_id, const QString &in_date)
{
	Error_State.Clear_Last_Error();
	if (Logger_Instance == 0 || Workout_Repository_Instance == 0)
		return Fail("Запись тренировок недоступна");
	SWorkout_Session pending;
	if (!Find_Pending(in_user_id, pending))
		return false;
	if (!pending.ID.isEmpty())
		return Resume(pending);
	User_Id = in_user_id;
	Plan.clear();
	Sets.clear();
	Rest_Until_Ms = 0;
	QString day_id;
	if (!Build_Plan(in_user_id, in_program_id, in_date, QString(), day_id))
		return false;
	if (!Logger_Instance->Start_Session(in_user_id, in_date, in_program_id, day_id, QString(), Session))
		return Fail(Logger_Instance->Get_Last_Error());
	if (!Save_Runtime())
		return false;
	Has_Pending = true;
	if (!Load_Facts())
		return false;
	emit changed();
	emit clockChanged();
	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Workout::startSolo(const QString &in_user_id, const QString &in_date, const QVariantList &in_plan)
{
	return Start_Custom(in_user_id, in_date, tr("Свободная тренировка"), in_plan, {});
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Workout::startServerDay(const QString &in_user_id, const QString &in_date, const QString &in_program_id,
                                 int in_week, int in_day, const QString &in_title, const QVariantList &in_plan)
{
	const QUuid program_id(in_program_id);
	if (program_id.isNull() || in_week < 1 || in_week > 52 || in_day < 1 || in_day > 7)
		return Fail("Не удалось определить день серверной программы");
	SWorkout_Session context;
	context.Server_Program_Id = program_id.toString(QUuid::WithoutBraces);
	context.Server_Week_Number = in_week;
	context.Server_Day_Of_Week = in_day;
	return Start_Custom(in_user_id, in_date, in_title, in_plan, context);
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Workout::Start_Custom(const QString &in_user_id, const QString &in_date, const QString &in_title,
                               const QVariantList &in_plan, const SWorkout_Session &in_server_day)
{
	if (Logger_Instance == 0 || Program_Repository_Instance == 0 || QUuid(in_user_id).isNull())
		return Fail("Проверьте упражнение, вес и число подходов");

	SWorkout_Session pending;
	if (!Find_Pending(in_user_id, pending))
		return false;
	if (!pending.ID.isEmpty())
	{
		if (pending.Server_Program_Id != in_server_day.Server_Program_Id ||
		    pending.Server_Week_Number != in_server_day.Server_Week_Number ||
		    pending.Server_Day_Of_Week != in_server_day.Server_Day_Of_Week)
			return Fail("Сначала завершите открытую тренировку");
		return Resume(pending);
	}
	if (in_plan.isEmpty())
		return Fail("В тренировке пока нет упражнений");

	QVector<QString> exercise_ids;
	QVariantList plan;
	for (const QVariant &value : in_plan)
	{
		QVariantMap row = value.toMap();
		const QString exercise_id = row.value("exerciseId").toString();
		const int sets = row.value("sets").toInt();
		const int reps_min = row.value("repsMin").toInt();
		const int reps_max = row.value("repsMax").toInt();
		const double weight = row.value("weightKg").toDouble();
		if (QUuid(exercise_id).isNull() || exercise_ids.contains(exercise_id) || sets < 1 || sets > 20 ||
		    reps_min < 1 || reps_max < reps_min || reps_max > 10000 || !qIsFinite(weight) || weight < 0 ||
		    weight > 1000)
			return Fail("Проверьте упражнение, вес и число подходов");
		exercise_ids.append(exercise_id);
		row.insert("id", exercise_id);
		row.insert("restSeconds", qBound(0, row.value("restSeconds", 90).toInt(), 3600));
		row.insert("hasWeight", row.value("hasWeight", true).toBool());
		plan.append(row);
	}
	QHash<QString, QString> titles;
	if (!Program_Repository_Instance->Load_Exercise_Titles(exercise_ids, titles) ||
	    titles.size() != exercise_ids.size())
		return Fail("Упражнение недоступно на устройстве");
	for (QVariant &value : plan)
	{
		QVariantMap row = value.toMap();
		row.insert("title", titles.value(row.value("exerciseId").toString()));
		value = row;
	}
	const QString snapshot = Plan_Snapshot(in_server_day, in_title, plan);
	if (!Logger_Instance->Start_Session(in_user_id, in_date, QString(), QString(), QString(), Session, snapshot))
		return Fail(Logger_Instance->Get_Last_Error());
	Session.Server_Program_Id = in_server_day.Server_Program_Id;
	Session.Server_Week_Number = in_server_day.Server_Week_Number;
	Session.Server_Day_Of_Week = in_server_day.Server_Day_Of_Week;
	Error_State.Clear_Last_Error();
	User_Id = in_user_id;
	Title_Text = in_title;
	Plan = plan;
	Rest_Until_Ms = 0;
	if (!Load_Facts())
		return false;
	Has_Pending = true;
	emit changed();
	emit clockChanged();
	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Workout::Resume(const SWorkout_Session &in_session)
{
	Error_State.Clear_Last_Error();
	User_Id = in_session.User_Id;
	Session = in_session;
	Plan.clear();
	Sets.clear();
	QString json;
	if (!Workout_Repository_Instance->Load_Runtime(Session.ID, json, Rest_Until_Ms))
		return Fail(Workout_Repository_Instance->Get_Last_Error());
	if (!json.isEmpty())
	{
		const QJsonObject snapshot = QJsonDocument::fromJson(json.toUtf8()).object();
		Plan = snapshot.value("exercises").toArray().toVariantList();
		Title_Text = snapshot.value("title").toString();
		if (Plan.isEmpty())
			return Fail("Не удалось восстановить сохранённый план тренировки");
	}
	else
	{
		QString day_id;
		if (!Build_Plan(User_Id, Session.Program_Id, Session.Date, Session.Program_Day_Id, day_id) || !Save_Runtime())
			return false;
	}
	Has_Pending = true;
	if (!Load_Facts())
		return false;
	emit changed();
	emit clockChanged();
	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Workout::Load_Facts()
{
	if (!Workout_Repository_Instance->Load_Sets({Session.ID}, Sets))
		return Fail(Workout_Repository_Instance->Get_Last_Error());
	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Workout::saveSet(int in_exercise_index, int in_set_number, double in_weight_kg, int in_reps, double in_rpe,
                          const QString &in_note)
{
	Error_State.Clear_Last_Error();
	if (Session.ID.isEmpty() || Session.State != EWorkout_State::Draft || in_exercise_index < 0 ||
	    in_exercise_index >= Plan.size())
		return Fail("Сначала откройте тренировку");
	const QVariantMap plan = Plan.at(in_exercise_index).toMap();
	if (in_set_number < 1 || in_set_number > plan.value("sets").toInt() || !qIsFinite(in_weight_kg) ||
	    in_weight_kg < 0 || in_reps < 1 || !qIsFinite(in_rpe) || (in_rpe != -1 && (in_rpe < 5 || in_rpe > 10)))
		return Fail("Проверьте вес, повторы и RPE подхода");
	SSet_Log set;
	set.ID = QUuid::createUuidV5(QUuid(Session.ID),
	                             (plan.value("id").toString() + "/" + QString::number(in_set_number)).toUtf8())
	             .toString(QUuid::WithoutBraces);
	set.Session_Id = Session.ID;
	set.Exercise_Id = plan.value("exerciseId").toString();
	// При правке структуры старый day_exercise_id мог быть удалён (FK SET NULL).
	// Снимок сохраняем независимо; ссылку добавляем, только пока она существует.
	QVector<SDay_Exercise> current;
	if (Program_Repository_Instance != 0 && !Session.Program_Day_Id.isEmpty() &&
	    Program_Repository_Instance->Load_Day_Exercises(Session.Program_Day_Id, current))
		for (const SDay_Exercise &exercise : current)
			if (exercise.ID == plan.value("id").toString())
				set.Day_Exercise_Id = exercise.ID;
	set.Set_Number = in_set_number;
	set.Weight_Kg = in_weight_kg;
	set.Reps = in_reps;
	set.Rpe = in_rpe;
	set.Has_Rpe = in_rpe >= 5;
	set.Note = in_note.trimmed();
	set.Target_Weight_Kg = plan.value("weightKg").toDouble();
	set.Has_Target_Weight_Kg = plan.value("hasWeight").toBool();
	set.Target_Reps_Min = plan.value("repsMin").toInt();
	set.Target_Reps_Max = plan.value("repsMax").toInt();
	set.Has_Target_Reps_Range = true;
	set.Target_Reps = set.Target_Reps_Min;
	set.Has_Target_Reps = true;
	set.Done = true;
	bool editing = false;
	for (const SSet_Log &existing : Sets)
		editing = editing || existing.ID == set.ID;
	if (!Logger_Instance->Save_Set(User_Id, set))
		return Fail(Logger_Instance->Get_Last_Error());
	if (!Load_Facts())
		return false;
	if (!editing)
		Rest_Until_Ms = Now().toMSecsSinceEpoch() + qMax(0, plan.value("restSeconds").toInt()) * qint64(1000);
	const bool saved = Save_Runtime();
	emit changed();
	emit clockChanged();
	return saved;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Workout::finish()
{
	Error_State.Clear_Last_Error();
	if (Logger_Instance == 0 || Session.ID.isEmpty())
		return Fail("Тренировка не открыта");
	if (!Logger_Instance->Finish_Session(User_Id, Session.ID, Session))
		return Fail(Logger_Instance->Get_Last_Error());
	Rest_Until_Ms = 0;
	Has_Pending = false;
	emit changed();
	emit clockChanged();
	emit sessionFinished(Session.Program_Id);
	return true;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Workout::Plan_Json() const
{
	return Plan_Snapshot(Session, Title_Text, Plan);
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Workout::Save_Runtime()
{
	if (!Workout_Repository_Instance->Save_Runtime(Session.ID, Plan_Json(), Rest_Until_Ms))
		return Fail(Workout_Repository_Instance->Get_Last_Error());
	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Workout::skipRest()
{
	Error_State.Clear_Last_Error();
	const qint64 previous = Rest_Until_Ms;
	Rest_Until_Ms = 0;
	if (Session.ID.isEmpty() || !Save_Runtime())
	{
		Rest_Until_Ms = previous;
		return false;
	}
	emit clockChanged();
	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Workout::adjustRest(int in_seconds)
{
	Error_State.Clear_Last_Error();
	if (Session.ID.isEmpty() || Session.State != EWorkout_State::Draft || in_seconds < 1 || in_seconds > 3600)
		return Fail("Не удалось изменить время отдыха");
	const qint64 previous = Rest_Until_Ms;
	Rest_Until_Ms = qMax(Rest_Until_Ms, Now().toMSecsSinceEpoch()) + qint64(in_seconds) * 1000;
	if (!Save_Runtime())
	{
		Rest_Until_Ms = previous;
		return false;
	}
	emit clockChanged();
	return true;
}
//-------------------------------------------------------------------------------------------------------------
QVariantList Avm_Workout::exercises() const
{
	QVariantList rows;
	for (const QVariant &value : Plan)
	{
		QVariantMap row = value.toMap();
		QVariantList sets;
		for (int number = 1; number <= row.value("sets").toInt(); ++number)
		{
			QVariantMap fact{{"number", number}, {"done", false}};
			const QString id =
			    QUuid::createUuidV5(QUuid(Session.ID),
			                        (row.value("id").toString() + "/" + QString::number(number)).toUtf8())
			        .toString(QUuid::WithoutBraces);
			for (const SSet_Log &set : Sets)
				if (set.ID == id || (set.Day_Exercise_Id == row.value("id").toString() && set.Set_Number == number))
				{
					fact["done"] = set.Done;
					fact["weightKg"] = set.Weight_Kg;
					fact["reps"] = set.Reps;
					fact["rpe"] = set.Has_Rpe ? QVariant(set.Rpe) : QVariant();
					fact["note"] = set.Note;
				}
			sets.append(fact);
		}
		row["facts"] = sets;
		rows.append(row);
	}
	return rows;
}
//-------------------------------------------------------------------------------------------------------------
QDateTime Avm_Workout::Now() const
{
	return Pinned_Now.isValid() ? Pinned_Now : QDateTime::currentDateTimeUtc();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Workout::Set_Now(const QDateTime &in_now)
{
	Pinned_Now = in_now;
	emit clockChanged();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Workout::refreshClock()
{
	emit clockChanged();
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Workout::sessionId() const
{
	return Session.ID;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Workout::title() const
{
	return Title_Text;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Workout::errorDetail() const
{
	return Error_State.Get_Last_Error();
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Workout::hasPending() const
{
	return Has_Pending;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Workout::completed() const
{
	return !Session.ID.isEmpty() && Session.State == EWorkout_State::Completed;
}
//-------------------------------------------------------------------------------------------------------------
int Avm_Workout::restRemaining() const
{
	return int(qMax(qint64(0), (Rest_Until_Ms - Now().toMSecsSinceEpoch() + 999) / 1000));
}
//-------------------------------------------------------------------------------------------------------------
int Avm_Workout::totalSets() const
{
	int total = 0;
	for (const QVariant &row : Plan)
		total += row.toMap().value("sets").toInt();
	return total;
}
//-------------------------------------------------------------------------------------------------------------
int Avm_Workout::completedSets() const
{
	int total = 0;
	for (const SSet_Log &set : Sets)
		if (set.Done && !set.Is_Warmup)
			++total;
	return total;
}
//-------------------------------------------------------------------------------------------------------------
double Avm_Workout::tonnageKg() const
{
	double total = 0;
	for (const SSet_Log &set : Sets)
		if (set.Done && !set.Is_Warmup)
			total += set.Weight_Kg * set.Reps;
	return total;
}
//-------------------------------------------------------------------------------------------------------------
int Avm_Workout::durationSeconds() const
{
	if (Session.ID.isEmpty())
		return 0;
	if (Session.State == EWorkout_State::Completed)
		return Session.Duration_Seconds;
	const QDateTime started = QDateTime::fromString(Session.Started_At, Qt::ISODate);
	return started.isValid() ? qMax(0, int(started.secsTo(Now()))) : 0;
}
//-------------------------------------------------------------------------------------------------------------
int Avm_Workout::completedExercises() const
{
	QSet<QString> ids;
	for (const SSet_Log &set : Sets)
		if (set.Done && !set.Is_Warmup)
			ids.insert(set.Exercise_Id);
	return ids.size();
}
//-------------------------------------------------------------------------------------------------------------
