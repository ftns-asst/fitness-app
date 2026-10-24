#include "Domain/Progress_Support.h"

#include <QVector>

//----------------------------------------------------------------------------
void SExtremes_Value::Add(const QString &in_date, double in_value)
{
	if (Has_First == false || in_date < First_Date)
	{
		First_Date = in_date;
		First_Value = in_value;
		Has_First = true;
	}

	if (Has_Last == false || in_date > Last_Date)
	{
		Last_Date = in_date;
		Last_Value = in_value;
		Has_Last = true;
	}
}
//----------------------------------------------------------------------------
bool SExtremes_Value::Has_Both() const
{
	return Has_First && Has_Last;
}
//----------------------------------------------------------------------------
double SExtremes_Value::Delta() const
{
	if (Has_Both() == false)
		return 0;

	return Last_Value - First_Value;
}
//----------------------------------------------------------------------------
int Week_Index(const QDate &in_start, const QDate &in_date)
{
	if (in_start.isValid() == false || in_date.isValid() == false || in_date < in_start)
		return 0;

	return int(in_start.daysTo(in_date) / 7) + 1;
}
//----------------------------------------------------------------------------
QDate Week_Start(const QDate &in_date)
{
	return in_date.addDays(1 - in_date.dayOfWeek());
}
//----------------------------------------------------------------------------
bool In_Window(const QDate &in_date, const QDate &in_start, const QDate &in_end)
{
	return in_date.isValid() && in_date >= in_start && in_date <= in_end;
}
//----------------------------------------------------------------------------
QString Session_Date(const SWorkout_Session &in_session)
{
	if (in_session.Date.isEmpty() == false)
		return in_session.Date;

	if (in_session.Started_At.size() >= 10)
		return in_session.Started_At.left(10);

	return QString();
}
//----------------------------------------------------------------------------
double E1rm(const SSet_Log &in_set)
{
	return in_set.Weight_Kg * (1 + in_set.Reps / 30.0);
}
//----------------------------------------------------------------------------
// Подход засчитывается в объёме: выполненный рабочий подход. Проверка
// наличия упражнения намеренно не входит: у недельного среза и у счёта
// силы разные условия отбора (§5.4), и смешивать их здесь нельзя.
//----------------------------------------------------------------------------
bool Set_Counts_In_Volume(const SSet_Log &in_set)
{
	return in_set.Done && in_set.Is_Warmup == false;
}
//----------------------------------------------------------------------------
double Set_Tonnage(const SSet_Log &in_set)
{
	if (Set_Counts_In_Volume(in_set) == false)
		return 0;

	return in_set.Weight_Kg * in_set.Reps;
}
//----------------------------------------------------------------------------
bool Session_Average_Rpe(const SWorkout_Session &in_session, const QVector<SSet_Log> &in_sets, double &out_average)
{
	double sum = 0;
	int count = 0;

	if (in_session.Has_Avg_Rpe)
	{
		out_average = in_session.Avg_Rpe;

		return true;
	}

	for (int i = 0; i < in_sets.size(); ++i)
	{
		// Разминочные подходы в нагрузку не входят (§3.8).
		if (in_sets.at(i).Has_Rpe == false || in_sets.at(i).Is_Warmup)
			continue;

		sum += in_sets.at(i).Rpe;
		++count;
	}

	if (count == 0)
		return false;

	out_average = sum / count;

	return true;
}
//----------------------------------------------------------------------------