#include "Presentation/Today_VM.h"

#include "Data/Program/Measurement_Repository.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Data/Program/Stats_Repository.h"
#include "Data/Program/Workout_Repository.h"
#include "Data/Users/Users_Repository.h"
#include "Presentation/Program_Rows.h"

#include <QDate>

Q_LOGGING_CATEGORY(TODAY_VM_Log, "trainingapp.today.vm")

namespace
{
// Вид дня в контракте — число EDay_Kind, поэтому вид слота расписания
// переводится в тот же enum. `Optional` — тренировка по выбору, и
// называть её отдыхом было бы враньём: день с упражнениями не
// бывает днём без плана (§5.2).
EDay_Kind Day_Kind_Of(const ESlot_Kind in_kind)
{
	switch (in_kind)
	{
	case ESlot_Kind::Rest:
		return EDay_Kind::Rest;
	case ESlot_Kind::Cardio:
		return EDay_Kind::Cardio;
	case ESlot_Kind::Mobility:
		return EDay_Kind::Mobility;
	case ESlot_Kind::Workout:
	case ESlot_Kind::Optional:
		break;
	}

	return EDay_Kind::Workout;
}
} // namespace

//----------------------------------------------------------------------------
Avm_Today::Avm_Today(AProgram_Repository *in_program_repository, ARule_Repository *in_rule_repository,
                     AWorkout_Repository *in_workout_repository, AMeasurement_Repository *in_measurement_repository,
                     AStats_Repository *in_stats_repository, AUsers_Repository *in_users_repository, QObject *in_parent)
    : QObject(in_parent), Program_Repository_Instance(in_program_repository),
      Rule_Repository_Instance(in_rule_repository), Workout_Repository_Instance(in_workout_repository),
      Measurement_Repository_Instance(in_measurement_repository), Stats_Repository_Instance(in_stats_repository),
      Users_Repository_Instance(in_users_repository), Exercises(new AList_Model(this)), Rules(new AList_Model(this))
{
	Today_Text = QDate::currentDate().toString("yyyy-MM-dd");
	Exercises->Set_Roles(Program_Rows::Day_Exercise_Roles());
	Rules->Set_Roles(Program_Rows::Rule_Roles());
}
//----------------------------------------------------------------------------
bool Avm_Today::reload(const QString &in_user_id)
{
	return Load(in_user_id);
}
//----------------------------------------------------------------------------
void Avm_Today::setToday(const QString &in_date)
{
	Set_Today(in_date);
}
//----------------------------------------------------------------------------
void Avm_Today::Set_Today(const QString &in_date)
{
	// Невалидная дата оставляет локальную: день недели вычисляется
	// из даты, и пустая строка дала бы нулевой слот вместо
	// сегодняшнего.
	if (QDate::fromString(in_date, Qt::ISODate).isValid() == false)
		return;

	Today_Text = in_date;
}
//----------------------------------------------------------------------------
bool Avm_Today::Load(const QString &in_user_id)
{
	SProgram program;
	QVector<SGoal_Target> targets;
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> day_exercises;
	QVector<SWeek_Slot> week_slots;

	Clear_Error();
	Clear_Data();

	if (Program_Repository_Instance == 0 || Rule_Repository_Instance == 0)
	{
		Set_Error("undefined_error");
		emit changed();

		return false;
	}

	// Замер читается до проверки программы: `body_measurements` от программы
	// не зависит (инвариант 20 §4), и без активной программы ввод веса
	// и обхватов остаётся осмысленным — иначе гость не смог бы
	// внести ни одного замера.
	Input_User_Id = in_user_id;
	Load_Measurement(in_user_id);
	Load_Macro_Plan(in_user_id);

	// Активная программа — единственный источник плана на сегодня
	// (§3.2). Её нет у гостя и у пользователя без программы: это пустое
	// состояние экрана, а не ошибка, поэтому reload возвращает true и
	// оставляет errorKey пустым.
	if (Program_Repository_Instance->Get_Active_Program_Id(in_user_id, Program_Id_Text) == false)
	{
		emit changed();

		return true;
	}

	// Расписание и дни читаются целиком: день суток собирается из
	// слота, дня плана и его упражнений, и по частям форма показала
	// бы половину дня.
	if (Program_Repository_Instance->Load_Program_Full(Program_Id_Text, program, targets, days, day_exercises,
	                                                   week_slots) == false)
	{
		Set_Error("undefined_error");
		emit changed();

		return false;
	}

	Program_Title_Text = program.Title;
	Goal_Text = To_String(program.Goal);
	Program_User_Id = in_user_id;

	// Срок программы целиком: неделя считается от фактического
	// старта, потому что план сдвигается вместе с ним
	// (program_actual_start_date, В§3.4).
	Weeks_Total_Value = program.Term.Weeks_Total;
	Current_Week_Value = Compute_Week(date_of_today(), program);

	const QDate date = date_of_today();

	// QDate::dayOfWeek() отдаёт 1 = понедельник…7 = воскресенье — то же
	// значение, что и program_schedule.day_of_week (§3.4).
	Weekday_Number = date.isValid() ? date.dayOfWeek() : 1;

	Load_Day(week_slots, days);
	Load_Day_Exercises(Slot_Day_Id_Text);
	Load_Rules_For_Today();
	Load_Session();

	emit changed();

	return true;
}
//----------------------------------------------------------------------------
// Дата экрана. Вынесена в отдельную функцию: её читают и Load, и
// расчёт недели, и очистка состояния, и расхождение правок
// означало бы три разные даты на одном экране.
//----------------------------------------------------------------------------
QDate Avm_Today::date_of_today() const
{
	return QDate::fromString(Today_Text, Qt::ISODate);
}
//----------------------------------------------------------------------------
// Текущая неделя программы: порядковый номер недели срока, считая
// от старта. Вне срока возвращается 0 — экран показывает «нед. 0»,
// что честнее, чем первая неделя программы, которой уже нет (§5.5).
//----------------------------------------------------------------------------
int Avm_Today::Compute_Week(const QDate &in_date, const SProgram &in_program) const
{
	const QString start_text =
	    in_program.Actual_Start_Date.isEmpty() ? in_program.Term.Start_Date : in_program.Actual_Start_Date;
	const QDate start = QDate::fromString(start_text, Qt::ISODate);

	if (in_date.isValid() == false || start.isValid() == false)
		return 0;

	if (in_date < start)
		return 0;

	const int days = static_cast<int>(start.daysTo(in_date));
	const int week = days / 7 + 1;

	// Открытый срок («до достижения цели») недели не ограничивает, а
	// у фиксированного она не может быть позже последней.
	if (in_program.Term.Weeks_Total > 0 && week > in_program.Term.Weeks_Total)
		return in_program.Term.Weeks_Total;

	return week;
}
//----------------------------------------------------------------------------
void Avm_Today::Load_Day(const QVector<SWeek_Slot> &in_slots, const QVector<SProgram_Day> &in_days)
{
	const SWeek_Slot *slot = Find_Slot(in_slots, Weekday_Number);

	// День, которого нет в расписании, — не отдых: отдых назначают
	// слотом с kind = rest. Здесь плана нет вообще, и показывать его
	// отдыхом значило бы утверждать, что программа запланировала
	// перерыв.
	Day_Kind_Value = int(EDay_Kind::Rest);
	Slot_Day_Id_Text.clear();
	Estimated_Minutes = 0;

	if (slot == 0)
		return;

	Slot_Day_Id_Text = slot->Program_Day_Id;
	// Ближайший тренировочный день ищется по всему расписанию, а не
	// по текущему дню: у дня отдыха подсказка «когда тренироваться»
	// нужна, и вычислять её должна модель, а не разметка.
	Next_Training_Weekday_Value = Find_Next_Training_Weekday(in_slots);

	Day_Kind_Value = int(Day_Kind_Of(slot->Kind));

	const SProgram_Day *day = Find_Day(in_days, Slot_Day_Id_Text);

	if (day == 0)
		return;

	// Вид дня и название берутся у дня плана: слот может указывать
	// на день с другим видом (кардио-день в слоте «тренировка»), и
	// тогда экран показал бы отдых вместо кардио или наоборот.
	Day_Kind_Value = int(day->Kind);
	Day_Title_Text = day->Title;
	Estimated_Minutes = day->Estimated_Minutes;
}
//----------------------------------------------------------------------------
int Avm_Today::Find_Next_Training_Weekday(const QVector<SWeek_Slot> &in_slots) const
{
	// Сегодняшний день входит в поиск: у тренировочного дня
	// подсказка про следующую тренировку не нужна, но число
	// остаётся верным.
	for (int offset = 0; offset < 7; ++offset)
	{
		const int weekday = ((Weekday_Number - 1 + offset) % 7) + 1;
		const SWeek_Slot *slot = Find_Slot(in_slots, weekday);

		if (slot != 0 && slot->Kind != ESlot_Kind::Rest && slot->Program_Day_Id.isEmpty() == false)
			return weekday;
	}

	return 0;
}
//----------------------------------------------------------------------------
void Avm_Today::Load_Day_Exercises(const QString &in_day_id)
{
	QVector<SDay_Exercise> day_exercises;
	QHash<QString, QString> titles;
	QVector<QString> exercise_ids;

	Exercises->Set_Rows(QVector<QVariantMap>());

	if (in_day_id.isEmpty() || Program_Repository_Instance->Load_Day_Exercises(in_day_id, day_exercises) == false)
		return;

	for (int i = 0; i < day_exercises.size(); ++i)
		exercise_ids.append(day_exercises.at(i).Exercise_Id);

	// Название упражнения лежит в каталоге, а не в предписании: без
	// него экран показывал бы «ex_squat» вместо «Присед».
	if (exercise_ids.isEmpty() == false)
		Program_Repository_Instance->Load_Exercise_Titles(exercise_ids, titles);

	Exercises->Set_Rows(Program_Rows::Build_Day_Exercises(day_exercises, titles));
}
//----------------------------------------------------------------------------
void Avm_Today::Load_Rules_For_Today()
{
	QVector<SRule_Checkin> checkins;
	QHash<QString, QString> states;

	Rules->Set_Rows(QVector<QVariantMap>());
	Loaded_Rules.clear();

	if (Rule_Repository_Instance->Load_Rules(Program_Id_Text, Loaded_Rules) == false)
		return;

	// Отметки читаются ровно за сегодня: правило на главной
	// показывает состояние текущего дня, а не окна недели (§5.1).
	Rule_Repository_Instance->Load_Program_Checkins_Range(Program_Id_Text, Today_Text, Today_Text, checkins);

	for (int i = 0; i < checkins.size(); ++i)
		states.insert(checkins.at(i).Rule_Id, Program_Model::To_String(checkins.at(i).State));

	Rules->Set_Rows(Program_Rows::Build_Rules(Loaded_Rules, states));
}
//----------------------------------------------------------------------------
void Avm_Today::Load_Session()
{
	QVector<SWorkout_Session> sessions;

	Session_Done = false;

	if (Workout_Repository_Instance == 0 || Program_Id_Text.isEmpty())
		return;

	if (Workout_Repository_Instance->Load_Sessions(Program_User_Id, Program_Id_Text, Today_Text, Today_Text,
	                                               sessions) == false)
		return;

	for (int i = 0; i < sessions.size(); ++i)
	{
		if (sessions.at(i).State == EWorkout_State::Completed)
		{
			Session_Done = true;

			break;
		}
	}
}
//----------------------------------------------------------------------------
// Результат авторасчёта КБЖУ (миграция v6). Отсутствие строки — «расчёт ещё не
// делали»: главная предлагает рассчитать, а не показывает нули, которые
// выглядели бы как честный результат.
//----------------------------------------------------------------------------
void Avm_Today::Load_Macro_Plan(const QString &in_user_id)
{
	Macro_Result = AsMacro_Calculator::SResult();
	Macro_Calculated_At_Text.clear();

	if (Users_Repository_Instance == 0)
		return;

	Users_Repository_Instance->Load_Macro_Result(in_user_id, Macro_Result, Macro_Calculated_At_Text);
}
//----------------------------------------------------------------------------
const SWeek_Slot *Avm_Today::Find_Slot(const QVector<SWeek_Slot> &in_slots, int in_weekday) const
{
	for (int i = 0; i < in_slots.size(); ++i)
	{
		if (in_slots.at(i).Day_Of_Week == in_weekday)
			return &in_slots.at(i);
	}

	return 0;
}
//----------------------------------------------------------------------------
void Avm_Today::Clear_Error()
{
	Error_Key_Text.clear();
}
//----------------------------------------------------------------------------
void Avm_Today::Clear_Data()
{
	const QDate date = date_of_today();

	Program_Id_Text.clear();
	Program_User_Id.clear();
	Program_Title_Text.clear();
	Goal_Text.clear();
	Day_Title_Text.clear();
	Slot_Day_Id_Text.clear();
	Weekday_Number = date.isValid() ? date.dayOfWeek() : 1;
	Day_Kind_Value = int(EDay_Kind::Rest);
	Estimated_Minutes = 0;
	Current_Week_Value = 0;
	Weeks_Total_Value = 0;
	Next_Training_Weekday_Value = 0;
	Session_Done = false;
	Loaded_Rules.clear();
	Exercises->Set_Rows(QVector<QVariantMap>());
	Rules->Set_Rows(QVector<QVariantMap>());
	Measurement = SBody_Measurement();
	Macro_Result = AsMacro_Calculator::SResult();
	Macro_Calculated_At_Text.clear();
}
//----------------------------------------------------------------------------
bool Avm_Today::Set_Error(const QString &in_error_key)
{
	Error_Key_Text = in_error_key;
	qCInfo(TODAY_VM_Log).noquote() << "today operation failed:" << in_error_key;

	return false;
}
//----------------------------------------------------------------------------
bool Avm_Today::Has_Program() const
{
	return Program_Id_Text.isEmpty() == false;
}
//----------------------------------------------------------------------------
QString Avm_Today::programId() const
{
	return Program_Id_Text;
}
//----------------------------------------------------------------------------
QString Avm_Today::programTitle() const
{
	return Program_Title_Text;
}
//----------------------------------------------------------------------------
QString Avm_Today::goal() const
{
	return Goal_Text;
}
//----------------------------------------------------------------------------
QString Avm_Today::today() const
{
	return Today_Text;
}
//----------------------------------------------------------------------------
int Avm_Today::weekday() const
{
	return Weekday_Number;
}
//----------------------------------------------------------------------------
int Avm_Today::dayKind() const
{
	return Day_Kind_Value;
}
//----------------------------------------------------------------------------
QString Avm_Today::Day_Kind_Name() const
{
	if (Day_Kind_Value < 0 || Day_Kind_Value >= int(EDay_Kind::Max))
		return To_String(EDay_Kind::Rest);

	return To_String(static_cast<EDay_Kind>(Day_Kind_Value));
}
//----------------------------------------------------------------------------
int Avm_Today::Next_Training_Weekday() const
{
	return Next_Training_Weekday_Value;
}
//----------------------------------------------------------------------------
bool Avm_Today::Is_Rest_Day() const
{
	return Has_Program() && Day_Kind_Value == int(EDay_Kind::Rest);
}
//----------------------------------------------------------------------------
QString Avm_Today::dayTitle() const
{
	return Day_Title_Text;
}
//----------------------------------------------------------------------------
int Avm_Today::estimated_minutes() const
{
	return Estimated_Minutes;
}
//----------------------------------------------------------------------------
int Avm_Today::Current_Week() const
{
	return Current_Week_Value;
}
//----------------------------------------------------------------------------
int Avm_Today::weeks_total() const
{
	return Weeks_Total_Value;
}
//----------------------------------------------------------------------------
bool Avm_Today::Session_Completed() const
{
	return Session_Done;
}
//----------------------------------------------------------------------------
QString Avm_Today::errorKey() const
{
	return Error_Key_Text;
}
//----------------------------------------------------------------------------
bool Avm_Today::Has_Macro_Plan() const
{
	return Macro_Result.Calories_Kcal > 0;
}
//----------------------------------------------------------------------------
double Avm_Today::macro_calories_kcal() const
{
	return Macro_Result.Calories_Kcal;
}
//----------------------------------------------------------------------------
double Avm_Today::macro_protein_g() const
{
	return Macro_Result.Protein_G;
}
//----------------------------------------------------------------------------
double Avm_Today::macro_fat_g() const
{
	return Macro_Result.Fat_G;
}
//----------------------------------------------------------------------------
double Avm_Today::macro_carbs_g() const
{
	return Macro_Result.Carbs_G;
}
//----------------------------------------------------------------------------
QString Avm_Today::macro_calculated_at() const
{
	return Macro_Calculated_At_Text;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Today::exercises() const
{
	return Exercises;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Today::rules() const
{
	return Rules;
}
//----------------------------------------------------------------------------
const SProgram_Day *Avm_Today::Find_Day(const QVector<SProgram_Day> &in_days, const QString &in_day_id) const
{
	if (in_day_id.isEmpty())
		return 0;

	for (int i = 0; i < in_days.size(); ++i)
	{
		if (in_days.at(i).ID == in_day_id)
			return &in_days.at(i);
	}

	return 0;
}
//----------------------------------------------------------------------------
// Ввод значений (спринт S6, issue 51)
//----------------------------------------------------------------------------
bool Avm_Today::saveMeasurement(double in_weight_kg, bool in_has_weight, double in_chest_cm, bool in_has_chest,
                                double in_waist_cm, bool in_has_waist, double in_hip_cm, bool in_has_hip,
                                double in_arm_cm, bool in_has_arm, double in_thigh_cm, bool in_has_thigh)
{
	const QString user_id = Current_User_Id();

	Clear_Error();

	if (Save_Measurement(user_id, in_weight_kg, in_has_weight, in_chest_cm, in_has_chest, in_waist_cm, in_has_waist,
	                     in_hip_cm, in_has_hip, in_arm_cm, in_has_arm, in_thigh_cm, in_has_thigh) == false)
	{
		emit changed();

		return false;
	}

	Load_Measurement(user_id);
	emit changed();

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Today::saveCheckin(const QString &in_rule_id, const QString &in_state, double in_value, bool in_has_value)
{
	Clear_Error();

	if (Save_Checkin(in_rule_id, in_state, in_value, in_has_value) == false)
	{
		emit changed();

		return false;
	}

	// Правила перечитываются вместе с отметками: состояние
	// «выполнено» на главной берётся из строки правила, и без
	// перечитывания точка и процент остались бы прежними до
	// перезагрузки экрана.
	Load_Rules_For_Today();
	emit changed();

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Today::Load_Measurement(const QString &in_user_id)
{
	Measurement = SBody_Measurement();

	if (Measurement_Repository_Instance == 0 || in_user_id.isEmpty())
		return false;

	// Отсутствие замера за день — не ошибка: это пустая форма ввода,
	// а не сбой. Владелец и дата всё равно заполняются: форме нужно
	// знать, что она пишет замер за сегодняшний день.
	Measurement.User_Id = in_user_id;
	Measurement.Date = Today_Text;

	if (Measurement_Repository_Instance->Find_Measurement(in_user_id, Today_Text, Measurement) == false)
	{
		Measurement = SBody_Measurement();
		Measurement.User_Id = in_user_id;
		Measurement.Date = Today_Text;

		return false;
	}

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Today::Save_Measurement(const QString &in_user_id, double in_weight_kg, bool in_has_weight, double in_chest_cm,
                                 bool in_has_chest, double in_waist_cm, bool in_has_waist, double in_hip_cm,
                                 bool in_has_hip, double in_arm_cm, bool in_has_arm, double in_thigh_cm,
                                 bool in_has_thigh)
{
	SBody_Measurement existing;
	SBody_Measurement measurement;

	// Запись без единого значения не пишется: репозиторий всё равно
	// отверг бы её, а пользователь увидел бы ошибку вместо тихого
	// «ничего не изменилось».
	if (in_has_weight == false && in_has_chest == false && in_has_waist == false && in_has_hip == false &&
	    in_has_arm == false && in_has_thigh == false)
	{
		Set_Error("undefined_error");

		return false;
	}

	if (Measurement_Repository_Instance == 0 || in_user_id.isEmpty())
	{
		Set_Error("undefined_error");

		return false;
	}

	// Уже сохранённые значения дня сохраняются: слияние на уровне
	// репозитория пишет только заданные поля, поэтому утренний вес
	// пережил бы вечернюю запись обхватов — но без чтения
	// существующей строки потерялся бы её id, и upsert создал бы вторую
	// запись за тот же день.
	if (Measurement_Repository_Instance->Find_Measurement(in_user_id, Today_Text, existing))
		measurement = existing;
	else
		measurement.User_Id = in_user_id;

	measurement.Date = Today_Text;
	measurement.Weight_Kg = in_weight_kg;
	measurement.Has_Weight_Kg = in_has_weight;
	measurement.Chest_Cm = in_chest_cm;
	measurement.Has_Chest_Cm = in_has_chest;
	measurement.Waist_Cm = in_waist_cm;
	measurement.Has_Waist_Cm = in_has_waist;
	measurement.Hip_Cm = in_hip_cm;
	measurement.Has_Hip_Cm = in_has_hip;
	measurement.Arm_Cm = in_arm_cm;
	measurement.Has_Arm_Cm = in_has_arm;
	measurement.Thigh_Cm = in_thigh_cm;
	measurement.Has_Thigh_Cm = in_has_thigh;

	if (Measurement_Repository_Instance->Save_Measurement(measurement) == false)
	{
		Set_Error("undefined_error");

		return false;
	}

	Measurement = measurement;
	Invalidate_Stats();

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Today::Save_Checkin(const QString &in_rule_id, const QString &in_state, double in_value, bool in_has_value)
{
	ECheckin_State state = ECheckin_State::Unknown;
	SRule_Checkin checkin;

	if (in_rule_id.isEmpty() || From_String(in_state, state) == false || state == ECheckin_State::Unknown)
	{
		Set_Error("invalid_checkin_state");

		return false;
	}

	// Отметка без программы некуда записать: у отметки есть programId
	// (§3.7), и пустой он сделал бы строку, которую не найдёт ни одна
	// выборка.
	if (Rule_Repository_Instance == 0 || Program_Id_Text.isEmpty())
	{
		Set_Error("no_active_program");

		return false;
	}

	checkin.Rule_Id = in_rule_id;
	checkin.Program_Id = Program_Id_Text;
	checkin.Date = Today_Text;
	checkin.Value = in_value;
	checkin.Has_Value = in_has_value;
	checkin.State = state;
	checkin.Source = ECheckin_Source::Manual;

	// Единица берётся у правила: «г» у белка и «л» у воды — из одного
	// захардкоженного значения статистика считала бы в молях.
	for (int i = 0; i < Loaded_Rules.size(); ++i)
	{
		if (Loaded_Rules.at(i).ID != in_rule_id)
			continue;

		checkin.Unit = To_String(Loaded_Rules.at(i).Display_Unit);
		break;
	}

	if (Rule_Repository_Instance->Save_Checkin(checkin) == false)
	{
		Set_Error("checkin_save_failed");

		return false;
	}

	Invalidate_Stats();

	return true;
}
//----------------------------------------------------------------------------
void Avm_Today::Invalidate_Stats()
{
	if (Stats_Repository_Instance == 0)
		return;

	const QString user_id = Current_User_Id();

	// Запись факта без метки пользователя инвалидировать нечем:
	// кэш общей статистики принадлежит пользователю, и без него
	// удалять нечего.
	if (user_id.isEmpty())
		return;

	Stats_Repository_Instance->Invalidate_User_Stats(user_id);
}
//----------------------------------------------------------------------------
// Пользователь, от имени которого идёт ввод. Отдельный снимок, а
// не поле программы: замеры и отметки не зависят от того, есть ли
// программа (замеры — инвариант 20 §4, отметка требует программы,
// но не владельца программы).
//----------------------------------------------------------------------------
QString Avm_Today::Current_User_Id() const
{
	return Input_User_Id;
}
//----------------------------------------------------------------------------
bool Avm_Today::Has_Measurement() const
{
	return Measurement.Has_Weight_Kg || Measurement.Has_Chest_Cm || Measurement.Has_Waist_Cm ||
	       Measurement.Has_Hip_Cm || Measurement.Has_Arm_Cm || Measurement.Has_Thigh_Cm;
}
//----------------------------------------------------------------------------
double Avm_Today::weightKg() const
{
	return Measurement.Weight_Kg;
}
//----------------------------------------------------------------------------
double Avm_Today::chestCm() const
{
	return Measurement.Chest_Cm;
}
//----------------------------------------------------------------------------
double Avm_Today::waistCm() const
{
	return Measurement.Waist_Cm;
}
//----------------------------------------------------------------------------
double Avm_Today::hipCm() const
{
	return Measurement.Hip_Cm;
}
//----------------------------------------------------------------------------
double Avm_Today::armCm() const
{
	return Measurement.Arm_Cm;
}
//----------------------------------------------------------------------------
double Avm_Today::thighCm() const
{
	return Measurement.Thigh_Cm;
}
//----------------------------------------------------------------------------
