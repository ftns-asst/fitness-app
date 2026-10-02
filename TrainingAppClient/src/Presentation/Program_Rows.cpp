#include "Presentation/Program_Rows.h"

#include <QDate>

namespace
{
// «Нет данных» для роли: QML читает это как null, а 0 означал бы
// «значение равно нулю» — разные состояния на экране (§5.5).
QVariant Null_Variant()
{
	return QVariant();
}
} // namespace

namespace Program_Rows
{
//----------------------------------------------------------------------------
// Роли списков (§7). Порядок объявления значим: он задаёт значения ролей.
//----------------------------------------------------------------------------
QStringList Week_Slot_Roles()
{
	return QStringList{"id", "dayOfWeek", "kind", "programDayId", "dayTitle", "isAnchor", "isToday", "note"};
}
//----------------------------------------------------------------------------
QStringList Program_Day_Roles()
{
	return QStringList{"id", "title", "kind", "focus", "exerciseCount", "estimatedMinutes", "orderIndex"};
}
//----------------------------------------------------------------------------
QStringList Day_Exercise_Roles()
{
	return QStringList{"id",          "programDayId",  "exerciseId", "title",          "orderIndex",
	                   "sets",        "repsText",      "weightMode", "targetWeightKg", "rpeTarget",
	                   "restSeconds", "supersetGroup", "isOptional"};
}
//----------------------------------------------------------------------------
QStringList Goal_Target_Roles()
{
	return QStringList{"id",          "metric",       "exerciseId",   "unit",      "startValue",
	                   "targetValue", "currentValue", "progressText", "direction", "state"};
}
//----------------------------------------------------------------------------
QStringList Rule_Roles()
{
	// Имя роли не «id»: QML отвергает свойство с именем id (reserved), и
	// делегат экрана правил не смог бы объявить required property.
	return QStringList{"ruleId",      "category",   "code",        "title",       "metric",      "periodicity",
	                   "targetValue", "targetUnit", "displayText", "checkSource", "enforcement", "todayState"};
}
//----------------------------------------------------------------------------
QStringList Checkin_Roles()
{
	return QStringList{"id", "ruleId", "date", "value", "unit", "state", "source", "note"};
}
//----------------------------------------------------------------------------
QStringList Measurement_Roles()
{
	return QStringList{"id", "date", "weightKg", "chestCm", "waistCm", "hipCm", "armCm", "thighCm"};
}
//----------------------------------------------------------------------------
QStringList Catalog_Program_Roles()
{
	// weeksTotal и updatedAt — единственные числа, которые каталог берёт из
	// строки programs. Показатели вроде тоннажа или adherence считаются доменом
	// по фактам и в списке не выводятся: строка каталога обязана оставаться
	// дешёвой, а не превращаться в N запросов статистики.
	return QStringList{"id", "title", "goal", "state", "weeksTotal", "startDate", "updatedAt", "canEdit"};
}
QStringList Program_Summary_Roles()
{
	return QStringList{"id",
	                   "title",
	                   "goal",
	                   "state",
	                   "weeksTotal",
	                   "sessions",
	                   "tonnageKg",
	                   "adherencePercent",
	                   "bodyWeightDeltaKg",
	                   "keyLiftDeltaKg",
	                   "isBest"};
}
//----------------------------------------------------------------------------
QStringList Rule_Stat_Roles()
{
	return QStringList{"id",         "ruleId",     "title", "adherencePercent", "avgValueText", "doneDays",
	                   "failedDays", "streakDays", "trend"};
}
//----------------------------------------------------------------------------
QStringList Exercise_Strength_Roles()
{
	return QStringList{"id", "workingWeightKg", "e1rmKg"};
}
//----------------------------------------------------------------------------
QStringList Goal_Progress_Roles()
{
	return QStringList{"id",           "metric",      "exerciseId",      "startValue",
	                   "currentValue", "targetValue", "progressPercent", "isAchieved"};
}
//----------------------------------------------------------------------------
QStringList Week_Aggregate_Roles()
{
	return QStringList{"weekIndex", "plannedSessions", "completedSessions", "adherencePercent", "tonnageKg"};
}
//----------------------------------------------------------------------------
// Необязательное число → null. «Нет данных» и «ноль» на экране различаются,
// поэтому флаг Has_ обязателен: -1 в роли означал бы «минус килограмм».
//----------------------------------------------------------------------------
static QVariant Optional_Number(double in_value, bool in_has)
{
	return in_has ? QVariant(in_value) : Null_Variant();
}
//----------------------------------------------------------------------------
// «5» или «5–8»: диапазон повностей одной строкой, как в карточке упражнения.
//----------------------------------------------------------------------------
static QString Reps_Text(int in_min, int in_max)
{
	if (in_min <= 0 && in_max <= 0)
		return QString();

	if (in_min == in_max)
		return QString::number(in_min);

	return QString("%1–%2").arg(in_min).arg(in_max);
}
//----------------------------------------------------------------------------
// Целевое значение правила: min/max/exact — одна роль, приоритет у exact.
//----------------------------------------------------------------------------
static QVariant Rule_Target_Value(const SProgram_Rule &in_rule)
{
	if (in_rule.Has_Target_Exact)
		return QVariant(in_rule.Target_Exact);

	if (in_rule.Has_Target_Max)
		return QVariant(in_rule.Target_Max);

	if (in_rule.Has_Target_Min)
		return QVariant(in_rule.Target_Min);

	return Null_Variant();
}
//----------------------------------------------------------------------------
// «≥ 140 г», «≤ 8», «7–9 h»: подпись цели для карточки правила. Перевод
// единиц в словари — в UI, здесь только форма и диапазон (§7).
//----------------------------------------------------------------------------
static QString Rule_Display_Text(const SProgram_Rule &in_rule)
{
	QString value;

	if (in_rule.Has_Target_Exact)
		value = QString::number(in_rule.Target_Exact);
	else if (in_rule.Has_Target_Min && in_rule.Has_Target_Max && in_rule.Target_Min != in_rule.Target_Max)
		value = Reps_Text(int(in_rule.Target_Min), int(in_rule.Target_Max));
	else if (in_rule.Has_Target_Max)
		value = QString("≤ %1").arg(in_rule.Target_Max);
	else if (in_rule.Has_Target_Min)
		value = QString("≥ %1").arg(in_rule.Target_Min);
	else
		return QString();

	return QString("%1 %2").arg(value, To_String(in_rule.Display_Unit));
}
//----------------------------------------------------------------------------
QVector<QVariantMap> Build_Week_Slots(const QVector<SWeek_Slot> &in_slots, const QHash<QString, QString> &in_day_titles,
                                      const QString &in_today)
{
	QVector<QVariantMap> rows;
	QDate today;

	if (in_today.isEmpty() == false)
		today = QDate::fromString(in_today, Qt::ISODate);

	for (int i = 0; i < in_slots.size(); ++i)
	{
		const SWeek_Slot &slot = in_slots.at(i);
		QVariantMap row;

		row.insert("id", slot.ID);
		row.insert("dayOfWeek", slot.Day_Of_Week);
		row.insert("kind", To_String(slot.Kind));
		row.insert("programDayId", slot.Program_Day_Id);
		row.insert("dayTitle", in_day_titles.value(slot.Program_Day_Id));
		row.insert("isAnchor", slot.Is_Anchor);
		// Сегодня — локальная дата устройства (§5.5); при пустой дате флаг
		// выключен, чтобы UI не подсветил случайный слот.
		row.insert("isToday", today.isValid() && today.dayOfWeek() == slot.Day_Of_Week);
		row.insert("note", slot.Note);
		rows.append(row);
	}

	return rows;
}
//----------------------------------------------------------------------------
QVector<QVariantMap> Build_Program_Days(const QVector<SProgram_Day> &in_days, int in_exercise_count)
{
	QVector<QVariantMap> rows;

	for (int i = 0; i < in_days.size(); ++i)
	{
		const SProgram_Day &day = in_days.at(i);
		QVariantMap row;

		row.insert("id", day.ID);
		row.insert("title", day.Title);
		row.insert("kind", To_String(day.Kind));
		row.insert("focus", day.Focus.join(","));
		row.insert("exerciseCount", in_exercise_count);
		row.insert("estimatedMinutes", day.Estimated_Minutes);
		row.insert("orderIndex", day.Order_Index);
		rows.append(row);
	}

	return rows;
}
//----------------------------------------------------------------------------
QVector<QVariantMap> Build_Day_Exercises(const QVector<SDay_Exercise> &in_exercises,
                                         const QHash<QString, QString> &in_titles)
{
	QVector<QVariantMap> rows;

	for (int i = 0; i < in_exercises.size(); ++i)
	{
		const SDay_Exercise &exercise = in_exercises.at(i);
		QVariantMap row;

		row.insert("id", exercise.ID);
		row.insert("programDayId", exercise.Program_Day_Id);
		row.insert("exerciseId", exercise.Exercise_Id);
		row.insert("title", in_titles.value(exercise.Exercise_Id));
		row.insert("orderIndex", exercise.Order_Index);
		row.insert("sets", exercise.Sets);
		row.insert("repsText", Reps_Text(exercise.Reps_Min, exercise.Reps_Max));
		row.insert("weightMode", To_String(exercise.Weight_Mode));
		row.insert("targetWeightKg", Optional_Number(exercise.Target_Weight_Kg, exercise.Has_Target_Weight_Kg));
		row.insert("rpeTarget", Optional_Number(exercise.Rpe_Target, exercise.Has_Rpe_Target));
		row.insert("restSeconds", exercise.Rest_Seconds);
		row.insert("supersetGroup", exercise.Superset_Group);
		row.insert("isOptional", exercise.Is_Optional);
		rows.append(row);
	}

	return rows;
}
//----------------------------------------------------------------------------
QVector<QVariantMap> Build_Goal_Targets(const QVector<SGoal_Target> &in_targets,
                                        const QHash<QString, double> &in_current_values)
{
	QVector<QVariantMap> rows;

	for (int i = 0; i < in_targets.size(); ++i)
	{
		const SGoal_Target &target = in_targets.at(i);
		QVariantMap row;
		bool has_current = in_current_values.contains(target.ID);
		double current = in_current_values.value(target.ID);
		double denominator = target.Target_Value - target.Start_Value;
		bool can_count = has_current && target.Has_Start_Value && denominator != 0;

		row.insert("id", target.ID);
		row.insert("metric", To_String(target.Metric));
		row.insert("exerciseId", target.Exercise_Id);
		row.insert("unit", To_String(target.Unit));
		row.insert("startValue", Optional_Number(target.Start_Value, target.Has_Start_Value));
		row.insert("targetValue", target.Target_Value);
		row.insert("currentValue", has_current ? QVariant(current) : Null_Variant());
		// Без старта или с вырожденным знаменателем прогресс не считается —
		// это «нет данных», а не 0 % (§5.2).
		row.insert("progressText", can_count
		                               ? QString("%1 %").arg(int((current - target.Start_Value) / denominator * 100))
		                               : QString());
		row.insert("direction", To_String(target.Direction));
		row.insert("state", To_String(target.State));
		rows.append(row);
	}

	return rows;
}
//----------------------------------------------------------------------------
QVector<QVariantMap> Build_Rules(const QVector<SProgram_Rule> &in_rules, const QHash<QString, QString> &in_today_state)
{
	QVector<QVariantMap> rows;

	for (int i = 0; i < in_rules.size(); ++i)
	{
		const SProgram_Rule &rule = in_rules.at(i);
		QVariantMap row;

		row.insert("ruleId", rule.ID);
		row.insert("category", To_String(rule.Category));
		row.insert("code", rule.Code);
		row.insert("title", rule.Title);
		row.insert("metric", To_String(rule.Metric));
		row.insert("periodicity", To_String(rule.Periodicity));
		row.insert("targetValue", Rule_Target_Value(rule));
		row.insert("targetUnit", To_String(rule.Display_Unit));
		row.insert("displayText", Rule_Display_Text(rule));
		row.insert("checkSource", To_String(rule.Check_Source));
		row.insert("enforcement", To_String(rule.Enforcement));
		// Правило без отметки за сегодня — «неизвестно», а не «выполнено»:
		// отсутствие данных не равно нулю (§3.6).
		row.insert("todayState", in_today_state.value(rule.ID, To_String(ECheckin_State::Unknown)));
		rows.append(row);
	}

	return rows;
}
//----------------------------------------------------------------------------
QVector<QVariantMap> Build_Checkins(const QVector<SRule_Checkin> &in_checkins)
{
	QVector<QVariantMap> rows;

	for (int i = 0; i < in_checkins.size(); ++i)
	{
		const SRule_Checkin &checkin = in_checkins.at(i);
		QVariantMap row;

		row.insert("id", checkin.ID);
		row.insert("ruleId", checkin.Rule_Id);
		row.insert("date", checkin.Date);
		row.insert("value", Optional_Number(checkin.Value, checkin.Has_Value));
		row.insert("unit", checkin.Unit);
		row.insert("state", To_String(checkin.State));
		row.insert("source", To_String(checkin.Source));
		row.insert("note", checkin.Note);
		rows.append(row);
	}

	return rows;
}
//----------------------------------------------------------------------------
QVector<QVariantMap> Build_Measurements(const QVector<SBody_Measurement> &in_measurements)
{
	QVector<QVariantMap> rows;

	for (int i = 0; i < in_measurements.size(); ++i)
	{
		const SBody_Measurement &measurement = in_measurements.at(i);
		QVariantMap row;

		row.insert("id", measurement.ID);
		row.insert("date", measurement.Date);
		row.insert("weightKg", Optional_Number(measurement.Weight_Kg, measurement.Has_Weight_Kg));
		row.insert("chestCm", Optional_Number(measurement.Chest_Cm, measurement.Has_Chest_Cm));
		row.insert("waistCm", Optional_Number(measurement.Waist_Cm, measurement.Has_Waist_Cm));
		row.insert("hipCm", Optional_Number(measurement.Hip_Cm, measurement.Has_Hip_Cm));
		row.insert("armCm", Optional_Number(measurement.Arm_Cm, measurement.Has_Arm_Cm));
		row.insert("thighCm", Optional_Number(measurement.Thigh_Cm, measurement.Has_Thigh_Cm));
		rows.append(row);
	}

	return rows;
}
//----------------------------------------------------------------------------
QVector<QVariantMap> Build_Catalog_Programs(const QVector<SProgram> &in_programs)
{
	QVector<QVariantMap> rows;

	for (int i = 0; i < in_programs.size(); ++i)
	{
		const SProgram &program = in_programs.at(i);
		QVariantMap row;

		row.insert("id", program.ID);
		row.insert("title", program.Title);
		row.insert("goal", To_String(program.Goal));
		row.insert("state", To_String(program.State));
		row.insert("weeksTotal", program.Term.Weeks_Total);
		row.insert("startDate", program.Term.Start_Date);
		row.insert("updatedAt", program.Updated_At);
		// Завершённую и отменённую программу нельзя править (§3.1), и каталог
		// обязан сказать это заранее, а не прятать пункт меню.
		row.insert("canEdit", Program_State_Is_Editable(program.State));
		rows.append(row);
	}

	return rows;
}
QVector<QVariantMap> Build_Program_Summaries(const QVector<SProgram_Summary> &in_summaries,
                                             const QString &in_best_program_id)
{
	QVector<QVariantMap> rows;

	for (int i = 0; i < in_summaries.size(); ++i)
	{
		const SProgram_Summary &summary = in_summaries.at(i);
		QVariantMap row;

		row.insert("id", summary.Program_Id);
		row.insert("title", summary.Title);
		row.insert("goal", To_String(summary.Goal));
		row.insert("state", To_String(summary.State));
		row.insert("weeksTotal", summary.Weeks_Total);
		row.insert("sessions", summary.Sessions);
		row.insert("tonnageKg", summary.Tonnage_Kg);
		row.insert("adherencePercent", Optional_Number(summary.Adherence_Percent, summary.Has_Adherence_Percent));
		row.insert("bodyWeightDeltaKg",
		           Optional_Number(summary.Body_Weight_Delta_Kg, summary.Has_Body_Weight_Delta_Kg));
		row.insert("keyLiftDeltaKg", Optional_Number(summary.Key_Lift_Delta_Kg, summary.Has_Key_Lift_Delta_Kg));
		row.insert("isBest", summary.Program_Id == in_best_program_id);
		rows.append(row);
	}

	return rows;
}
//----------------------------------------------------------------------------
QVector<QVariantMap> Build_Rule_Stats(const QVector<SRule_Stat_Row> &in_rows, const QHash<QString, int> &in_streaks)
{
	QVector<QVariantMap> rows;

	for (int i = 0; i < in_rows.size(); ++i)
	{
		const SRule_Stat_Row &source = in_rows.at(i);
		QVariantMap row;

		row.insert("id", source.Rule_Id);
		row.insert("ruleId", source.Rule_Id);
		row.insert("title", source.Title);
		row.insert("adherencePercent",
		           Optional_Number(source.Stats.Adherence_Percent, source.Stats.Has_Adherence_Percent));
		row.insert("avgValueText",
		           source.Stats.Has_Average_Value ? QString::number(source.Stats.Average_Value) : QString());
		row.insert("doneDays", source.Stats.Done_Count);
		row.insert("failedDays", source.Stats.Failed_Count);
		row.insert("streakDays", in_streaks.value(source.Rule_Id));
		row.insert("trend", QString());
		rows.append(row);
	}

	return rows;
}
//----------------------------------------------------------------------------
QVector<QVariantMap> Build_Exercise_Strength(const QVector<SExercise_Strength> &in_strength)
{
	QVector<QVariantMap> rows;

	for (int i = 0; i < in_strength.size(); ++i)
	{
		const SExercise_Strength &strength = in_strength.at(i);
		QVariantMap row;

		// id — exerciseId: список силы всегда по упражнениям, отдельного
		// идентификатора строки у метрики нет.
		row.insert("id", strength.Exercise_Id);
		row.insert("workingWeightKg", Optional_Number(strength.Working_Weight_Kg, strength.Has_Working_Weight_Kg));
		row.insert("e1rmKg", Optional_Number(strength.E1rm_Kg, strength.Has_E1rm_Kg));
		rows.append(row);
	}

	return rows;
}
//----------------------------------------------------------------------------
QVector<QVariantMap> Build_Goal_Progress(const QVector<SGoal_Progress> &in_progress)
{
	QVector<QVariantMap> rows;

	for (int i = 0; i < in_progress.size(); ++i)
	{
		const SGoal_Progress &progress = in_progress.at(i);
		QVariantMap row;

		row.insert("id", progress.Goal_Target_Id);
		row.insert("metric", To_String(progress.Metric));
		row.insert("exerciseId", progress.Exercise_Id);
		row.insert("startValue", Optional_Number(progress.Start_Value, progress.Has_Start_Value));
		row.insert("currentValue", Optional_Number(progress.Current_Value, progress.Has_Current_Value));
		row.insert("targetValue", progress.Target_Value);
		row.insert("progressPercent", Optional_Number(progress.Progress_Percent, progress.Has_Progress_Percent));
		row.insert("isAchieved", progress.Is_Achieved);
		rows.append(row);
	}

	return rows;
}
//----------------------------------------------------------------------------
QVector<QVariantMap> Build_Week_Aggregates(const QVector<SWeek_Aggregate> &in_weeks)
{
	QVector<QVariantMap> rows;

	for (int i = 0; i < in_weeks.size(); ++i)
	{
		const SWeek_Aggregate &week = in_weeks.at(i);
		QVariantMap row;

		row.insert("weekIndex", week.Week_Index);
		row.insert("plannedSessions", week.Planned_Sessions);
		row.insert("completedSessions", week.Completed_Sessions);
		row.insert("adherencePercent", Optional_Number(week.Adherence_Percent, week.Has_Adherence_Percent));
		row.insert("tonnageKg", week.Tonnage_Kg);
		rows.append(row);
	}

	return rows;
}
} // namespace Program_Rows
