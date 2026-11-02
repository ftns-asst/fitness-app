#pragma once

#include "Domain/Program_Model.h"

#include <QString>
#include <QStringList>

using namespace Program_Model;

//-------------------------------------------------------------------------------------------------------------
// Расписание замеров тела программы (FR-T7, docs/09): первая точка — старт,
// дальше с шагом interval_days, последняя — финиш программы, даже если он не
// кратен шагу. У открытого срока финиша нет: точки идут бесконечно.
// Чистые функции от дат: состояние не хранится, поэтому расчёт одинаково
// проверяется в тестах и не зависит от часов устройства.
//-------------------------------------------------------------------------------------------------------------
class AMeasurement_Schedule
{
public:
	// Шаг расписания по умолчанию (раз в 3 дня) и допустимые границы.
	static const int Default_Interval_Days = 3;
	static const int Min_Interval_Days = 1;
	static const int Max_Interval_Days = 30;

	static bool Is_Valid_Interval(int in_interval_days);

	// Старт срока для расписания и динамики: фактический старт программы (план
	// сдвигается вместе с ним), а если её ещё не начинали — плановый. Пусто —
	// дата не задана.
	static QString Start_Date_Of(const SProgram &in_program);
	// Финиш: старт + недели × 7 − 1 день. У открытого срока финиша нет — пусто.
	static QString End_Date_Of(const SProgram &in_program);

	// Все даты срока (YYYY-MM-DD): старт, старт + k × шаг, финиш. Пусто, если
	// финиша нет, даты некорректны или финиш раньше старта.
	static QStringList Planned_Dates(const QString &in_start_date, const QString &in_end_date, int in_interval_days);

	// Ближайшая точка расписания не раньше in_from_date. false — точек больше
	// нет (срок закончился) или данные некорректны.
	static bool Next_Date(const QString &in_start_date, const QString &in_end_date, int in_interval_days,
	                      const QString &in_from_date, QString &out_date);

	// Последняя точка расписания не позже in_to_date. false — срок ещё не начался.
	static bool Previous_Date(const QString &in_start_date, const QString &in_end_date, int in_interval_days,
	                          const QString &in_to_date, QString &out_date);

	// «Пора мерить»: точка расписания уже наступила, а замера на неё или позже
	// нет. in_last_measurement_date пустая — замеров не было вовсе.
	static bool Is_Due(const QString &in_start_date, const QString &in_end_date, int in_interval_days,
	                   const QString &in_today, const QString &in_last_measurement_date);
};
//-------------------------------------------------------------------------------------------------------------
