#include "Domain/Progress_Calculator.h"

//-------------------------------------------------------------------------------------------------------------
// AsProgress_Calculator — фасад. Здесь нет расчётов: каждый публичный метод
// делегирует подчинённому классу и переносит его сообщение об ошибке в своё
// состояние, чтобы вызывающий по-прежнему читал Get_Last_Error().
//-------------------------------------------------------------------------------------------------------------
AsProgress_Calculator::AsProgress_Calculator()
{
}
//-------------------------------------------------------------------------------------------------------------
QString AsProgress_Calculator::Get_Last_Error() const
{
	return Error_State.Get_Last_Error();
}
//-------------------------------------------------------------------------------------------------------------
// Отказ подчинённого объекта переносится в состояние фасада. Текст ошибки уже
// залогирован самим объектом, поэтому здесь он только переносится.
//-------------------------------------------------------------------------------------------------------------
bool AsProgress_Calculator::Adopt_Error(const QString &in_source_error)
{
	return Error_State.Set_Last_Error(in_source_error);
}
//-------------------------------------------------------------------------------------------------------------
bool AsProgress_Calculator::Compute_Program_Stats(const SProgress_Facts &in_facts, SProgram_Stats &out_stats)
{
	Error_State.Clear_Last_Error();

	if (Program_Calculator.Compute(in_facts, out_stats))
		return true;

	return Adopt_Error(Program_Calculator.Get_Last_Error());
}
//-------------------------------------------------------------------------------------------------------------
bool AsProgress_Calculator::Recompute_Week(const SProgress_Facts &in_facts, int in_week_index,
                                           SProgram_Stats &in_out_stats)
{
	Error_State.Clear_Last_Error();

	if (Program_Calculator.Recompute_Week(in_facts, in_week_index, in_out_stats))
		return true;

	return Adopt_Error(Program_Calculator.Get_Last_Error());
}
//-------------------------------------------------------------------------------------------------------------
bool AsProgress_Calculator::Compute_Lifetime_Stats(const SLifetime_Facts &in_facts,
                                                   const QVector<SProgress_Facts> &in_programs,
                                                   SLifetime_Stats &out_stats)
{
	Error_State.Clear_Last_Error();

	// Сводки программ считаются тем же расчётом, что и экран программы,
	// поэтому калькулятор программы передаётся внутрь (§5.3).
	if (Lifetime_Calculator.Compute(in_facts, in_programs, &Program_Calculator, out_stats))
		return true;

	return Adopt_Error(Lifetime_Calculator.Get_Last_Error());
}
//-------------------------------------------------------------------------------------------------------------
bool AsProgress_Calculator::Encode_Program_Stats(const SProgram_Stats &in_stats, QString &out_json)
{
	Error_State.Clear_Last_Error();

	if (Encoder.Encode_Program_Stats(in_stats, out_json))
		return true;

	return Adopt_Error(Encoder.Get_Last_Error());
}
//-------------------------------------------------------------------------------------------------------------
bool AsProgress_Calculator::Encode_Lifetime_Stats(const SLifetime_Stats &in_stats, QString &out_json)
{
	Error_State.Clear_Last_Error();

	if (Encoder.Encode_Lifetime_Stats(in_stats, out_json))
		return true;

	return Adopt_Error(Encoder.Get_Last_Error());
}
//-------------------------------------------------------------------------------------------------------------