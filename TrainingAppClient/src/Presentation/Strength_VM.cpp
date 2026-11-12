#include "Presentation/Strength_VM.h"

#include <QtMath>

//-------------------------------------------------------------------------------------------------------------
Avm_Strength::Avm_Strength(AProgram_Repository *in_program_repository, AWorkout_Repository *in_workout_repository,
                           AExercises_Repository *in_exercises_repository, QObject *in_parent)
    : QObject(in_parent), Program_Repository_Instance(in_program_repository),
      Workout_Repository_Instance(in_workout_repository), Exercises_Repository_Instance(in_exercises_repository),
      Exercises_Model(new AList_Model(this)), Splits_Model(new AList_Model(this)),
      Status_Rows_Model(new AList_Model(this)), Status_Plot_Model(new AList_Model(this))
{
	QStringList common = {"exerciseId",   "exerciseTitle",   "splitId",     "splitTitle",  "status",
	                      "symbol",       "statusText",      "reason",      "reasonText",  "previousWeightKg",
	                      "previousReps", "currentWeightKg", "currentReps", "currentSets", "deltaKg",
	                      "deltaPercent", "repsMin",         "repsMax",     "rangeSource", "rangeNote"};
	Exercises_Model->Set_Roles(common + QStringList{"startWeightKg", "startReps", "startSets", "startDeltaKg",
	                                                "startDeltaPercent", "better", "worse", "evaluated", "indexPercent",
	                                                "nextGoal"});
	Status_Rows_Model->Set_Roles(common + QStringList{"sessionId", "date"});
	Splits_Model->Set_Roles({"splitId", "splitTitle", "better", "worse", "evaluated", "percent"});
	Status_Plot_Model->Set_Roles(
	    {"sessionId", "date", "splitId", "splitTitle", "better", "same", "start", "attention", "worse", "total"});
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Strength::reload(const QString &in_user_id, const QString &in_program_id)
{
	Clear();
	if (Program_Repository_Instance == 0 || Workout_Repository_Instance == 0 || in_user_id.isEmpty())
		return Fail("strength.unavailable");

	QString program_id = in_program_id;
	if (program_id.isEmpty())
	{
		QVector<SProgram> programs;
		if (!Program_Repository_Instance->Load_Programs(in_user_id, programs))
			return Fail("strength.loadFailed");
		for (const SProgram &program : programs)
			if (program.State == EProgram_State::Active)
			{
				program_id = program.ID;
				break;
			}
		if (program_id.isEmpty())
		{
			emit changed();
			return true;
		}
	}

	SProgram program;
	QVector<SGoal_Target> targets;
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> prescriptions;
	QVector<SWeek_Slot> week_slots;
	if (!Program_Repository_Instance->Load_Program_Full(program_id, program, targets, days, prescriptions, week_slots))
		return Fail("strength.loadFailed");
	if (program.User_Id != in_user_id)
		return Fail("strength.foreignProgram");

	QVector<SWorkout_Session> sessions;
	QVector<SSet_Log> sets;
	if (!Workout_Repository_Instance->Load_Program_Facts(in_user_id, program_id, QString(), QString(), sessions, sets))
		return Fail("strength.loadFailed");
	if (program.Rep_Tolerance < 0 || program.Rep_Tolerance > 20)
		return Fail("strength.invalidTolerance");

	Program = program;
	Report = AStrength_Calculator::Compute(program, days, prescriptions, sessions, sets);
	QVector<QString> exercise_ids;
	for (const SStrength_Exercise &exercise : Report.Exercises)
		exercise_ids.append(exercise.Exercise_Id);
	if (!exercise_ids.isEmpty() && !Program_Repository_Instance->Load_Exercise_Titles(exercise_ids, Exercise_Titles))
		return Fail("strength.loadFailed");
	if (Exercises_Repository_Instance != 0)
	{
		QVector<SExercises_Item> items;
		if (Exercises_Repository_Instance->Load_Offline_List(items))
			for (const SExercises_Item &item : items)
				Exercise_Titles.insert(item.ID, item.Name);
	}
	QVector<QVariantMap> exercise_rows;
	QVector<QVariantMap> split_rows;
	QVector<QVariantMap> history_rows;
	QVector<QVariantMap> plot_rows;
	for (const SStrength_Exercise &exercise : Report.Exercises)
		exercise_rows << Exercise_Row(exercise);
	for (const SStrength_Split &split : Report.Splits)
	{
		QVariantMap row = Index_Row(split.Index);
		row["splitId"] = split.ID;
		row["splitTitle"] = Split_Title(split.ID);
		split_rows << row;
	}
	for (const SStrength_Observation &observation : Report.History)
	{
		history_rows << Observation_Row(observation);
		if (plot_rows.isEmpty() || plot_rows.last().value("sessionId").toString() != observation.Session_Id)
			plot_rows << QVariantMap{{"sessionId", observation.Session_Id},
			                         {"date", observation.Date},
			                         {"splitId", observation.Split_Id},
			                         {"splitTitle", Split_Title(observation.Split_Id)},
			                         {"better", 0},
			                         {"same", 0},
			                         {"start", 0},
			                         {"attention", 0},
			                         {"worse", 0},
			                         {"total", 0}};
		QVariantMap &plot_row = plot_rows.last();
		plot_row[observation.Trend.Status] = plot_row.value(observation.Trend.Status).toInt() + 1;
		plot_row["total"] = plot_row.value("total").toInt() + 1;
	}
	Exercises_Model->Set_Rows(exercise_rows);
	Splits_Model->Set_Rows(split_rows);
	Status_Rows_Model->Set_Rows(history_rows);
	Status_Plot_Model->Set_Rows(plot_rows);
	emit changed();

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Strength::Has_Data() const
{
	return !Report.History.isEmpty();
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Strength::programId() const
{
	return Program.ID;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Strength::programTitle() const
{
	return Program.Title;
}
//-------------------------------------------------------------------------------------------------------------
int Avm_Strength::repTolerance() const
{
	return Program.Rep_Tolerance;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Strength::errorKey() const
{
	return Error_Key_Text;
}
//-------------------------------------------------------------------------------------------------------------
QVariantMap Avm_Strength::programIndex() const
{
	return Index_Row(Report.Index);
}
//-------------------------------------------------------------------------------------------------------------
AList_Model *Avm_Strength::exercises() const
{
	return Exercises_Model;
}
//-------------------------------------------------------------------------------------------------------------
AList_Model *Avm_Strength::splits() const
{
	return Splits_Model;
}
//-------------------------------------------------------------------------------------------------------------
AList_Model *Avm_Strength::status_rows() const
{
	return Status_Rows_Model;
}
//-------------------------------------------------------------------------------------------------------------
AList_Model *Avm_Strength::status_plot() const
{
	return Status_Plot_Model;
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Strength::Clear()
{
	Program = SProgram();
	Report = SStrength_Report();
	Error_Key_Text.clear();
	Exercise_Titles.clear();
	Exercises_Model->Set_Rows({});
	Splits_Model->Set_Rows({});
	Status_Rows_Model->Set_Rows({});
	Status_Plot_Model->Set_Rows({});
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Strength::Fail(const QString &in_error_key)
{
	Clear();
	Error_Key_Text = in_error_key;
	emit changed();

	return false;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Strength::Exercise_Title(const QString &in_id) const
{
	return Exercise_Titles.value(in_id, in_id);
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Strength::Split_Title(const QString &in_id) const
{
	for (const SStrength_Split &split : Report.Splits)
		if (split.ID == in_id && !split.Title.isEmpty())
			return split.Title;

	return in_id.isEmpty() ? tr("Без сплита") : tr("Сплит %1").arg(in_id);
}
//-------------------------------------------------------------------------------------------------------------
QVariantMap Avm_Strength::Observation_Row(const SStrength_Observation &in_observation) const
{
	QVariantMap row;
	row["exerciseId"] = in_observation.Exercise_Id;
	row["exerciseTitle"] = Exercise_Title(in_observation.Exercise_Id);
	row["splitId"] = in_observation.Split_Id;
	row["splitTitle"] = Split_Title(in_observation.Split_Id);
	row["sessionId"] = in_observation.Session_Id;
	row["date"] = in_observation.Date;
	row["repsMin"] = in_observation.Reps_Min > 0 ? QVariant(in_observation.Reps_Min) : QVariant();
	row["repsMax"] = in_observation.Reps_Max > 0 ? QVariant(in_observation.Reps_Max) : QVariant();
	row["rangeSource"] = in_observation.Range_Source;
	QString note;
	if (in_observation.Range_Source == "currentPlan")
		note = tr("Диапазон из текущего плана: в старой записи не сохранён");
	else if (in_observation.Range_Source == "legacyTarget")
		note = tr("Старый целевой повтор; полный диапазон не сохранён");
	else if (in_observation.Range_Source != "snapshot")
		note = tr("Диапазон неизвестен: оценка только по весу и повторам");
	row["rangeNote"] = note;
	Add_Workload(row, "previous", in_observation.Previous);
	Add_Workload(row, "current", in_observation.Current);
	Add_Trend(row, in_observation.Trend);

	return row;
}
//-------------------------------------------------------------------------------------------------------------
QVariantMap Avm_Strength::Exercise_Row(const SStrength_Exercise &in_exercise) const
{
	SStrength_Observation observation;
	observation.Exercise_Id = in_exercise.Exercise_Id;
	observation.Split_Id = in_exercise.Split_Id;
	observation.Previous = in_exercise.Previous;
	observation.Current = in_exercise.Current;
	observation.Trend = in_exercise.Trend;
	for (int i = Report.History.size() - 1; i >= 0; --i)
		if (Report.History.at(i).Exercise_Id == in_exercise.Exercise_Id &&
		    Report.History.at(i).Split_Id == in_exercise.Split_Id)
		{
			observation = Report.History.at(i);
			break;
		}
	QVariantMap row = Observation_Row(observation);
	Add_Workload(row, "start", in_exercise.Start);
	row["startDeltaKg"] =
	    in_exercise.Start.Has_Data ? QVariant(in_exercise.Current.Weight_Kg - in_exercise.Start.Weight_Kg) : QVariant();
	row["startDeltaPercent"] = in_exercise.Start.Has_Data && in_exercise.Start.Weight_Kg > 0
	                               ? QVariant(100 * (in_exercise.Current.Weight_Kg - in_exercise.Start.Weight_Kg) /
	                                          in_exercise.Start.Weight_Kg)
	                               : QVariant();
	row["better"] = in_exercise.Index.Better;
	row["worse"] = in_exercise.Index.Worse;
	row["evaluated"] = in_exercise.Index.Evaluated;
	row["indexPercent"] = in_exercise.Index.Evaluated > 0 ? QVariant(in_exercise.Index.Percent) : QVariant();
	QString goal;
	if (in_exercise.Trend.Reason == "aboveRange")
	{
		goal = tr("Пора добавить вес");
		const SProgression_Rule &progression = in_exercise.Progression;
		if (in_exercise.Has_Progression && progression.Type != EProgression_Type::Manual &&
		    progression.Has_Increment_Kg && qIsFinite(progression.Increment_Kg) && progression.Increment_Kg > 0)
			goal +=
			    tr(" · следующий вес %1 кг").arg(in_exercise.Current.Weight_Kg + progression.Increment_Kg, 0, 'g', 6);
	}
	row["nextGoal"] = goal;

	return row;
}
//-------------------------------------------------------------------------------------------------------------
QVariantMap Avm_Strength::Index_Row(const SProgress_Index &in_index)
{
	return {{"better", in_index.Better},
	        {"worse", in_index.Worse},
	        {"evaluated", in_index.Evaluated},
	        {"percent", in_index.Evaluated > 0 ? QVariant(in_index.Percent) : QVariant()}};
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Strength::Add_Trend(QVariantMap &io_row, const SExercise_Trend &in_trend)
{
	io_row["status"] = in_trend.Status;
	io_row["symbol"] = Symbol(in_trend.Status);
	io_row["statusText"] = Status_Text(in_trend.Status);
	io_row["reason"] = in_trend.Reason;
	io_row["reasonText"] = Reason_Text(in_trend.Reason);
	io_row["deltaKg"] = in_trend.Has_Delta ? QVariant(in_trend.Delta_Kg) : QVariant();
	io_row["deltaPercent"] = in_trend.Has_Delta_Percent ? QVariant(in_trend.Delta_Percent) : QVariant();
}
//-------------------------------------------------------------------------------------------------------------
void Avm_Strength::Add_Workload(QVariantMap &io_row, const QString &in_prefix, const SStrength_Workload &in_workload)
{
	io_row[in_prefix + "WeightKg"] = in_workload.Has_Data ? QVariant(in_workload.Weight_Kg) : QVariant();
	io_row[in_prefix + "Reps"] = in_workload.Has_Data ? QVariant(in_workload.Average_Reps) : QVariant();
	io_row[in_prefix + "Sets"] = in_workload.Has_Data ? QVariant(in_workload.Set_Count) : QVariant();
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Strength::Status_Text(const QString &in_status)
{
	if (in_status == "better")
		return tr("Лучше");
	if (in_status == "worse")
		return tr("Хуже");
	if (in_status == "attention")
		return tr("Внимание");
	if (in_status == "start")
		return tr("Старт");
	if (in_status == "same")
		return tr("Так же");

	return tr("Нет данных");
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Strength::Reason_Text(const QString &in_reason)
{
	if (in_reason == "firstRecord")
		return tr("Первая завершённая запись");
	if (in_reason == "belowRange")
		return tr("Повторы ниже диапазона");
	if (in_reason == "aboveRange")
		return tr("Повторы выше диапазона — пора добавить вес");
	if (in_reason == "weightInTolerance")
		return tr("Вес вырос, снижение повторов в допуске");
	if (in_reason == "repDrop")
		return tr("При росте веса снижение повторов больше допуска");
	if (in_reason == "lighterMoreReps")
		return tr("Вес снизился, повторов больше");
	if (in_reason == "lessWeight")
		return tr("Вес снизился без роста повторов");
	if (in_reason == "moreReps")
		return tr("При том же весе повторов больше");
	if (in_reason == "fewerReps")
		return tr("При том же весе повторов меньше");
	if (in_reason == "unchanged")
		return tr("Вес и средние повторы не изменились");

	return tr("Нет выполненных рабочих подходов");
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Strength::Symbol(const QString &in_status)
{
	if (in_status == "better")
		return QString::fromUtf8("▲");
	if (in_status == "worse")
		return QString::fromUtf8("▼");
	if (in_status == "attention")
		return QString::fromUtf8("◆");
	if (in_status == "same" || in_status == "start")
		return QString::fromUtf8("●");

	return QString::fromUtf8("—");
}
//-------------------------------------------------------------------------------------------------------------
