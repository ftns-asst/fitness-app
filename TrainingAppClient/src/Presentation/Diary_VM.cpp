#include "Presentation/Diary_VM.h"

#include <QLoggingCategory>
#include <cmath>

Q_LOGGING_CATEGORY(Diary_Log, "trainingapp.diary")

//-------------------------------------------------------------------------------------------------------------
Avm_Diary::Avm_Diary(AProgram_Repository *in_programs, ARule_Repository *in_rules, AWorkout_Repository *in_workouts,
                     AMeasurement_Repository *in_measurements, ADay_Note_Repository *in_notes,
                     AStats_Repository *in_stats, QObject *in_parent)
    : QObject(in_parent), Error_State(&Diary_Log())
{
	Programs = in_programs;
	Rules = in_rules;
	Workouts = in_workouts;
	Measurements = in_measurements;
	Notes = in_notes;
	Stats = in_stats;
	Days = new AList_Model(this);
	Norms = new AList_Model(this);
	Summaries = new AList_Model(this);
	Days->Set_Roles(QStringList() << "date"
	                              << "dayTitle"
	                              << "weight"
	                              << "average7"
	                              << "workoutDone"
	                              << "note"
	                              << "editable"
	                              << "values"
	                              << "weekday");
	Norms->Set_Roles(QStringList() << "ruleId"
	                               << "title"
	                               << "target"
	                               << "unit");
	Summaries->Set_Roles(QStringList() << "ruleId"
	                                   << "title"
	                                   << "average"
	                                   << "daysInNorm"
	                                   << "daysOff");
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Diary::reload(const QString &in_user_id, const QString &in_program_id)
{
	// Копии нужны: внутренний refresh передаёт поля модели, а Clear_Data их сбрасывает.
	const QString user_id(in_user_id);
	QString program_id(in_program_id);
	QVector<SProgram_Day> days;
	QVector<SWeek_Slot> week_slots;
	QVector<SRule_Checkin> checkins;
	QVector<SBody_Measurement> measurements;
	QVector<SWorkout_Session> sessions;
	QVector<SDay_Note> notes;
	QVector<SDiary_Summary> summaries;
	Error_State.Clear_Last_Error();
	Clear_Data();
	if (Programs == 0 || Rules == 0 || Workouts == 0 || Measurements == 0 || Notes == 0 || Stats == 0)
		return Set_Error("diary.unavailable");
	if (user_id.trimmed().isEmpty())
		return Set_Error("diary.user_required");
	User_Id = user_id;
	if (program_id.isEmpty())
	{
		QVector<SProgram> programs;
		if (!Programs->Load_Programs(user_id, programs))
			return Set_Error("diary.load_failed");
		for (const SProgram &program : programs)
		{
			if (program.State == EProgram_State::Active)
			{
				program_id = program.ID;
				break;
			}
		}
		if (program_id.isEmpty())
		{
			emit changed();
			return true;
		}
	}
	if (!Programs->Load_Program(program_id, Program))
		return Set_Error("diary.load_failed");
	if (Program.User_Id != user_id)
	{
		Clear_Data();
		return Set_Error("diary.forbidden");
	}
	STerm term(Program.Term);
	if (!Program.Actual_Start_Date.isEmpty())
		term.Start_Date = Program.Actual_Start_Date;
	const QDate start = QDate::fromString(term.Start_Date, Qt::ISODate);
	QString end = term.Compute_End_Date();
	// У открытого срока нет будущей границы: дневник накапливается до сегодня.
	if (term.Mode == ETerm_Mode::OpenEnded)
		end = Today().toString(Qt::ISODate);
	if (!start.isValid() || !QDate::fromString(end, Qt::ISODate).isValid())
	{
		Clear_Data();
		return Set_Error("diary.invalid_term");
	}
	// До старта открытой программы запросы имеют корректные границы, а список дней пуст.
	const QString query_end = end < term.Start_Date ? term.Start_Date : end;
	if (!Programs->Load_Days(program_id, days) || !Programs->Load_Schedule(program_id, week_slots) ||
	    !Rules->Load_Rules(program_id, Loaded_Rules) ||
	    !Rules->Load_Program_Checkins_Range(program_id, term.Start_Date, query_end, checkins) ||
	    !Measurements->Load_Range(user_id, start.addDays(-6).toString(Qt::ISODate), query_end, measurements) ||
	    !Workouts->Load_Sessions(user_id, program_id, term.Start_Date, query_end, sessions) ||
	    !Notes->Load_Range(user_id, term.Start_Date, query_end, notes))
	{
		Clear_Data();
		return Set_Error("diary.load_failed");
	}
	QVector<SProgram_Rule> daily_rules;
	for (const SProgram_Rule &rule : Loaded_Rules)
	{
		if (rule.Program_Id == program_id && rule.Periodicity == ERule_Periodicity::PerDay &&
		    Get_Rule_Kind(rule) == ERule_Kind::Measurable)
			daily_rules.append(rule);
	}
	Loaded_Rules = daily_rules;
	if (!ADiary_Calculator::Build(Program, days, week_slots, Loaded_Rules, checkins, measurements, sessions, notes,
	                              Today(), Day_Logs, summaries))
	{
		Clear_Data();
		return Set_Error("diary.calculate_failed");
	}
	Set_Rows(Day_Logs, summaries);
	emit changed();
	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Diary::openDay(const QString &in_date)
{
	Error_State.Clear_Last_Error();
	Selection.clear();
	if (QDate::fromString(in_date, Qt::ISODate) > Today())
		return Set_Error("diary.future_day");
	for (int i = 0; i < Day_Logs.size(); ++i)
	{
		if (Day_Logs.at(i).Date == in_date)
		{
			Selection = Days->get(i);
			emit changed();
			return true;
		}
	}
	return Set_Error("diary.invalid_date");
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Diary::saveRuleValue(const QString &in_rule_id, const QVariant &in_value)
{
	Error_State.Clear_Last_Error();
	if (!Check_Editable())
		return false;
	for (const SProgram_Rule &rule : Loaded_Rules)
	{
		if (rule.ID != in_rule_id)
			continue;
		SDay_Log day;
		for (const SDay_Log &candidate : Day_Logs)
		{
			if (candidate.Date == selectedDate())
				day = candidate;
		}
		if (!ADiary_Calculator::Applies(rule, day))
			return Set_Error("diary.rule_not_applicable");
		if (in_value.isNull())
		{
			if (!Rules->Delete_Checkin(rule.ID, selectedDate()))
				return Set_Error("diary.save_failed");
			return Finish_Save();
		}
		bool is_number = false;
		const double value = in_value.toDouble(&is_number);
		if (!is_number || !std::isfinite(value) || value < 0 || in_value.metaType().id() == QMetaType::Bool)
			return Set_Error("diary.invalid_value");
		SRule_Checkin checkin;
		AsRule_Engine engine;
		if (!engine.Evaluate_Value(rule, value, checkin.State))
			return Set_Error("diary.invalid_value");
		checkin.Rule_Id = rule.ID;
		checkin.Program_Id = Program.ID;
		checkin.Date = selectedDate();
		checkin.Has_Value = true;
		checkin.Value = value;
		checkin.Unit = To_String(rule.Display_Unit);
		checkin.Source = ECheckin_Source::Manual;
		if (!Rules->Save_Checkin(checkin))
			return Set_Error("diary.save_failed");
		return Finish_Save();
	}
	return Set_Error("diary.unknown_rule");
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Diary::saveWeight(const QVariant &in_value)
{
	Error_State.Clear_Last_Error();
	if (!Check_Editable())
		return false;
	bool is_number = false;
	const double value = in_value.toDouble(&is_number);
	if (!is_number || !std::isfinite(value) || value <= 0 || in_value.metaType().id() == QMetaType::Bool)
		return Set_Error("diary.invalid_weight");
	SBody_Measurement measurement;
	measurement.User_Id = User_Id;
	measurement.Date = selectedDate();
	measurement.Weight_Kg = value;
	measurement.Has_Weight_Kg = true;
	if (!Measurements->Save_Measurement(measurement))
		return Set_Error("diary.save_failed");
	return Finish_Save();
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Diary::saveNote(const QString &in_text)
{
	Error_State.Clear_Last_Error();
	if (!Check_Editable())
		return false;
	SDay_Note note;
	note.User_Id = User_Id;
	note.Date = selectedDate();
	note.Text = in_text;
	if (!Notes->Save_Note(note))
		return Set_Error("diary.save_failed");
	return Refresh_Selection();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Diary::setToday(const QString &in_date)
{
	Pinned_Today = QDate::fromString(in_date, Qt::ISODate);
	Selection.clear();
	if (!User_Id.isEmpty())
		reload(User_Id, Program.ID);
	else
		emit changed();
}
//-------------------------------------------------------------------------------------------------------------
AList_Model *Avm_Diary::days() const
{
	return Days;
}
//-------------------------------------------------------------------------------------------------------------
AList_Model *Avm_Diary::norms() const
{
	return Norms;
}
//-------------------------------------------------------------------------------------------------------------
AList_Model *Avm_Diary::summaries() const
{
	return Summaries;
}
//-------------------------------------------------------------------------------------------------------------
QVariantList Avm_Diary::selectedValues() const
{
	return Selection.value("values").toList();
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Diary::selectedNote() const
{
	return Selection.value("note").toString();
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Diary::selectedDate() const
{
	return Selection.value("date").toString();
}
//-------------------------------------------------------------------------------------------------------------
QVariant Avm_Diary::selectedWeight() const
{
	return Selection.value("weight");
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Diary::errorKey() const
{
	return Error_State.Get_Last_Error();
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Diary::programTitle() const
{
	return Program.Title;
}
//-------------------------------------------------------------------------------------------------------------
QDate Avm_Diary::Today() const
{
	return Pinned_Today.isValid() ? Pinned_Today : QDate::currentDate();
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Diary::Set_Error(const QString &in_key)
{
	Error_State.Set_Last_Error(in_key);
	emit changed();
	return false;
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Diary::Clear_Data()
{
	Selection.clear();
	Loaded_Rules.clear();
	Day_Logs.clear();
	Program = SProgram();
	User_Id.clear();
	Days->Set_Rows(QVector<QVariantMap>());
	Norms->Set_Rows(QVector<QVariantMap>());
	Summaries->Set_Rows(QVector<QVariantMap>());
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Diary::Check_Editable()
{
	// Перед записью перечитываем владельца, границы срока и применимость правил.
	// Дата берётся с часов при каждом вызове: открытая вчера форма не пишет в будущее.
	if (Selection.isEmpty())
		return Set_Error("diary.select_day");
	if (!Refresh_Selection())
		return false;
	if (QDate::fromString(selectedDate(), Qt::ISODate) > Today())
		return Set_Error("diary.future_day");
	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Diary::Refresh_Selection()
{
	const QString date(selectedDate());
	const QString user_id(User_Id);
	const QString program_id(Program.ID);
	if (program_id.isEmpty())
		return Set_Error("diary.select_day");
	if (!reload(user_id, program_id))
		return false;
	return openDay(date);
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Diary::Finish_Save()
{
	if (!Stats->Invalidate_User_Stats(User_Id))
		return Set_Error("diary.stats_failed");
	return Refresh_Selection();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Diary::Set_Rows(const QVector<SDay_Log> &in_days, const QVector<SDiary_Summary> &in_summaries)
{
	QVector<QVariantMap> days;
	QVector<QVariantMap> norms;
	QVector<QVariantMap> summaries;
	for (const SProgram_Rule &rule : Loaded_Rules)
		norms.append(Rule_Row(rule));
	for (const SDay_Log &day : in_days)
	{
		QVariantMap row;
		QVariantList values;
		row.insert("date", day.Date);
		row.insert("weekday", day.Weekday);
		row.insert("dayTitle", day.Day_Title);
		row.insert("weight", day.Has_Weight ? QVariant(day.Weight) : QVariant());
		row.insert("average7", day.Has_Average_7 ? QVariant(day.Average_7) : QVariant());
		row.insert("workoutDone", day.Workout_Done);
		row.insert("note", day.Note);
		row.insert("editable", day.Editable);
		for (int i = 0; i < day.Values.size(); ++i)
		{
			const SDiary_Value &value = day.Values.at(i);
			QVariantMap cell(Rule_Row(Loaded_Rules.at(i)));
			cell.insert("value", value.Has_Value ? QVariant(value.Value) : QVariant());
			cell.insert("verdict", To_String(value.Verdict));
			cell.insert("editable", day.Editable && value.Applicable);
			values.append(cell);
		}
		row.insert("values", values);
		days.append(row);
	}
	for (int i = 0; i < in_summaries.size(); ++i)
	{
		const SDiary_Summary &summary = in_summaries.at(i);
		QVariantMap row(Rule_Row(Loaded_Rules.at(i)));
		row.insert("average", summary.Has_Average ? QVariant(summary.Average) : QVariant());
		row.insert("daysInNorm", summary.Days_In_Norm);
		row.insert("daysOff", summary.Days_Off);
		summaries.append(row);
	}
	Days->Set_Rows(days);
	Norms->Set_Rows(norms);
	Summaries->Set_Rows(summaries);
}
//-------------------------------------------------------------------------------------------------------------
QVariantMap Avm_Diary::Rule_Row(const SProgram_Rule &in_rule) const
{
	QVariantMap row;
	row.insert("ruleId", in_rule.ID);
	row.insert("title", in_rule.Title);
	row.insert("target", ADiary_Calculator::Target_Text(in_rule));
	row.insert("unit", To_String(in_rule.Display_Unit));
	return row;
}
//-------------------------------------------------------------------------------------------------------------
