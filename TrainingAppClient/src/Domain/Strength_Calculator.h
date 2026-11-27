#pragma once

#include "Domain/Program_Model.h"

#include <QHash>
#include <QVector>

using namespace Program_Model;

//-------------------------------------------------------------------------------------------------------------
struct SStrength_Workload
{
	bool Has_Data = false;
	double Weight_Kg = 0;
	double Average_Reps = 0;
	int Set_Count = 0;
};
//-------------------------------------------------------------------------------------------------------------
struct SExercise_Trend
{
	QString Status = "noData";
	QString Reason = "noWorkingSets";
	bool Is_Evaluated = false;
	bool Has_Delta = false;
	double Delta_Kg = 0;
	bool Has_Delta_Percent = false;
	double Delta_Percent = 0;
};
//-------------------------------------------------------------------------------------------------------------
struct SProgress_Index
{
	int Better = 0;
	int Worse = 0;
	int Evaluated = 0;
	double Percent = 0;
};
//-------------------------------------------------------------------------------------------------------------
struct SStrength_Exercise
{
	QString Split_Id;
	QString Exercise_Id;
	int Reps_Min = 0;
	int Reps_Max = 0;
	bool Has_Progression = false;
	SProgression_Rule Progression;
	SStrength_Workload Start;
	SStrength_Workload Previous;
	SStrength_Workload Current;
	SExercise_Trend Trend;
	SProgress_Index Index;
};
//-------------------------------------------------------------------------------------------------------------
struct SStrength_Observation
{
	QString Session_Id;
	QString Date;
	QString Split_Id;
	QString Exercise_Id;
	int Reps_Min = 0;
	int Reps_Max = 0;
	QString Range_Source = "unavailable";
	SStrength_Workload Previous;
	SStrength_Workload Current;
	SExercise_Trend Trend;
};
//-------------------------------------------------------------------------------------------------------------
struct SStrength_Split
{
	QString ID;
	QString Title;
	SProgress_Index Index;
};
//-------------------------------------------------------------------------------------------------------------
struct SStrength_Report
{
	QVector<SStrength_Exercise> Exercises;
	QVector<SStrength_Observation> History;
	QVector<SStrength_Split> Splits;
	SProgress_Index Index;
};
//-------------------------------------------------------------------------------------------------------------
// T6: derived values only, no storage or clock access. Missing facts never become
// zero-valued workouts. One valid working set is sufficient (FR-T28).
class AStrength_Calculator
{
public:
	static SStrength_Workload Summarize(const QVector<SSet_Log> &in_sets);
	// First valid observation is Start, then range violations take precedence.
	// Missing range (0/0) permits relative comparison but no range judgement.
	static SExercise_Trend Evaluate_Exercise_Trend(const SStrength_Workload &in_previous,
	                                               const SStrength_Workload &in_current, int in_reps_min,
	                                               int in_reps_max, int in_tolerance);
	// Start/noData are not comparisons. Same and attention enter the denominator.
	static void Accumulate(SProgress_Index &io_index, const QString &in_status);
	static SStrength_Report Compute(const SProgram &in_program, const QVector<SProgram_Day> &in_days,
	                                const QVector<SDay_Exercise> &in_exercises,
	                                const QVector<SWorkout_Session> &in_sessions, const QVector<SSet_Log> &in_sets);

private:
	static int Find_Exercise(const SStrength_Report &in_report, const QString &in_split_id,
	                         const QString &in_exercise_id);
	static int Ensure_Split(SStrength_Report &io_report, const QString &in_split_id);
	static void Resolve_Range(const QVector<SSet_Log> &in_sets, const SStrength_Exercise &in_exercise,
	                          SStrength_Observation &out_observation);
};
//-------------------------------------------------------------------------------------------------------------
