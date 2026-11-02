#include "Domain/Measurement_Schedule.h"

#include <QDate>

//-------------------------------------------------------------------------------------------------------------
// AMeasurement_Schedule
//-------------------------------------------------------------------------------------------------------------
bool AMeasurement_Schedule::Is_Valid_Interval(int in_interval_days)
{
	return in_interval_days >= Min_Interval_Days && in_interval_days <= Max_Interval_Days;
}
//-------------------------------------------------------------------------------------------------------------
QString AMeasurement_Schedule::Start_Date_Of(const SProgram &in_program)
{
	const QString text =
	    in_program.Actual_Start_Date.isEmpty() ? in_program.Term.Start_Date : in_program.Actual_Start_Date;
	const QDate start = QDate::fromString(text, Qt::ISODate);

	return start.isValid() ? start.toString("yyyy-MM-dd") : QString();
}
//-------------------------------------------------------------------------------------------------------------
QString AMeasurement_Schedule::End_Date_Of(const SProgram &in_program)
{
	const QDate start = QDate::fromString(Start_Date_Of(in_program), Qt::ISODate);

	if (!start.isValid() || in_program.Term.Mode != ETerm_Mode::FixedWeeks || in_program.Term.Weeks_Total <= 0)
		return QString();

	return start.addDays(in_program.Term.Weeks_Total * 7 - 1).toString("yyyy-MM-dd");
}
//-------------------------------------------------------------------------------------------------------------
QStringList AMeasurement_Schedule::Planned_Dates(const QString &in_start_date, const QString &in_end_date,
                                                 int in_interval_days)
{
	QStringList dates;
	QDate start = QDate::fromString(in_start_date, Qt::ISODate);
	QDate end = QDate::fromString(in_end_date, Qt::ISODate);
	QDate current;

	if (!Is_Valid_Interval(in_interval_days) || !start.isValid() || !end.isValid() || end < start)
		return dates;

	for (current = start; current <= end; current = current.addDays(in_interval_days))
		dates.append(current.toString("yyyy-MM-dd"));

	// Финиш — всегда последняя точка, даже если он не попал в сетку шага.
	if (dates.last() != end.toString("yyyy-MM-dd"))
		dates.append(end.toString("yyyy-MM-dd"));

	return dates;
}
//-------------------------------------------------------------------------------------------------------------
bool AMeasurement_Schedule::Next_Date(const QString &in_start_date, const QString &in_end_date, int in_interval_days,
                                      const QString &in_from_date, QString &out_date)
{
	QDate start = QDate::fromString(in_start_date, Qt::ISODate);
	QDate end = QDate::fromString(in_end_date, Qt::ISODate);
	QDate from = QDate::fromString(in_from_date, Qt::ISODate);
	QDate candidate;
	qint64 offset;
	qint64 steps;

	out_date.clear();

	if (!Is_Valid_Interval(in_interval_days) || !start.isValid() || !from.isValid())
		return false;

	if (from <= start)
		candidate = start;
	else
	{ // Округление вверх до сетки шага от старта.
		offset = start.daysTo(from);
		steps = (offset + in_interval_days - 1) / in_interval_days;
		candidate = start.addDays(steps * in_interval_days);
	}

	if (end.isValid())
	{
		if (end < start || from > end)
			return false;

		// Последний отрезок до финиша: финиш ближе следующей точки сетки.
		if (candidate > end)
			candidate = end;
	}

	out_date = candidate.toString("yyyy-MM-dd");

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AMeasurement_Schedule::Previous_Date(const QString &in_start_date, const QString &in_end_date,
                                          int in_interval_days, const QString &in_to_date, QString &out_date)
{
	QDate start = QDate::fromString(in_start_date, Qt::ISODate);
	QDate end = QDate::fromString(in_end_date, Qt::ISODate);
	QDate to = QDate::fromString(in_to_date, Qt::ISODate);
	QDate candidate;
	qint64 steps;

	out_date.clear();

	if (!Is_Valid_Interval(in_interval_days) || !start.isValid() || !to.isValid() || to < start)
		return false;

	if (end.isValid())
	{
		if (end < start)
			return false;

		// После финиша последняя точка — сам финиш.
		if (to >= end)
		{
			out_date = end.toString("yyyy-MM-dd");

			return true;
		}
	}

	steps = start.daysTo(to) / in_interval_days;
	candidate = start.addDays(steps * in_interval_days);
	out_date = candidate.toString("yyyy-MM-dd");

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AMeasurement_Schedule::Is_Due(const QString &in_start_date, const QString &in_end_date, int in_interval_days,
                                   const QString &in_today, const QString &in_last_measurement_date)
{
	QString planned;

	if (!Previous_Date(in_start_date, in_end_date, in_interval_days, in_today, planned))
		return false;

	// ISO-даты сравниваются как строки: порядок символов совпадает с порядком дат.
	return in_last_measurement_date.isEmpty() || in_last_measurement_date < planned;
}
//-------------------------------------------------------------------------------------------------------------
