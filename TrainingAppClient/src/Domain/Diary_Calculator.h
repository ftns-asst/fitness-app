#pragma once

#include "Domain/Rule_Engine.h"
#include <QDate>

//-------------------------------------------------------------------------------------------------------------
struct SDiary_Value
{
	QString Rule_Id;
	double Value = 0;
	bool Has_Value = false;
	bool Applicable = false;
	ECheckin_State Verdict = ECheckin_State::Unknown;
};
//-------------------------------------------------------------------------------------------------------------
struct SDay_Log
{
	QString Date;
	QString Day_Id;
	QString Day_Title;
	int Weekday = 0;
	bool Is_Training = false;
	bool Editable = false;
	double Weight = 0;
	bool Has_Weight = false;
	double Average_7 = 0;
	bool Has_Average_7 = false;
	bool Workout_Done = false;
	QString Note;
	QVector<SDiary_Value> Values;
};
//-------------------------------------------------------------------------------------------------------------
struct SDiary_Summary
{
	QString Rule_Id;
	double Average = 0;
	bool Has_Average = false;
	int Days_In_Norm = 0;
	int Days_Off = 0;
};
//-------------------------------------------------------------------------------------------------------------
// Чистый расчёт дневника из фактов. Будущие факты не участвуют в итогах.
class ADiary_Calculator
{
public:
	static bool Build(const SProgram &in_program, const QVector<SProgram_Day> &in_days,
	                  const QVector<SWeek_Slot> &in_slots, const QVector<SProgram_Rule> &in_rules,
	                  const QVector<SRule_Checkin> &in_checkins, const QVector<SBody_Measurement> &in_measurements,
	                  const QVector<SWorkout_Session> &in_sessions, const QVector<SDay_Note> &in_notes,
	                  const QDate &in_today, QVector<SDay_Log> &out_days, QVector<SDiary_Summary> &out_summaries);
	static bool Applies(const SProgram_Rule &in_rule, const SDay_Log &in_day);
	static QString Target_Text(const SProgram_Rule &in_rule);
};
//-------------------------------------------------------------------------------------------------------------
