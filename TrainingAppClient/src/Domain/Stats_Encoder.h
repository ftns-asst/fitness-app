#pragma once

#include "Domain/Error_State.h"
#include "Domain/Progress_Stats.h"

#include <QJsonObject>
#include <QString>

//-------------------------------------------------------------------------------------------------------------
// Сериализация статистики в JSON-нагрузку кэша (§5.4).
//
// Отдельный класс, потому что у него другая причина для изменения: не формулы
// расчёта, а контракт ролей mock-данных (§7 `02-program-model.md`). Смешанный
// с расчётом класс приходилось бы править целиком при каждом изменении формы
// нагрузки.
//
// Правило кодирования (§5.5): метрика без данных кодируется как JSON null, а не
// нулём — «нечего считать» и «ноль выполнено» на экране различаются.
//-------------------------------------------------------------------------------------------------------------
class AStats_Encoder
{
public:
	AStats_Encoder();

	QString Get_Last_Error() const;

	bool Encode_Program_Stats(const SProgram_Stats &in_stats, QString &out_json);
	bool Encode_Lifetime_Stats(const SLifetime_Stats &in_stats, QString &out_json);

private:
	bool Set_Last_Error(const QString &in_error);
	void Clear_Last_Error();

	QJsonObject Encode_Program_Scalars(const SProgram_Stats &in_stats) const;
	QJsonObject Encode_Week(const SWeek_Aggregate &in_week) const;
	QJsonObject Encode_Strength(const SExercise_Strength &in_strength) const;
	QJsonObject Encode_Goal(const SGoal_Progress &in_goal) const;
	QJsonObject Encode_Delta(const SMeasurement_Delta &in_delta) const;
	QJsonObject Encode_Rule(const SRule_Stat_Row &in_rule) const;
	QJsonObject Encode_Lifetime_Scalars(const SLifetime_Stats &in_stats) const;
	QJsonObject Encode_Regularity(const SRegularity_Week &in_week) const;
	QJsonObject Encode_Record(const SExercise_Record &in_record) const;
	QJsonObject Encode_Summary(const SProgram_Summary &in_summary) const;

	AError_State Error_State;
};
//-------------------------------------------------------------------------------------------------------------