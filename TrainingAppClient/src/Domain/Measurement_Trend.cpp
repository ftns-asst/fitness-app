#include "Domain/Measurement_Trend.h"

#include <QDate>

//-------------------------------------------------------------------------------------------------------------
// AMeasurement_Trend
//-------------------------------------------------------------------------------------------------------------
static const char *const Param_Texts[] = {"weight", "chest", "waist",     "hip",    "arm",
                                          "thigh",  "belly", "shoulders", "calves", "neck"};
static const char *const Trend_Texts[] = {"unknown", "down", "up", "flat"};
const double AMeasurement_Trend::Flat_Tolerance = 0.05;
//-------------------------------------------------------------------------------------------------------------
QString AMeasurement_Trend::To_String(EMeasurement_Param in_param)
{
	int index = (int)in_param;

	if (index < 0 || index >= (int)EMeasurement_Param::Max)
		return QString();

	return QString::fromLatin1(Param_Texts[index]);
}
//-------------------------------------------------------------------------------------------------------------
bool AMeasurement_Trend::From_String(const QString &in_text, EMeasurement_Param &out_param)
{
	for (int i = 0; i < (int)EMeasurement_Param::Max; ++i)
	{
		if (in_text == QLatin1String(Param_Texts[i]))
		{
			out_param = (EMeasurement_Param)i;

			return true;
		}
	}

	return false;
}
//-------------------------------------------------------------------------------------------------------------
QString AMeasurement_Trend::To_String(ETrend_Kind in_kind)
{
	int index = (int)in_kind;

	if (index < 0 || index >= (int)ETrend_Kind::Max)
		return QString();

	return QString::fromLatin1(Trend_Texts[index]);
}
//-------------------------------------------------------------------------------------------------------------
bool AMeasurement_Trend::Get_Value(const SBody_Measurement &in_measurement, EMeasurement_Param in_param,
                                   double &out_value)
{
	int index = (int)in_param;

	if (index < 0 || index >= (int)EMeasurement_Param::Max)
		return false;

	if (in_param == EMeasurement_Param::Weight)
	{
		if (!in_measurement.Has_Weight_Kg)
			return false;

		out_value = in_measurement.Weight_Kg;

		return true;
	}

	// Обхваты идут следом за весом в том же порядке, что EMeasurement_Key.
	return Get_Measurement_Value(in_measurement, (EMeasurement_Key)(index - 1), out_value);
}
//-------------------------------------------------------------------------------------------------------------
// Вставками: замеров десятки, а сортировка нужна один раз на расчёт. Равные
// даты не переставляются (стабильно), хотя в базе пара (user, date) уникальна.
QVector<SBody_Measurement> AMeasurement_Trend::Sorted_By_Date(const QVector<SBody_Measurement> &in_measurements)
{
	QVector<SBody_Measurement> sorted;
	int position;

	for (int i = 0; i < in_measurements.size(); ++i)
	{
		position = sorted.size();

		while (position > 0 && sorted.at(position - 1).Date > in_measurements.at(i).Date)
			--position;

		sorted.insert(position, in_measurements.at(i));
	}

	return sorted;
}
//-------------------------------------------------------------------------------------------------------------
// Направление активной цели параметра: вес — цель bodyWeightKg, обхват — цель
// measurementCm с этим ключом. Достигнутые и снятые цели не учитываются.
//-------------------------------------------------------------------------------------------------------------
bool AMeasurement_Trend::Find_Goal_Direction(const QVector<SGoal_Target> &in_targets, EMeasurement_Param in_param,
                                             EGoal_Direction &out_direction)
{
	for (int i = 0; i < in_targets.size(); ++i)
	{
		const SGoal_Target &target = in_targets.at(i);

		if (target.State != EGoal_Target_State::Active)
			continue;

		if (in_param == EMeasurement_Param::Weight)
		{
			if (target.Metric != EGoal_Metric::BodyWeightKg)
				continue;
		}
		else if (target.Metric != EGoal_Metric::MeasurementCm || !target.Has_Measurement_Key ||
		         (int)target.Measurement_Key != (int)in_param - 1)
			continue;

		out_direction = target.Direction;

		return true;
	}

	return false;
}
//-------------------------------------------------------------------------------------------------------------
QVector<SMeasurement_Trend> AMeasurement_Trend::Compute_Trends(const QVector<SBody_Measurement> &in_measurements,
                                                               const QString &in_end_date,
                                                               const QVector<SGoal_Target> &in_targets)
{
	QVector<SMeasurement_Trend> trends;
	const QVector<SBody_Measurement> sorted = Sorted_By_Date(in_measurements);
	const QDate end_date = QDate::fromString(in_end_date, Qt::ISODate);
	EGoal_Direction direction = EGoal_Direction::Maintain;
	double value;
	int days_to_end;

	for (int p = 0; p < (int)EMeasurement_Param::Max; ++p)
	{
		SMeasurement_Trend trend;

		trend.Param = (EMeasurement_Param)p;

		for (int i = 0; i < sorted.size(); ++i)
		{
			if (!Get_Value(sorted.at(i), trend.Param, value))
				continue;

			++trend.Points;

			if (!trend.Has_Start)
			{
				trend.Has_Start = true;
				trend.Start = value;
				trend.Start_Date = sorted.at(i).Date;
			}

			trend.Has_Latest = true;
			trend.Latest = value;
			trend.Latest_Date = sorted.at(i).Date;
		}

		// Динамика определена только по двум и более точкам: одно измерение —
		// это «старт», а не изменение.
		if (trend.Points >= 2)
		{
			trend.Has_Delta = true;
			trend.Delta = trend.Latest - trend.Start;
			trend.Days_Passed = QDate::fromString(trend.Start_Date, Qt::ISODate)
			                        .daysTo(QDate::fromString(trend.Latest_Date, Qt::ISODate));

			if (trend.Start != 0)
			{
				trend.Has_Delta_Percent = true;
				trend.Delta_Percent = trend.Delta / trend.Start * 100.0;
			}

			if (trend.Delta > Flat_Tolerance)
				trend.Trend = ETrend_Kind::Up;
			else if (trend.Delta < -Flat_Tolerance)
				trend.Trend = ETrend_Kind::Down;
			else
				trend.Trend = ETrend_Kind::Flat;

			if (trend.Days_Passed > 0)
			{
				trend.Has_Rate = true;
				trend.Rate_Per_Week = trend.Delta / trend.Days_Passed * 7.0;

				// Прогноз — продолжение прямой от последней точки до финиша. После
				// финиша экстраполировать некуда: берётся последнее значение.
				if (end_date.isValid())
				{
					days_to_end = (int)QDate::fromString(trend.Latest_Date, Qt::ISODate).daysTo(end_date);

					if (days_to_end < 0)
						days_to_end = 0;

					trend.Has_Forecast = true;
					trend.Forecast_At_End = trend.Latest + trend.Rate_Per_Week / 7.0 * days_to_end;
				}
			}

			if (Find_Goal_Direction(in_targets, trend.Param, direction))
			{
				trend.Has_Favorable = true;

				if (direction == EGoal_Direction::Increase)
					trend.Is_Favorable = trend.Trend == ETrend_Kind::Up;
				else if (direction == EGoal_Direction::Decrease)
					trend.Is_Favorable = trend.Trend == ETrend_Kind::Down;
				else
					trend.Is_Favorable = trend.Trend == ETrend_Kind::Flat;
			}
		}

		trends.append(trend);
	}

	return trends;
}
//-------------------------------------------------------------------------------------------------------------
bool AMeasurement_Trend::Weight_Average_7(const QVector<SBody_Measurement> &in_measurements, const QString &in_date,
                                          double &out_average, int &out_points)
{
	const QDate date = QDate::fromString(in_date, Qt::ISODate);
	QDate measured;
	double sum = 0;

	out_average = 0;
	out_points = 0;

	if (!date.isValid())
		return false;

	for (int i = 0; i < in_measurements.size(); ++i)
	{
		if (!in_measurements.at(i).Has_Weight_Kg)
			continue;

		measured = QDate::fromString(in_measurements.at(i).Date, Qt::ISODate);

		// Окно — семь календарных дней, оканчивающихся датой: дни без
		// взвешивания не считаются нулями, а просто не входят в среднее.
		if (!measured.isValid() || measured > date || measured.daysTo(date) >= Average_Window_Days)
			continue;

		sum += in_measurements.at(i).Weight_Kg;
		++out_points;
	}

	if (out_points == 0)
		return false;

	out_average = sum / out_points;

	return true;
}
//-------------------------------------------------------------------------------------------------------------
QVector<SWeight_Point> AMeasurement_Trend::Weight_Series(const QVector<SBody_Measurement> &in_measurements)
{
	QVector<SWeight_Point> series;
	const QVector<SBody_Measurement> sorted = Sorted_By_Date(in_measurements);

	for (int i = 0; i < sorted.size(); ++i)
	{
		SWeight_Point point;

		if (!sorted.at(i).Has_Weight_Kg)
			continue;

		point.Date = sorted.at(i).Date;
		point.Weight_Kg = sorted.at(i).Weight_Kg;
		Weight_Average_7(sorted, point.Date, point.Average_7_Kg, point.Average_Points);
		series.append(point);
	}

	return series;
}
//-------------------------------------------------------------------------------------------------------------
QVector<SChange_Row> AMeasurement_Trend::Change_Map(const QVector<SBody_Measurement> &in_measurements)
{
	QVector<SChange_Row> rows;
	const QVector<SBody_Measurement> sorted = Sorted_By_Date(in_measurements);
	const int param_count = (int)EMeasurement_Param::Max;
	QVector<SChange_Cell> start_values(param_count);
	QVector<SChange_Cell> previous_values(param_count);
	double value;

	for (int i = 0; i < sorted.size(); ++i)
	{
		SChange_Row row;

		row.Date = sorted.at(i).Date;
		row.From_Start = QVector<SChange_Cell>(param_count);
		row.From_Previous = QVector<SChange_Cell>(param_count);

		for (int p = 0; p < param_count; ++p)
		{
			if (!Get_Value(sorted.at(i), (EMeasurement_Param)p, value))
				continue;

			// Опорное значение — первое и предыдущее измерение именно этого
			// параметра: частичные замеры не обнуляют карту соседей.
			if (!start_values.at(p).Has_Value)
			{
				start_values[p].Has_Value = true;
				start_values[p].Value = value;
			}

			row.From_Start[p].Has_Value = true;
			row.From_Start[p].Value = value - start_values.at(p).Value;

			if (previous_values.at(p).Has_Value)
			{
				row.From_Previous[p].Has_Value = true;
				row.From_Previous[p].Value = value - previous_values.at(p).Value;
			}

			previous_values[p].Has_Value = true;
			previous_values[p].Value = value;
		}

		rows.append(row);
	}

	return rows;
}
//-------------------------------------------------------------------------------------------------------------
