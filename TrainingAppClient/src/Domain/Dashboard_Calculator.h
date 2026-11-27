#pragma once

#include "Domain/Program_Model.h"

#include <QVariantMap>

using namespace Program_Model;

//-------------------------------------------------------------------------------------------------------------
// Чистый расчёт плиток. Отсутствующие показатели — null, а не ноль.
// Завершённые паузы передаются числом дней; текущая пауза — отдельной датой.
class ADashboard_Calculator
{
public:
	static QVariantMap Compute(const SProgram &in_program, const QVector<SGoal_Target> &in_targets,
	                           const QVector<SBody_Measurement> &in_measurements, const QString &in_today,
	                           int in_paused_days = 0, const QString &in_paused_on = QString());
	// Среднее долей срока и тренировок; отсутствующая доля исключается.
	static QVariant Completion_Progress(const QVariant &in_term_progress, int in_completed, int in_planned);
};
//-------------------------------------------------------------------------------------------------------------
