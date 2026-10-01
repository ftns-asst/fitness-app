#include "Presentation/Program_Stats_VM.h"

#include "Data/Program/Measurement_Repository.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Data/Program/Stats_Repository.h"
#include "Data/Program/Workout_Repository.h"
#include "Presentation/Program_Rows.h"

#include <QDate>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(PROGRAM_STATS_VM_Log, "trainingapp.program.stats_vm")

//----------------------------------------------------------------------------
Avm_Program_Stats::Avm_Program_Stats(AProgram_Repository *in_program_repository,
                                     AWorkout_Repository *in_workout_repository, ARule_Repository *in_rule_repository,
                                     AMeasurement_Repository *in_measurement_repository,
                                     AStats_Repository *in_stats_repository, QObject *in_parent)
    : QObject(in_parent), Program_Repository_Instance(in_program_repository),
      Workout_Repository_Instance(in_workout_repository), Rule_Repository_Instance(in_rule_repository),
      Measurement_Repository_Instance(in_measurement_repository), Stats_Repository_Instance(in_stats_repository),
      Weeks(new AList_Model(this)), Rule_Stats_Model(new AList_Model(this)),
      Exercise_Strength_Model(new AList_Model(this)), Goal_Progress_Model(new AList_Model(this)),
      Measurements_Model(new AList_Model(this))
{
	Today_Text = QDate::currentDate().toString("yyyy-MM-dd");

	Weeks->Set_Roles(Program_Rows::Week_Aggregate_Roles());
	Rule_Stats_Model->Set_Roles(Program_Rows::Rule_Stat_Roles());
	Measurements_Model->Set_Roles(Program_Rows::Measurement_Roles());
}
//----------------------------------------------------------------------------
void Avm_Program_Stats::Set_Today(const QString &in_date)
{
	Today_Text = in_date;
}
//----------------------------------------------------------------------------
bool Avm_Program_Stats::Refresh(const QString &in_program_id, const QString &in_date_from, const QString &in_date_to)
{
	SProgress_Facts facts;
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> exercises;
	QVector<SRule_Checkin> checkins;

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
	if (Program_Repository_Instance->Load_Program_Full(in_program_id, facts.Program, facts.Goal_Targets, days,
	                                                   exercises, facts.Schedule) == false)
	{
		Set_Error();

		return false;
	}

	if (Workout_Repository_Instance->Load_Program_Facts(facts.Program.User_Id, in_program_id, in_date_from, in_date_to,
	                                                    facts.Sessions, facts.Sets) == false)
	{
		Set_Error();

		return false;
	}

	if (Rule_Repository_Instance != 0)
	{
		Rule_Repository_Instance->Load_Rules(in_program_id, facts.Rules);
		Rule_Repository_Instance->Load_Program_Checkins_Range(in_program_id, in_date_from, in_date_to, checkins);
		facts.Checkins = checkins;
	}

	if (Measurement_Repository_Instance != 0)
		Measurement_Repository_Instance->Load_Range(facts.Program.User_Id, in_date_from, in_date_to,
		                                            facts.Measurements);

	if (Calculator.Compute_Program_Stats(facts, Stats) == false)
	{
		Set_Error();

		return false;
	}

	Weeks->Set_Rows(Program_Rows::Build_Week_Aggregates(Stats.Weeks));
	Rule_Stats_Model->Set_Rows(Program_Rows::Build_Rule_Stats(Stats.Rule_Stats, QHash<QString, int>()));
	Exercise_Strength_Model->Set_Roles(Program_Rows::Exercise_Strength_Roles());
	Exercise_Strength_Model->Set_Rows(Program_Rows::Build_Exercise_Strength(Stats.Exercise_Strength));
	Goal_Progress_Model->Set_Roles(Program_Rows::Goal_Progress_Roles());
	Goal_Progress_Model->Set_Rows(Program_Rows::Build_Goal_Progress(Stats.Goal_Progress));
	Measurements_Model->Set_Rows(Program_Rows::Build_Measurements(facts.Measurements));

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
//----------------------------------------------------------------------------
void Avm_Program_Stats::Clear_Stats()
{
	Stats = SProgram_Stats();

	Weeks->Set_Rows(QVector<QVariantMap>());
	Rule_Stats_Model->Set_Rows(QVector<QVariantMap>());
	Exercise_Strength_Model->Set_Rows(QVector<QVariantMap>());
	Goal_Progress_Model->Set_Rows(QVector<QVariantMap>());
	Measurements_Model->Set_Rows(QVector<QVariantMap>());
}
//----------------------------------------------------------------------------
void Avm_Program_Stats::Clear_Error()
{
	Error_Key_Text.clear();
}
//----------------------------------------------------------------------------
void Avm_Program_Stats::Set_Error()
{
	Error_Key_Text = "undefined_error";
	qCInfo(PROGRAM_STATS_VM_Log).noquote() << "stats refresh failed";
}
//----------------------------------------------------------------------------
bool Avm_Program_Stats::Has_Stats() const
{
	return Stats.Program_Id.isEmpty() == false;
}
//----------------------------------------------------------------------------
double Avm_Program_Stats::adherencePercent() const
{
	return Stats.Adherence_Percent;
}
//----------------------------------------------------------------------------
bool Avm_Program_Stats::Has_Adherence_Percent() const
{
	return Stats.Has_Adherence_Percent;
}
//----------------------------------------------------------------------------
int Avm_Program_Stats::plannedSessions() const
{
	return Stats.Planned_Sessions;
}
//----------------------------------------------------------------------------
int Avm_Program_Stats::completedSessions() const
{
	return Stats.Completed_Sessions;
}
//----------------------------------------------------------------------------
double Avm_Program_Stats::tonnageKg() const
{
	return Stats.Tonnage_Kg;
}
//----------------------------------------------------------------------------
int Avm_Program_Stats::setsCount() const
{
	return Stats.Sets_Count;
}
//----------------------------------------------------------------------------
int Avm_Program_Stats::repsCount() const
{
	return Stats.Reps_Count;
}
//----------------------------------------------------------------------------
double Avm_Program_Stats::durationMinutes() const
{
	return Stats.Duration_Minutes;
}
//----------------------------------------------------------------------------
double Avm_Program_Stats::avgRpe() const
{
	return Stats.Average_Rpe;
}
//----------------------------------------------------------------------------
bool Avm_Program_Stats::Has_Average_Rpe() const
{
	return Stats.Has_Average_Rpe;
}
//----------------------------------------------------------------------------
int Avm_Program_Stats::currentStreakWeeks() const
{
	return Stats.Current_Streak_Weeks;
}
//----------------------------------------------------------------------------
int Avm_Program_Stats::longestStreakWeeks() const
{
	return Stats.Longest_Streak_Weeks;
}
//----------------------------------------------------------------------------
double Avm_Program_Stats::bodyWeightDeltaKg() const
{
	return Stats.Body_Weight_Delta_Kg;
}
//----------------------------------------------------------------------------
bool Avm_Program_Stats::Has_Body_Weight_Delta_Kg() const
{
	return Stats.Has_Body_Weight_Delta_Kg;
}
//----------------------------------------------------------------------------
double Avm_Program_Stats::ruleAdherencePercent() const
{
	return Stats.Rule_Adherence_Percent;
}
//----------------------------------------------------------------------------
bool Avm_Program_Stats::Has_Rule_Adherence_Percent() const
{
	return Stats.Has_Rule_Adherence_Percent;
}
//----------------------------------------------------------------------------
bool Avm_Program_Stats::goalAchieved() const
{
	return Stats.Goal_Achieved;
}
//----------------------------------------------------------------------------
QString Avm_Program_Stats::dataRangeFrom() const
{
	return Stats.Data_Range_From;
}
//----------------------------------------------------------------------------
QString Avm_Program_Stats::dataRangeTo() const
{
	return Stats.Data_Range_To;
}
//----------------------------------------------------------------------------
QString Avm_Program_Stats::errorKey() const
{
	return Error_Key_Text;
}
//----------------------------------------------------------------------------
const SProgram_Stats &Avm_Program_Stats::program_stats() const
{
	return Stats;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program_Stats::weeks() const
{
	return Weeks;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program_Stats::rule_stats() const
{
	return Rule_Stats_Model;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program_Stats::exercise_strength() const
{
	return Exercise_Strength_Model;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program_Stats::goal_progress() const
{
	return Goal_Progress_Model;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program_Stats::measurements() const
{
	return Measurements_Model;
}
//----------------------------------------------------------------------------
