#include "Domain/Program_Template_Service.h"

#include <QDate>
#include <QUuid>

#include <QtMath>

Q_LOGGING_CATEGORY(PROGRAM_Template_Log, "trainingapp.program.template")

//-------------------------------------------------------------------------------------------------------------
AProgram_Template_Service::AProgram_Template_Service(AProgram_Template_Repository_Port *in_repository,
                                                     QObject *in_parent)
    : QObject(in_parent), Repository_Instance(in_repository), Error_State(&PROGRAM_Template_Log())
{
}
//-------------------------------------------------------------------------------------------------------------
QString AProgram_Template_Service::Get_Last_Error() const
{
	return Error_State.Get_Last_Error();
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Template_Service::Create_PSMF_Draft(const QString &in_user_id, const SPSMF_Template_Request &in_request,
                                                  QString &out_program_id)
{
	SProgram_Template_Draft draft;

	Error_State.Clear_Last_Error();
	out_program_id.clear();
	if (Repository_Instance == 0)
		return Error_State.Set_Last_Error("Хранилище шаблонов не подключено");
	if (!Validate_Request(in_user_id, in_request))
		return false;

	Build_Draft(in_request, draft);
	if (!Repository_Instance->Insert_Template_Draft(in_user_id, draft))
	{
		QString detail = Repository_Instance->Get_Last_Error();
		if (detail.isEmpty())
			detail = "Не удалось атомарно сохранить программу из шаблона";
		return Error_State.Set_Last_Error(detail);
	}

	out_program_id = draft.Program.ID;
	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Template_Service::Validate_Request(const QString &in_user_id, const SPSMF_Template_Request &in_request)
{
	const Program_Model::SBody_Measurement &measurement = in_request.Initial_Measurement;
	double value = 0;

	if (in_user_id.trimmed().isEmpty() || in_user_id != in_user_id.trimmed() || in_request.User_Id != in_user_id)
		return Error_State.Set_Last_Error("Запрос должен принадлежать текущему пользователю");
	if (!Is_Valid_Date(in_request.Start_Date) ||
	    !QDate::fromString(in_request.Start_Date, Qt::ISODate).addDays(27).isValid())
		return Error_State.Set_Last_Error("Нужна корректная дата старта YYYY-MM-DD");
	if (!Is_Nonnegative_Number(in_request.Calories_Max) || in_request.Calories_Max == 0 ||
	    !Is_Nonnegative_Number(in_request.Protein_Min) || in_request.Protein_Min == 0 ||
	    !Is_Nonnegative_Number(in_request.Carbs_Max) || !Is_Nonnegative_Number(in_request.Fat_Max))
		return Error_State.Set_Last_Error("Нужны четыре конечных пользовательских значения питания");
	if ((in_request.Has_Water_Min && !Is_Nonnegative_Number(in_request.Water_Min_Ml)) ||
	    (in_request.Has_Sleep_Min &&
	     (!Is_Nonnegative_Number(in_request.Sleep_Min_Hours) || in_request.Sleep_Min_Hours > 24)) ||
	    (in_request.Has_Steps_Min &&
	     (!Is_Nonnegative_Number(in_request.Steps_Min) || qFloor(in_request.Steps_Min) != in_request.Steps_Min)))
		return Error_State.Set_Last_Error("Некорректные значения воды, сна или шагов");
	if ((!measurement.User_Id.isEmpty() && measurement.User_Id != in_user_id) ||
	    (!measurement.Date.isEmpty() && measurement.Date != in_request.Start_Date) || !measurement.ID.isEmpty())
		return Error_State.Set_Last_Error("Стартовый замер должен принадлежать пользователю и дате старта");
	if (measurement.Has_Weight_Kg && (!qIsFinite(measurement.Weight_Kg) || measurement.Weight_Kg <= 0))
		return Error_State.Set_Last_Error("Начальный вес должен быть положительным конечным числом");
	for (int i = 0; i < (int)Program_Model::EMeasurement_Key::Max; ++i)
	{
		if (Program_Model::Get_Measurement_Value(measurement, (Program_Model::EMeasurement_Key)i, value) &&
		    (!qIsFinite(value) || value <= 0))
			return Error_State.Set_Last_Error("Начальные обхваты должны быть положительными конечными числами");
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Template_Service::Build_Draft(const SPSMF_Template_Request &in_request,
                                            SProgram_Template_Draft &out_draft)
{
	Program_Model::SProgram &program = out_draft.Program;
	program.ID = New_Id();
	program.User_Id = in_request.User_Id;
	program.Title = "PSMF 4 недели";
	program.Source_Kind = Program_Model::EProgram_Source_Kind::Template;
	program.Source_Template_Id = "psmf-4-weeks-v1";
	program.Goal = Program_Model::EProgram_Goal::FatLoss;
	program.Schedule_Kind = Program_Model::ESchedule_Kind::Rotation;
	program.State = Program_Model::EProgram_State::Draft;
	program.Revision = 1;
	program.Rep_Tolerance = 2;
	program.Term.Start_Date = in_request.Start_Date;
	program.Term.Weeks_Total = 4;
	program.Term.Mode = Program_Model::ETerm_Mode::FixedWeeks;
	program.Note = "Пользовательский пресет. Настройте программу и начните её в редакторе.";

	for (int i = 0; i < 2; ++i)
	{
		Program_Model::SProgram_Day day;
		day.ID = New_Id();
		day.Program_Id = program.ID;
		day.Title = i == 0 ? "Верх" : "Низ + пресс";
		day.Kind = Program_Model::EDay_Kind::Workout;
		day.Order_Index = i;
		day.Source_Day_Id = QString("psmf-4-weeks-v1-day-%1").arg(i + 1);
		if (i == 0)
			day.Focus << "push" << "pull";
		else
			day.Focus << "legs" << "core";
		out_draft.Days.append(day);
	}
	// Идентификаторы уже определены в AExercises_Repository::Base_Exercises(), сетевой каталог не нужен.
	const QStringList exercise_ids = Exercise_Ids();
	Add_Exercise(out_draft, 0, exercise_ids.at(0));
	Add_Exercise(out_draft, 0, exercise_ids.at(1));
	Add_Exercise(out_draft, 1, exercise_ids.at(2));
	Add_Exercise(out_draft, 1, exercise_ids.at(3));
	for (int i = 0; i < 4; ++i)
	{
		Program_Model::SWeek_Slot slot;
		slot.ID = New_Id();
		slot.Program_Id = program.ID;
		slot.Rotation_Index = i + 1;
		slot.Is_Anchor = i == 0;
		if (i % 2 == 0)
		{
			slot.Kind = Program_Model::ESlot_Kind::Workout;
			slot.Program_Day_Id = out_draft.Days.at(i / 2).ID;
		}
		out_draft.Slots.append(slot);
	}

	Add_Rule(out_draft, "psmf_calories", "Калорийность", Program_Model::ERule_Category::Nutrition,
	         Program_Model::ERule_Metric::CaloriesKcal, Program_Model::ERule_Display_Unit::Kcal,
	         in_request.Calories_Max, false);
	Add_Rule(out_draft, "psmf_protein", "Белок", Program_Model::ERule_Category::Nutrition,
	         Program_Model::ERule_Metric::ProteinGrams, Program_Model::ERule_Display_Unit::G, in_request.Protein_Min,
	         true);
	Add_Rule(out_draft, "psmf_carbs", "Углеводы", Program_Model::ERule_Category::Nutrition,
	         Program_Model::ERule_Metric::CarbGrams, Program_Model::ERule_Display_Unit::G, in_request.Carbs_Max, false);
	Add_Rule(out_draft, "psmf_fat", "Жиры", Program_Model::ERule_Category::Nutrition,
	         Program_Model::ERule_Metric::FatGrams, Program_Model::ERule_Display_Unit::G, in_request.Fat_Max, false);
	if (in_request.Has_Water_Min)
		Add_Rule(out_draft, "psmf_water", "Вода", Program_Model::ERule_Category::Nutrition,
		         Program_Model::ERule_Metric::WaterMl, Program_Model::ERule_Display_Unit::Ml, in_request.Water_Min_Ml,
		         true);
	if (in_request.Has_Sleep_Min)
		Add_Rule(out_draft, "psmf_sleep", "Сон", Program_Model::ERule_Category::Recovery,
		         Program_Model::ERule_Metric::SleepHours, Program_Model::ERule_Display_Unit::Hours,
		         in_request.Sleep_Min_Hours, true);
	if (in_request.Has_Steps_Min)
		Add_Rule(out_draft, "psmf_steps", "Шаги", Program_Model::ERule_Category::Lifestyle,
		         Program_Model::ERule_Metric::StepCount, Program_Model::ERule_Display_Unit::Count, in_request.Steps_Min,
		         true);

	out_draft.Has_Initial_Measurement = Program_Model::Has_Any_Measurement_Value(in_request.Initial_Measurement);
	if (out_draft.Has_Initial_Measurement)
	{
		out_draft.Initial_Measurement = in_request.Initial_Measurement;
		out_draft.Initial_Measurement.User_Id = in_request.User_Id;
		out_draft.Initial_Measurement.Date = in_request.Start_Date;
	}
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Template_Service::Add_Exercise(SProgram_Template_Draft &io_draft, int in_day_index,
                                             const QString &in_exercise_id)
{
	Program_Model::SDay_Exercise exercise;
	exercise.ID = New_Id();
	exercise.Program_Day_Id = io_draft.Days.at(in_day_index).ID;
	exercise.Exercise_Id = in_exercise_id;
	exercise.Order_Index = io_draft.Exercises.size() % 2;
	exercise.Sets = 3;
	exercise.Reps_Min = 8;
	exercise.Reps_Max = 12;
	exercise.Weight_Mode = Program_Model::EWeight_Mode::ByFeel;
	exercise.Progression.Type = Program_Model::EProgression_Type::Manual;
	io_draft.Exercises.append(exercise);
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Template_Service::Add_Rule(SProgram_Template_Draft &io_draft, const QString &in_code,
                                         const QString &in_title, Program_Model::ERule_Category in_category,
                                         Program_Model::ERule_Metric in_metric,
                                         Program_Model::ERule_Display_Unit in_unit, double in_value, bool in_is_minimum)
{
	Program_Model::SProgram_Rule rule;
	rule.ID = New_Id();
	rule.Program_Id = io_draft.Program.ID;
	rule.Code = in_code;
	rule.Title = in_title;
	rule.Category = in_category;
	rule.Metric = in_metric;
	rule.Target_Unit = in_unit;
	rule.Display_Unit = in_unit;
	rule.Periodicity = Program_Model::ERule_Periodicity::PerDay;
	rule.Applies_To = Program_Model::ERule_Applies_To::AllDays;
	rule.Active_From = io_draft.Program.Term.Start_Date;
	rule.Active_To = io_draft.Program.Term.Compute_End_Date();
	rule.Note = "Значение задано пользователем";
	if (in_is_minimum)
	{
		rule.Has_Target_Min = true;
		rule.Target_Min = in_value;
	}
	else
	{
		rule.Has_Target_Max = true;
		rule.Target_Max = in_value;
	}
	io_draft.Rules.append(rule);
}
//-------------------------------------------------------------------------------------------------------------
QString AProgram_Template_Service::New_Id()
{
	return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Template_Service::Is_Valid_Date(const QString &in_date)
{
	QDate date = QDate::fromString(in_date, Qt::ISODate);
	return date.isValid() && date.toString(Qt::ISODate) == in_date;
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Template_Service::Is_Nonnegative_Number(double in_value)
{
	return qIsFinite(in_value) && in_value >= 0;
}
//-------------------------------------------------------------------------------------------------------------

QStringList AProgram_Template_Service::Exercise_Ids()
{
	return {"builtin-bench-press", "builtin-bent-over-row", "builtin-squat", "builtin-crunch"};
}
//-------------------------------------------------------------------------------------------------------------
