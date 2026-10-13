#pragma once

#include "Domain/Error_State.h"

#include <QDate>
#include <QString>
#include <QStringList>

//----------------------------------------------------------------------------
// Авторасчёт КБЖУ (спринт S7, issue 50): BMR → TDEE → макросы.
//
// Формулы и их источники (срез D, docs/05-recommendations-catalog-today.md):
// - BMR — формула Миффлина–Сан Жеора. Выбрана вместо Харриса–Бенедикта:
//     у последней результат систематически занижен для людей с лишним весом,
//   а счётчик КБЖУ на жиросжигании нужен именно им.
// - TDEE — BMR × коэффициент активности (1,2…2,725).
// - Белки — 1,8 г/кг массы тела, жиры — 25 % калорийности, углеводы —
//   остаток. Жёстких границ нет: «углеводы не могут быть нулевыми» — полезное
//   правило, потому что результат ниже нуля нельзя показать.
//
// Результат не пустой и не заниженный: без коэффициентов активности расчёт
// вводил бы в заблуждение, поэтому при недостающих данных Calculate
// возвращает false, а не догадку.
//----------------------------------------------------------------------------
class AsMacro_Calculator
{
public:
	explicit AsMacro_Calculator(const QString &in_logging_category = QString());
	~AsMacro_Calculator();

	// Поля расчёта. Не заданные (0 или пустая строка) — недостающие данные.
	//
	// weight_kg и height_cm обязательны: без них BMR не считается вовсе.
	// age передаётся числом, а датой рождения: считающий код не должен знать о
	// календаре, а вызывающий (ViewModel) переводит дату в возраст.
	struct SInput
	{
		int Age = 0;
		bool Is_Female = false;
		double Weight_Kg = 0;
		double Height_Cm = 0;
		QString Activity_Level;
		// Поправка на цель: без неё результат описывал бы поддержание веса, а
		// экран обещает «сколько нужно, чтобы худеть».
		QString Goal;
	};

	// Результат расчёта. Числа округлены до целых: калории до 5 ккал, белки и
	// жиры до грамма — иначе экран показывал бы «1843,7 ккал», что выглядит
	// точнее, чем есть на самом деле.
	struct SResult
	{
		double Bmr_Kcal = 0;
		double Tdee_Kcal = 0;
		double Calories_Kcal = 0;
		double Protein_G = 0;
		double Fat_G = 0;
		double Carbs_G = 0;
	};

	// Рассчитывает макросы. false — не хватает данных; out_error_key получает
	// ключ для показа на экране (§7 docs/07-programs-ui-spec.md).
	bool Calculate(const SInput &in_input, SResult &out_result, QString &out_error_key);

	// Возраст на дату. Год неполный: даты рождения указывают без дня и месяца
	// (01.01), поэтому день и месяц не сравниваются — иначе родившийся 1 июля
	// человек всю жизнь считался бы на полгода моложе в январе.
	static int Age_On(const QDate &in_birth_date, const QDate &in_date);

	// Тексты режимов для выбора на экране. Отдельная функция, а не данные в
	// структуре: подписи переводит разметка, а перечисление живёт рядом с
	// формулами, чтобы набор режимов и коэффициенты не разошлись.
	static QStringList Activity_Levels();
	static QString Activity_Level_Caption(const QString &in_level);
	static double Activity_Factor(const QString &in_level);

private:
	// Проверки входных данных — по одной на каждое недостающее поле, чтобы
	// экран показал, чего именно не хватает, а не «ошибка расчёта».
	bool Is_Valid_Input(const SInput &in_input, QString &out_error_key);
	double Bmr(const SInput &in_input) const;
	// Поправка на цель в долях от TDEE.
	static double Goal_Adjust(const QString &in_goal);

	QString Logging_Category;
};
//----------------------------------------------------------------------------