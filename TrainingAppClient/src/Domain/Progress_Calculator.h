#pragma once

#include "Domain/Error_State.h"
#include "Domain/Lifetime_Calculator.h"
#include "Domain/Program_Stats_Calculator.h"
#include "Domain/Progress_Stats.h"
#include "Domain/Stats_Encoder.h"

#include <QString>

//-------------------------------------------------------------------------------------------------------------
// Фасад расчёта статистики (срез W7, 02-program-model.md §5).
//
// Существует ради одного публичного контракта, которым пользуются ViewModel и
// тесты. Сама логика разнесена по трём классам с разными причинами для
// изменения:
//
//   AsProgram_Stats_Calculator — программа: окно сроков, недельные срезы,
//                                сила, тело, цели, правила (§5.1);
//   AsLifetime_Calculator      — «всё время»: все программы плюс соло-
//                                тренировки, регулярность, рекорды (§5.3);
//   AStats_Encoder             — JSON-нагрузка кэша по ролям §7.
//
// До их появления всё это было одним классом на 1200 строк, где правка формы
// нагрузки требовала трогать формулы расчёта и наоборот.
//
// Фасад хранит ошибку последнего вызова: подчинённые объекты stateless, но
// сообщают об отказе через свои Get_Last_Error().
//-------------------------------------------------------------------------------------------------------------
class AsProgress_Calculator
{
public:
	AsProgress_Calculator();

	QString Get_Last_Error() const;

	// Полный пересчёт статистики программы по её фактам.
	bool Compute_Program_Stats(const SProgress_Facts &in_facts, SProgram_Stats &out_stats);

	// Инкрементальный пересчёт одной недели: её срез пересчитывается из фактов,
	// итоги заново собираются из массива недель. Требует, чтобы массив недель
	// уже содержал срезы остальных недель (результат полного пересчёта).
	bool Recompute_Week(const SProgress_Facts &in_facts, int in_week_index, SProgram_Stats &in_out_stats);

	// Полный пересчёт общей статистики пользователя по его программам.
	bool Compute_Lifetime_Stats(const SLifetime_Facts &in_facts, const QVector<SProgress_Facts> &in_programs,
	                            SLifetime_Stats &out_stats);

	// JSON-нагрузка кэша (§5.4). Ключи — роли mock-контракта (§7
	// `02-program-model.md`): camelCase, числа без локализации.
	bool Encode_Program_Stats(const SProgram_Stats &in_stats, QString &out_json);
	bool Encode_Lifetime_Stats(const SLifetime_Stats &in_stats, QString &out_json);

private:
	bool Adopt_Error(const QString &in_source_error);

	AsProgram_Stats_Calculator Program_Calculator;
	AsLifetime_Calculator Lifetime_Calculator;
	AStats_Encoder Encoder;
	AError_State Error_State; // без категории: ошибки подчинённых уже залогированы
};
//-------------------------------------------------------------------------------------------------------------
