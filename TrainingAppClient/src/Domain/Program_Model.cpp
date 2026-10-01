#include "Domain/Program_Model.h"

#include <QDate>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>

namespace Program_Model
{
	static const char *const Program_Source_Kind_Texts[] = {"template", "copy", "scratch", "matched"};
	static const char *const Program_Goal_Texts[] = {"strength", "muscle", "fitness", "fatLoss", "endurance"};
	static const char *const Program_Level_Texts[] = {"beginner", "intermediate", "advanced"};
	static const char *const Schedule_Kind_Texts[] = {"weekday", "rotation"};
	static const char *const Program_State_Texts[] = {"draft", "active", "paused", "completed", "cancelled", "archived"};
	static const char *const Term_Mode_Texts[] = {"fixedWeeks", "openEnded"};
	static const char *const Sync_State_Texts[] = {"synced", "local", "syncing", "offline"};
	static const char *const Goal_Metric_Texts[] = {"bodyWeightKg",  "workingWeightKg", "bodyFatPercent",
													"measurementCm", "workoutsPerWeek", "tonnagePerWeekKg",
													"repsAtWeight"};
	static const char *const Goal_Unit_Texts[] = {"kg", "percent", "cm", "count", "timesPerWeek"};
	static const char *const Measurement_Key_Texts[] = {"chest", "waist", "hip", "arm", "thigh"};
	static const char *const Goal_Direction_Texts[] = {"increase", "decrease", "maintain"};
	static const char *const Rule_Check_Source_Texts[] = {"selfReport",           "computedFromSets", "computedFromProfile",
														  "computedFromSessions", "computedFromLog",  "mixed"};
	static const char *const Slot_Kind_Texts[] = {"workout", "rest", "optional", "cardio", "mobility"};
	static const char *const Day_Kind_Texts[] = {"workout", "rest", "cardio", "mobility"};
	static const char *const Goal_Target_State_Texts[] = {"active", "achieved", "dropped"};
	static const char *const Rule_Category_Texts[] = {"nutrition", "training", "recovery", "lifestyle", "technique"};
	static const char *const Revision_Reason_Texts[] = {"goal",  "term",         "schedule", "days",
														"rules", "exercisePlan", "state"};
	static const char *const Workout_State_Texts[] = {"draft", "completed", "cancelled"};
	static const char *const Deviation_Texts[] = {"asPlanned", "partial", "substituted", "extraDay", "movedDay"};
	static const char *const Checkin_Source_Texts[] = {"manual", "computed", "imported"};
	static const char *const Checkin_State_Texts[] = {"done", "failed", "skipped", "partial", "unknown"};
	static const char *const Rule_Display_Unit_Texts[] = {"g", "gPerKg", "kcal", "ml", "hours", "count", "rpe", "units"};
	static const char *const Rule_Enforcement_Texts[] = {"soft", "hard"};
	static const char *const Rule_Applies_To_Texts[] = {"allDays", "trainingDays", "restDays", "customDays"};
	static const char *const Rule_Periodicity_Texts[] = {"perDay", "perWeek", "perWorkout", "once"};
	static const char *const Rule_Metric_Texts[] = {"proteinGrams", "caloriesKcal", "waterMl", "sleepHours",
													"stepCount",    "bodyWeightKg", "maxRpe",  "restHoursBetweenSessions",
													"alcoholUnits", "custom"};
	static const char *const Weight_Mode_Texts[] = {"fixed", "percentOf1RM", "byFeel", "fromLastSession", "bodyWeight"};
	static const char *const Progression_Type_Texts[] = {"linearWeight", "doubleProgression", "percentPerWeek", "manual"};




	//----------------------------------------------------------------------------
	// STerm
	//----------------------------------------------------------------------------
	QString STerm::Compute_End_Date() const
	{
		QDate start_date;

			   // §3.3: endDate = startDate + weeksTotal × 7 − 1 день; открытый срок —
			   // пустая дата. Продление (Extended_Weeks) в формуле не участвует.
		if (Mode != ETerm_Mode::FixedWeeks || Weeks_Total <= 0)
			return QString();

		start_date = QDate::fromString(Start_Date, "yyyy-MM-dd");

		if (start_date.isValid() == false)
			return QString();

		return start_date.addDays(Weeks_Total * 7 - 1).toString("yyyy-MM-dd");
	}
	//----------------------------------------------------------------------------




	//----------------------------------------------------------------------------
	QString To_String(EProgram_Source_Kind in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)EProgram_Source_Kind::Max)
			return QString();

		return QString::fromLatin1(Program_Source_Kind_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, EProgram_Source_Kind &out_value)
	{
		for (int i = 0; i < (int)EProgram_Source_Kind::Max; ++i)
		{
			if (in_text == QLatin1String(Program_Source_Kind_Texts[i]))
			{
				out_value = (EProgram_Source_Kind)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(EProgram_Goal in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)EProgram_Goal::Max)
			return QString();

		return QString::fromLatin1(Program_Goal_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, EProgram_Goal &out_value)
	{
		for (int i = 0; i < (int)EProgram_Goal::Max; ++i)
		{
			if (in_text == QLatin1String(Program_Goal_Texts[i]))
			{
				out_value = (EProgram_Goal)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(EProgram_Level in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)EProgram_Level::Max)
			return QString();

		return QString::fromLatin1(Program_Level_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, EProgram_Level &out_value)
	{
		for (int i = 0; i < (int)EProgram_Level::Max; ++i)
		{
			if (in_text == QLatin1String(Program_Level_Texts[i]))
			{
				out_value = (EProgram_Level)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(ESchedule_Kind in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)ESchedule_Kind::Max)
			return QString();

		return QString::fromLatin1(Schedule_Kind_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, ESchedule_Kind &out_value)
	{
		for (int i = 0; i < (int)ESchedule_Kind::Max; ++i)
		{
			if (in_text == QLatin1String(Schedule_Kind_Texts[i]))
			{
				out_value = (ESchedule_Kind)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(EProgram_State in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)EProgram_State::Max)
			return QString();

		return QString::fromLatin1(Program_State_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, EProgram_State &out_value)
	{
		for (int i = 0; i < (int)EProgram_State::Max; ++i)
		{
			if (in_text == QLatin1String(Program_State_Texts[i]))
			{
				out_value = (EProgram_State)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(ETerm_Mode in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)ETerm_Mode::Max)
			return QString();

		return QString::fromLatin1(Term_Mode_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, ETerm_Mode &out_value)
	{
		for (int i = 0; i < (int)ETerm_Mode::Max; ++i)
		{
			if (in_text == QLatin1String(Term_Mode_Texts[i]))
			{
				out_value = (ETerm_Mode)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(ESync_State in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)ESync_State::Max)
			return QString();

		return QString::fromLatin1(Sync_State_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, ESync_State &out_value)
	{
		for (int i = 0; i < (int)ESync_State::Max; ++i)
		{
			if (in_text == QLatin1String(Sync_State_Texts[i]))
			{
				out_value = (ESync_State)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(EGoal_Metric in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)EGoal_Metric::Max)
			return QString();

		return QString::fromLatin1(Goal_Metric_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, EGoal_Metric &out_value)
	{
		for (int i = 0; i < (int)EGoal_Metric::Max; ++i)
		{
			if (in_text == QLatin1String(Goal_Metric_Texts[i]))
			{
				out_value = (EGoal_Metric)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(EGoal_Unit in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)EGoal_Unit::Max)
			return QString();

		return QString::fromLatin1(Goal_Unit_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, EGoal_Unit &out_value)
	{
		for (int i = 0; i < (int)EGoal_Unit::Max; ++i)
		{
			if (in_text == QLatin1String(Goal_Unit_Texts[i]))
			{
				out_value = (EGoal_Unit)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(EGoal_Direction in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)EGoal_Direction::Max)
			return QString();

		return QString::fromLatin1(Goal_Direction_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, EGoal_Direction &out_value)
	{
		for (int i = 0; i < (int)EGoal_Direction::Max; ++i)
		{
			if (in_text == QLatin1String(Goal_Direction_Texts[i]))
			{
				out_value = (EGoal_Direction)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(EGoal_Target_State in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)EGoal_Target_State::Max)
			return QString();

		return QString::fromLatin1(Goal_Target_State_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, EGoal_Target_State &out_value)
	{
		for (int i = 0; i < (int)EGoal_Target_State::Max; ++i)
		{
			if (in_text == QLatin1String(Goal_Target_State_Texts[i]))
			{
				out_value = (EGoal_Target_State)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(EMeasurement_Key in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)EMeasurement_Key::Max)
			return QString();

		return QString::fromLatin1(Measurement_Key_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, EMeasurement_Key &out_value)
	{
		for (int i = 0; i < (int)EMeasurement_Key::Max; ++i)
		{
			if (in_text == QLatin1String(Measurement_Key_Texts[i]))
			{
				out_value = (EMeasurement_Key)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(ESlot_Kind in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)ESlot_Kind::Max)
			return QString();

		return QString::fromLatin1(Slot_Kind_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, ESlot_Kind &out_value)
	{
		for (int i = 0; i < (int)ESlot_Kind::Max; ++i)
		{
			if (in_text == QLatin1String(Slot_Kind_Texts[i]))
			{
				out_value = (ESlot_Kind)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(EDay_Kind in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)EDay_Kind::Max)
			return QString();

		return QString::fromLatin1(Day_Kind_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, EDay_Kind &out_value)
	{
		for (int i = 0; i < (int)EDay_Kind::Max; ++i)
		{
			if (in_text == QLatin1String(Day_Kind_Texts[i]))
			{
				out_value = (EDay_Kind)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(EWeight_Mode in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)EWeight_Mode::Max)
			return QString();

		return QString::fromLatin1(Weight_Mode_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, EWeight_Mode &out_value)
	{
		for (int i = 0; i < (int)EWeight_Mode::Max; ++i)
		{
			if (in_text == QLatin1String(Weight_Mode_Texts[i]))
			{
				out_value = (EWeight_Mode)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(EProgression_Type in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)EProgression_Type::Max)
			return QString();

		return QString::fromLatin1(Progression_Type_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, EProgression_Type &out_value)
	{
		for (int i = 0; i < (int)EProgression_Type::Max; ++i)
		{
			if (in_text == QLatin1String(Progression_Type_Texts[i]))
			{
				out_value = (EProgression_Type)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(ERule_Category in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)ERule_Category::Max)
			return QString();

		return QString::fromLatin1(Rule_Category_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, ERule_Category &out_value)
	{
		for (int i = 0; i < (int)ERule_Category::Max; ++i)
		{
			if (in_text == QLatin1String(Rule_Category_Texts[i]))
			{
				out_value = (ERule_Category)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(ERule_Metric in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)ERule_Metric::Max)
			return QString();

		return QString::fromLatin1(Rule_Metric_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, ERule_Metric &out_value)
	{
		for (int i = 0; i < (int)ERule_Metric::Max; ++i)
		{
			if (in_text == QLatin1String(Rule_Metric_Texts[i]))
			{
				out_value = (ERule_Metric)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(ERule_Periodicity in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)ERule_Periodicity::Max)
			return QString();

		return QString::fromLatin1(Rule_Periodicity_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, ERule_Periodicity &out_value)
	{
		for (int i = 0; i < (int)ERule_Periodicity::Max; ++i)
		{
			if (in_text == QLatin1String(Rule_Periodicity_Texts[i]))
			{
				out_value = (ERule_Periodicity)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(ERule_Applies_To in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)ERule_Applies_To::Max)
			return QString();

		return QString::fromLatin1(Rule_Applies_To_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, ERule_Applies_To &out_value)
	{
		for (int i = 0; i < (int)ERule_Applies_To::Max; ++i)
		{
			if (in_text == QLatin1String(Rule_Applies_To_Texts[i]))
			{
				out_value = (ERule_Applies_To)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(ERule_Check_Source in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)ERule_Check_Source::Max)
			return QString();

		return QString::fromLatin1(Rule_Check_Source_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, ERule_Check_Source &out_value)
	{
		for (int i = 0; i < (int)ERule_Check_Source::Max; ++i)
		{
			if (in_text == QLatin1String(Rule_Check_Source_Texts[i]))
			{
				out_value = (ERule_Check_Source)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(ERule_Enforcement in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)ERule_Enforcement::Max)
			return QString();

		return QString::fromLatin1(Rule_Enforcement_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, ERule_Enforcement &out_value)
	{
		for (int i = 0; i < (int)ERule_Enforcement::Max; ++i)
		{
			if (in_text == QLatin1String(Rule_Enforcement_Texts[i]))
			{
				out_value = (ERule_Enforcement)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(ERule_Display_Unit in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)ERule_Display_Unit::Max)
			return QString();

		return QString::fromLatin1(Rule_Display_Unit_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, ERule_Display_Unit &out_value)
	{
		for (int i = 0; i < (int)ERule_Display_Unit::Max; ++i)
		{
			if (in_text == QLatin1String(Rule_Display_Unit_Texts[i]))
			{
				out_value = (ERule_Display_Unit)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(ECheckin_State in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)ECheckin_State::Max)
			return QString();

		return QString::fromLatin1(Checkin_State_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, ECheckin_State &out_value)
	{
		for (int i = 0; i < (int)ECheckin_State::Max; ++i)
		{
			if (in_text == QLatin1String(Checkin_State_Texts[i]))
			{
				out_value = (ECheckin_State)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(ECheckin_Source in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)ECheckin_Source::Max)
			return QString();

		return QString::fromLatin1(Checkin_Source_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, ECheckin_Source &out_value)
	{
		for (int i = 0; i < (int)ECheckin_Source::Max; ++i)
		{
			if (in_text == QLatin1String(Checkin_Source_Texts[i]))
			{
				out_value = (ECheckin_Source)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(EDeviation in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)EDeviation::Max)
			return QString();

		return QString::fromLatin1(Deviation_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, EDeviation &out_value)
	{
		for (int i = 0; i < (int)EDeviation::Max; ++i)
		{
			if (in_text == QLatin1String(Deviation_Texts[i]))
			{
				out_value = (EDeviation)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(EWorkout_State in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)EWorkout_State::Max)
			return QString();

		return QString::fromLatin1(Workout_State_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, EWorkout_State &out_value)
	{
		for (int i = 0; i < (int)EWorkout_State::Max; ++i)
		{
			if (in_text == QLatin1String(Workout_State_Texts[i]))
			{
				out_value = (EWorkout_State)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	QString To_String(ERevision_Reason in_value)
	{
		int index;

		index = (int)in_value;

		if (index < 0 || index >= (int)ERevision_Reason::Max)
			return QString();

		return QString::fromLatin1(Revision_Reason_Texts[index]);
	}
	//----------------------------------------------------------------------------
	bool From_String(const QString &in_text, ERevision_Reason &out_value)
	{
		for (int i = 0; i < (int)ERevision_Reason::Max; ++i)
		{
			if (in_text == QLatin1String(Revision_Reason_Texts[i]))
			{
				out_value = (ERevision_Reason)i;

				return true;
			}
		}

		return false;
	}
	//----------------------------------------------------------------------------
	// JSON-сериализация правила прогрессии (§3.5). Ключи camelCase — как роли
	// mock-контракта; отсутствующий ключ = необязательное поле не
	// задано.
	//----------------------------------------------------------------------------
	QString To_Progression_Json(const SProgression_Rule &in_rule)
	{
		QJsonObject object;

		object.insert("type", To_String(in_rule.Type));

		if (in_rule.Has_Increment_Kg)
			object.insert("incrementKg", in_rule.Increment_Kg);
		if (in_rule.Has_Trigger_Sets)
			object.insert("triggerSets", in_rule.Trigger_Sets);
		if (in_rule.Has_Trigger_Reps_Min)
			object.insert("triggerRepsMin", in_rule.Trigger_Reps_Min);
		if (in_rule.Has_Max_Rpe)
			object.insert("maxRpe", in_rule.Max_Rpe);
		if (in_rule.Has_Deload_Every_Weeks)
			object.insert("deloadEveryWeeks", in_rule.Deload_Every_Weeks);

		return QString::fromUtf8(QJsonDocument(object).toJson(QJsonDocument::Compact));
	}
	//----------------------------------------------------------------------------
	bool Parse_Progression_Json(const QString &in_json, SProgression_Rule &out_rule)
	{
		QJsonDocument document;
		QJsonObject object;
		EProgression_Type type;

		document = QJsonDocument::fromJson(in_json.toUtf8());

		if (document.isObject() == false)
			return false;

		object = document.object();

		if (From_String(object.value("type").toString(), type) == false)
			return false;

		out_rule = SProgression_Rule();
		out_rule.Type = type;

		if (object.contains("incrementKg"))
		{
			out_rule.Increment_Kg = object.value("incrementKg").toDouble();
			out_rule.Has_Increment_Kg = true;
		}

		if (object.contains("triggerSets"))
		{
			out_rule.Trigger_Sets = object.value("triggerSets").toInt();
			out_rule.Has_Trigger_Sets = true;
		}

		if (object.contains("triggerRepsMin"))
		{
			out_rule.Trigger_Reps_Min = object.value("triggerRepsMin").toInt();
			out_rule.Has_Trigger_Reps_Min = true;
		}

		if (object.contains("maxRpe"))
		{
			out_rule.Max_Rpe = object.value("maxRpe").toDouble();
			out_rule.Has_Max_Rpe = true;
		}

		if (object.contains("deloadEveryWeeks"))
		{
			out_rule.Deload_Every_Weeks = object.value("deloadEveryWeeks").toInt();
			out_rule.Has_Deload_Every_Weeks = true;
		}

		return true;
	}
	//----------------------------------------------------------------------------
	// Списки строк (focus, substitution_ids, applies_days) как JSON-массив.
	//----------------------------------------------------------------------------
	QString To_Json_Array(const QStringList &in_list)
	{
		QJsonArray array;

		for (int i = 0; i < in_list.size(); ++i)
			array.append(in_list.at(i));

		return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
	}
	//----------------------------------------------------------------------------
	bool Parse_Json_Array(const QString &in_json, QStringList &out_list)
	{
		QJsonDocument document;
		QJsonArray array;

		document = QJsonDocument::fromJson(in_json.toUtf8());

		if (document.isArray() == false)
			return false;

		out_list.clear();
		array = document.array();

		for (int i = 0; i < array.size(); ++i)
			out_list.append(array.at(i).toString());

		return true;
	}
	//----------------------------------------------------------------------------
}

