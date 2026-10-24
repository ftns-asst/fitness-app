#include "Presentation/Macro_VM.h"

#include "Data/Program/Measurement_Repository.h"
#include "Data/Users/Users_Repository.h"

#include <QDate>
#include <QDateTime>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(MACRO_VM_Log, "trainingapp.macro.vm")

//----------------------------------------------------------------------------
// Avm_Macro
//----------------------------------------------------------------------------
Avm_Macro::Avm_Macro(AsMacro_Calculator *in_calculator, AMeasurement_Repository *in_measurement_repository,
                     AUsers_Repository *in_users_repository, QObject *in_parent)
    : QObject(in_parent), Calculator_Instance(in_calculator),
      Measurement_Repository_Instance(in_measurement_repository), Users_Repository_Instance(in_users_repository)
{
	Today_Text = QDate::currentDate().toString("yyyy-MM-dd");
}
//----------------------------------------------------------------------------
bool Avm_Macro::load(const QString &in_user_id)
{
	return Load(in_user_id);
}
//----------------------------------------------------------------------------
bool Avm_Macro::calculate(const QString &in_user_id, double in_weight_kg, double in_height_cm, bool in_is_female,
                          const QString &in_birth_date, const QString &in_activity_level, const QString &in_goal)
{
	return Calculate(in_user_id, in_weight_kg, in_height_cm, in_is_female, in_birth_date, in_activity_level, in_goal);
}
//----------------------------------------------------------------------------
QStringList Avm_Macro::activityLevels() const
{
	return AsMacro_Calculator::Activity_Levels();
}
//----------------------------------------------------------------------------
QString Avm_Macro::activityCaption(const QString &in_level) const
{
	return AsMacro_Calculator::Activity_Level_Caption(in_level);
}
//----------------------------------------------------------------------------
void Avm_Macro::setToday(const QString &in_date)
{
	Set_Today(in_date);
}
//----------------------------------------------------------------------------
void Avm_Macro::Set_Today(const QString &in_date)
{
	// Невалидная дата оставляет локальную: от неё считается возраст, и пустая
	// строка дала бы нулевой возраст вместо сегодняшнего.
	if (QDate::fromString(in_date, Qt::ISODate).isValid() == false)
		return;

	Today_Text = in_date;
}
//----------------------------------------------------------------------------
bool Avm_Macro::Load(const QString &in_user_id)
{
	SBody_Measurement measurement;

	Clear_Error();
	Clear_Result();

	Birth_Date_Text.clear();
	Activity_Level_Text.clear();
	Goal_Text.clear();

	// Параметры сохраняются при первом расчёте, поэтому отсутствие строки —
	// обычное состояние, а не сбой: экран покажет пустую форму.
	if (Users_Repository_Instance != 0)
		Users_Repository_Instance->Load_Macro_Params(in_user_id, Birth_Date_Text, Activity_Level_Text);

	// Сохранённый результат подставляется сразу: человек возвращается на экран,
	// чтобы пересчитать, и пустая карточка «Результат» выглядела бы как
	// потерянный расчёт (миграция v6).
	Calculated_At_Text.clear();

	if (Users_Repository_Instance != 0)
		Users_Repository_Instance->Load_Macro_Result(in_user_id, Result, Calculated_At_Text);

	Height_Value = 0;
	Has_Height_Value = false;
	Weight_Value = 0;
	Has_Weight_Value = false;
	Female_Value = false;

	// Вес и рост берутся из последнего замера: спрашивать их отдельно значило бы
	// завести второй источник правды о том же весе.
	if (Measurement_Repository_Instance != 0 &&
	    Measurement_Repository_Instance->Load_Latest(in_user_id, measurement) == true)
	{
		if (measurement.Has_Weight_Kg)
		{
			Weight_Value = measurement.Weight_Kg;
			Has_Weight_Value = true;
		}
	}

	emit changed();

	return true;
}
//----------------------------------------------------------------------------
double Avm_Macro::Resolve_Weight(double in_weight_kg) const
{
	// Введённый вес приоритетнее замера, но не всегда задан: форма может быть
	// открыта до первого взвешивания, и тогда единственный источник — замер.
	if (in_weight_kg > 0)
		return in_weight_kg;

	return Has_Weight_Value ? Weight_Value : 0;
}
//----------------------------------------------------------------------------
bool Avm_Macro::Calculate(const QString &in_user_id, double in_weight_kg, double in_height_cm, bool in_is_female,
                          const QString &in_birth_date, const QString &in_activity_level, const QString &in_goal)
{
	AsMacro_Calculator::SInput input;
	const QDate birth_date = QDate::fromString(in_birth_date, Qt::ISODate);
	QString error_key;

	Clear_Error();
	Clear_Result();

	Height_Value = in_height_cm;
	Has_Height_Value = in_height_cm > 0;
	Female_Value = in_is_female;
	Birth_Date_Text = birth_date.isValid() ? in_birth_date : QString();
	Activity_Level_Text = in_activity_level;
	Goal_Text = in_goal;

	input.Age = AsMacro_Calculator::Age_On(birth_date, QDate::fromString(Today_Text, Qt::ISODate));
	input.Is_Female = in_is_female;
	input.Weight_Kg = Resolve_Weight(in_weight_kg);
	input.Height_Cm = in_height_cm;
	input.Activity_Level = in_activity_level;
	input.Goal = in_goal;

	// Заполненный вес запоминается: без этого повторный расчёт после перезахода
	// спрашивал бы вес заново, хотя человек его уже вводил.
	if (input.Weight_Kg > 0)
	{
		Weight_Value = input.Weight_Kg;
		Has_Weight_Value = true;
	}

	if (Calculator_Instance == 0 || Calculator_Instance->Calculate(input, Result, error_key) == false)
	{
		Error_Key_Text = error_key.isEmpty() ? QString("undefined_error") : error_key;
		qCInfo(MACRO_VM_Log).noquote() << "macro calculation failed:" << Error_Key_Text;
		emit changed();

		return false;
	}

	// Параметры сохраняются только при успешном расчёте: так в базу не попадает
	// мусор из недозаполненной формы, который потом пришлось бы отличать от
	// настоящих данных.
	if (Users_Repository_Instance != 0 && in_user_id.isEmpty() == false)
	{
		if (Users_Repository_Instance->Save_Macro_Params(in_user_id, Birth_Date_Text, Activity_Level_Text) == false)
		{
			Error_Key_Text = "macro_params_save_failed";
			Clear_Result();
			emit changed();

			return false;
		}

		// Результат сохраняется рядом с параметрами: главная показывает числа по
		// строке profile_local, не вызывая домен. Сбой записи не отменяет расчёт —
		// числа уже посчитаны и видны на экране, и терять их из-за базы хуже, чем
		// не показать их на главной.
		if (Users_Repository_Instance->Save_Macro_Result(in_user_id, Result) == false)
			qCWarning(MACRO_VM_Log).noquote() << "macro result is not persisted:" << in_user_id;
	}

	Calculated_At_Text = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

	emit changed();

	return true;
}
//----------------------------------------------------------------------------
void Avm_Macro::Clear_Result()
{
	Result = AsMacro_Calculator::SResult();
}
//----------------------------------------------------------------------------
void Avm_Macro::Clear_Error()
{
	Error_Key_Text.clear();
}
//----------------------------------------------------------------------------
bool Avm_Macro::Has_Result() const
{
	return Result.Calories_Kcal > 0;
}
//----------------------------------------------------------------------------
double Avm_Macro::bmr_kcal() const
{
	return Result.Bmr_Kcal;
}
//----------------------------------------------------------------------------
double Avm_Macro::tdee_kcal() const
{
	return Result.Tdee_Kcal;
}
//----------------------------------------------------------------------------
double Avm_Macro::calories_kcal() const
{
	return Result.Calories_Kcal;
}
//----------------------------------------------------------------------------
double Avm_Macro::protein_g() const
{
	return Result.Protein_G;
}
//----------------------------------------------------------------------------
double Avm_Macro::fat_g() const
{
	return Result.Fat_G;
}
//----------------------------------------------------------------------------
double Avm_Macro::carbs_g() const
{
	return Result.Carbs_G;
}
//----------------------------------------------------------------------------
double Avm_Macro::weight_kg() const
{
	return Weight_Value;
}
//----------------------------------------------------------------------------
double Avm_Macro::height_cm() const
{
	return Height_Value;
}
//----------------------------------------------------------------------------
bool Avm_Macro::is_female() const
{
	return Female_Value;
}
//----------------------------------------------------------------------------
QString Avm_Macro::activity_level() const
{
	return Activity_Level_Text;
}
//----------------------------------------------------------------------------
QString Avm_Macro::goal() const
{
	return Goal_Text;
}
//----------------------------------------------------------------------------
bool Avm_Macro::Has_Weight() const
{
	return Has_Weight_Value;
}
//----------------------------------------------------------------------------
bool Avm_Macro::Has_Height() const
{
	return Has_Height_Value;
}
//----------------------------------------------------------------------------
bool Avm_Macro::Has_Birth_Date() const
{
	return Birth_Date_Text.isEmpty() == false;
}
//----------------------------------------------------------------------------
QString Avm_Macro::birth_date() const
{
	return Birth_Date_Text;
}
//----------------------------------------------------------------------------
QString Avm_Macro::error_key() const
{
	return Error_Key_Text;
}
//----------------------------------------------------------------------------
QString Avm_Macro::calculated_at() const
{
	return Calculated_At_Text;
}
//----------------------------------------------------------------------------
