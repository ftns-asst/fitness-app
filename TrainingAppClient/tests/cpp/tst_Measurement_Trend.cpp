#include "Domain/Measurement_Trend.h"

#include <QtTest>

// Динамика тела (FR-T8…T11, docs/09). Эталон — ручной расчёт по данным в духе
// книги: вес 89,0 → 88,2 → 87,1 за 1, 4 и 7 октября, финиш 28 октября.
class AMeasurement_Trend_Test : public QObject
{
	Q_OBJECT

private slots:
	void paramNamesMatchMeasurementKeys();
	void weightRateAndForecastMatchHandCalculation();
	void waistAndSinglePointAreHandledSeparately();
	void noDataGivesNothingNotZero();
	void flatChangeIsNotATrend();
	void forecastNeedsFinishAndStopsAfterIt();
	void favorableFollowsGoalDirection();
	void inputOrderDoesNotMatter();
	void averageOverSevenCalendarDays();
	void weightSeriesCarriesAverage();
	void changeMapIsRelativeToStartAndPrevious();

private:
	static SBody_Measurement Make(const QString &in_date, double in_weight, double in_waist = -1, double in_neck = -1);
	static QVector<SBody_Measurement> Tracker();
	static const SMeasurement_Trend &Trend_Of(const QVector<SMeasurement_Trend> &in_trends,
	                                          EMeasurement_Param in_param);
};

//-------------------------------------------------------------------------------------------------------------
SBody_Measurement AMeasurement_Trend_Test::Make(const QString &in_date, double in_weight, double in_waist,
                                                double in_neck)
{
	SBody_Measurement measurement;

	measurement.Date = in_date;

	if (in_weight > 0)
	{
		measurement.Weight_Kg = in_weight;
		measurement.Has_Weight_Kg = true;
	}

	if (in_waist > 0)
		Set_Measurement_Value(measurement, EMeasurement_Key::Waist, in_waist);

	if (in_neck > 0)
		Set_Measurement_Value(measurement, EMeasurement_Key::Neck, in_neck);

	return measurement;
}
//-------------------------------------------------------------------------------------------------------------
QVector<SBody_Measurement> AMeasurement_Trend_Test::Tracker()
{
	QVector<SBody_Measurement> measurements;

	measurements.append(Make("2026-10-01", 89.0, 100.0));
	measurements.append(Make("2026-10-04", 88.2, 99.0));
	measurements.append(Make("2026-10-07", 87.1, 98.0, 40.0));

	return measurements;
}
//-------------------------------------------------------------------------------------------------------------
const SMeasurement_Trend &AMeasurement_Trend_Test::Trend_Of(const QVector<SMeasurement_Trend> &in_trends,
                                                            EMeasurement_Param in_param)
{
	return in_trends.at((int)in_param);
}
//-------------------------------------------------------------------------------------------------------------
// Get_Value держится на совпадении порядка: параметр N+1 — это ключ N. Имена
// двух перечислений обязаны совпадать, иначе обхваты перепутаются.
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Trend_Test::paramNamesMatchMeasurementKeys()
{
	EMeasurement_Param param;

	QCOMPARE(AMeasurement_Trend::To_String(EMeasurement_Param::Weight), QString("weight"));

	for (int p = 1; p < (int)EMeasurement_Param::Max; ++p)
	{
		QCOMPARE(AMeasurement_Trend::To_String((EMeasurement_Param)p), To_String((EMeasurement_Key)(p - 1)));
		QVERIFY(AMeasurement_Trend::From_String(To_String((EMeasurement_Key)(p - 1)), param));
		QCOMPARE((int)param, p);
	}

	QVERIFY(!AMeasurement_Trend::From_String("elbow", param));
	QCOMPARE((int)EMeasurement_Param::Max, (int)EMeasurement_Key::Max + 1);
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Trend_Test::weightRateAndForecastMatchHandCalculation()
{
	const QVector<SMeasurement_Trend> trends =
	    AMeasurement_Trend::Compute_Trends(Tracker(), "2026-10-28", QVector<SGoal_Target>());
	const SMeasurement_Trend &weight = Trend_Of(trends, EMeasurement_Param::Weight);

	QCOMPARE(trends.size(), (int)EMeasurement_Param::Max);
	QCOMPARE(weight.Points, 3);
	QVERIFY(weight.Has_Start && weight.Has_Latest && weight.Has_Delta);
	QCOMPARE(weight.Start, 89.0);
	QCOMPARE(weight.Latest, 87.1);
	QCOMPARE(weight.Start_Date, QString("2026-10-01"));
	QCOMPARE(weight.Latest_Date, QString("2026-10-07"));
	QCOMPARE(weight.Delta, -1.9);
	QCOMPARE(weight.Days_Passed, 6);
	QVERIFY(weight.Has_Delta_Percent);
	QCOMPARE(weight.Delta_Percent, -1.9 / 89.0 * 100.0);
	QCOMPARE(weight.Trend, ETrend_Kind::Down);

	// Темп: −1,9 кг за 6 дней = −2,2167 кг в неделю. До финиша 21 день:
	// 87,1 − 1,9 / 6 × 21 = 80,45 кг.
	QVERIFY(weight.Has_Rate);
	QCOMPARE(weight.Rate_Per_Week, -1.9 / 6.0 * 7.0);
	QVERIFY(weight.Has_Forecast);
	QCOMPARE(weight.Forecast_At_End, 80.45);
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Trend_Test::waistAndSinglePointAreHandledSeparately()
{
	const QVector<SMeasurement_Trend> trends =
	    AMeasurement_Trend::Compute_Trends(Tracker(), "2026-10-28", QVector<SGoal_Target>());
	const SMeasurement_Trend &waist = Trend_Of(trends, EMeasurement_Param::Waist);
	const SMeasurement_Trend &neck = Trend_Of(trends, EMeasurement_Param::Neck);

	QCOMPARE(waist.Delta, -2.0);
	QCOMPARE(waist.Forecast_At_End, 91.0);
	QCOMPARE(waist.Rate_Per_Week, -2.0 / 6.0 * 7.0);

	// Шея измерена один раз: это старт, а не динамика — ни разницы, ни темпа,
	// ни прогноза, ни стрелки.
	QCOMPARE(neck.Points, 1);
	QVERIFY(neck.Has_Start && neck.Has_Latest);
	QCOMPARE(neck.Start, 40.0);
	QVERIFY(!neck.Has_Delta);
	QVERIFY(!neck.Has_Delta_Percent);
	QVERIFY(!neck.Has_Rate);
	QVERIFY(!neck.Has_Forecast);
	QCOMPARE(neck.Trend, ETrend_Kind::Unknown);
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Trend_Test::noDataGivesNothingNotZero()
{
	const QVector<SMeasurement_Trend> trends =
	    AMeasurement_Trend::Compute_Trends(Tracker(), "2026-10-28", QVector<SGoal_Target>());
	const SMeasurement_Trend &chest = Trend_Of(trends, EMeasurement_Param::Chest);

	QCOMPARE(chest.Points, 0);
	QVERIFY(!chest.Has_Start);
	QVERIFY(!chest.Has_Latest);
	QVERIFY(!chest.Has_Delta);
	QCOMPARE(chest.Trend, ETrend_Kind::Unknown);

	// Совсем без замеров все десять параметров пусты.
	const QVector<SMeasurement_Trend> empty =
	    AMeasurement_Trend::Compute_Trends(QVector<SBody_Measurement>(), "2026-10-28", QVector<SGoal_Target>());

	QCOMPARE(empty.size(), (int)EMeasurement_Param::Max);

	for (int i = 0; i < empty.size(); ++i)
		QVERIFY(!empty.at(i).Has_Start);
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Trend_Test::flatChangeIsNotATrend()
{
	QVector<SBody_Measurement> measurements;
	QVector<SMeasurement_Trend> trends;

	measurements.append(Make("2026-10-01", 80.00));
	measurements.append(Make("2026-10-04", 80.04));
	trends = AMeasurement_Trend::Compute_Trends(measurements, "2026-10-28", QVector<SGoal_Target>());
	QCOMPARE(Trend_Of(trends, EMeasurement_Param::Weight).Trend, ETrend_Kind::Flat);

	measurements[1] = Make("2026-10-04", 80.2);
	trends = AMeasurement_Trend::Compute_Trends(measurements, "2026-10-28", QVector<SGoal_Target>());
	QCOMPARE(Trend_Of(trends, EMeasurement_Param::Weight).Trend, ETrend_Kind::Up);
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Trend_Test::forecastNeedsFinishAndStopsAfterIt()
{
	QVector<SMeasurement_Trend> trends;

	// Открытый срок: темп есть, прогнозировать некуда.
	trends = AMeasurement_Trend::Compute_Trends(Tracker(), "", QVector<SGoal_Target>());
	QVERIFY(Trend_Of(trends, EMeasurement_Param::Weight).Has_Rate);
	QVERIFY(!Trend_Of(trends, EMeasurement_Param::Weight).Has_Forecast);

	// Финиш раньше последнего замера: продолжать прямую некуда, прогноз — сам замер.
	trends = AMeasurement_Trend::Compute_Trends(Tracker(), "2026-10-05", QVector<SGoal_Target>());
	QVERIFY(Trend_Of(trends, EMeasurement_Param::Weight).Has_Forecast);
	QCOMPARE(Trend_Of(trends, EMeasurement_Param::Weight).Forecast_At_End, 87.1);
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Trend_Test::favorableFollowsGoalDirection()
{
	QVector<SGoal_Target> targets;
	SGoal_Target weight_goal;
	SGoal_Target waist_goal;
	SGoal_Target dropped_goal;
	QVector<SMeasurement_Trend> trends;

	weight_goal.Metric = EGoal_Metric::BodyWeightKg;
	weight_goal.Direction = EGoal_Direction::Decrease;
	waist_goal.Metric = EGoal_Metric::MeasurementCm;
	waist_goal.Has_Measurement_Key = true;
	waist_goal.Measurement_Key = EMeasurement_Key::Waist;
	waist_goal.Direction = EGoal_Direction::Increase;
	// Снятая цель по шее не должна давать оценку.
	dropped_goal.Metric = EGoal_Metric::MeasurementCm;
	dropped_goal.Has_Measurement_Key = true;
	dropped_goal.Measurement_Key = EMeasurement_Key::Neck;
	dropped_goal.Direction = EGoal_Direction::Decrease;
	dropped_goal.State = EGoal_Target_State::Dropped;
	targets << weight_goal << waist_goal << dropped_goal;

	trends = AMeasurement_Trend::Compute_Trends(Tracker(), "2026-10-28", targets);

	// Вес падает при цели «снизить» — в сторону цели; талия падает при цели «набрать» — нет.
	QVERIFY(Trend_Of(trends, EMeasurement_Param::Weight).Has_Favorable);
	QVERIFY(Trend_Of(trends, EMeasurement_Param::Weight).Is_Favorable);
	QVERIFY(Trend_Of(trends, EMeasurement_Param::Waist).Has_Favorable);
	QVERIFY(!Trend_Of(trends, EMeasurement_Param::Waist).Is_Favorable);
	QVERIFY(!Trend_Of(trends, EMeasurement_Param::Neck).Has_Favorable);
	// Без цели оценки нет.
	QVERIFY(!Trend_Of(trends, EMeasurement_Param::Chest).Has_Favorable);
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Trend_Test::inputOrderDoesNotMatter()
{
	QVector<SBody_Measurement> reversed;
	const QVector<SBody_Measurement> straight = Tracker();
	QVector<SMeasurement_Trend> a;
	QVector<SMeasurement_Trend> b;

	for (int i = straight.size() - 1; i >= 0; --i)
		reversed.append(straight.at(i));

	a = AMeasurement_Trend::Compute_Trends(straight, "2026-10-28", QVector<SGoal_Target>());
	b = AMeasurement_Trend::Compute_Trends(reversed, "2026-10-28", QVector<SGoal_Target>());

	QCOMPARE(Trend_Of(b, EMeasurement_Param::Weight).Start, Trend_Of(a, EMeasurement_Param::Weight).Start);
	QCOMPARE(Trend_Of(b, EMeasurement_Param::Weight).Forecast_At_End,
	         Trend_Of(a, EMeasurement_Param::Weight).Forecast_At_End);
}
//-------------------------------------------------------------------------------------------------------------
// Q2: окно — семь календарных дней, оканчивающихся датой; дни без взвешивания
// в среднее не входят, а не считаются нулями.
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Trend_Test::averageOverSevenCalendarDays()
{
	const QVector<SBody_Measurement> measurements = Tracker();
	double average = 0;
	int points = 0;

	// 7 октября: окно 1–7 октября, все три взвешивания.
	QVERIFY(AMeasurement_Trend::Weight_Average_7(measurements, "2026-10-07", average, points));
	QCOMPARE(points, 3);
	QCOMPARE(average, (89.0 + 88.2 + 87.1) / 3.0);

	// 8 октября: окно 2–8 октября, первое взвешивание выпало.
	QVERIFY(AMeasurement_Trend::Weight_Average_7(measurements, "2026-10-08", average, points));
	QCOMPARE(points, 2);
	QCOMPARE(average, (88.2 + 87.1) / 2.0);

	// 4 октября: будущее взвешивание (7-е) в среднее не попадает.
	QVERIFY(AMeasurement_Trend::Weight_Average_7(measurements, "2026-10-04", average, points));
	QCOMPARE(points, 2);
	QCOMPARE(average, (89.0 + 88.2) / 2.0);

	// 20 октября: в окне пусто — среднего нет, а не ноль.
	QVERIFY(!AMeasurement_Trend::Weight_Average_7(measurements, "2026-10-20", average, points));
	QCOMPARE(points, 0);
	QVERIFY(!AMeasurement_Trend::Weight_Average_7(measurements, "garbage", average, points));
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Trend_Test::weightSeriesCarriesAverage()
{
	QVector<SBody_Measurement> measurements = Tracker();
	QVector<SWeight_Point> series;

	// Замер без веса (только талия) в график веса не попадает.
	measurements.append(Make("2026-10-10", -1, 97.0));
	series = AMeasurement_Trend::Weight_Series(measurements);

	QCOMPARE(series.size(), 3);
	QCOMPARE(series.at(0).Date, QString("2026-10-01"));
	QCOMPARE(series.at(0).Average_7_Kg, 89.0);
	QCOMPARE(series.at(0).Average_Points, 1);
	QCOMPARE(series.at(2).Weight_Kg, 87.1);
	QCOMPARE(series.at(2).Average_7_Kg, (89.0 + 88.2 + 87.1) / 3.0);
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Trend_Test::changeMapIsRelativeToStartAndPrevious()
{
	const QVector<SChange_Row> rows = AMeasurement_Trend::Change_Map(Tracker());
	const int weight = (int)EMeasurement_Param::Weight;
	const int neck = (int)EMeasurement_Param::Neck;

	QCOMPARE(rows.size(), 3);
	QCOMPARE(rows.at(0).Date, QString("2026-10-01"));

	// Первый замер — сам старт: к старту 0, к предыдущему сравнивать не с чем.
	QVERIFY(rows.at(0).From_Start.at(weight).Has_Value);
	QCOMPARE(rows.at(0).From_Start.at(weight).Value, 0.0);
	QVERIFY(!rows.at(0).From_Previous.at(weight).Has_Value);

	QCOMPARE(rows.at(1).From_Start.at(weight).Value, 88.2 - 89.0);
	QCOMPARE(rows.at(1).From_Previous.at(weight).Value, 88.2 - 89.0);
	QCOMPARE(rows.at(2).From_Start.at(weight).Value, 87.1 - 89.0);
	QCOMPARE(rows.at(2).From_Previous.at(weight).Value, 87.1 - 88.2);

	// Шея измерена только в последнем замере: в первых двух ячеек нет, а в
	// последнем она — старт, и сравнивать её не с чем.
	QVERIFY(!rows.at(0).From_Start.at(neck).Has_Value);
	QVERIFY(!rows.at(1).From_Start.at(neck).Has_Value);
	QVERIFY(rows.at(2).From_Start.at(neck).Has_Value);
	QCOMPARE(rows.at(2).From_Start.at(neck).Value, 0.0);
	QVERIFY(!rows.at(2).From_Previous.at(neck).Has_Value);
}
//-------------------------------------------------------------------------------------------------------------

QTEST_GUILESS_MAIN(AMeasurement_Trend_Test)
#include "tst_Measurement_Trend.moc"
