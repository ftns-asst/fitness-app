#include "Domain/Macro_Calculator.h"

#include <QObject>

#include <cmath>
//----------------------------------------------------------------------------
// AsMacro_Calculator
//----------------------------------------------------------------------------
namespace
{
// Границы значений: не «на всякий случай», а по формуле. Вес 300 кг или
// рост 0 см — не опечатка человека, а испорченные данные, и считать по ним
// можно только бессмысленное число.
const double Min_Weight_Kg = 20.0;
const double Max_Weight_Kg = 300.0;
const double Min_Height_Cm = 100.0;
const double Max_Height_Cm = 250.0;

// Округление «до 5 ккал»: число кратно пяти, и глазу его легче принять как
// оценку, а не как точное измерение.
double Round_To_Five(double in_value)
{
	return std::round(in_value / 5.0) * 5.0;
}
} // namespace
//----------------------------------------------------------------------------
AsMacro_Calculator::AsMacro_Calculator(const QString &in_logging_category) : Logging_Category(in_logging_category)
{
}
//------------------------------------------------------------------------------
// Деструктор объявлен явно: калькулятор владеет только строкой, но создаётся
// через new в composition root, и без него компилятор счёл бы удаление через
// невидимый оператор без noexcept — а такой оператор бросает исключение при
// удалении.
//------------------------------------------------------------------------------
AsMacro_Calculator::~AsMacro_Calculator() = default;
//----------------------------------------------------------------------------
QStringList AsMacro_Calculator::Activity_Levels()
{
	// Порядок значим: он же порядок выбора на экране, и он идёт от
	// наименее активного к самому активному.
	return QStringList{"sedentary", "light", "moderate", "active", "athlete"};
}
//----------------------------------------------------------------------------
double AsMacro_Calculator::Activity_Factor(const QString &in_level)
{
	// Канонические коэффициенты активности.
	//
	// Неизвестный уровень сводится к минимальному как защита от вызова в обход
	// проверки входа. Обычным путём сюда такое значение не доходит: Is_Valid_Input
	// отвергает его с ключом macro_activity_required. Ставить коэффициент здесь
	// «по умолчанию» было бы утечкой: любой новый вызовящий, забывший про
	// проверку, получил бы молча заниженный результат.
	if (in_level == "athlete")
		return 2.725;
	if (in_level == "active")
		return 1.725;
	if (in_level == "moderate")
		return 1.55;
	if (in_level == "light")
		return 1.375;

	return 1.2;
}
//----------------------------------------------------------------------------
QString AsMacro_Calculator::Activity_Level_Caption(const QString &in_level)
{
	if (in_level == "athlete")
		return QObject::tr("Спортсмен");
	if (in_level == "active")
		return QObject::tr("Высокая");
	if (in_level == "moderate")
		return QObject::tr("Средняя");
	if (in_level == "light")
		return QObject::tr("Низкая");

	return QObject::tr("Минимальная");
}
//----------------------------------------------------------------------------
int AsMacro_Calculator::Age_On(const QDate &in_birth_date, const QDate &in_date)
{
	if (in_birth_date.isValid() == false)
		return 0;

	int age = in_date.year() - in_birth_date.year();

	if (in_date.month() < in_birth_date.month() ||
	    (in_date.month() == in_birth_date.month() && in_date.day() < in_birth_date.day()))
		age -= 1;

	// Отрицательный возраст — дата рождения в будущем: данные неверны, но
	// считать с ними всё равно нельзя, поэтому возвращается ноль, который
	// проверка входа отвергнет с понятным ключом.
	return age < 0 ? 0 : age;
}
//----------------------------------------------------------------------------
bool AsMacro_Calculator::Is_Valid_Input(const SInput &in_input, QString &out_error_key)
{
	out_error_key.clear();

	if (in_input.Weight_Kg < Min_Weight_Kg || in_input.Weight_Kg > Max_Weight_Kg)
	{
		out_error_key = "macro_weight_out_of_range";

		return false;
	}

	if (in_input.Height_Cm < Min_Height_Cm || in_input.Height_Cm > Max_Height_Cm)
	{
		out_error_key = "macro_height_out_of_range";

		return false;
	}

	// Возраст проверяется отдельно от роста и веса: он приходит из даты
	// рождения, и пустая дата — это не «0 лет», а отсутствие ответа.
	if (in_input.Age < 14 || in_input.Age > 100)
	{
		out_error_key = "macro_age_out_of_range";

		return false;
	}

	if (in_input.Activity_Level.isEmpty() || Activity_Levels().contains(in_input.Activity_Level) == false)
	{
		out_error_key = "macro_activity_required";

		return false;
	}

	return true;
}
//----------------------------------------------------------------------------
double AsMacro_Calculator::Bmr(const SInput &in_input) const
{
	// Миффлин–Сан Жеор: у мужчин константа 5, у женщин −161, вес в кг, рост в см.
	return 10.0 * in_input.Weight_Kg + 6.25 * in_input.Height_Cm - 5.0 * in_input.Age +
	       (in_input.Is_Female ? -161.0 : 5.0);
}
//----------------------------------------------------------------------------
double AsMacro_Calculator::Goal_Adjust(const QString &in_goal)
{
	// Поправка задана долей от расхода, а не от нормы: −20 % от TDEE даёт
	// дефицит, при котором темп похудения близок к рекомендованным 0,5 кг/нед.
	if (in_goal == "fatLoss")
		return -0.2;
	if (in_goal == "muscle")
		return 0.1;

	// maintain и пустая цель — поддержание без поправки.
	return 0.0;
}
//----------------------------------------------------------------------------
bool AsMacro_Calculator::Calculate(const SInput &in_input, SResult &out_result, QString &out_error_key)
{
	out_result = SResult();

	if (Is_Valid_Input(in_input, out_error_key) == false)
		return false;

	out_result.Bmr_Kcal = Round_To_Five(Bmr(in_input));
	out_result.Tdee_Kcal = Round_To_Five(out_result.Bmr_Kcal * Activity_Factor(in_input.Activity_Level));
	out_result.Calories_Kcal = Round_To_Five(out_result.Tdee_Kcal * (1.0 + Goal_Adjust(in_input.Goal)));

	// Белок — по массе тела, а не по калорийности: это единственная из трёх
	// норм, которая защищает мышцы при дефиците, и именно на неё опираются
	// рекомендации по кетозу и белковым диетам.
	out_result.Protein_G = std::round(in_input.Weight_Kg * 1.8);
	// Жиры — доля калорийности, а не граммы на килограмм: так доля жиров
	// остаётся постоянной при любом весе и любой активности.
	out_result.Fat_G = std::round(out_result.Calories_Kcal * 0.25 / 9.0);
	out_result.Carbs_G =
	    std::round((out_result.Calories_Kcal - out_result.Protein_G * 4.0 - out_result.Fat_G * 9.0) / 4.0);

	// Отрицательные углеводы — это провал расчёта, а не результат: при таком
	// дефиците норма несовместима с заданным белком. Возвращается ошибка с
	// ключом, а не ноль граммов.
	if (out_result.Carbs_G < 0)
	{
		out_result = SResult();
		out_error_key = "macro_no_space_for_carbs";

		return false;
	}

	// Калорийность ниже основного обмена — признак неверной комбинации
	// параметров; такой результат показывать нельзя, потому что «ешь 1200,
	// а основной обмен 1800» — противоречие, а не план питания.
	if (out_result.Calories_Kcal < out_result.Bmr_Kcal)
	{
		out_result = SResult();
		out_error_key = "macro_below_bmr";

		return false;
	}

	return true;
}
//----------------------------------------------------------------------------
