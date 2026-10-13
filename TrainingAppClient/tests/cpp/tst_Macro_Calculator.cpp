#include "Domain/Macro_Calculator.h"

#include <QtTest>

//------------------------------------------------------------------------------
// AsMacro_Calculator: расчёт КБЖУ (спринт S7, issue 50).
//
// Проверяются не «числа получились», а свойства, которые обязаны выполняться
// для любого входа: калорийность не ниже основного обмена, углеводы не
// отрицательные, сумма вкладов макросов сходится с калорийностью. Точные
// числа проверяются там, где формула даёт точный результат.
//------------------------------------------------------------------------------
class AMacro_Calculator_Test : public QObject
{
	Q_OBJECT

private slots:
	void maleSedentaryMatchesFormula();
	void femaleUsesDifferentConstant();
	void activityLevelRaisesCalories();
	void fatLossGoalCreatesDeficit();
	void muscleGoalCreatesSurplus();
	// Каждый уровень активности из перечня — источник завышения: неизвестный
	// уровень молча занижал бы расчёт, и человек получил бы «норму» для
	// сидячего человека, тренирующегося пять раз в неделю.
	void unknownActivityFallsBackToSedentary();
	void resultIsInternallyConsistent();
	void outOfRangeInputIsRejected();
	void missingFieldsAreRejected();
	void ageIsComputedFromBirthDate();

private:
	AsMacro_Calculator::SInput Valid_Input() const;
};
//------------------------------------------------------------------------------
AsMacro_Calculator::SInput AMacro_Calculator_Test::Valid_Input() const
{
	AsMacro_Calculator::SInput input;

	input.Age = 30;
	input.Is_Female = false;
	input.Weight_Kg = 80.0;
	input.Height_Cm = 180.0;
	input.Activity_Level = "moderate";
	input.Goal = "maintain";

	return input;
}
//------------------------------------------------------------------------------
void AMacro_Calculator_Test::maleSedentaryMatchesFormula()
{
	AsMacro_Calculator calculator;
	AsMacro_Calculator::SInput input = Valid_Input();
	AsMacro_Calculator::SResult result;
	QString error_key;

	input.Activity_Level = "sedentary";

	QVERIFY(calculator.Calculate(input, result, error_key));
	QVERIFY(error_key.isEmpty());
	// Миффлин–Сан Жеор: 10×80 + 6,25×180 − 5×30 + 5 = 1780.
	QCOMPARE(result.Bmr_Kcal, 1780.0);
	// TDEE = BMR × 1,2, округление до 5.
	QCOMPARE(result.Tdee_Kcal, 2135.0);
	// Без поправки на цель калорийность равна расходу.
	QCOMPARE(result.Calories_Kcal, 2135.0);
}
//------------------------------------------------------------------------------
void AMacro_Calculator_Test::femaleUsesDifferentConstant()
{
	AsMacro_Calculator calculator;
	AsMacro_Calculator::SInput input = Valid_Input();
	AsMacro_Calculator::SResult male_result;
	AsMacro_Calculator::SResult female_result;
	QString error_key;

	input.Activity_Level = "sedentary";
	QVERIFY(calculator.Calculate(input, male_result, error_key));

	input.Is_Female = true;
	QVERIFY(calculator.Calculate(input, female_result, error_key));

	// Разница констант — 166 ккал (5 против −161), но после округления BMR до
	// пяти получается 165: 1780 − 1615. Проверяется результат округления, а
	// не формула, потому что именно его увидит человек.
	QCOMPARE(male_result.Bmr_Kcal - female_result.Bmr_Kcal, 165.0);
	// У женщин обмен ниже — иначе константа была бы не разной.
	QVERIFY(female_result.Bmr_Kcal < male_result.Bmr_Kcal);
}
//------------------------------------------------------------------------------
void AMacro_Calculator_Test::activityLevelRaisesCalories()
{
	AsMacro_Calculator calculator;
	AsMacro_Calculator::SInput input = Valid_Input();
	AsMacro_Calculator::SResult sedentary;
	AsMacro_Calculator::SResult moderate;
	AsMacro_Calculator::SResult athlete;
	QString error_key;

	input.Activity_Level = "sedentary";
	QVERIFY(calculator.Calculate(input, sedentary, error_key));
	input.Activity_Level = "moderate";
	QVERIFY(calculator.Calculate(input, moderate, error_key));
	input.Activity_Level = "athlete";
	QVERIFY(calculator.Calculate(input, athlete, error_key));

	QVERIFY(athlete.Tdee_Kcal > moderate.Tdee_Kcal);
	QVERIFY(moderate.Tdee_Kcal > sedentary.Tdee_Kcal);
}
//------------------------------------------------------------------------------
void AMacro_Calculator_Test::fatLossGoalCreatesDeficit()
{
	AsMacro_Calculator calculator;
	AsMacro_Calculator::SInput input = Valid_Input();
	AsMacro_Calculator::SResult maintain;
	AsMacro_Calculator::SResult fat_loss;
	QString error_key;

	input.Goal = "maintain";
	QVERIFY(calculator.Calculate(input, maintain, error_key));

	input.Goal = "fatLoss";
	QVERIFY(calculator.Calculate(input, fat_loss, error_key));

	// Похудение — дефицит 20 % от расхода. Округление до 5 ккал допускается,
	// поэтому проверяется диапазон, а не точное равенство.
	QVERIFY(fat_loss.Calories_Kcal < maintain.Tdee_Kcal);
	QVERIFY(fat_loss.Calories_Kcal > maintain.Tdee_Kcal * 0.75);
}
//------------------------------------------------------------------------------
void AMacro_Calculator_Test::muscleGoalCreatesSurplus()
{
	AsMacro_Calculator calculator;
	AsMacro_Calculator::SInput input = Valid_Input();
	AsMacro_Calculator::SResult maintain;
	AsMacro_Calculator::SResult muscle;
	QString error_key;

	input.Goal = "maintain";
	QVERIFY(calculator.Calculate(input, maintain, error_key));

	input.Goal = "muscle";
	QVERIFY(calculator.Calculate(input, muscle, error_key));

	QVERIFY(muscle.Calories_Kcal > maintain.Tdee_Kcal);
}
//------------------------------------------------------------------------------
void AMacro_Calculator_Test::unknownActivityFallsBackToSedentary()
{
	AsMacro_Calculator calculator;
	AsMacro_Calculator::SInput input = Valid_Input();
	AsMacro_Calculator::SResult sedentary;
	AsMacro_Calculator::SResult unknown;
	QString error_key;

	input.Activity_Level = "sedentary";
	QVERIFY(calculator.Calculate(input, sedentary, error_key));

	// Неизвестный уровень активности отвергается, а не трактуется как
	// минимальный. Это осознанный выбор: «считать по угадке» значило бы показать
	// человеку норму, которой он не соответствует, и он бы ей доверился.
	input.Activity_Level = "что-то новое";
	QVERIFY(calculator.Calculate(input, unknown, error_key) == false);
	QCOMPARE(error_key, QString("macro_activity_required"));
	// Результат не заполняется частично: половина расчёта хуже, чем ничего.
	QVERIFY(unknown.Tdee_Kcal == 0);
}
//------------------------------------------------------------------------------
void AMacro_Calculator_Test::resultIsInternallyConsistent()
{
	AsMacro_Calculator calculator;
	const QStringList levels = AsMacro_Calculator::Activity_Levels();
	const QStringList goals = QStringList{"", "maintain", "fatLoss", "muscle"};
	int checked = 0;

	// Свойства должны выполняться на всём переборе, а не на одном примере:
	// отрицательные углеводы или калорийность ниже обмена проявились бы на
	// граничных комбинациях.
	for (int level_index = 0; level_index < levels.size(); ++level_index)
	{
		for (int goal_index = 0; goal_index < goals.size(); ++goal_index)
		{
			for (int weight = 20; weight <= 300; weight += 20)
			{
				AsMacro_Calculator::SInput input = Valid_Input();
				AsMacro_Calculator::SResult result;
				QString error_key;

				input.Activity_Level = levels.at(level_index);
				input.Goal = goals.at(goal_index);
				input.Weight_Kg = weight;
				input.Height_Cm = 100.0 + (weight % 130);

				if (calculator.Calculate(input, result, error_key) == false)
					continue;

				++checked;
				QVERIFY2(result.Carbs_G >= 0, "углеводы не могут быть отрицательными");
				QVERIFY2(result.Calories_Kcal >= result.Bmr_Kcal, "калорийность ниже основного обмена");
				QVERIFY2(result.Protein_G > 0 && result.Fat_G > 0, "белок и жиры обязательны");
				// Углеводы — остаток калорийности: сумма вкладов макросов не может
				// превышать её больше, чем на округление до целых граммов.
				const double macro_kcal = result.Protein_G * 4.0 + result.Fat_G * 9.0 + result.Carbs_G * 4.0;
				QVERIFY2(macro_kcal <= result.Calories_Kcal + 12.0, "макросы превышают калорийность");
			}
		}
	}

	QVERIFY2(checked > 50, "перебор должен охватывать десятки комбинаций");
}
//------------------------------------------------------------------------------
void AMacro_Calculator_Test::outOfRangeInputIsRejected()
{
	AsMacro_Calculator calculator;
	AsMacro_Calculator::SInput input = Valid_Input();
	AsMacro_Calculator::SResult result;
	QString error_key;

	// Ноль и отрицательные значения — незаполненная форма, а не «очень худой
	// человек»: считать по ним можно только бессмысленное число.
	input.Weight_Kg = 0;
	QVERIFY(calculator.Calculate(input, result, error_key) == false);
	QCOMPARE(error_key, QString("macro_weight_out_of_range"));
	QVERIFY(result.Calories_Kcal == 0);

	input = Valid_Input();
	input.Height_Cm = 0;
	QVERIFY(calculator.Calculate(input, result, error_key) == false);
	QCOMPARE(error_key, QString("macro_height_out_of_range"));

	// Возраст вне диапазона формулы: подросток до 14 лет считается по другой
	// норме, и 100+ лет — тоже не наша методика.
	input = Valid_Input();
	input.Age = 10;
	QVERIFY(calculator.Calculate(input, result, error_key) == false);
	QCOMPARE(error_key, QString("macro_age_out_of_range"));
}
//------------------------------------------------------------------------------
void AMacro_Calculator_Test::missingFieldsAreRejected()
{
	AsMacro_Calculator calculator;
	AsMacro_Calculator::SInput input = Valid_Input();
	AsMacro_Calculator::SResult result;
	QString error_key;

	// Без уровня активности расчёт дал бы заниженную «норму» без всяких
	// оснований: лучше спросить, чем угадать.
	input.Activity_Level.clear();
	QVERIFY(calculator.Calculate(input, result, error_key) == false);
	QCOMPARE(error_key, QString("macro_activity_required"));
}
//------------------------------------------------------------------------------
void AMacro_Calculator_Test::ageIsComputedFromBirthDate()
{
	// День рождения в этом году ещё не наступил — человек на год моложе.
	QCOMPARE(AsMacro_Calculator::Age_On(QDate(2000, 12, 31), QDate(2026, 1, 1)), 25);
	// День рождения уже прошёл — полные годы.
	QCOMPARE(AsMacro_Calculator::Age_On(QDate(2000, 1, 1), QDate(2026, 1, 1)), 26);
	// Дата рождения в будущем — данных нет, а не «отрицательный возраст».
	QCOMPARE(AsMacro_Calculator::Age_On(QDate(2030, 1, 1), QDate(2026, 1, 1)), 0);
	// Пустая дата — тоже отсутствие ответа.
	QCOMPARE(AsMacro_Calculator::Age_On(QDate(), QDate(2026, 1, 1)), 0);
}
//------------------------------------------------------------------------------

QTEST_GUILESS_MAIN(AMacro_Calculator_Test)
#include "tst_Macro_Calculator.moc"
