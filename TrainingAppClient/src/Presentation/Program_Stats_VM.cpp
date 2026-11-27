#include "Presentation/Program_Stats_VM.h"

#include "Data/Program/Measurement_Repository.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Data/Program/Stats_Repository.h"
#include "Data/Program/Workout_Repository.h"
#include "Domain/Measurement_Schedule.h"
#include "Domain/Measurement_Trend.h"
#include "Presentation/Program_Rows.h"

#include <QDate>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(PROGRAM_STATS_VM_Log, "trainingapp.program.stats_vm")

//-------------------------------------------------------------------------------------------------------------
Avm_Program_Stats::Avm_Program_Stats(AProgram_Repository *in_program_repository,
                                     AWorkout_Repository *in_workout_repository, ARule_Repository *in_rule_repository,
                                     AMeasurement_Repository *in_measurement_repository,
                                     AStats_Repository *in_stats_repository, QObject *in_parent)
    : QObject(in_parent), Program_Repository_Instance(in_program_repository),
      Workout_Repository_Instance(in_workout_repository), Rule_Repository_Instance(in_rule_repository),
      Measurement_Repository_Instance(in_measurement_repository), Stats_Repository_Instance(in_stats_repository),
      Weeks(new AList_Model(this)), Rule_Stats_Model(new AList_Model(this)),
      Exercise_Strength_Model(new AList_Model(this)), Goal_Progress_Model(new AList_Model(this)),
      Measurements_Model(new AList_Model(this)), Trends_Model(new AList_Model(this)),
      Weight_Series_Model(new AList_Model(this)), Change_Map_Model(new AList_Model(this)),
      Nutrition_Summary_Model(new AList_Model(this))
{
	Today_Text = QDate::currentDate().toString("yyyy-MM-dd");

	Weeks->Set_Roles(Program_Rows::Week_Aggregate_Roles());
	Rule_Stats_Model->Set_Roles(Program_Rows::Rule_Stat_Roles());
	Measurements_Model->Set_Roles(Program_Rows::Measurement_Roles());
	Trends_Model->Set_Roles(Program_Rows::Trend_Roles());
	Weight_Series_Model->Set_Roles(Program_Rows::Weight_Point_Roles());
	Change_Map_Model->Set_Roles(Program_Rows::Change_Row_Roles());
	Nutrition_Summary_Model->Set_Roles(
	    {"ruleId", "title", "unit", "targetText", "average", "daysInNorm", "daysOff", "points"});
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Program_Stats::Set_Today(const QString &in_date)
{
	Today_Text = in_date;
	Today_Is_Pinned = true;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Program_Stats::Refresh_Today()
{
	QString current;

	if (Today_Is_Pinned)
		return false;

	current = QDate::currentDate().toString("yyyy-MM-dd");

	if (current == Today_Text)
		return false;

	Today_Text = current;

	return true;
}
// — QML-мост (camelCase) —
// Обёртки отделены от методов предметной области: контракт экрана не должен
// повторять внутренний стиль имён (§2 CODE_STYLE) и зависеть от него.
bool Avm_Program_Stats::refresh(const QString &in_program_id, const QString &in_date_from, const QString &in_date_to)
{
	return Refresh(in_program_id, in_date_from, in_date_to);
}

//-------------------------------------------------------------------------------------------------------------
bool Avm_Program_Stats::Refresh(const QString &in_program_id, const QString &in_date_from, const QString &in_date_to)
{
	SProgress_Facts facts;
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> exercises;
	QVector<SRule_Checkin> checkins;

	Refresh_Today();
	Clear_Stats();
	Clear_Error();

	if (Program_Repository_Instance == 0 || Workout_Repository_Instance == 0)
	{
		Set_Error();

		return false;
	}

	facts.Today = Today_Text;

	// Структура программы нужна и для окна расчёта, и для adherence по
	// расписанию (§5.2); дни и предписания статистике не требуются.
	if (!Program_Repository_Instance->Load_Program_Full(in_program_id, facts.Program, facts.Goal_Targets, days,
	                                                    exercises, facts.Schedule))
	{
		Set_Error();

		return false;
	}

	// Границы окна. Экран зовёт refresh без дат, и тогда запрос уходит с пустыми
	// границами: Load_Sessions это терпит (пусто — вся история), а
	// Load_Range и Load_Program_Checkins_Range — нет, они читают строго
	// заданный диапазон и молча возвращают пусто. Из-за этого метрика
	// «Тело, кг» и процент по отметкам оставались прочерками при полной базе.
	//
	// Поэтому пустые границы заменяются окном самой программы: от её старта
	// до сегодняшнего дня. Это ровно то окно, по которому считает калькулятор
	// (§5.5), поэтому отметки и замеры за пределами программы не попадут.
	QString date_from = in_date_from;
	QString date_to = in_date_to;

	if (date_from.isEmpty() || date_to.isEmpty())
	{
		const QString program_start =
		    facts.Program.Actual_Start_Date.isEmpty() ? facts.Program.Term.Start_Date : facts.Program.Actual_Start_Date;
		const QDate today = QDate::fromString(facts.Today, Qt::ISODate);
		const QDate start = QDate::fromString(program_start, Qt::ISODate);

		if (start.isValid() && today.isValid())
		{
			if (date_from.isEmpty())
				date_from = start.toString("yyyy-MM-dd");

			if (date_to.isEmpty())
				date_to = today.toString("yyyy-MM-dd");
		}
	}

	// Явный период также ограничен программой и сегодняшней датой.
	const QString minimum_date = AMeasurement_Schedule::Start_Date_Of(facts.Program);
	if (!minimum_date.isEmpty() && (date_from.isEmpty() || date_from < minimum_date))
		date_from = minimum_date;
	if (date_to.isEmpty() || date_to > facts.Today)
		date_to = facts.Today;

	if (!Workout_Repository_Instance->Load_Program_Facts(facts.Program.User_Id, in_program_id, date_from, date_to,
	                                                     facts.Sessions, facts.Sets))
	{
		Set_Error();

		return false;
	}

	if (Rule_Repository_Instance != 0)
	{
		if (!Rule_Repository_Instance->Load_Rules(in_program_id, facts.Rules) ||
		    !Rule_Repository_Instance->Load_Program_Checkins_Range(in_program_id, date_from, date_to, checkins))
		{
			Set_Error();

			return false;
		}
		facts.Checkins = checkins;
	}

	if (Measurement_Repository_Instance != 0 &&
	    !Measurement_Repository_Instance->Load_Range(facts.Program.User_Id, date_from, date_to, facts.Measurements))
	{
		Set_Error();

		return false;
	}

	Dashboard = ADashboard_Calculator::Compute(
	    facts.Program, facts.Goal_Targets, facts.Measurements, facts.Today,
	    Clock_Program_Id == facts.Program.ID ? Clock_Paused_Days : facts.Program.Paused_Days,
	    Clock_Program_Id == facts.Program.ID ? Clock_Paused_On : facts.Program.Paused_On);
	Dashboard.insert("endDate", AMeasurement_Schedule::End_Date_Of(facts.Program));
	QVector<SDay_Log> diary_days;
	QVector<SDiary_Summary> summaries;
	QVector<QVariantMap> nutrition_rows;
	int planned_total = 0;
	if (ADiary_Calculator::Build(facts.Program, days, facts.Schedule, facts.Rules, checkins, facts.Measurements,
	                             facts.Sessions, {}, QDate::fromString(facts.Today, Qt::ISODate), diary_days,
	                             summaries))
	{
		for (const SDay_Log &day : diary_days)
		{
			if (day.Is_Training)
				++planned_total;
		}
		for (const SDiary_Summary &summary : summaries)
		{
			for (const SProgram_Rule &rule : facts.Rules)
			{
				if (rule.ID != summary.Rule_Id || Get_Rule_Kind(rule) != ERule_Kind::Measurable)
					continue;
				QVariantMap row;
				row.insert("ruleId", rule.ID);
				row.insert("title", rule.Title);
				row.insert("unit", To_String(rule.Display_Unit));
				row.insert("targetText", ADiary_Calculator::Target_Text(rule));
				row.insert("average", summary.Has_Average ? QVariant(summary.Average) : QVariant());
				row.insert("daysInNorm", summary.Days_In_Norm);
				row.insert("daysOff", summary.Days_Off);
				QVariantList points;
				for (const SDay_Log &day : diary_days)
				{
					if (day.Date < date_from || day.Date > date_to || day.Date > facts.Today)
						continue;
					for (const SDiary_Value &value : day.Values)
					{
						if (value.Rule_Id != rule.ID || !value.Applicable || !value.Has_Value)
							continue;
						QVariantMap point;
						point.insert("date", day.Date);
						point.insert("value", value.Value);
						points.append(point);
					}
				}
				row.insert("points", points);
				nutrition_rows.append(row);
			}
		}
	}
	Nutrition_Summary_Model->Set_Rows(nutrition_rows);

	if (!Calculator.Compute_Program_Stats(facts, Stats))
	{
		Set_Error();

		return false;
	}
	Dashboard.insert("plannedSessionsTotal", planned_total);
	Dashboard.insert("completedSessions", Stats.Completed_Sessions);
	Dashboard.insert("completionProgress",
	                 ADashboard_Calculator::Completion_Progress(Dashboard.value("termProgress"),
	                                                            Stats.Completed_Sessions, planned_total));

	Weeks->Set_Rows(Program_Rows::Build_Week_Aggregates(Stats.Weeks));
	Rule_Stats_Model->Set_Rows(Program_Rows::Build_Rule_Stats(Stats.Rule_Stats, QHash<QString, int>()));
	Exercise_Strength_Model->Set_Roles(Program_Rows::Exercise_Strength_Roles());
	Exercise_Strength_Model->Set_Rows(Program_Rows::Build_Exercise_Strength(Stats.Exercise_Strength));
	Goal_Progress_Model->Set_Roles(Program_Rows::Goal_Progress_Roles());
	Goal_Progress_Model->Set_Rows(Program_Rows::Build_Goal_Progress(Stats.Goal_Progress));
	Measurements_Model->Set_Rows(Program_Rows::Build_Measurements(facts.Measurements));

	// Динамика тела — чистая функция от замеров окна программы, её финиша и
	// целей: в кэш статистики не входит и пересчитывается при каждом обновлении.
	Trends_Model->Set_Rows(Program_Rows::Build_Trends(AMeasurement_Trend::Compute_Trends(
	    facts.Measurements, AMeasurement_Schedule::End_Date_Of(facts.Program), facts.Goal_Targets)));
	Weight_Series_Model->Set_Rows(
	    Program_Rows::Build_Weight_Series(AMeasurement_Trend::Weight_Series(facts.Measurements)));
	Change_Map_Model->Set_Rows(Program_Rows::Build_Change_Map(AMeasurement_Trend::Change_Map(facts.Measurements)));

	// Кэш — ускоритель отображения, не источник истины (§5.4): он пишется
	// после успешного пересчёта и помечает ревизию структуры.
	if (Stats_Repository_Instance != 0)
	{
		QString payload;

		if (Calculator.Encode_Program_Stats(Stats, payload))
			Stats_Repository_Instance->Save_Program_Stats(in_program_id, Stats.Program_Revision, payload);
	}

	emit changed();

	return true;
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Program_Stats::Clear_Stats()
{
	Stats = SProgram_Stats();
	Dashboard.clear();
	Nutrition_Summary_Model->Set_Rows({});

	Weeks->Set_Rows(QVector<QVariantMap>());
	Rule_Stats_Model->Set_Rows(QVector<QVariantMap>());
	Exercise_Strength_Model->Set_Rows(QVector<QVariantMap>());
	Goal_Progress_Model->Set_Rows(QVector<QVariantMap>());
	Measurements_Model->Set_Rows(QVector<QVariantMap>());
	Trends_Model->Set_Rows(QVector<QVariantMap>());
	Weight_Series_Model->Set_Rows(QVector<QVariantMap>());
	Change_Map_Model->Set_Rows(QVector<QVariantMap>());
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Program_Stats::Clear_Error()
{
	Error_Key_Text.clear();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Program_Stats::Set_Error()
{
	Clear_Stats();
	Error_Key_Text = "undefined_error";
	qCInfo(PROGRAM_STATS_VM_Log).noquote() << "stats refresh failed";
	emit changed();
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Program_Stats::Has_Stats() const
{
	return !Stats.Program_Id.isEmpty();
}
//-------------------------------------------------------------------------------------------------------------
QVariantMap Avm_Program_Stats::dashboard() const
{
	return Dashboard;
}
//-------------------------------------------------------------------------------------------------------------
AList_Model *Avm_Program_Stats::nutrition_summary() const
{
	return Nutrition_Summary_Model;
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Program_Stats::Set_Program_Clock(const QString &in_program_id, int in_paused_days, const QString &in_paused_on)
{
	Clock_Program_Id = in_program_id;
	Clock_Paused_Days = qMax(0, in_paused_days);
	Clock_Paused_On = in_paused_on;
}
//-------------------------------------------------------------------------------------------------------------
double Avm_Program_Stats::adherencePercent() const
{
	return Stats.Adherence_Percent;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Program_Stats::Has_Adherence_Percent() const
{
	return Stats.Has_Adherence_Percent;
}
//-------------------------------------------------------------------------------------------------------------
int Avm_Program_Stats::plannedSessions() const
{
	return Stats.Planned_Sessions;
}
//-------------------------------------------------------------------------------------------------------------
int Avm_Program_Stats::completedSessions() const
{
	return Stats.Completed_Sessions;
}
//-------------------------------------------------------------------------------------------------------------
double Avm_Program_Stats::tonnageKg() const
{
	return Stats.Tonnage_Kg;
}
//-------------------------------------------------------------------------------------------------------------
int Avm_Program_Stats::setsCount() const
{
	return Stats.Sets_Count;
}
//-------------------------------------------------------------------------------------------------------------
int Avm_Program_Stats::repsCount() const
{
	return Stats.Reps_Count;
}
//-------------------------------------------------------------------------------------------------------------
double Avm_Program_Stats::durationMinutes() const
{
	return Stats.Duration_Minutes;
}
//-------------------------------------------------------------------------------------------------------------
double Avm_Program_Stats::avgRpe() const
{
	return Stats.Average_Rpe;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Program_Stats::Has_Average_Rpe() const
{
	return Stats.Has_Average_Rpe;
}
//-------------------------------------------------------------------------------------------------------------
int Avm_Program_Stats::currentStreakWeeks() const
{
	return Stats.Current_Streak_Weeks;
}
//-------------------------------------------------------------------------------------------------------------
int Avm_Program_Stats::longestStreakWeeks() const
{
	return Stats.Longest_Streak_Weeks;
}
//-------------------------------------------------------------------------------------------------------------
double Avm_Program_Stats::bodyWeightDeltaKg() const
{
	return Stats.Body_Weight_Delta_Kg;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Program_Stats::Has_Body_Weight_Delta_Kg() const
{
	return Stats.Has_Body_Weight_Delta_Kg;
}
//-------------------------------------------------------------------------------------------------------------
double Avm_Program_Stats::ruleAdherencePercent() const
{
	return Stats.Rule_Adherence_Percent;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Program_Stats::Has_Rule_Adherence_Percent() const
{
	return Stats.Has_Rule_Adherence_Percent;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Program_Stats::goalAchieved() const
{
	return Stats.Goal_Achieved;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Program_Stats::dataRangeFrom() const
{
	return Stats.Data_Range_From;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Program_Stats::dataRangeTo() const
{
	return Stats.Data_Range_To;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Program_Stats::errorKey() const
{
	return Error_Key_Text;
}
//-------------------------------------------------------------------------------------------------------------
const SProgram_Stats &Avm_Program_Stats::program_stats() const
{
	return Stats;
}
//-------------------------------------------------------------------------------------------------------------
AList_Model *Avm_Program_Stats::weeks() const
{
	return Weeks;
}
//-------------------------------------------------------------------------------------------------------------
AList_Model *Avm_Program_Stats::rule_stats() const
{
	return Rule_Stats_Model;
}
//-------------------------------------------------------------------------------------------------------------
AList_Model *Avm_Program_Stats::exercise_strength() const
{
	return Exercise_Strength_Model;
}
//-------------------------------------------------------------------------------------------------------------
AList_Model *Avm_Program_Stats::goal_progress() const
{
	return Goal_Progress_Model;
}
//-------------------------------------------------------------------------------------------------------------
AList_Model *Avm_Program_Stats::measurements() const
{
	return Measurements_Model;
}
//-------------------------------------------------------------------------------------------------------------
AList_Model *Avm_Program_Stats::trends() const
{
	return Trends_Model;
}
//-------------------------------------------------------------------------------------------------------------
AList_Model *Avm_Program_Stats::weight_series() const
{
	return Weight_Series_Model;
}
//-------------------------------------------------------------------------------------------------------------
AList_Model *Avm_Program_Stats::change_map() const
{
	return Change_Map_Model;
}
//-------------------------------------------------------------------------------------------------------------
