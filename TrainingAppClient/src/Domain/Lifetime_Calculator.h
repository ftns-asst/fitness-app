#pragma once

#include "Domain/Error_State.h"
#include "Domain/Program_Stats_Calculator.h"
#include "Domain/Progress_Stats.h"

#include <QDate>
#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

//-------------------------------------------------------------------------------------------------------------
// Общая статистика пользователя «всё время» (02-program-model.md §5.3).
//
// Отдельный класс, а не метод калькулятора программы: агрегация идёт по всем
// программам сразу и включает соло-тренировки вне программы (§4 инвариант 18),
// тогда как статистика программы живёт в своём окне сроков. Считать и то и
// другое одним классом означало бы держать в нём две несовместимые модели
// времени.
//
// Сводки программ для сравнения считаются теми же формулами, что и статистика
// программы: сравнивать можно только метрики, посчитанные одинаково (§5.3).
//-------------------------------------------------------------------------------------------------------------
class AsLifetime_Calculator
{
public:
	AsLifetime_Calculator();

	QString Get_Last_Error() const;

	// in_program_calculator передаётся вызывающим: сводки программ должны
	// считаться тем же расчётом, что и экран программы.
	bool Compute(const SLifetime_Facts &in_facts, const QVector<SProgress_Facts> &in_programs,
	             AsProgram_Stats_Calculator *in_program_calculator, SLifetime_Stats &out_stats);

private:
	bool Set_Last_Error(const QString &in_error);
	void Clear_Last_Error();

	void Count_Programs(const QVector<SProgram> &in_programs, SLifetime_Stats &in_out_stats) const;
	void Count_Sessions(const SLifetime_Facts &in_facts, QSet<QString> &out_completed_ids,
	                    QSet<QString> &out_active_dates, QHash<QString, int> &out_week_counts,
	                    SLifetime_Stats &in_out_stats) const;
	void Count_Sets(const SLifetime_Facts &in_facts, const QSet<QString> &in_completed_ids,
	                QHash<QString, SExercise_Record> &in_out_records, QStringList &in_out_record_order,
	                SLifetime_Stats &in_out_stats) const;
	void Build_Regularity(const QHash<QString, int> &in_week_counts, SLifetime_Stats &in_out_stats) const;
	void Resolve_Records(const SLifetime_Facts &in_facts, const QHash<QString, SExercise_Record> &in_records,
	                     const QStringList &in_record_order, SLifetime_Stats &in_out_stats) const;
	void Collect_Body_Weight(const SLifetime_Facts &in_facts, SLifetime_Stats &in_out_stats) const;
	void Build_Summaries(const SLifetime_Facts &in_facts, const QVector<SProgress_Facts> &in_programs,
	                     AsProgram_Stats_Calculator *in_program_calculator, SLifetime_Stats &in_out_stats) const;
	void Fill_Summary_Metrics(const SProgress_Facts &in_facts, AsProgram_Stats_Calculator &in_calculator,
	                          SProgram_Summary &in_out_summary) const;

	AError_State Error_State;
};
//-------------------------------------------------------------------------------------------------------------