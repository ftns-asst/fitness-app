#include "Data/Program/Program_Repository.h"

#include <QDate>
#include <QDateTime>
#include <QHash>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>

//-------------------------------------------------------------------------------------------------------------
// Логирование ошибок, Text_Column/Number_Column/Int_Column и журнал
// ревизий (Write_Revision_Row/Bump_Revision_Internal) — в
// AProgram_Repository_Base.
//-------------------------------------------------------------------------------------------------------------
static void Read_Program_Row(QSqlQuery &in_query, SProgram &out_program)
{
	out_program = SProgram();
	out_program.ID = in_query.value(0).toString();
	out_program.User_Id = in_query.value(1).toString();
	out_program.Title = in_query.value(2).toString();
	From_String(in_query.value(3).toString(), out_program.Source_Kind);
	out_program.Source_Template_Id = in_query.value(4).toString();
	From_String(in_query.value(5).toString(), out_program.Goal);
	From_String(in_query.value(6).toString(), out_program.Level);
	From_String(in_query.value(7).toString(), out_program.Schedule_Kind);
	From_String(in_query.value(8).toString(), out_program.State);
	out_program.Revision = in_query.value(9).toInt();
	out_program.Term.Start_Date = in_query.value(10).toString();
	out_program.Actual_Start_Date = in_query.value(11).toString();
	out_program.Term.Weeks_Total = in_query.value(12).toInt();
	out_program.Term.Extended_Weeks = in_query.value(13).toInt();
	From_String(in_query.value(14).toString(), out_program.Term.Mode);
	out_program.Created_At = in_query.value(15).toString();
	out_program.Updated_At = in_query.value(16).toString();
	From_String(in_query.value(17).toString(), out_program.Sync_State);
	out_program.Note = in_query.value(18).toString();
}
//-------------------------------------------------------------------------------------------------------------
// Чтение строк остальных сущностей программы. Вынесены из методов Load_*,
// чтобы SQL-запрос и разбор полей не лежали в одном теле: правка маппинга и
// правка запроса — разные изменения.
//
// in_offset — сдвиг начала полей в наборе результата: 0 для обычного
// SELECT сущности, 1 когда перед полями идёт присоединённая колонка
// (program_day_id в Load_Day_Exercises_By_Program).
//-------------------------------------------------------------------------------------------------------------
static void Read_Goal_Target_Row(QSqlQuery &in_query, SGoal_Target &out_target)
{
	out_target = SGoal_Target();
	out_target.ID = in_query.value(0).toString();
	From_String(in_query.value(1).toString(), out_target.Metric);
	out_target.Exercise_Id = in_query.value(2).toString();

	if (!in_query.isNull(3))
	{
		out_target.Has_Measurement_Key = true;
		From_String(in_query.value(3).toString(), out_target.Measurement_Key);
	}

	From_String(in_query.value(4).toString(), out_target.Unit);

	if (!in_query.isNull(5))
	{
		out_target.Has_Start_Value = true;
		out_target.Start_Value = in_query.value(5).toDouble();
	}

	out_target.Target_Value = in_query.value(6).toDouble();

	if (!in_query.isNull(7))
	{
		out_target.Has_Target_Weight_Kg = true;
		out_target.Target_Weight_Kg = in_query.value(7).toDouble();
	}

	if (!in_query.isNull(8))
	{
		out_target.Has_Target_Reps = true;
		out_target.Target_Reps = in_query.value(8).toInt();
	}

	From_String(in_query.value(9).toString(), out_target.Direction);
	out_target.Target_Date = in_query.value(10).toString();
	From_String(in_query.value(11).toString(), out_target.State);
}
//-------------------------------------------------------------------------------------------------------------
static void Read_Day_Row(QSqlQuery &in_query, const QString &in_program_id, SProgram_Day &out_day)
{
	out_day = SProgram_Day();
	out_day.Program_Id = in_program_id;
	out_day.ID = in_query.value(0).toString();
	out_day.Title = in_query.value(1).toString();
	From_String(in_query.value(2).toString(), out_day.Kind);

	if (!in_query.isNull(3))
		Parse_Json_Array(in_query.value(3).toString(), out_day.Focus);

	out_day.Estimated_Minutes = in_query.value(4).toInt();
	out_day.Order_Index = in_query.value(5).toInt();
	out_day.Source_Day_Id = in_query.value(6).toString();
	out_day.Note = in_query.value(7).toString();
}
//-------------------------------------------------------------------------------------------------------------
static void Read_Day_Exercise_Fields(QSqlQuery &in_query, int in_offset, SDay_Exercise &out_exercise)
{
	out_exercise = SDay_Exercise();
	out_exercise.ID = in_query.value(in_offset + 0).toString();
	out_exercise.Exercise_Id = in_query.value(in_offset + 1).toString();
	out_exercise.Order_Index = in_query.value(in_offset + 2).toInt();
	out_exercise.Sets = in_query.value(in_offset + 3).toInt();
	out_exercise.Reps_Min = in_query.value(in_offset + 4).toInt();
	out_exercise.Reps_Max = in_query.value(in_offset + 5).toInt();
	From_String(in_query.value(in_offset + 6).toString(), out_exercise.Weight_Mode);

	if (!in_query.isNull(in_offset + 7))
	{
		out_exercise.Has_Target_Weight_Kg = true;
		out_exercise.Target_Weight_Kg = in_query.value(in_offset + 7).toDouble();
	}

	if (!in_query.isNull(in_offset + 8))
	{
		out_exercise.Has_Percent_Of_1rm = true;
		out_exercise.Percent_Of_1rm = in_query.value(in_offset + 8).toDouble();
	}

	if (!in_query.isNull(in_offset + 9))
	{
		out_exercise.Has_Rpe_Target = true;
		out_exercise.Rpe_Target = in_query.value(in_offset + 9).toDouble();
	}

	out_exercise.Rest_Seconds = in_query.value(in_offset + 10).toInt();
	out_exercise.Tempo = in_query.value(in_offset + 11).toString();
	out_exercise.Superset_Group = in_query.value(in_offset + 12).toString();
	out_exercise.Warmup_Sets = in_query.value(in_offset + 13).toInt();

	if (!in_query.isNull(in_offset + 14))
	{
		out_exercise.Has_Progression = true;
		Parse_Progression_Json(in_query.value(in_offset + 14).toString(), out_exercise.Progression);
	}

	if (!in_query.isNull(in_offset + 15))
		Parse_Json_Array(in_query.value(in_offset + 15).toString(), out_exercise.Substitution_Ids);

	out_exercise.Is_Optional = in_query.value(in_offset + 16).toInt() != 0;
	out_exercise.Note = in_query.value(in_offset + 17).toString();
}
//-------------------------------------------------------------------------------------------------------------
static void Read_Slot_Row(QSqlQuery &in_query, const QString &in_program_id, SWeek_Slot &out_slot)
{
	out_slot = SWeek_Slot();
	out_slot.Program_Id = in_program_id;
	out_slot.ID = in_query.value(0).toString();
	out_slot.Day_Of_Week = in_query.value(1).isNull() ? 0 : in_query.value(1).toInt();
	out_slot.Rotation_Index = in_query.value(2).isNull() ? 0 : in_query.value(2).toInt();
	out_slot.Program_Day_Id = in_query.value(3).toString();
	From_String(in_query.value(4).toString(), out_slot.Kind);
	out_slot.Is_Anchor = in_query.value(5).toInt() != 0;
	out_slot.Note = in_query.value(6).toString();
}
//-------------------------------------------------------------------------------------------------------------
// Снимки разделов для program_revisions.snapshot_json (§3.9): старое
// состояние до правки. Формат — компактный JSON, ключи camelCase.
//-------------------------------------------------------------------------------------------------------------
static QJsonObject Goal_Target_Snapshot_Object(const SGoal_Target &in_target)
{
	QJsonObject object;

	object.insert("id", in_target.ID);
	object.insert("metric", To_String(in_target.Metric));

	if (!in_target.Exercise_Id.isEmpty())
		object.insert("exerciseId", in_target.Exercise_Id);

	if (in_target.Has_Measurement_Key)
		object.insert("measurementKey", To_String(in_target.Measurement_Key));

	object.insert("unit", To_String(in_target.Unit));

	if (in_target.Has_Start_Value)
		object.insert("startValue", in_target.Start_Value);

	object.insert("targetValue", in_target.Target_Value);

	if (in_target.Has_Target_Weight_Kg)
		object.insert("targetWeightKg", in_target.Target_Weight_Kg);

	if (in_target.Has_Target_Reps)
		object.insert("targetReps", in_target.Target_Reps);

	object.insert("direction", To_String(in_target.Direction));

	if (!in_target.Target_Date.isEmpty())
		object.insert("targetDate", in_target.Target_Date);

	object.insert("state", To_String(in_target.State));

	return object;
}
//-------------------------------------------------------------------------------------------------------------
static QJsonObject Day_Snapshot_Object(const SProgram_Day &in_day)
{
	QJsonObject object;
	QJsonArray focus;

	object.insert("id", in_day.ID);
	object.insert("title", in_day.Title);
	object.insert("kind", To_String(in_day.Kind));

	for (int i = 0; i < in_day.Focus.size(); ++i)
		focus.append(in_day.Focus.at(i));

	if (!focus.isEmpty())
		object.insert("focus", focus);

	object.insert("estimatedMinutes", in_day.Estimated_Minutes);
	object.insert("orderIndex", in_day.Order_Index);

	if (!in_day.Source_Day_Id.isEmpty())
		object.insert("sourceDayId", in_day.Source_Day_Id);

	if (!in_day.Note.isEmpty())
		object.insert("note", in_day.Note);

	return object;
}
//-------------------------------------------------------------------------------------------------------------
static QJsonObject Day_Exercise_Snapshot_Object(const SDay_Exercise &in_exercise)
{
	QJsonObject object;
	QJsonArray substitutions;
	QJsonDocument progression;

	object.insert("id", in_exercise.ID);
	object.insert("programDayId", in_exercise.Program_Day_Id);
	object.insert("exerciseId", in_exercise.Exercise_Id);
	object.insert("orderIndex", in_exercise.Order_Index);
	object.insert("sets", in_exercise.Sets);
	object.insert("repsMin", in_exercise.Reps_Min);
	object.insert("repsMax", in_exercise.Reps_Max);
	object.insert("weightMode", To_String(in_exercise.Weight_Mode));

	if (in_exercise.Has_Target_Weight_Kg)
		object.insert("targetWeightKg", in_exercise.Target_Weight_Kg);

	if (in_exercise.Has_Percent_Of_1rm)
		object.insert("percentOf1rm", in_exercise.Percent_Of_1rm);

	if (in_exercise.Has_Rpe_Target)
		object.insert("rpeTarget", in_exercise.Rpe_Target);

	object.insert("restSeconds", in_exercise.Rest_Seconds);

	if (!in_exercise.Tempo.isEmpty())
		object.insert("tempo", in_exercise.Tempo);

	if (!in_exercise.Superset_Group.isEmpty())
		object.insert("supersetGroup", in_exercise.Superset_Group);

	object.insert("warmupSets", in_exercise.Warmup_Sets);

	if (in_exercise.Has_Progression)
	{
		progression = QJsonDocument::fromJson(To_Progression_Json(in_exercise.Progression).toUtf8());
		object.insert("progression", progression.object());
	}

	for (int i = 0; i < in_exercise.Substitution_Ids.size(); ++i)
		substitutions.append(in_exercise.Substitution_Ids.at(i));

	if (!substitutions.isEmpty())
		object.insert("substitutionIds", substitutions);

	object.insert("isOptional", in_exercise.Is_Optional);

	if (!in_exercise.Note.isEmpty())
		object.insert("note", in_exercise.Note);

	return object;
}
//-------------------------------------------------------------------------------------------------------------
static QJsonObject Slot_Snapshot_Object(const SWeek_Slot &in_slot)
{
	QJsonObject object;

	object.insert("id", in_slot.ID);

	if (in_slot.Day_Of_Week != 0)
		object.insert("dayOfWeek", in_slot.Day_Of_Week);

	if (in_slot.Rotation_Index != 0)
		object.insert("rotationIndex", in_slot.Rotation_Index);

	if (!in_slot.Program_Day_Id.isEmpty())
		object.insert("programDayId", in_slot.Program_Day_Id);

	object.insert("kind", To_String(in_slot.Kind));

	object.insert("isAnchor", in_slot.Is_Anchor);

	if (!in_slot.Note.isEmpty())
		object.insert("note", in_slot.Note);

	return object;
}
//-------------------------------------------------------------------------------------------------------------
static QString Term_Snapshot_Json(const STerm &in_term)
{
	QJsonObject object;

	object.insert("startDate", in_term.Start_Date);
	object.insert("weeksTotal", in_term.Weeks_Total);
	object.insert("extendedWeeks", in_term.Extended_Weeks);
	object.insert("mode", To_String(in_term.Mode));

	if (!in_term.Compute_End_Date().isEmpty())
		object.insert("endDate", in_term.Compute_End_Date());

	return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}

//-------------------------------------------------------------------------------------------------------------
// AProgram_Repository
//-------------------------------------------------------------------------------------------------------------
AProgram_Repository::AProgram_Repository(AsSql_Database *in_database, QObject *in_parent)
    : AProgram_Repository_Base(in_database, in_parent)
{
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Check_Editable(const QString &in_program_id, SProgram &out_program)
{
	if (!Load_Program(in_program_id, out_program))
		return false;

	// Инвариант 24 (§4): completed/cancelled/archived — read-only по
	// структуре (общая проверка Is_Read_Only_State в базе).
	if (Is_Read_Only_State(out_program.State))
		return Set_Last_Error(QString("Программа %1 в состоянии %2: правка запрещена")
		                          .arg(in_program_id)
		                          .arg(To_String(out_program.State)));

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Load_Program(const QString &in_program_id, SProgram &out_program)
{
	QSqlQuery query(Connection());
	SProgram program;

	Clear_Last_Error();
	out_program = SProgram();

	if (!Check_Open())
		return false;

	query.prepare("SELECT id, user_id, title, source_kind, source_template_id, goal, level, schedule_kind, state, "
	              "revision, start_date, actual_start_date, weeks_total, extended_weeks, term_mode, created_at, "
	              "updated_at, sync_state, note FROM programs WHERE id = ?");
	query.addBindValue(in_program_id);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось прочитать программу: %1").arg(query.lastError().text()));

	if (!query.next())
		return Set_Last_Error(QString("Программа %1 не найдена").arg(in_program_id));

	Read_Program_Row(query, program);
	out_program = program;

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Get_Active_Program_Id(const QString &in_user_id, QString &out_program_id)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();
	out_program_id.clear();

	if (!Check_Open())
		return false;

	query.prepare("SELECT id FROM programs WHERE user_id = ? AND state = 'active' ORDER BY created_at LIMIT 1");
	query.addBindValue(in_user_id);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось найти активную программу: %1").arg(query.lastError().text()));

	if (!query.next())
		return Set_Last_Error(QString("У пользователя %1 нет активной программы").arg(in_user_id));

	out_program_id = query.value(0).toString();

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Load_Programs(const QString &in_user_id, QVector<SProgram> &out_programs)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();
	out_programs.clear();

	if (!Check_Open())
		return false;

	query.prepare("SELECT id, user_id, title, source_kind, source_template_id, goal, level, schedule_kind, state, "
	              "revision, start_date, actual_start_date, weeks_total, extended_weeks, term_mode, created_at, "
	              "updated_at, sync_state, note FROM programs WHERE user_id = ? ORDER BY updated_at DESC, id");
	query.addBindValue(in_user_id);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось прочитать список программ: %1").arg(query.lastError().text()));

	while (query.next())
	{
		SProgram program;

		Read_Program_Row(query, program);
		out_programs.push_back(program);
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Load_Goal_Targets(const QString &in_program_id, QVector<SGoal_Target> &out_targets)
{
	QSqlQuery query(Connection());
	SGoal_Target target;

	Clear_Last_Error();
	out_targets.clear();

	if (!Check_Open())
		return false;

	query.prepare("SELECT id, metric, exercise_id, measurement_key, unit, start_value, target_value, "
	              "target_weight_kg, target_reps, direction, target_date, state FROM goal_targets "
	              "WHERE program_id = ? ORDER BY id");
	query.addBindValue(in_program_id);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось прочитать цели: %1").arg(query.lastError().text()));

	while (query.next())
	{
		Read_Goal_Target_Row(query, target);
		out_targets.append(target);
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Load_Days(const QString &in_program_id, QVector<SProgram_Day> &out_days)
{
	QSqlQuery query(Connection());
	SProgram_Day day;

	Clear_Last_Error();
	out_days.clear();

	if (!Check_Open())
		return false;

	query.prepare("SELECT id, title, kind, focus, estimated_minutes, order_index, source_day_id, note "
	              "FROM program_days WHERE program_id = ? ORDER BY order_index");
	query.addBindValue(in_program_id);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось прочитать дни: %1").arg(query.lastError().text()));

	while (query.next())
	{
		day = SProgram_Day();
		Read_Day_Row(query, in_program_id, day);
		out_days.append(day);
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Load_Day_Exercises(const QString &in_program_day_id, QVector<SDay_Exercise> &out_exercises)
{
	QSqlQuery query(Connection());
	SDay_Exercise exercise;

	Clear_Last_Error();
	out_exercises.clear();

	if (!Check_Open())
		return false;

	query.prepare("SELECT id, exercise_id, order_index, sets, reps_min, reps_max, weight_mode, target_weight_kg, "
	              "percent_of_1rm, rpe_target, rest_seconds, tempo, superset_group, warmup_sets, progression_json, "
	              "substitution_ids, is_optional, note FROM program_day_exercises WHERE program_day_id = ? "
	              "ORDER BY order_index");
	query.addBindValue(in_program_day_id);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось прочитать предписания: %1").arg(query.lastError().text()));

	while (query.next())
	{
		Read_Day_Exercise_Fields(query, 0, exercise);
		exercise.Program_Day_Id = in_program_day_id;
		out_exercises.append(exercise);
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Load_Exercise_Titles(const QVector<QString> &in_exercise_ids,
                                               QHash<QString, QString> &out_titles)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();
	out_titles.clear();

	if (in_exercise_ids.isEmpty())
		return true;

	if (!Check_Open())
		return false;

	// IN-список строится из подготовленных плейсхолдеров: значения не
	// склеиваются в строку запроса.
	query.prepare(QString("SELECT id, title FROM exercises WHERE id IN (%1)")
	                  .arg(QStringList(in_exercise_ids.size(), QString("?")).join(", ")));

	for (int i = 0; i < in_exercise_ids.size(); ++i)
		query.addBindValue(in_exercise_ids.at(i));

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось прочитать названия упражнений: %1").arg(query.lastError().text()));

	while (query.next())
		out_titles.insert(query.value(0).toString(), query.value(1).toString());

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Load_Schedule(const QString &in_program_id, QVector<SWeek_Slot> &out_slots)
{
	QSqlQuery query(Connection());
	SWeek_Slot slot;

	Clear_Last_Error();
	out_slots.clear();

	if (!Check_Open())
		return false;

	query.prepare("SELECT id, day_of_week, rotation_index, program_day_id, kind, is_anchor, note "
	              "FROM program_schedule WHERE program_id = ? ORDER BY day_of_week, rotation_index");
	query.addBindValue(in_program_id);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось прочитать расписание: %1").arg(query.lastError().text()));

	while (query.next())
	{
		Read_Slot_Row(query, in_program_id, slot);
		out_slots.append(slot);
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
// Предписания всех дней программы одним запросом. Раньше здесь был цикл с
bool AProgram_Repository::Load_Day_Exercises_By_Days(const QVector<SProgram_Day> &in_days,
                                                     QVector<SDay_Exercise> &out_exercises)
{
	QSqlQuery query(Connection());
	QHash<QString, QVector<SDay_Exercise>> by_day;
	SDay_Exercise exercise;

	Clear_Last_Error();
	out_exercises.clear();

	if (in_days.isEmpty())
		return true;

	if (!Check_Open())
		return false;

	// Один запрос на всю программу вместо запроса на каждый день (N+1). Идентификаторы
	// дней подставляются плейсхолдерами: значения не склеиваются в строку запроса.
	query.prepare(QString("SELECT program_day_id, id, exercise_id, order_index, sets, reps_min, reps_max, "
	                      "weight_mode, target_weight_kg, percent_of_1rm, rpe_target, rest_seconds, tempo, "
	                      "superset_group, warmup_sets, progression_json, substitution_ids, is_optional, note "
	                      "FROM program_day_exercises WHERE program_day_id IN (%1) ORDER BY order_index")
	                  .arg(QStringList(in_days.size(), QString("?")).join(", ")));

	for (int i = 0; i < in_days.size(); ++i)
		query.addBindValue(in_days.at(i).ID);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось прочитать предписания программы: %1").arg(query.lastError().text()));

	while (query.next())
	{
		Read_Day_Exercise_Fields(query, 1, exercise);
		exercise.Program_Day_Id = query.value(0).toString();
		by_day[exercise.Program_Day_Id].append(exercise);
	}

	// Порядок дней задаётся составом программы, а не идентификатором: внутри
	// дня упражнения уже упорядочены запросом.
	for (int i = 0; i < in_days.size(); ++i)
		out_exercises.append(by_day.value(in_days.at(i).ID));

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Load_Program_Full(const QString &in_program_id, SProgram &out_program,
                                            QVector<SGoal_Target> &out_targets, QVector<SProgram_Day> &out_days,
                                            QVector<SDay_Exercise> &out_exercises, QVector<SWeek_Slot> &out_slots)
{
	Clear_Last_Error();
	out_targets.clear();
	out_days.clear();
	out_exercises.clear();
	out_slots.clear();

	if (!Load_Program(in_program_id, out_program))
		return false;

	if (!Load_Goal_Targets(in_program_id, out_targets))
		return false;

	if (!Load_Days(in_program_id, out_days))
		return false;

	if (!Load_Day_Exercises_By_Days(out_days, out_exercises))
		return false;

	return Load_Schedule(in_program_id, out_slots);
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Insert_Program(const SProgram &in_program)
{
	QSqlQuery query(Connection());
	QString created_at;
	QString updated_at;

	Clear_Last_Error();

	if (!Check_Open())
		return false;

	if (in_program.ID.isEmpty())
		return Set_Last_Error("Пустой идентификатор программы");

	// §3.1: userId и title обязательны — NOT NULL в схеме не отличает
	// пустую строку от незаполненной.
	if (in_program.User_Id.isEmpty())
		return Set_Last_Error("Программе нужен владелец (userId)");

	if (in_program.Title.isEmpty())
		return Set_Last_Error("Программе нужно название (title)");

	if (in_program.Revision < 1)
		return Set_Last_Error("Номер ревизии должен быть не меньше 1");

	created_at =
	    in_program.Created_At.isEmpty() ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate) : in_program.Created_At;
	updated_at = in_program.Updated_At.isEmpty() ? created_at : in_program.Updated_At;

	if (!Connection().transaction())
		return Set_Last_Error("Не удалось начать транзакцию");

	query.prepare("INSERT INTO programs(id, user_id, title, source_kind, source_template_id, goal, level, "
	              "schedule_kind, state, revision, start_date, actual_start_date, weeks_total, extended_weeks, "
	              "term_mode, end_date_generated, created_at, updated_at, sync_state, note) "
	              "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
	query.addBindValue(in_program.ID);
	query.addBindValue(in_program.User_Id);
	query.addBindValue(in_program.Title);
	query.addBindValue(To_String(in_program.Source_Kind));
	query.addBindValue(Text_Column(in_program.Source_Template_Id));
	query.addBindValue(To_String(in_program.Goal));
	query.addBindValue(To_String(in_program.Level));
	query.addBindValue(To_String(in_program.Schedule_Kind));
	query.addBindValue(To_String(in_program.State));
	query.addBindValue(in_program.Revision);
	query.addBindValue(Text_Column(in_program.Term.Start_Date));
	query.addBindValue(Text_Column(in_program.Actual_Start_Date));
	query.addBindValue(in_program.Term.Weeks_Total);
	query.addBindValue(in_program.Term.Extended_Weeks);
	query.addBindValue(To_String(in_program.Term.Mode));
	query.addBindValue(Text_Column(in_program.Term.Compute_End_Date()));
	query.addBindValue(created_at);
	query.addBindValue(updated_at);
	query.addBindValue(To_String(in_program.Sync_State));
	query.addBindValue(Text_Column(in_program.Note));

	if (!query.exec() || !Write_Revision_Row(in_program.ID, in_program.Revision, ERevision_Reason::State,
	                                         "Программа создана", QString()))
	{
		Connection().rollback();

		if (!Error_State.Has_Error())
			Set_Last_Error(QString("Не удалось сохранить программу: %1").arg(query.lastError().text()));

		return false;
	}

	if (!Connection().commit())
		return Set_Last_Error(QString("Не удалось зафиксировать программу: %1").arg(Connection().lastError().text()));

	return true;
}
//-------------------------------------------------------------------------------------------------------------
// Создание полной программы: строка программы, её структура и стартовая
// ревизия фиксируются атомарно, чтобы не оставлять пустые/частичные планы.
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Insert_Program_Full(const SProgram &in_program, const QVector<SGoal_Target> &in_targets,
                                              const QVector<SProgram_Day> &in_days,
                                              const QVector<SDay_Exercise> &in_exercises,
                                              const QVector<SWeek_Slot> &in_slots)
{
	QSqlQuery query(Connection());
	QString created_at;
	QString updated_at;

	Clear_Last_Error();

	if (!Check_Open())
		return false;

	if (in_program.ID.isEmpty() || in_program.User_Id.isEmpty() || in_program.Title.trimmed().isEmpty())
		return Set_Last_Error("Для программы нужны id, userId и title");

	if (in_program.State != EProgram_State::Draft || in_program.Revision != 1)
		return Set_Last_Error("Новая программа должна начинаться в состоянии draft с ревизии 1");

	if (!Validate_Targets(in_program, in_targets) ||
	    !Validate_Structure(in_program.Schedule_Kind, in_days, in_exercises, in_slots))
		return false;

	created_at =
	    in_program.Created_At.isEmpty() ? QDateTime::currentDateTimeUtc().toString(Qt::ISODate) : in_program.Created_At;
	updated_at = in_program.Updated_At.isEmpty() ? created_at : in_program.Updated_At;

	if (!Connection().transaction())
		return Set_Last_Error("Не удалось начать транзакцию создания программы");

	query.prepare("INSERT INTO programs(id, user_id, title, source_kind, source_template_id, goal, level, "
	              "schedule_kind, state, revision, start_date, actual_start_date, weeks_total, extended_weeks, "
	              "term_mode, end_date_generated, created_at, updated_at, sync_state, note) "
	              "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");
	query.addBindValue(in_program.ID);
	query.addBindValue(in_program.User_Id);
	query.addBindValue(in_program.Title.trimmed());
	query.addBindValue(To_String(in_program.Source_Kind));
	query.addBindValue(Text_Column(in_program.Source_Template_Id));
	query.addBindValue(To_String(in_program.Goal));
	query.addBindValue(To_String(in_program.Level));
	query.addBindValue(To_String(in_program.Schedule_Kind));
	query.addBindValue(To_String(EProgram_State::Draft));
	query.addBindValue(1);
	query.addBindValue(Text_Column(in_program.Term.Start_Date));
	// Фактический старт пишется, только если он задан явно: у черновика его
	// быть не должно, а ретроспективный посев (демо-данные, перенос с другого
	// устройства) обязан сохранить собственную дату. Change_State заполняет
	// поле лишь когда оно пусто, поэтому переданное значение переживает
	// активацию, а незаданное остаётся однозначным «не фиксировано».
	query.addBindValue(Text_Column(in_program.Actual_Start_Date));
	query.addBindValue(in_program.Term.Weeks_Total);
	query.addBindValue(in_program.Term.Extended_Weeks);
	query.addBindValue(To_String(in_program.Term.Mode));
	query.addBindValue(Text_Column(in_program.Term.Compute_End_Date()));
	query.addBindValue(created_at);
	query.addBindValue(updated_at);
	query.addBindValue(To_String(ESync_State::Local));
	query.addBindValue(Text_Column(in_program.Note));

	if (!query.exec() || !Insert_Targets_Internal(in_program.ID, in_targets) ||
	    !Insert_Days_Internal(in_program.ID, in_days) || !Insert_Day_Exercises_Internal(in_exercises) ||
	    !Insert_Slots_Internal(in_program.ID, in_slots) ||
	    Write_Revision_Row(in_program.ID, 1, ERevision_Reason::State, "Программа создана со структурой", QString()) ==
	        false)
	{
		Connection().rollback();

		if (!Error_State.Has_Error())
			Set_Last_Error(QString("Не удалось создать программу: %1").arg(query.lastError().text()));

		return false;
	}

	if (!Connection().commit())
		return Set_Last_Error(
		    QString("Не удалось зафиксировать создание программы: %1").arg(Connection().lastError().text()));

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Insert_Targets_Internal(const QString &in_program_id, const QVector<SGoal_Target> &in_targets)
{
	QSqlQuery query(Connection());

	if (in_targets.isEmpty())
		return true;

	query.prepare("INSERT INTO goal_targets(id, program_id, metric, exercise_id, measurement_key, unit, "
	              "start_value, target_value, target_weight_kg, target_reps, direction, target_date, state) "
	              "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

	for (int i = 0; i < in_targets.size(); ++i)
	{
		const SGoal_Target &target = in_targets.at(i);

		query.addBindValue(target.ID);
		query.addBindValue(in_program_id);
		query.addBindValue(To_String(target.Metric));
		query.addBindValue(Text_Column(target.Exercise_Id));
		query.addBindValue(target.Has_Measurement_Key ? QVariant(To_String(target.Measurement_Key)) : QVariant());
		query.addBindValue(To_String(target.Unit));
		query.addBindValue(Number_Column(target.Start_Value, target.Has_Start_Value));
		query.addBindValue(target.Target_Value);
		query.addBindValue(Number_Column(target.Target_Weight_Kg, target.Has_Target_Weight_Kg));
		query.addBindValue(Int_Column(target.Target_Reps, target.Has_Target_Reps));
		query.addBindValue(To_String(target.Direction));
		query.addBindValue(Text_Column(target.Target_Date));
		query.addBindValue(To_String(target.State));

		if (!query.exec())
			return Set_Last_Error(
			    QString("Не удалось сохранить цель %1: %2").arg(target.ID).arg(query.lastError().text()));
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Insert_Days_Internal(const QString &in_program_id, const QVector<SProgram_Day> &in_days)
{
	QSqlQuery query(Connection());

	if (in_days.isEmpty())
		return true;

	query.prepare("INSERT INTO program_days(id, program_id, title, kind, focus, estimated_minutes, order_index, "
	              "source_day_id, note) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?)");

	for (int i = 0; i < in_days.size(); ++i)
	{
		const SProgram_Day &day = in_days.at(i);

		query.addBindValue(day.ID);
		query.addBindValue(in_program_id);
		query.addBindValue(day.Title);
		query.addBindValue(To_String(day.Kind));
		query.addBindValue(day.Focus.isEmpty() ? QVariant() : QVariant(To_Json_Array(day.Focus)));
		query.addBindValue(day.Estimated_Minutes);
		query.addBindValue(day.Order_Index);
		query.addBindValue(Text_Column(day.Source_Day_Id));
		query.addBindValue(Text_Column(day.Note));

		if (!query.exec())
			return Set_Last_Error(
			    QString("Не удалось сохранить день %1: %2").arg(day.ID).arg(query.lastError().text()));
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Insert_Day_Exercises_Internal(const QVector<SDay_Exercise> &in_exercises)
{
	QSqlQuery query(Connection());

	if (in_exercises.isEmpty())
		return true;

	query.prepare("INSERT INTO program_day_exercises(id, program_day_id, exercise_id, order_index, sets, reps_min, "
	              "reps_max, weight_mode, target_weight_kg, percent_of_1rm, rpe_target, rest_seconds, tempo, "
	              "superset_group, warmup_sets, progression_json, substitution_ids, is_optional, note) "
	              "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

	for (int i = 0; i < in_exercises.size(); ++i)
	{
		const SDay_Exercise &exercise = in_exercises.at(i);

		query.addBindValue(exercise.ID);
		query.addBindValue(exercise.Program_Day_Id);
		query.addBindValue(exercise.Exercise_Id);
		query.addBindValue(exercise.Order_Index);
		query.addBindValue(exercise.Sets);
		query.addBindValue(exercise.Reps_Min);
		query.addBindValue(exercise.Reps_Max);
		query.addBindValue(To_String(exercise.Weight_Mode));
		query.addBindValue(Number_Column(exercise.Target_Weight_Kg, exercise.Has_Target_Weight_Kg));
		query.addBindValue(Number_Column(exercise.Percent_Of_1rm, exercise.Has_Percent_Of_1rm));
		query.addBindValue(Number_Column(exercise.Rpe_Target, exercise.Has_Rpe_Target));
		query.addBindValue(exercise.Rest_Seconds);
		query.addBindValue(Text_Column(exercise.Tempo));
		query.addBindValue(Text_Column(exercise.Superset_Group));
		query.addBindValue(exercise.Warmup_Sets);
		query.addBindValue(exercise.Has_Progression ? QVariant(To_Progression_Json(exercise.Progression)) : QVariant());
		query.addBindValue(exercise.Substitution_Ids.isEmpty() ? QVariant()
		                                                       : QVariant(To_Json_Array(exercise.Substitution_Ids)));
		query.addBindValue(exercise.Is_Optional ? 1 : 0);
		query.addBindValue(Text_Column(exercise.Note));

		if (!query.exec())
			return Set_Last_Error(
			    QString("Не удалось сохранить предписание %1: %2").arg(exercise.ID).arg(query.lastError().text()));
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Insert_Slots_Internal(const QString &in_program_id, const QVector<SWeek_Slot> &in_slots)
{
	QSqlQuery query(Connection());

	if (in_slots.isEmpty())
		return true;

	query.prepare("INSERT INTO program_schedule(id, program_id, day_of_week, rotation_index, program_day_id, kind, "
	              "is_anchor, note) VALUES(?, ?, ?, ?, ?, ?, ?, ?)");

	for (int i = 0; i < in_slots.size(); ++i)
	{
		const SWeek_Slot &slot = in_slots.at(i);

		query.addBindValue(slot.ID);
		query.addBindValue(in_program_id);
		query.addBindValue(slot.Day_Of_Week != 0 ? QVariant(slot.Day_Of_Week) : QVariant());
		query.addBindValue(slot.Rotation_Index != 0 ? QVariant(slot.Rotation_Index) : QVariant());
		query.addBindValue(Text_Column(slot.Program_Day_Id));
		query.addBindValue(To_String(slot.Kind));
		query.addBindValue(slot.Is_Anchor ? 1 : 0);
		query.addBindValue(Text_Column(slot.Note));

		if (!query.exec())
			return Set_Last_Error(
			    QString("Не удалось сохранить слот %1: %2").arg(slot.ID).arg(query.lastError().text()));
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Validate_Structure(ESchedule_Kind in_kind, const QVector<SProgram_Day> &in_days,
                                             const QVector<SDay_Exercise> &in_exercises,
                                             const QVector<SWeek_Slot> &in_slots)
{
	QSet<QString> day_ids;

	if (!Validate_Days(in_days, day_ids))
		return false;

	if (!Validate_Day_Exercises(in_exercises, day_ids))
		return false;

	return Validate_Slots(in_kind, in_slots, day_ids);
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Validate_Days(const QVector<SProgram_Day> &in_days, QSet<QString> &out_day_ids)
{
	out_day_ids.clear();

	for (int i = 0; i < in_days.size(); ++i)
	{
		if (in_days.at(i).ID.isEmpty())
			return Set_Last_Error("Пустой идентификатор дня");

		if (out_day_ids.contains(in_days.at(i).ID))
			return Set_Last_Error(QString("Дубликат дня %1").arg(in_days.at(i).ID));

		out_day_ids.insert(in_days.at(i).ID);
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Validate_Day_Exercises(const QVector<SDay_Exercise> &in_exercises,
                                                 const QSet<QString> &in_day_ids)
{
	for (int i = 0; i < in_exercises.size(); ++i)
	{
		const SDay_Exercise &exercise = in_exercises.at(i);

		if (exercise.Exercise_Id.isEmpty())
			return Set_Last_Error(QString("Предписание %1 без упражнения").arg(exercise.ID));

		if (!in_day_ids.contains(exercise.Program_Day_Id))
			return Set_Last_Error(QString("Предписание %1 ссылается на неизвестный день %2")
			                          .arg(exercise.ID)
			                          .arg(exercise.Program_Day_Id));

		if (exercise.Sets < 1 || exercise.Sets > 20)
			return Set_Last_Error(QString("Предписание %1: число подходов вне 1-20").arg(exercise.ID));

		if (exercise.Reps_Max < exercise.Reps_Min)
			return Set_Last_Error(QString("Предписание %1: диапазон повторов некорректен").arg(exercise.ID));
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
static QString Slot_Position_Error(ESchedule_Kind in_kind, const SWeek_Slot &in_slot)
{
	if (in_kind == ESchedule_Kind::Weekday)
		return QString("Слот %1: ISO-день недели вне 1-7").arg(in_slot.ID);

	return QString("Слот %1: позиция ротации должна быть не меньше 1").arg(in_slot.ID);
}
//-------------------------------------------------------------------------------------------------------------
// Слоты расписания: отдых не ссылается на день, тренировка ссылается на день
// из нового состава (§3.4), позиция уникальна внутри расписания.
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Validate_Slots(ESchedule_Kind in_kind, const QVector<SWeek_Slot> &in_slots,
                                         const QSet<QString> &in_day_ids)
{
	QSet<int> positions;

	for (int i = 0; i < in_slots.size(); ++i)
	{
		const SWeek_Slot &slot = in_slots.at(i);
		int position = 0;

		if (slot.ID.isEmpty())
			return Set_Last_Error("Пустой идентификатор слота расписания");

		if (!Validate_Slot_Day(slot, in_day_ids))
			return false;

		position = Slot_Position(in_kind, slot);

		if (position < 1)
			return Set_Last_Error(Slot_Position_Error(in_kind, slot));

		if (positions.contains(position))
			return Set_Last_Error(QString("Слот %1: дубликат позиции %2 в расписании").arg(slot.ID).arg(position));

		positions.insert(position);
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Validate_Slot_Day(const SWeek_Slot &in_slot, const QSet<QString> &in_day_ids)
{
	if (in_slot.Kind == ESlot_Kind::Rest)
	{
		if (!in_slot.Program_Day_Id.isEmpty())
			return Set_Last_Error(QString("Слот отдыха %1 не может ссылаться на день").arg(in_slot.ID));

		return true;
	}

	if (in_slot.Program_Day_Id.isEmpty())
		return Set_Last_Error(QString("Слот %1 без ссылки на день плана").arg(in_slot.ID));

	if (!in_day_ids.contains(in_slot.Program_Day_Id))
		return Set_Last_Error(
		    QString("Слот %1 ссылается на неизвестный день %2").arg(in_slot.ID).arg(in_slot.Program_Day_Id));

	return true;
}
//-------------------------------------------------------------------------------------------------------------
// Позиция слота: для расписания по дням недели — ISO-день, для ротации —
// индекс витка. Ноль означает «позиция некорректна», текст ошибки строит
// Slot_Position_Error.
//-------------------------------------------------------------------------------------------------------------
int AProgram_Repository::Slot_Position(ESchedule_Kind in_kind, const SWeek_Slot &in_slot) const
{
	if (in_kind == ESchedule_Kind::Weekday)
	{
		if (in_slot.Day_Of_Week < 1 || in_slot.Day_Of_Week > 7)
			return 0;

		return in_slot.Day_Of_Week;
	}

	return in_slot.Rotation_Index < 1 ? 0 : in_slot.Rotation_Index;
}
// Валидация целей (§4): инвариант 8 — активная программа с измеримой целью
// (fatLoss/strength) не может остаться без целей; инвариант 9 — направление
// согласовано со стартовым и целевым значением (проверяется только при
// заданном startValue).
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Validate_Targets(const SProgram &in_program, const QVector<SGoal_Target> &in_targets)
{
	QSet<QString> target_ids;

	if (in_targets.isEmpty())
	{
		if (in_program.State == EProgram_State::Active &&
		    (in_program.Goal == EProgram_Goal::FatLoss || in_program.Goal == EProgram_Goal::Strength))
			return Set_Last_Error("Активной программе с измеримой целью нужны хотя бы одни цели (инвариант 8 §4)");

		return true;
	}

	for (int i = 0; i < in_targets.size(); ++i)
	{
		const SGoal_Target &target = in_targets.at(i);

		if (target.ID.isEmpty())
			return Set_Last_Error("Пустой идентификатор цели");

		if (target_ids.contains(target.ID))
			return Set_Last_Error(QString("Дубликат цели %1").arg(target.ID));

		target_ids.insert(target.ID);

		if (!Check_Goal_Direction(target))
			return false;
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
// Инвариант 9 §4: направление согласовано со стартовым и целевым
// значением. Проверяется только при заданном стартовом значении: без него
// нечего сравнивать.
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Check_Goal_Direction(const SGoal_Target &in_target)
{
	double tolerance = 1e-6;

	if (!in_target.Has_Start_Value)
		return true;

	if (in_target.Direction == EGoal_Direction::Decrease && in_target.Target_Value >= in_target.Start_Value)
		return Set_Last_Error(
		    QString("Цель %1: при direction=decrease цель должна быть меньше стартового значения").arg(in_target.ID));

	if (in_target.Direction == EGoal_Direction::Increase && in_target.Target_Value <= in_target.Start_Value)
		return Set_Last_Error(
		    QString("Цель %1: при direction=increase цель должна быть больше стартового значения").arg(in_target.ID));

	if (in_target.Direction != EGoal_Direction::Maintain)
		return true;

	if (in_target.Target_Value - in_target.Start_Value <= tolerance &&
	    in_target.Start_Value - in_target.Target_Value <= tolerance)
		return true;

	return Set_Last_Error(
	    QString("Цель %1: при direction=maintain цель должна равняться стартовому значению").arg(in_target.ID));
}
//-------------------------------------------------------------------------------------------------------------
QString AProgram_Repository::Build_Goal_Targets_Snapshot(const QString &in_program_id)
{
	QVector<SGoal_Target> targets;
	QJsonArray array;

	if (!Load_Goal_Targets(in_program_id, targets))
		return QString();

	for (int i = 0; i < targets.size(); ++i)
		array.append(Goal_Target_Snapshot_Object(targets.at(i)));

	return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}
//-------------------------------------------------------------------------------------------------------------
QString AProgram_Repository::Build_Structure_Snapshot(const QString &in_program_id)
{
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> exercises;
	QVector<SWeek_Slot> week_slots;
	QJsonObject object;
	QJsonArray day_array;
	QJsonArray exercise_array;
	QJsonArray slot_array;

	if (!Load_Days(in_program_id, days) || !Load_Schedule(in_program_id, week_slots))
		return QString();

	for (int i = 0; i < days.size(); ++i)
	{
		QVector<SDay_Exercise> day_exercises;

		if (!Load_Day_Exercises(days.at(i).ID, day_exercises))
			return QString();

		day_array.append(Day_Snapshot_Object(days.at(i)));

		for (int j = 0; j < day_exercises.size(); ++j)
		{
			exercises.append(day_exercises.at(j));
			exercise_array.append(Day_Exercise_Snapshot_Object(day_exercises.at(j)));
		}
	}

	for (int i = 0; i < week_slots.size(); ++i)
		slot_array.append(Slot_Snapshot_Object(week_slots.at(i)));

	object.insert("days", day_array);
	object.insert("exercises", exercise_array);
	object.insert("week_slots", slot_array);

	return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
}
//-------------------------------------------------------------------------------------------------------------
// Снимок только расписания (§3.9: «снимок изменённой части») — для
// Replace_Schedule, который дни и предписания не трогает.
//-------------------------------------------------------------------------------------------------------------
QString AProgram_Repository::Build_Schedule_Snapshot(const QString &in_program_id)
{
	QVector<SWeek_Slot> week_slots;
	QJsonArray slot_array;

	if (!Load_Schedule(in_program_id, week_slots))
		return QString();

	for (int i = 0; i < week_slots.size(); ++i)
		slot_array.append(Slot_Snapshot_Object(week_slots.at(i)));

	return QString::fromUtf8(QJsonDocument(slot_array).toJson(QJsonDocument::Compact));
}
//-------------------------------------------------------------------------------------------------------------
// Снимок старого состояния обязателен для каждой правки с журналом: пустой
// снимок (ошибка чтения) означал бы запись ревизии без истории §3.9 —
// такую правку корректнее откатить. Пустая строка при успешном чтении
// невозможна: сериализаторы всегда возвращают «[]» или объект.
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Require_Snapshot(const QString &in_snapshot, const QString &in_section)
{
	if (!in_snapshot.isEmpty())
		return true;

	if (!Error_State.Has_Error())
		return Set_Last_Error(
		    QString("Не удалось прочитать прежнее состояние раздела %1 для журнала ревизий").arg(in_section));

	return false;
}
//-------------------------------------------------------------------------------------------------------------
// Общий каркас транзакционной правки: снимок старого раздела →
// замена → инкремент ревизии → строка журнала → commit. Отказ на
// любом шаге = rollback.
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Replace_Goal_Targets(const QString &in_program_id, const QVector<SGoal_Target> &in_targets)
{
	SProgram program;
	QSqlQuery query(Connection());
	QString snapshot;

	Clear_Last_Error();

	if (!Check_Open())
		return false;

	if (!Check_Editable(in_program_id, program))
		return false;

	if (!Validate_Targets(program, in_targets))
		return false;

	if (!Connection().transaction())
		return Set_Last_Error("Не удалось начать транзакцию");

	snapshot = Build_Goal_Targets_Snapshot(in_program_id);

	if (!Require_Snapshot(snapshot, "целей"))
	{
		Connection().rollback();

		return false;
	}

	query.prepare("DELETE FROM goal_targets WHERE program_id = ?");
	query.addBindValue(in_program_id);

	if (!query.exec() || !Insert_Targets_Internal(in_program_id, in_targets) ||
	    !Bump_Revision_Internal(in_program_id, program.Revision) ||
	    !Write_Revision_Row(in_program_id, program.Revision + 1, ERevision_Reason::Goal, "Правка целей программы",
	                        snapshot))
	{
		Connection().rollback();

		if (!Error_State.Has_Error())
			Set_Last_Error(QString("Не удалось заменить цели: %1").arg(query.lastError().text()));

		return false;
	}

	if (!Connection().commit())
		return Set_Last_Error("Не удалось зафиксировать правку целей");

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Change_Profile(const QString &in_program_id, const QString &in_title, EProgram_Goal in_goal,
                                         EProgram_Level in_level)
{
	SProgram program;
	QSqlQuery query(Connection());
	QJsonObject snapshot;

	Clear_Last_Error();

	if (!Check_Open())
		return false;

	if (!Check_Editable(in_program_id, program))
		return false;

	// Пустое название оставлять нельзя: каталог показывал бы строку без
	// подписи, и найти программу было бы невозможно.
	if (in_title.trimmed().isEmpty())
		return Set_Last_Error("Название программы не может быть пустым");

	if (!Connection().transaction())
		return Set_Last_Error("Не удалось начать транзакцию");

	// Снимок паспортных полей: журнал ревизий объясняет, что менялось, даже
	// если правка потом откатывается глазами по экрану.
	snapshot.insert("title", program.Title);
	snapshot.insert("goal", To_String(program.Goal));
	snapshot.insert("level", To_String(program.Level));

	query.prepare("UPDATE programs SET title = ?, goal = ?, level = ?, revision = revision + 1, updated_at = ? "
	              "WHERE id = ? AND revision = ?");
	query.addBindValue(in_title.trimmed());
	query.addBindValue(To_String(in_goal));
	query.addBindValue(To_String(in_level));
	query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
	query.addBindValue(in_program_id);
	query.addBindValue(program.Revision);

	if (!query.exec())
	{
		Connection().rollback();

		return Set_Last_Error(QString("Не удалось изменить программу: %1").arg(query.lastError().text()));
	}

	if (query.numRowsAffected() != 1)
	{
		Connection().rollback();

		return Set_Last_Error("Ревизия программы изменилась параллельно, правка отклонена");
	}

	if (!Write_Revision_Row(in_program_id, program.Revision + 1, ERevision_Reason::Goal, "Изменение описания",
	                        QString::fromUtf8(QJsonDocument(snapshot).toJson(QJsonDocument::Compact))))
	{
		Connection().rollback();

		return false;
	}

	if (!Connection().commit())
		return Set_Last_Error("Не удалось сохранить изменения программы");

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Change_Term(const QString &in_program_id, const STerm &in_term)
{
	SProgram program;
	QSqlQuery query(Connection());
	QString snapshot;

	Clear_Last_Error();

	if (!Check_Open())
		return false;

	if (!Check_Editable(in_program_id, program))
		return false;

	if (!Connection().transaction())
		return Set_Last_Error("Не удалось начать транзакцию");

	snapshot = Term_Snapshot_Json(program.Term);

	// Оптимистичная блокировка как в Bump_Revision_Internal: правка
	// применяется только к прочитанной ревизии, параллельная — отклоняется.
	query.prepare("UPDATE programs SET start_date = ?, weeks_total = ?, extended_weeks = ?, term_mode = ?, "
	              "end_date_generated = ?, revision = revision + 1, updated_at = ? WHERE id = ? AND revision = ?");
	query.addBindValue(Text_Column(in_term.Start_Date));
	query.addBindValue(in_term.Weeks_Total);
	query.addBindValue(in_term.Extended_Weeks);
	query.addBindValue(To_String(in_term.Mode));
	query.addBindValue(Text_Column(in_term.Compute_End_Date()));
	query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
	query.addBindValue(in_program_id);
	query.addBindValue(program.Revision);

	if (!query.exec())
	{
		Connection().rollback();

		return Set_Last_Error(QString("Не удалось изменить срок: %1").arg(query.lastError().text()));
	}

	if (query.numRowsAffected() != 1)
	{
		Connection().rollback();

		return Set_Last_Error("Ревизия программы изменилась параллельно, правка срока отклонена");
	}

	if (Write_Revision_Row(in_program_id, program.Revision + 1, ERevision_Reason::Term, "Изменение срока", snapshot) ==
	    false)
	{
		Connection().rollback();

		return false;
	}

	if (!Connection().commit())
		return Set_Last_Error("Не удалось зафиксировать изменение срока");

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Replace_Schedule(const QString &in_program_id, const QVector<SWeek_Slot> &in_slots)
{
	SProgram program;
	QVector<SProgram_Day> days;
	QSqlQuery query(Connection());
	QString snapshot;

	Clear_Last_Error();

	if (!Check_Open())
		return false;

	if (!Check_Editable(in_program_id, program))
		return false;

	if (!Load_Days(in_program_id, days))
		return false;

	if (!Validate_Structure(program.Schedule_Kind, days, QVector<SDay_Exercise>(), in_slots))
		return false;

	if (!Connection().transaction())
		return Set_Last_Error("Не удалось начать транзакцию");

	// Снимок только изменяемого раздела (§3.9): правка расписания не
	// затрагивает дни и предписания.
	snapshot = Build_Schedule_Snapshot(in_program_id);

	if (!Require_Snapshot(snapshot, "расписания"))
	{
		Connection().rollback();

		return false;
	}

	query.prepare("DELETE FROM program_schedule WHERE program_id = ?");
	query.addBindValue(in_program_id);

	if (!query.exec() || !Insert_Slots_Internal(in_program_id, in_slots) ||
	    !Bump_Revision_Internal(in_program_id, program.Revision) ||
	    !Write_Revision_Row(in_program_id, program.Revision + 1, ERevision_Reason::Schedule, "Правка расписания",
	                        snapshot))
	{
		Connection().rollback();

		if (!Error_State.Has_Error())
			Set_Last_Error(QString("Не удалось заменить расписание: %1").arg(query.lastError().text()));

		return false;
	}

	if (!Connection().commit())
		return Set_Last_Error("Не удалось зафиксировать расписание");

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Replace_Structure(const QString &in_program_id, const QVector<SProgram_Day> &in_days,
                                            const QVector<SDay_Exercise> &in_exercises,
                                            const QVector<SWeek_Slot> &in_slots, ERevision_Reason in_reason,
                                            const QString &in_summary)
{
	SProgram program;
	QSqlQuery query(Connection());
	QString snapshot;
	QString summary;

	Clear_Last_Error();

	if (!Check_Open())
		return false;

	if (!Check_Editable(in_program_id, program))
		return false;

	if (!Validate_Structure(program.Schedule_Kind, in_days, in_exercises, in_slots))
		return false;

	if (!Connection().transaction())
		return Set_Last_Error("Не удалось начать транзакцию");

	// Снимок старой структуры — до удаления дней: после него ссылки
	// фактов обнуляются, и план прошлых недель живёт только здесь
	// (§3.9).
	snapshot = Build_Structure_Snapshot(in_program_id);
	summary = in_summary.isEmpty() ? "Правка структуры программы" : in_summary;

	if (!Require_Snapshot(snapshot, "структуры"))
	{
		Connection().rollback();

		return false;
	}

	query.prepare("DELETE FROM program_schedule WHERE program_id = ?");
	query.addBindValue(in_program_id);

	if (!query.exec())
	{
		Connection().rollback();

		return Set_Last_Error(QString("Не удалось очистить расписание: %1").arg(query.lastError().text()));
	}

	query.prepare("DELETE FROM program_days WHERE program_id = ?");
	query.addBindValue(in_program_id);

	if (!query.exec() || !Insert_Days_Internal(in_program_id, in_days) ||
	    !Insert_Day_Exercises_Internal(in_exercises) || !Insert_Slots_Internal(in_program_id, in_slots) ||
	    !Bump_Revision_Internal(in_program_id, program.Revision) ||
	    !Write_Revision_Row(in_program_id, program.Revision + 1, in_reason, summary, snapshot))
	{
		Connection().rollback();

		if (!Error_State.Has_Error())
			Set_Last_Error(QString("Не удалось заменить структуру: %1").arg(query.lastError().text()));

		return false;
	}

	if (!Connection().commit())
		return Set_Last_Error("Не удалось зафиксировать структуру");

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository::Change_State(const QString &in_program_id, EProgram_State in_new_state)
{
	SProgram program;
	QSqlQuery query(Connection());
	QString actual_start_date;
	bool allowed = false;

	Clear_Last_Error();

	if (!Check_Open())
		return false;

	if (!Load_Program(in_program_id, program))
		return false;

	// Разрешённые переходы — §3.1.
	allowed = (program.State == EProgram_State::Draft && in_new_state == EProgram_State::Active) ||
	          (program.State == EProgram_State::Active &&
	           (in_new_state == EProgram_State::Paused || in_new_state == EProgram_State::Completed ||
	            in_new_state == EProgram_State::Cancelled)) ||
	          (program.State == EProgram_State::Paused && in_new_state == EProgram_State::Active) ||
	          ((program.State == EProgram_State::Completed || program.State == EProgram_State::Cancelled) &&
	           in_new_state == EProgram_State::Archived);

	if (!allowed)
		return Set_Last_Error(QString("Переход состояния %1 → %2 не разрешён (§3.1)")
		                          .arg(To_String(program.State))
		                          .arg(To_String(in_new_state)));

	if (!Connection().transaction())
		return Set_Last_Error("Не удалось начать транзакцию");

	// Активация фиксирует фактическую дату старта, если она ещё не
	// проставлена.
	actual_start_date = program.Actual_Start_Date;

	if (in_new_state == EProgram_State::Active && actual_start_date.isEmpty())
		actual_start_date = QDate::currentDate().toString("yyyy-MM-dd");

	// Оптимистичная блокировка (как в Bump_Revision_Internal): переход
	// применяется только к прочитанной ревизии.
	query.prepare("UPDATE programs SET state = ?, revision = revision + 1, actual_start_date = ?, updated_at = ? "
	              "WHERE id = ? AND revision = ?");
	query.addBindValue(To_String(in_new_state));
	query.addBindValue(Text_Column(actual_start_date));
	query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
	query.addBindValue(in_program_id);
	query.addBindValue(program.Revision);

	if (!query.exec())
	{
		Connection().rollback();

		return Set_Last_Error(QString("Не удалось изменить состояние: %1").arg(query.lastError().text()));
	}

	if (query.numRowsAffected() != 1)
	{
		Connection().rollback();

		return Set_Last_Error("Ревизия программы изменилась параллельно, смена состояния отклонена");
	}

	if (!Write_Revision_Row(in_program_id, program.Revision + 1, ERevision_Reason::State,
	                        QString("Состояние: %1 → %2").arg(To_String(program.State)).arg(To_String(in_new_state)),
	                        QString()))
	{
		Connection().rollback();

		return false;
	}

	if (!Connection().commit())
		return Set_Last_Error("Не удалось зафиксировать состояние");

	return true;
}
//-------------------------------------------------------------------------------------------------------------
// Включение программы при инварианте «одна активная на пользователя» (§2.3).
// Прежняя активная программа переводится в паузу — без этого вторая активация
// упирается в частичный уникальный индекс programs_one_active_per_user, и
// пользователь вместо результата видел ошибку базы («Не удалось изменить
// состояние: UNIQUE constraint failed: programs.user_id»).
//
// Оба перехода идут одной транзакцией: отказ на середине оставил бы
// пользователя без активной программы, а восстановление прежней отдельным
// вызовом разошлось бы с ней при следующем сбое.
//
// Повторная активация уже активной программы — не ошибка и ничего не меняет:
// экран может сохранять программу второй раз подряд.
bool AProgram_Repository::Activate_Exclusive(const QString &in_program_id, QString &out_paused_program_id)
{
	SProgram program;
	SProgram previous;
	QSqlQuery query(Connection());
	QString active_program_id;
	QString actual_start_date;

	Clear_Last_Error();
	out_paused_program_id.clear();

	if (!Check_Open())
		return false;

	if (!Load_Program(in_program_id, program))
		return false;

	if (program.State == EProgram_State::Active)
		return true;

	// Разрешённые переходы — §3.1.
	if (program.State != EProgram_State::Draft && program.State != EProgram_State::Paused)
	{
		return Set_Last_Error(QString("Переход состояния %1 → %2 не разрешён (§3.1)")
		                          .arg(To_String(program.State))
		                          .arg(To_String(EProgram_State::Active)));
	}

	// Активная программа читается запросом, а не Get_Active_Program_Id: тот
	// считает отсутствие активной программы ошибкой, а для включения это
	// нормальный случай — включать нечего снимать.
	query.prepare("SELECT id FROM programs WHERE user_id = ? AND state = 'active' ORDER BY created_at LIMIT 1");
	query.addBindValue(program.User_Id);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось найти активную программу: %1").arg(query.lastError().text()));

	active_program_id = query.next() ? query.value(0).toString() : QString();

	if (active_program_id == in_program_id)
		return true;

	if (!active_program_id.isEmpty() && !Load_Program(active_program_id, previous))
		return false;

	if (!Connection().transaction())
		return Set_Last_Error("Не удалось начать транзакцию");

	if (!active_program_id.isEmpty())
	{
		// Оптимистичная блокировка, как в Change_State: пауза применяется
		// только к прочитанной ревизии прежней программы.
		query.prepare(
		    "UPDATE programs SET state = ?, revision = revision + 1, updated_at = ? WHERE id = ? AND revision = ?");
		query.addBindValue(To_String(EProgram_State::Paused));
		query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
		query.addBindValue(active_program_id);
		query.addBindValue(previous.Revision);

		if (!query.exec())
		{
			Connection().rollback();

			return Set_Last_Error(
			    QString("Не удалось приостановить прежнюю программу: %1").arg(query.lastError().text()));
		}

		if (query.numRowsAffected() != 1)
		{
			Connection().rollback();

			return Set_Last_Error("Ревизия прежней программы изменилась параллельно, включение отклонено");
		}

		if (!Write_Revision_Row(active_program_id, previous.Revision + 1, ERevision_Reason::State,
		                        QString("Пауза: включена программа «%1»").arg(program.Title), QString()))
		{
			Connection().rollback();

			return false;
		}
	}

	// Активация фиксирует фактическую дату старта, если она ещё не
	// проставлена (как в Change_State).
	actual_start_date = program.Actual_Start_Date;

	if (actual_start_date.isEmpty())
		actual_start_date = QDate::currentDate().toString("yyyy-MM-dd");

	query.prepare("UPDATE programs SET state = ?, revision = revision + 1, actual_start_date = ?, updated_at = ? "
	              "WHERE id = ? AND revision = ?");
	query.addBindValue(To_String(EProgram_State::Active));
	query.addBindValue(Text_Column(actual_start_date));
	query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
	query.addBindValue(in_program_id);
	query.addBindValue(program.Revision);

	if (!query.exec())
	{
		Connection().rollback();

		return Set_Last_Error(QString("Не удалось изменить состояние: %1").arg(query.lastError().text()));
	}

	if (query.numRowsAffected() != 1)
	{
		Connection().rollback();

		return Set_Last_Error("Ревизия программы изменилась параллельно, смена состояния отклонена");
	}

	if (!Write_Revision_Row(
	        in_program_id, program.Revision + 1, ERevision_Reason::State,
	        QString("Состояние: %1 → %2").arg(To_String(program.State)).arg(To_String(EProgram_State::Active)),
	        QString()))
	{
		Connection().rollback();

		return false;
	}

	if (!Connection().commit())
		return Set_Last_Error("Не удалось зафиксировать состояние");

	out_paused_program_id = active_program_id;

	return true;
}
//-------------------------------------------------------------------------------------------------------------
// Удаление черновика (§3.1). Брошенное создание не должно копиться в каталоге
// вечно: «Удалить черновик» в редакторе стирает его вместе с днями,
// упражнениями, целями, расписанием, правилами и журналом ревизий — все
// дочерние таблицы снимает каскад FK, и одного DELETE достаточно.
//
// Состояние проверяется явно, а не условием в WHERE: отказ для не-черновика —
// ошибка вызывающего, и без проверки он выглядел бы как тихое «0 строк».
// Активную программу пользователь из редактора не удаляет: её состояния
// меняются только переходами §3.1 (Change_State/Activate_Exclusive).
bool AProgram_Repository::Delete_Draft(const QString &in_program_id)
{
	SProgram program;
	QSqlQuery query(Connection());

	Clear_Last_Error();

	if (!Check_Open())
		return false;

	if (!Load_Program(in_program_id, program))
		return false;

	if (program.State != EProgram_State::Draft)
		return Set_Last_Error(
		    QString("Удалить можно только черновик, состояние программы — %1 (§3.1)").arg(To_String(program.State)));

	if (!Connection().transaction())
		return Set_Last_Error("Не удалось начать транзакцию");

	query.prepare("DELETE FROM programs WHERE id = ?");
	query.addBindValue(in_program_id);

	if (!query.exec())
	{
		Connection().rollback();

		return Set_Last_Error(QString("Не удалось удалить черновик: %1").arg(query.lastError().text()));
	}

	if (!Connection().commit())
		return Set_Last_Error("Не удалось зафиксировать удаление черновика");

	return true;
}
//-------------------------------------------------------------------------------------------------------------
