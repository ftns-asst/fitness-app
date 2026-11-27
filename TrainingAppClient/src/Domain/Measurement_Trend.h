#pragma once

#include "Domain/Program_Model.h"

#include <QString>
#include <QVector>

using namespace Program_Model;

//-------------------------------------------------------------------------------------------------------------
// Параметр динамики тела: вес и девять обхватов книги-эталона (docs/09, FR-T5).
// Порядок обхватов совпадает с EMeasurement_Key (Chest…Neck), и индекс ключа =
// индекс параметра − 1: на этом держится Get_Value, а тест сверяет имена.
//-------------------------------------------------------------------------------------------------------------
enum class EMeasurement_Param : unsigned char
{
	Weight,
	Chest,
	Waist,
	Hip,
	Arm,
	Thigh,
	Belly,
	Shoulders,
	Calves,
	Neck,
	Max
};

// Направление изменения за срок. Unknown — точек меньше двух, сравнивать нечего.
enum class ETrend_Kind : unsigned char
{
	Unknown,
	Down,
	Up,
	Flat,
	Max
};

//-------------------------------------------------------------------------------------------------------------
// Динамика одного параметра (FR-T8, FR-T9). Поля «значение + Has_»: отсутствие
// данных — не ноль. Start — первое измерение окна, Latest — последнее.
//-------------------------------------------------------------------------------------------------------------
struct SMeasurement_Trend
{
	EMeasurement_Param Param = EMeasurement_Param::Weight;
	int Points = 0; // сколько замеров содержат параметр
	QString Start_Date;
	double Start = 0;
	bool Has_Start = false;
	QString Latest_Date;
	double Latest = 0;
	bool Has_Latest = false;
	double Delta = 0; // Latest − Start; нужны две точки
	bool Has_Delta = false;
	double Delta_Percent = 0; // от Start; нулевой старт процента не даёт
	bool Has_Delta_Percent = false;
	int Days_Passed = 0;
	double Rate_Per_Week = 0; // линейный темп: Delta / дни × 7
	bool Has_Rate = false;
	double Forecast_At_End = 0; // Latest + темп до финиша; нужен срок с финишем
	bool Has_Forecast = false;
	ETrend_Kind Trend = ETrend_Kind::Unknown;
	// Изменение в сторону цели? Известно, только если у параметра есть активная
	// цель с направлением и динамика определена.
	bool Has_Favorable = false;
	bool Is_Favorable = false;
};

// Точка графика веса: вес дня и среднее за 7 календарных дней, оканчивающихся им.
struct SWeight_Point
{
	QString Date;
	double Weight_Kg = 0;
	double Average_7_Kg = 0;
	int Average_Points = 0; // сколько взвешиваний вошло в среднее
};

// Ячейка карты изменений: разница значения параметра с опорным замером.
struct SChange_Cell
{
	double Value = 0;
	bool Has_Value = false;
};

// Строка карты изменений (FR-T10): замер и по ячейке на каждый параметр, к
// старту и к предыдущему замеру. Индекс ячейки = (int)EMeasurement_Param.
struct SChange_Row
{
	QString Date;
	QVector<SChange_Cell> From_Start;
	QVector<SChange_Cell> From_Previous;
};

//-------------------------------------------------------------------------------------------------------------
// Динамика тела (T2). Чистые функции от замеров, срока и целей: состояния нет,
// поэтому расчёт одинаково проверяется в тестах и не зависит от часов.
// Замеры могут прийти в любом порядке — функции сортируют их по дате сами.
//-------------------------------------------------------------------------------------------------------------
class AMeasurement_Trend
{
public:
	// Изменение меньше этого порога (кг или см) считается «без изменений»:
	// точность ввода — десятая доля, и шум в сотую тенденцией не является.
	static const double Flat_Tolerance;
	// Окно среднего веса, календарных дней (Q2: по календарным дням, с пропусками).
	static const int Average_Window_Days = 7;

	static QString To_String(EMeasurement_Param in_param);
	static bool From_String(const QString &in_text, EMeasurement_Param &out_param);
	static QString To_String(ETrend_Kind in_kind);

	// Значение параметра в замере; false — параметр не задан.
	static bool Get_Value(const SBody_Measurement &in_measurement, EMeasurement_Param in_param, double &out_value);

	// По записи на каждый параметр в порядке EMeasurement_Param. in_end_date —
	// финиш программы (пусто — у срока финиша нет и прогноза тоже). in_targets
	// нужны только для признака «в сторону цели».
	static QVector<SMeasurement_Trend> Compute_Trends(const QVector<SBody_Measurement> &in_measurements,
	                                                  const QString &in_end_date,
	                                                  const QVector<SGoal_Target> &in_targets);

	// Среднее вес за окно, оканчивающееся in_date. false — в окне нет взвешиваний.
	static bool Weight_Average_7(const QVector<SBody_Measurement> &in_measurements, const QString &in_date,
	                             double &out_average, int &out_points);

	// Серия для графика: по точке на каждое взвешивание, по возрастанию даты.
	static QVector<SWeight_Point> Weight_Series(const QVector<SBody_Measurement> &in_measurements);

	// Карта изменений: по строке на каждый замер, по возрастанию даты.
	static QVector<SChange_Row> Change_Map(const QVector<SBody_Measurement> &in_measurements);

private:
	static QVector<SBody_Measurement> Sorted_By_Date(const QVector<SBody_Measurement> &in_measurements);
	static bool Find_Goal_Direction(const QVector<SGoal_Target> &in_targets, EMeasurement_Param in_param,
	                                EGoal_Direction &out_direction);
};
//-------------------------------------------------------------------------------------------------------------
