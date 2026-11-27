#include "Domain/Diary_Calculator.h"

#include <QHash>
#include <QSet>

//-------------------------------------------------------------------------------------------------------------
bool ADiary_Calculator::Build(const SProgram &in_program, const QVector<SProgram_Day> &in_days,
                              const QVector<SWeek_Slot> &in_slots, const QVector<SProgram_Rule> &in_rules,
                              const QVector<SRule_Checkin> &in_checkins,
                              const QVector<SBody_Measurement> &in_measurements,
                              const QVector<SWorkout_Session> &in_sessions, const QVector<SDay_Note> &in_notes,
                              const QDate &in_today, QVector<SDay_Log> &out_days,
                              QVector<SDiary_Summary> &out_summaries)
{
	STerm term(in_program.Term);
	AsRule_Engine engine;
	QVector<SDay_Log> days;
	QVector<SDiary_Summary> summaries;
	QVector<QVector<SRule_Checkin>> summary_checkins(in_rules.size());
	QHash<QString, QHash<QString, SRule_Checkin>> checkins_by_date;
	QHash<QString, double> weights_by_date;
	QHash<QString, QString> notes_by_date;
	QSet<QString> completed_dates;
	QHash<int, SWeek_Slot> slots_by_position;
	QHash<QString, SProgram_Day> plans_by_id;
	int rotation_length = 0;
	double weight_sum = 0;
	int weight_count = 0;

	out_days.clear();
	out_summaries.clear();
	if (!in_program.Actual_Start_Date.isEmpty())
		term.Start_Date = in_program.Actual_Start_Date;
	const QDate start = QDate::fromString(term.Start_Date, Qt::ISODate);
	QDate end = QDate::fromString(term.Compute_End_Date(), Qt::ISODate);
	// Открытый срок накапливает дни до сегодня, до старта список пустой.
	// Для фиксированного срока сохраняем ограничение модели 1–52 недели.
	if (term.Mode == ETerm_Mode::OpenEnded)
		end = in_today;
	else if (term.Mode != ETerm_Mode::FixedWeeks || term.Weeks_Total < 1 || term.Weeks_Total > 52)
		return false;
	if (!start.isValid() || start.toString(Qt::ISODate) != term.Start_Date || !end.isValid() || !in_today.isValid())
		return false;
	for (const SProgram_Rule &rule : in_rules)
	{
		if (rule.Program_Id != in_program.ID)
			return false;
	}
	for (const SRule_Checkin &checkin : in_checkins)
	{
		if (checkin.Program_Id == in_program.ID)
			checkins_by_date[checkin.Date].insert(checkin.Rule_Id, checkin);
	}
	for (const SBody_Measurement &measurement : in_measurements)
	{
		if (measurement.User_Id == in_program.User_Id && measurement.Has_Weight_Kg &&
		    measurement.Date <= in_today.toString(Qt::ISODate))
			weights_by_date.insert(measurement.Date, measurement.Weight_Kg);
	}
	for (const SDay_Note &note : in_notes)
	{
		if (note.User_Id == in_program.User_Id)
			notes_by_date.insert(note.Date, note.Text);
	}
	for (const SWorkout_Session &session : in_sessions)
	{
		if (session.User_Id == in_program.User_Id && session.Program_Id == in_program.ID &&
		    session.State == EWorkout_State::Completed)
			completed_dates.insert(session.Date);
	}
	for (const SWeek_Slot &slot : in_slots)
	{
		const int position =
		    in_program.Schedule_Kind == ESchedule_Kind::Rotation ? slot.Rotation_Index : slot.Day_Of_Week;
		slots_by_position.insert(position, slot);
		rotation_length = qMax(rotation_length, slot.Rotation_Index);
	}
	for (const SProgram_Day &plan : in_days)
		plans_by_id.insert(plan.ID, plan);
	// Индексы дают O(факты + дни × правила); окно веса сдвигается за O(1).
	for (int offset = -6; offset < 0; ++offset)
	{
		const QString date = start.addDays(offset).toString(Qt::ISODate);
		if (weights_by_date.contains(date))
		{
			weight_sum += weights_by_date.value(date);
			++weight_count;
		}
	}
	for (QDate date = start; date <= end; date = date.addDays(1))
	{
		SDay_Log day;
		day.Date = date.toString(Qt::ISODate);
		day.Weekday = date.dayOfWeek();
		day.Editable = date <= in_today;
		day.Day_Title = QString::fromUtf8("Отдых");
		int position = date.dayOfWeek();
		if (in_program.Schedule_Kind == ESchedule_Kind::Rotation)
			position = rotation_length > 0 ? (int)(start.daysTo(date) % rotation_length) + 1 : 0;
		const SWeek_Slot slot = slots_by_position.value(position);
		day.Day_Id = slot.Program_Day_Id;
		if (plans_by_id.contains(day.Day_Id))
		{
			const SProgram_Day plan = plans_by_id.value(day.Day_Id);
			day.Day_Title = plan.Title;
			day.Is_Training = plan.Kind != EDay_Kind::Rest;
		}
		else
			day.Is_Training = slot.Kind != ESlot_Kind::Rest;
		const QString expired = date.addDays(-7).toString(Qt::ISODate);
		if (date != start && weights_by_date.contains(expired))
		{
			weight_sum -= weights_by_date.value(expired);
			--weight_count;
		}
		if (weights_by_date.contains(day.Date))
		{
			weight_sum += weights_by_date.value(day.Date);
			++weight_count;
		}
		if (day.Editable)
		{
			day.Has_Weight = weights_by_date.contains(day.Date);
			day.Weight = weights_by_date.value(day.Date);
			day.Has_Average_7 = weight_count > 0;
			day.Average_7 = weight_count > 0 ? weight_sum / weight_count : 0;
			day.Workout_Done = completed_dates.contains(day.Date);
			day.Note = notes_by_date.value(day.Date);
		}
		const QHash<QString, SRule_Checkin> day_checkins = checkins_by_date.value(day.Date);
		for (int i = 0; i < in_rules.size(); ++i)
		{
			const SProgram_Rule &rule = in_rules.at(i);
			const SRule_Checkin checkin = day_checkins.value(rule.ID);
			SDiary_Value cell;
			cell.Rule_Id = rule.ID;
			cell.Applicable = Applies(rule, day);
			if (day.Editable && cell.Applicable && checkin.Has_Value)
			{
				if (!engine.Evaluate_Checkin(rule, checkin, cell.Verdict))
					return false;
				cell.Value = checkin.Value;
				cell.Has_Value = true;
				summary_checkins[i].append(checkin);
			}
			day.Values.append(cell);
		}
		days.append(day);
	}
	for (int i = 0; i < in_rules.size(); ++i)
	{
		SRule_Stats stats;
		SDiary_Summary summary;
		if (!engine.Compute_Rule_Stats(in_rules.at(i), summary_checkins.at(i), stats))
			return false;
		summary.Rule_Id = in_rules.at(i).ID;
		summary.Has_Average = stats.Has_Average_Value;
		summary.Average = stats.Average_Value;
		summary.Days_In_Norm = stats.Done_Count;
		summary.Days_Off = stats.Failed_Count + stats.Partial_Count + stats.Skipped_Count;
		summaries.append(summary);
	}
	out_days = days;
	out_summaries = summaries;

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool ADiary_Calculator::Applies(const SProgram_Rule &in_rule, const SDay_Log &in_day)
{
	AsRule_Engine engine;
	if (Get_Rule_Kind(in_rule) != ERule_Kind::Measurable || in_rule.Periodicity != ERule_Periodicity::PerDay ||
	    !engine.Is_Rule_Active(in_rule, in_day.Date))
		return false;
	switch (in_rule.Applies_To)
	{
	case ERule_Applies_To::AllDays:
		return true;
	case ERule_Applies_To::TrainingDays:
		return in_day.Is_Training;
	case ERule_Applies_To::RestDays:
		return !in_day.Is_Training;
	case ERule_Applies_To::CustomDays:
		return in_rule.Applies_Days.contains(in_day.Day_Id);
	default:
		return false;
	}
}
//-------------------------------------------------------------------------------------------------------------
QString ADiary_Calculator::Target_Text(const SProgram_Rule &in_rule)
{
	if (in_rule.Has_Target_Exact)
		return QString("= %1").arg(in_rule.Target_Exact);
	if (in_rule.Has_Target_Min && in_rule.Has_Target_Max)
		return QString::fromUtf8("%1–%2").arg(in_rule.Target_Min).arg(in_rule.Target_Max);
	if (in_rule.Has_Target_Min)
		return QString::fromUtf8("≥ %1").arg(in_rule.Target_Min);
	if (in_rule.Has_Target_Max)
		return QString::fromUtf8("≤ %1").arg(in_rule.Target_Max);
	return QString();
}
//-------------------------------------------------------------------------------------------------------------
