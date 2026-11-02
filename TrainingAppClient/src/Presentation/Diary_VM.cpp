#include "Presentation/Diary_VM.h"

#include <QLoggingCategory>
#include <algorithm>
#include <cmath>

Q_LOGGING_CATEGORY(Diary_Log, "trainingapp.diary")

namespace
{
// Окно личных записей без программы: двенадцать недель до сегодня. Достаточно,
// чтобы видеть динамику веса и заметки, и не превращает список дат в архив.
constexpr int kPersonalWindowWeeks = 12;
// Обхваты по умолчанию, когда программы нет или её упражнения не задают ни
// одной группы мышц: три обхвата, которые человек измеряет чаще всего.
const char *const kDefaultMeasurementKeys[] = {"chest", "waist", "hip"};
// Шаг дельты веса: «за 7 дней» — неделя, а не календарная неделя программы.
constexpr int kWeightDeltaDays = 7;
}

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
	Sessions = new AList_Model(this);
	Days->Set_Roles(QStringList() << "date"
	                              << "dayTitle"
	                              << "weight"
	                              << "average7"
	                              << "delta7"
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
	// Строка истории: дата, название дня, состояние и объём (минуты, тоннаж).
	// Подпись собирает QML — единицы и запятая разделителя зависят от локали.
	Sessions->Set_Roles(QStringList() << "date"
	                                  << "title"
	                                  << "workoutState"
	                                  << "minutes"
	                                  << "tonnage"
	                                  << "setCount");
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
	}
	// Программы нет — дневник всё равно обязан работать: вес и заметки
	// принадлежат пользователю и дате, а не программе. Срок заменяется окном по
	// умолчанию (12 недель до сегодня), правила и дни не загружаются.
	Program_Loaded = false;
	if (!program_id.isEmpty())
	{
		if (!Programs->Load_Program(program_id, Program))
			return Set_Error("diary.load_failed");
		if (Program.User_Id != user_id)
		{
			Clear_Data();
			return Set_Error("diary.forbidden");
		}
		Program_Loaded = true;
	}
	else
	{
		Program = SProgram();
		Program.User_Id = user_id;
		Program.Term.Mode = ETerm_Mode::FixedWeeks;
		Program.Term.Weeks_Total = kPersonalWindowWeeks;
		Program.Term.Start_Date = Today().addDays(-(kPersonalWindowWeeks * 7 - 1)).toString(Qt::ISODate);
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
	// Данные программы грузятся только когда она есть; личные записи — всегда.
	if (Program_Loaded && (!Programs->Load_Days(program_id, days) || !Programs->Load_Schedule(program_id, week_slots) ||
	                       !Rules->Load_Rules(program_id, Loaded_Rules) ||
	                       !Rules->Load_Program_Checkins_Range(program_id, term.Start_Date, query_end, checkins)))
	{
		Clear_Data();
		return Set_Error("diary.load_failed");
	}
	if (!Measurements->Load_Range(user_id, start.addDays(-6).toString(Qt::ISODate), query_end, measurements) ||
	    !Workouts->Load_Sessions(user_id, program_id, term.Start_Date, query_end, sessions) ||
	    !Notes->Load_Range(user_id, term.Start_Date, query_end, notes))
	{
		Clear_Data();
		return Set_Error("diary.load_failed");
	}
	// Замеры и тренировки держатся в памяти: из них собираются дельта веса,
	// подпись обхватов и история занятий. Второй раз те же данные из БД не
	// читаются — reload и так вызывается после каждой записи.
	Loaded_Measurements = measurements;
	Loaded_Sessions = sessions;
	Loaded_Days = days;
	Loaded_Slots = week_slots;
	Prepare_Measurement_Keys();
	// Без программы наборы пусты: репозиториям пустой идентификатор означает
	// «только личные данные пользователя».
	QVector<SProgram_Rule> daily_rules;
	if (Program_Loaded)
	{
		for (const SProgram_Rule &rule : Loaded_Rules)
		{
			if (rule.Program_Id == program_id && rule.Periodicity == ERule_Periodicity::PerDay &&
			    Get_Rule_Kind(rule) == ERule_Kind::Measurable)
				daily_rules.append(rule);
		}
	}
	Loaded_Rules = daily_rules;
	if (!ADiary_Calculator::Build(Program, days, week_slots, Loaded_Rules, checkins, measurements, sessions, notes,
	                              Today(), Day_Logs, summaries))
	{
		Clear_Data();
		return Set_Error("diary.calculate_failed");
	}
	Set_Rows(Day_Logs, summaries);
	Set_Session_Rows();
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
bool Avm_Diary::saveMeasurements(const QVariantMap &in_values)
{
	Error_State.Clear_Last_Error();
	if (!Check_Editable())
		return false;
	SBody_Measurement measurement;
	bool any = false;

	measurement.User_Id = User_Id;
	measurement.Date = selectedDate();
	Fill_Measurement_Values(measurement, in_values, any);

	if (!any)
		return Set_Error("diary.invalid_value");
	// Сохранение — upsert с слиянием (см. AMeasurement_Repository): незаполненные
	// поля дня остаются прежними, поэтому обхваты не стирают утренний вес.
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
AList_Model *Avm_Diary::sessions() const
{
	return Sessions;
}
//-------------------------------------------------------------------------------------------------------------
QStringList Avm_Diary::measurementKeys() const
{
	return Measurement_Keys;
}
//-------------------------------------------------------------------------------------------------------------
QVariantMap Avm_Diary::selectedMeasurement() const
{
	const SBody_Measurement *measurement = Find_Measurement(selectedDate());

	return measurement == 0 ? QVariantMap() : Measurement_Map(*measurement);
}
//-------------------------------------------------------------------------------------------------------------
// Последний замер, в котором есть обхваты: подпись «обхваты измерены <дата>»
// относится к последнему измерению тела, а не к последней записи веса — вес
// пишется каждый день, и дата подписи обновлялась бы ежедневно.
QVariantMap Avm_Diary::lastMeasurement() const
{
	QVariantMap result;
	QString last_date;

	for (const SBody_Measurement &measurement : Loaded_Measurements)
	{
		bool has_girth = false;

		for (int i = 0; i < (int)EMeasurement_Key::Max; ++i)
		{
			double value = 0;

			if (Get_Measurement_Value(measurement, (EMeasurement_Key)i, value))
				has_girth = true;
		}

		if (!has_girth || measurement.Date <= last_date)
			continue;

		last_date = measurement.Date;
		result = Measurement_Map(measurement);
	}

	if (!last_date.isEmpty())
		result.insert("date", last_date);

	return result;
}
//-------------------------------------------------------------------------------------------------------------
// Тренировка выбранной даты. Черновик и прерванная сессия — не результат дня,
// поэтому «тренировка дня» показывает только завершённое занятие.
QVariantMap Avm_Diary::selectedSession() const
{
	const QString date(selectedDate());

	if (date.isEmpty())
		return QVariantMap();

	for (const SWorkout_Session &session : Loaded_Sessions)
	{
		if (session.Date == date && session.State == EWorkout_State::Completed)
			return Session_Row(session);
	}

	return QVariantMap();
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
// Программы нет — дневник работает как личные записи: даты, вес и заметки.
bool Avm_Diary::Has_Program() const
{
	return Program_Loaded;
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
	Loaded_Days.clear();
	Loaded_Slots.clear();
	Loaded_Measurements.clear();
	Loaded_Sessions.clear();
	Measurement_Keys.clear();
	Program = SProgram();
	Program_Loaded = false;
	User_Id.clear();
	Days->Set_Rows(QVector<QVariantMap>());
	Norms->Set_Rows(QVector<QVariantMap>());
	Summaries->Set_Rows(QVector<QVariantMap>());
	Sessions->Set_Rows(QVector<QVariantMap>());
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
	// Без программы личные записи сохраняются так же: reload подставит окно по
	// умолчанию и вернёт пустые правила, а дата останется в списке дней.
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
		bool has_previous = false;
		double previous = 0;

		row.insert("date", day.Date);
		row.insert("weekday", day.Weekday);
		row.insert("dayTitle", day.Day_Title);
		row.insert("weight", day.Has_Weight ? QVariant(day.Weight) : QVariant());
		row.insert("average7", day.Has_Average_7 ? QVariant(day.Average_7) : QVariant());
		// Дельта к весу неделю назад: «−1,2 за 7 дней» из макета 13. Считается
		// только когда оба замера есть — иначе подпись обещала бы динамику,
		// которой нет.
		const SBody_Measurement *week_ago = Find_Measurement(
		    QDate::fromString(day.Date, Qt::ISODate).addDays(-kWeightDeltaDays).toString(Qt::ISODate));
		if (day.Has_Weight && week_ago != 0 && week_ago->Has_Weight_Kg)
		{
			has_previous = true;
			previous = week_ago->Weight_Kg;
		}
		row.insert("delta7", has_previous ? QVariant(day.Weight - previous) : QVariant());
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
const SBody_Measurement *Avm_Diary::Find_Measurement(const QString &in_date) const
{
	if (in_date.isEmpty())
		return 0;

	for (const SBody_Measurement &measurement : Loaded_Measurements)
	{
		if (measurement.Date == in_date)
			return &measurement;
	}

	return 0;
}
//-------------------------------------------------------------------------------------------------------------
QVariantMap Avm_Diary::Measurement_Map(const SBody_Measurement &in_measurement) const
{
	QVariantMap values;
	double number = 0;

	if (in_measurement.Has_Weight_Kg)
		values.insert("weight", in_measurement.Weight_Kg);

	for (int i = 0; i < (int)EMeasurement_Key::Max; ++i)
	{
		if (Get_Measurement_Value(in_measurement, (EMeasurement_Key)i, number))
			values.insert(To_String((EMeasurement_Key)i), number);
	}

	return values;
}
//-------------------------------------------------------------------------------------------------------------
// Поля формы замеров. Вес — всегда; обхваты — те, что нужны упражнениям
// программы (обхваты для групп мышц, см. Measurement_Keys_For_Muscles), а если
// программа их не задаёт — три базовых. Спрашивать все девять обхватов у того,
// кто тренирует ноги, значит заставлять заполнять пустые поля.
void Avm_Diary::Prepare_Measurement_Keys()
{
	QStringList girths;

	Measurement_Keys.clear();
	if (Program_Loaded && Measurements != 0)
		Measurements->Load_Program_Measurement_Keys(Program.ID, girths);
	if (girths.isEmpty())
	{
		for (const char *key : kDefaultMeasurementKeys)
			girths.append(QString::fromLatin1(key));
	}

	Measurement_Keys << "weight";

	for (const QString &key : girths)
	{
		if (!Measurement_Keys.contains(key))
			Measurement_Keys << key;
	}
}
//-------------------------------------------------------------------------------------------------------------
// Разбор формы: заполненные поля переносятся в замер, пустые и некорректные
// пропускаются. out_any — записывать нечего (все поля пусты или неверны).
void Avm_Diary::Fill_Measurement_Values(SBody_Measurement &io_measurement, const QVariantMap &in_values,
                                        bool &out_any) const
{
	out_any = false;

	for (auto it = in_values.constBegin(); it != in_values.constEnd(); ++it)
	{
		const QString key(it.key());
		bool is_number = false;
		const double value = it.value().toDouble(&is_number);

		// Пустое поле приходит пустой строкой или null — это «не менялось», а не
		// ошибка: форма отправляет все поля сразу, заполнены могут быть не все.
		if (!is_number || !std::isfinite(value))
			continue;
		if (key == "weight")
		{
			if (value <= 0)
				continue;
			io_measurement.Weight_Kg = value;
			io_measurement.Has_Weight_Kg = true;
			out_any = true;
			continue;
		}
		EMeasurement_Key measurement_key;
		if (!From_String(key, measurement_key) || value < 0)
			continue;
		Set_Measurement_Value(io_measurement, measurement_key, value);
		out_any = true;
	}
}
//-------------------------------------------------------------------------------------------------------------
// История тренировок — от новых к старым: дневник отвечает на «что было»,
// и последняя тренировка интереснее первой.
void Avm_Diary::Set_Session_Rows()
{
	QVector<QVariantMap> rows;
	QVector<SWorkout_Session> ordered(Loaded_Sessions);

	std::sort(ordered.begin(), ordered.end(), [](const SWorkout_Session &a, const SWorkout_Session &b) {
		if (a.Date != b.Date)
			return a.Date > b.Date;

		return a.Completed_At > b.Completed_At;
	});

	// Черновики и отменённые сессии в историю не попадают: незавершённая тренировка
	// — не результат, а отменённая — пропущенный день, а не занятие. Оба факта
	// остаются в статистике, но не в списке «прошедших тренировок».
	for (const SWorkout_Session &session : ordered)
	{
		if (session.State != EWorkout_State::Completed)
			continue;
		rows.append(Session_Row(session));
	}

	Sessions->Set_Rows(rows);
}
//-------------------------------------------------------------------------------------------------------------
QVariantMap Avm_Diary::Session_Row(const SWorkout_Session &in_session) const
{
	QVariantMap row;
	QString day_id(in_session.Program_Day_Id);
	QString title;

	// Сессия могла быть записана без привязки к дню плана (демо-данные, свободная
	// тренировка): день восстанавливается по дню недели даты — расписание уже
	// загружено для дневника, отдельного запроса не нужно.
	if (day_id.isEmpty())
	{
		const QDate date = QDate::fromString(in_session.Date, Qt::ISODate);

		for (const SWeek_Slot &slot : Loaded_Slots)
		{
			if (slot.Day_Of_Week == date.dayOfWeek())
			{
				day_id = slot.Program_Day_Id;
				break;
			}
		}
	}

	for (const SProgram_Day &day : Loaded_Days)
	{
		if (day.ID == day_id)
			title = day.Title;
	}

	row.insert("date", in_session.Date);
	// Соло-тренировка вне программы (пустой Program_Id, инвариант 18 §4) — тоже
	// тренировка: подпись говорит об этом прямо, иначе строка была бы безымянной.
	row.insert("title", title.isEmpty() ? (in_session.Program_Id.isEmpty() ? QStringLiteral("Свободная тренировка")
	                                                                        : QStringLiteral("Тренировка по программе"))
	                                    : title);
	row.insert("workoutState", To_String(in_session.State));
	row.insert("minutes", in_session.Duration_Seconds > 0 ? in_session.Duration_Seconds / 60 : 0);
	row.insert("tonnage", in_session.Tonnage_Kg);
	row.insert("setCount", in_session.Set_Count);

	return row;
}
//-------------------------------------------------------------------------------------------------------------
