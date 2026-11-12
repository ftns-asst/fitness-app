#include "Domain/Dashboard_Calculator.h"

#include <QDate>
#include <QtMath>

//-------------------------------------------------------------------------------------------------------------
QVariantMap ADashboard_Calculator::Compute(const SProgram &in_program, const QVector<SGoal_Target> &in_targets,
                                           const QVector<SBody_Measurement> &in_measurements, const QString &in_today,
                                           int in_paused_days, const QString &in_paused_on)
{
	QVariantMap result;
	const QStringList nullable_keys = {"currentDay",     "totalDays",       "daysLeft",      "termProgress",
	                                   "startWeightKg",  "currentWeightKg", "weightDeltaKg", "weightDeltaPercent",
	                                   "targetWeightKg", "remainingKg",     "favorable",     "waistCm",
	                                   "bellyCm",        "average7Kg"};
	for (const QString &key : nullable_keys)
		result.insert(key, QVariant());
	const QString start_text =
	    in_program.Actual_Start_Date.isEmpty() ? in_program.Term.Start_Date : in_program.Actual_Start_Date;
	result.insert("startDate", start_text);
	const QDate start = QDate::fromString(start_text, Qt::ISODate);
	const QDate today = QDate::fromString(in_today, Qt::ISODate);
	QDate clock_date = today;
	if (in_program.State == EProgram_State::Paused)
	{
		// Старые записи не содержат историю пауз: дата изменения — запасная
		// опора только для текущей паузы. Для возобновления нужен in_paused_days.
		const QString paused_text = in_paused_on.isEmpty() ? in_program.Updated_At.left(10) : in_paused_on;
		const QDate paused = QDate::fromString(paused_text, Qt::ISODate);
		if (paused.isValid() && paused < clock_date)
			clock_date = paused;
		else if (!paused.isValid())
			clock_date = QDate();
	}
	if (start.isValid() && clock_date.isValid())
	{
		int day = qMax(0, (int)start.daysTo(clock_date) + 1 - qMax(0, in_paused_days));
		if (in_program.Term.Mode == ETerm_Mode::FixedWeeks && in_program.Term.Weeks_Total > 0)
		{
			const int total = in_program.Term.Weeks_Total * 7;
			day = qMin(day, total);
			result.insert("totalDays", total);
			result.insert("daysLeft", total - day);
			result.insert("termProgress", (double)day / total);
		}
		result.insert("currentDay", day);
	}

	const SGoal_Target *weight_goal = 0;
	for (const SGoal_Target &target : in_targets)
	{
		if (target.Metric == EGoal_Metric::BodyWeightKg && target.State != EGoal_Target_State::Dropped)
		{
			weight_goal = &target;
			break;
		}
	}
	QString first_weight_date;
	QString latest_weight_date;
	QString waist_date;
	QString belly_date;
	double first_weight = 0;
	double recent_weight_sum = 0;
	int recent_weight_count = 0;
	for (const SBody_Measurement &measurement : in_measurements)
	{
		const QDate date = QDate::fromString(measurement.Date, Qt::ISODate);
		if (measurement.User_Id != in_program.User_Id || !date.isValid() || !today.isValid() || !start.isValid() ||
		    date < start || date > today)
			continue;
		if (measurement.Has_Weight_Kg && qIsFinite(measurement.Weight_Kg))
		{
			if (date >= today.addDays(-6))
			{
				recent_weight_sum += measurement.Weight_Kg;
				++recent_weight_count;
			}
			if (first_weight_date.isEmpty() || measurement.Date < first_weight_date)
			{
				first_weight_date = measurement.Date;
				first_weight = measurement.Weight_Kg;
			}
			if (latest_weight_date.isEmpty() || measurement.Date > latest_weight_date)
			{
				latest_weight_date = measurement.Date;
				result.insert("currentWeightKg", measurement.Weight_Kg);
				result.insert("currentWeightDate", measurement.Date);
			}
		}
		if (measurement.Has_Waist_Cm && qIsFinite(measurement.Waist_Cm) && measurement.Date > waist_date)
		{
			waist_date = measurement.Date;
			result.insert("waistCm", measurement.Waist_Cm);
		}
		if (measurement.Has_Belly_Cm && qIsFinite(measurement.Belly_Cm) && measurement.Date > belly_date)
		{
			belly_date = measurement.Date;
			result.insert("bellyCm", measurement.Belly_Cm);
		}
	}
	result.insert("weightReadings7d", recent_weight_count);
	if (recent_weight_count > 0)
		result.insert("average7Kg", recent_weight_sum / recent_weight_count);
	if (!first_weight_date.isEmpty())
		result.insert("startWeightKg", first_weight);
	if (weight_goal != 0)
	{
		result.insert("targetWeightKg", weight_goal->Target_Value);
		result.insert("direction", To_String(weight_goal->Direction));
		if (weight_goal->Has_Start_Value)
			result.insert("startWeightKg", weight_goal->Start_Value);
	}
	const QVariant current = result.value("currentWeightKg");
	const QVariant baseline = result.value("startWeightKg");
	// Единственный замер без явного старта не доказывает отсутствие изменения.
	if (!current.isNull() && !baseline.isNull() &&
	    ((weight_goal != 0 && weight_goal->Has_Start_Value) || first_weight_date != latest_weight_date))
	{
		const double delta = current.toDouble() - baseline.toDouble();
		result.insert("weightDeltaKg", delta);
		if (baseline.toDouble() != 0)
			result.insert("weightDeltaPercent", delta / baseline.toDouble() * 100);
		if (weight_goal != 0)
		{
			bool favorable = delta == 0;
			if (weight_goal->Direction == EGoal_Direction::Decrease)
				favorable = delta <= 0;
			else if (weight_goal->Direction == EGoal_Direction::Increase)
				favorable = delta >= 0;
			else
				favorable = qAbs(current.toDouble() - weight_goal->Target_Value) <=
				            qAbs(baseline.toDouble() - weight_goal->Target_Value);
			result.insert("favorable", favorable);
		}
	}
	if (!current.isNull() && weight_goal != 0)
	{
		double remaining = weight_goal->Target_Value - current.toDouble();
		if (weight_goal->Direction == EGoal_Direction::Decrease)
			remaining = -remaining;
		else if (weight_goal->Direction == EGoal_Direction::Maintain)
			remaining = qAbs(remaining);
		result.insert("remainingKg", qMax(0.0, remaining));
	}

	return result;
}
//-------------------------------------------------------------------------------------------------------------
QVariant ADashboard_Calculator::Completion_Progress(const QVariant &in_term_progress, int in_completed, int in_planned)
{
	double sum = 0;
	int count = 0;
	if (!in_term_progress.isNull())
	{
		sum += qBound(0.0, in_term_progress.toDouble(), 1.0);
		++count;
	}
	if (in_planned > 0)
	{
		sum += qBound(0.0, (double)in_completed / in_planned, 1.0);
		++count;
	}
	if (count == 0)
		return QVariant();

	return sum / count;
}
//-------------------------------------------------------------------------------------------------------------
