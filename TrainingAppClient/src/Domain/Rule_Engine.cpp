#include "Domain/Rule_Engine.h"

#include <QDate>
#include <QDateTime>
#include <QLoggingCategory>

#include <cmath>

Q_LOGGING_CATEGORY(RULE_Engine_Log, "trainingapp.program.rule")

//----------------------------------------------------------------------------
// Допуск точной цели (exact): вес и часы измеряются с шагом 0,5, поэтому
// «ориентир» считается достигнутым в пределах половины шага.
//----------------------------------------------------------------------------
static const double Exact_Target_Tolerance = 0.5;

//----------------------------------------------------------------------------
// Проверка канонической ISO-даты «YYYY-MM-DD».
//----------------------------------------------------------------------------
static bool Is_Iso_Date(const QString &in_text)
{
	QDate date = QDate::fromString(in_text, Qt::ISODate);

	return date.isValid() && date.toString("yyyy-MM-dd") == in_text;
}

// AsRule_Engine
//----------------------------------------------------------------------------
AsRule_Engine::AsRule_Engine() : Error_State(&RULE_Engine_Log())
{
}
//----------------------------------------------------------------------------
QString AsRule_Engine::Get_Last_Error() const
{
	return Error_State.Get_Last_Error();
}
//----------------------------------------------------------------------------
bool AsRule_Engine::Set_Last_Error(const QString &in_error)
{
	return Error_State.Set_Last_Error(in_error);
}
//----------------------------------------------------------------------------
void AsRule_Engine::Clear_Last_Error()
{
	Error_State.Clear_Last_Error();
}
//----------------------------------------------------------------------------
bool AsRule_Engine::Is_Rule_Active(const SProgram_Rule &in_rule, const QString &in_date)
{
	QString date = in_date.trimmed();

	Clear_Last_Error();

	if (Is_Iso_Date(date) == false)
		return Set_Last_Error(QString("Некорректная дата правила: %1").arg(in_date));

	if (in_rule.Active_From.isEmpty() == false)
	{
		if (Is_Iso_Date(in_rule.Active_From) == false)
			return Set_Last_Error(QString("Некорректная дата начала правила: %1").arg(in_rule.Active_From));

		if (date < in_rule.Active_From)
			return false;
	}

	if (in_rule.Active_To.isEmpty() == false)
	{
		if (Is_Iso_Date(in_rule.Active_To) == false)
			return Set_Last_Error(QString("Некорректная дата окончания правила: %1").arg(in_rule.Active_To));

		if (date > in_rule.Active_To)
			return false;
	}

	return true;
}
//----------------------------------------------------------------------------
bool AsRule_Engine::Evaluate_Value(const SProgram_Rule &in_rule, double in_value, ECheckin_State &out_state)
{
	Clear_Last_Error();

	if (std::isfinite(in_value) == false)
		return Set_Last_Error("Значение отметки должно быть конечным числом");

	// Рекомендация отвечает на вопрос «что делать», а не «сколько»: вердикт
	// по числу для неё выдуман, и молча вернуть «выполнено» значило бы
	// сообщить вызывающему, что цель достигнута, хотя цели нет.
	if (Get_Rule_Kind(in_rule) == ERule_Kind::Advice)
		return Set_Last_Error(QString("Рекомендация %1 не измерима: оценивать её не по чему").arg(in_rule.ID));

	if (in_rule.Has_Target_Min && in_rule.Has_Target_Max && in_rule.Target_Min > in_rule.Target_Max)
		return Set_Last_Error(QString("Минимум цели правила %1 превышает максимум").arg(in_rule.ID));

	if (in_rule.Has_Target_Exact)
	{
		out_state = std::fabs(in_value - in_rule.Target_Exact) <= Exact_Target_Tolerance ? ECheckin_State::Done
		                                                                                 : ECheckin_State::Failed;

		return true;
	}

	if (in_rule.Has_Target_Min && in_rule.Has_Target_Max)
	{
		out_state = (in_value >= in_rule.Target_Min && in_value <= in_rule.Target_Max) ? ECheckin_State::Done
		                                                                               : ECheckin_State::Failed;

		return true;
	}

	if (in_rule.Has_Target_Min)
	{
		out_state = in_value >= in_rule.Target_Min ? ECheckin_State::Done : ECheckin_State::Failed;

		return true;
	}

	if (in_rule.Has_Target_Max)
	{
		out_state = in_value <= in_rule.Target_Max ? ECheckin_State::Done : ECheckin_State::Failed;

		return true;
	}

	return Set_Last_Error(QString("У правила %1 не задано целевое значение").arg(in_rule.ID));
}
//----------------------------------------------------------------------------
bool AsRule_Engine::Evaluate_Checkin(const SProgram_Rule &in_rule, const SRule_Checkin &in_checkin,
                                     ECheckin_State &out_state)
{
	Clear_Last_Error();

	if (in_checkin.State == ECheckin_State::Skipped || in_checkin.State == ECheckin_State::Partial)
	{
		out_state = in_checkin.State;

		return true;
	}

	if (in_checkin.Has_Value == false)
	{
		out_state = in_checkin.State;

		return true;
	}

	return Evaluate_Value(in_rule, in_checkin.Value, out_state);
}
//----------------------------------------------------------------------------
bool AsRule_Engine::Build_Computed_Checkin(const SProgram_Rule &in_rule, const SWorkout_Session &in_session,
                                           const QVector<SSet_Log> &in_sets,
                                           const QVector<SWorkout_Session> &in_earlier_sessions,
                                           SRule_Checkin &out_checkin)
{
	double value = 0;
	bool has_value = false;
	SRule_Checkin checkin;

	Clear_Last_Error();
	out_checkin = SRule_Checkin();

	if (in_rule.ID.isEmpty())
		return Set_Last_Error("Расчётная отметка требует правило с id");

	if (in_session.ID.isEmpty() || Is_Iso_Date(in_session.Date) == false)
		return Set_Last_Error("Расчётная отметка требует тренировку с id и датой");

	checkin.Rule_Id = in_rule.ID;
	checkin.Program_Id = in_session.Program_Id;
	checkin.Date = in_session.Date;
	checkin.Unit = To_String(in_rule.Display_Unit);
	checkin.Source = ECheckin_Source::Computed;
	checkin.Session_Id = in_session.ID;
	checkin.State = ECheckin_State::Unknown;

	if (in_rule.Check_Source == ERule_Check_Source::ComputedFromSets)
	{
		if (in_rule.Metric == ERule_Metric::MaxRpe)
			Compute_Max_Rpe_Value(in_sets, value, has_value);
		else if (in_rule.Metric == ERule_Metric::Custom)
			Compute_Progression_Value(in_sets, value, has_value);
		else
			return Set_Last_Error(QString("Метрика %1 не вычисляется из подходов").arg(To_String(in_rule.Metric)));
	}
	else if (in_rule.Check_Source == ERule_Check_Source::ComputedFromSessions)
	{
		if (in_rule.Metric == ERule_Metric::RestHoursBetweenSessions || in_rule.Metric == ERule_Metric::Custom)
			Compute_Rest_Hours(in_session, in_earlier_sessions, value, has_value);
		else
			return Set_Last_Error(QString("Метрика %1 не вычисляется из сессий").arg(To_String(in_rule.Metric)));
	}
	else
	{
		return Set_Last_Error(QString("Источник %1 требует ручной отметки").arg(To_String(in_rule.Check_Source)));
	}

	if (has_value)
	{
		ECheckin_State state = ECheckin_State::Unknown;

		if (Evaluate_Value(in_rule, value, state) == false)
			return false;

		checkin.Value = value;
		checkin.Has_Value = true;
		checkin.State = state;
	}

	out_checkin = checkin;

	return true;
}
//----------------------------------------------------------------------------
bool AsRule_Engine::Compute_Rule_Stats(const SProgram_Rule &in_rule, const QVector<SRule_Checkin> &in_checkins,
                                       SRule_Stats &out_stats)
{
	double value_sum = 0;
	int value_count = 0;

	Clear_Last_Error();
	out_stats = SRule_Stats();

	if (in_rule.ID.isEmpty())
		return Set_Last_Error("Сводка правил требует правило с id");

	// Отметка рекомендации — это «прочитал совет», а не результат измерения.
	// Считать её исполнением или неисполнением нельзя: иначе процент
	// выполнения менялся бы от того, что человек отметил текст, и менялся бы
	// вследствие поступка, который ничего не измеряет.
	if (Get_Rule_Kind(in_rule) == ERule_Kind::Advice)
		return true;

	for (int i = 0; i < in_checkins.size(); ++i)
	{
		const SRule_Checkin &checkin = in_checkins.at(i);
		ECheckin_State state = ECheckin_State::Unknown;

		if (checkin.Rule_Id != in_rule.ID)
			continue;

		if (Evaluate_Checkin(in_rule, checkin, state) == false)
			return false;

		Accumulate_State(state, out_stats);

		if (checkin.Has_Value)
		{
			value_sum += checkin.Value;
			++value_count;
		}
	}

	if (value_count > 0)
	{
		out_stats.Average_Value = value_sum / value_count;
		out_stats.Has_Average_Value = true;
	}

	Finalize_Stats(out_stats);

	return true;
}
//----------------------------------------------------------------------------
bool AsRule_Engine::Compute_Overall_Stats(const QVector<SProgram_Rule> &in_rules,
                                          const QVector<SRule_Checkin> &in_checkins, SRule_Stats &out_stats)
{
	SRule_Stats total;

	Clear_Last_Error();
	out_stats = SRule_Stats();

	if (in_rules.isEmpty())
		return Set_Last_Error("Сводка правил требует хотя бы одно правило");

	for (int i = 0; i < in_rules.size(); ++i)
	{
		SRule_Stats rule_stats;

		if (Compute_Rule_Stats(in_rules.at(i), in_checkins, rule_stats) == false)
			return false;

		total.Done_Count += rule_stats.Done_Count;
		total.Failed_Count += rule_stats.Failed_Count;
		total.Skipped_Count += rule_stats.Skipped_Count;
		total.Partial_Count += rule_stats.Partial_Count;
		total.Unknown_Count += rule_stats.Unknown_Count;
	}

	Finalize_Stats(total);

	out_stats = total;

	return true;
}
//----------------------------------------------------------------------------
void AsRule_Engine::Compute_Max_Rpe_Value(const QVector<SSet_Log> &in_sets, double &out_value,
                                          bool &out_has_value) const
{
	out_value = 0;
	out_has_value = false;

	for (int i = 0; i < in_sets.size(); ++i)
	{
		const SSet_Log &set = in_sets.at(i);

		if (set.Is_Warmup || set.Done == false || set.Has_Rpe == false)
			continue;

		if (out_has_value == false || set.Rpe > out_value)
		{
			out_value = set.Rpe;
			out_has_value = true;
		}
	}
}
//----------------------------------------------------------------------------
void AsRule_Engine::Compute_Progression_Value(const QVector<SSet_Log> &in_sets, double &out_value,
                                              bool &out_has_value) const
{
	int planned = 0;
	int reached = 0;

	out_value = 0;
	out_has_value = false;

	for (int i = 0; i < in_sets.size(); ++i)
	{
		const SSet_Log &set = in_sets.at(i);

		if (set.Is_Warmup || set.Has_Target_Reps == false)
			continue;

		++planned;

		if (set.Done && set.Reps >= set.Target_Reps)
			++reached;
	}

	if (planned == 0)
		return;

	out_value = reached * 100.0 / planned;
	out_has_value = true;
}
//----------------------------------------------------------------------------
void AsRule_Engine::Compute_Rest_Hours(const SWorkout_Session &in_session,
                                       const QVector<SWorkout_Session> &in_earlier_sessions, double &out_value,
                                       bool &out_has_value) const
{
	QDateTime current = QDateTime::fromString(in_session.Started_At, Qt::ISODate);
	QDateTime latest_end;

	out_value = 0;
	out_has_value = false;

	if (current.isValid() == false)
		return;

	for (int i = 0; i < in_earlier_sessions.size(); ++i)
	{
		const SWorkout_Session &earlier = in_earlier_sessions.at(i);
		QDateTime end = QDateTime::fromString(earlier.Completed_At, Qt::ISODate);

		if (end.isValid() == false)
			end = QDateTime::fromString(earlier.Started_At, Qt::ISODate);

		if (end.isValid() == false || end >= current)
			continue;

		if (latest_end.isValid() == false || end > latest_end)
			latest_end = end;
	}

	if (latest_end.isValid() == false)
		return;

	out_value = latest_end.secsTo(current) / 3600.0;
	out_has_value = true;
}
//----------------------------------------------------------------------------
void AsRule_Engine::Accumulate_State(ECheckin_State in_state, SRule_Stats &out_stats) const
{
	switch (in_state)
	{
	case ECheckin_State::Done:
		++out_stats.Done_Count;
		break;

	case ECheckin_State::Failed:
		++out_stats.Failed_Count;
		break;

	case ECheckin_State::Skipped:
		++out_stats.Skipped_Count;
		break;

	case ECheckin_State::Partial:
		++out_stats.Partial_Count;
		break;

	default:
		++out_stats.Unknown_Count;
		break;
	}
}
//----------------------------------------------------------------------------
void AsRule_Engine::Finalize_Stats(SRule_Stats &out_stats) const
{
	out_stats.Evaluated_Count =
	    out_stats.Done_Count + out_stats.Failed_Count + out_stats.Skipped_Count + out_stats.Partial_Count;
	out_stats.Has_Adherence_Percent = out_stats.Evaluated_Count > 0;
	out_stats.Adherence_Percent =
	    out_stats.Has_Adherence_Percent ? out_stats.Done_Count * 100.0 / out_stats.Evaluated_Count : 0;
}
//----------------------------------------------------------------------------
