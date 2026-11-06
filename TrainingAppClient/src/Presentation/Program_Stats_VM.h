#pragma once

#include "Domain/Program_Model.h"
#include "Domain/Progress_Calculator.h"
#include "Domain/Progress_Stats.h"
#include "Presentation/List_Model.h"

#include <QDate>
#include <QObject>
#include <QString>

using namespace Program_Model;

class AProgram_Repository;
class ARule_Repository;
class AWorkout_Repository;
class AMeasurement_Repository;
class AStats_Repository;

//-------------------------------------------------------------------------------------------------------------
// ViewModel статистики программы (срез W9, issue 26; §5.1 `Avm_Program_Stats`).
//
// Считает KPI по фактам через `AsProgress_Calculator` и кладёт результат в
// кэш `program_stats` (`AStats_Repository`) — по §5.4 кэш хранит только
// производное, первичные данные остаются фактами.
//
// Кэш используется как ускоритель, а не как источник истины: при
// `revision++` или записи факта он инвалидируется, и числа пересчитываются из
// фактов. Поэтому refresh всегда пересчитывает, а кэш лишь снимает повторное
// чтение для отображения.
//
// Владение: списки — дети ViewModel (родитель-потомок QObject).
//-------------------------------------------------------------------------------------------------------------
class Avm_Program_Stats : public QObject
{
	Q_OBJECT

	Q_PROPERTY(bool hasStats READ Has_Stats NOTIFY changed)
	Q_PROPERTY(double adherencePercent READ adherencePercent NOTIFY changed)
	Q_PROPERTY(bool hasAdherencePercent READ Has_Adherence_Percent NOTIFY changed)
	Q_PROPERTY(int plannedSessions READ plannedSessions NOTIFY changed)
	Q_PROPERTY(int completedSessions READ completedSessions NOTIFY changed)
	Q_PROPERTY(double tonnageKg READ tonnageKg NOTIFY changed)
	Q_PROPERTY(int setsCount READ setsCount NOTIFY changed)
	Q_PROPERTY(int repsCount READ repsCount NOTIFY changed)
	Q_PROPERTY(double durationMinutes READ durationMinutes NOTIFY changed)
	Q_PROPERTY(double avgRpe READ avgRpe NOTIFY changed)
	Q_PROPERTY(bool hasAvgRpe READ Has_Average_Rpe NOTIFY changed)
	Q_PROPERTY(int currentStreakWeeks READ currentStreakWeeks NOTIFY changed)
	Q_PROPERTY(int longestStreakWeeks READ longestStreakWeeks NOTIFY changed)
	Q_PROPERTY(double bodyWeightDeltaKg READ bodyWeightDeltaKg NOTIFY changed)
	Q_PROPERTY(bool hasBodyWeightDeltaKg READ Has_Body_Weight_Delta_Kg NOTIFY changed)
	Q_PROPERTY(double ruleAdherencePercent READ ruleAdherencePercent NOTIFY changed)
	Q_PROPERTY(bool hasRuleAdherencePercent READ Has_Rule_Adherence_Percent NOTIFY changed)
	Q_PROPERTY(bool goalAchieved READ goalAchieved NOTIFY changed)
	Q_PROPERTY(QString dataRangeFrom READ dataRangeFrom NOTIFY changed)
	Q_PROPERTY(QString dataRangeTo READ dataRangeTo NOTIFY changed)
	Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)

	// Модели списков объявлены свойствами, а не только методами: иначе QML
	// видел бы их как функции и получал пустую модель (§5 QML-контракт).
	Q_PROPERTY(AList_Model *weeks READ weeks CONSTANT)
	Q_PROPERTY(AList_Model *rule_stats READ rule_stats CONSTANT)
	Q_PROPERTY(AList_Model *exercise_strength READ exercise_strength CONSTANT)
	Q_PROPERTY(AList_Model *goal_progress READ goal_progress CONSTANT)
	Q_PROPERTY(AList_Model *measurements READ measurements CONSTANT)

public:
	Avm_Program_Stats(AProgram_Repository *in_program_repository, AWorkout_Repository *in_workout_repository,
	                  ARule_Repository *in_rule_repository, AMeasurement_Repository *in_measurement_repository,
	                  AStats_Repository *in_stats_repository, QObject *in_parent = 0);

	// Пересчёт статистики программы по её фактам и запись в кэш. Период по
	// умолчанию — вся программа (пустые границы даты).
	Q_INVOKABLE bool refresh(const QString &in_program_id, const QString &in_date_from = QString(),
	                         const QString &in_date_to = QString());
	bool Refresh(const QString &in_program_id, const QString &in_date_from = QString(),
	             const QString &in_date_to = QString());

	// «Сегодня» для окна расчёта: локальная дата устройства (§5.5). Задаётся
	// явно, чтобы тесты не зависели от системных часов.
	void Set_Today(const QString &in_date);
	// Сверяет «сегодня» с часами устройства: приложение живёт днями, а дата,
	// запомненная при создании, после полуночи писала бы данные во вчера.
	// Дата, заданная через setToday, не трогается (тесты). true — день сменился.
	bool Refresh_Today();

	bool Has_Stats() const;
	double adherencePercent() const;
	bool Has_Adherence_Percent() const;
	int plannedSessions() const;
	int completedSessions() const;
	double tonnageKg() const;
	int setsCount() const;
	int repsCount() const;
	double durationMinutes() const;
	double avgRpe() const;
	bool Has_Average_Rpe() const;
	int currentStreakWeeks() const;
	int longestStreakWeeks() const;
	double bodyWeightDeltaKg() const;
	bool Has_Body_Weight_Delta_Kg() const;
	double ruleAdherencePercent() const;
	bool Has_Rule_Adherence_Percent() const;
	bool goalAchieved() const;
	QString dataRangeFrom() const;
	QString dataRangeTo() const;
	QString errorKey() const;

	const SProgram_Stats &program_stats() const;
	AList_Model *weeks() const;
	AList_Model *rule_stats() const;
	AList_Model *exercise_strength() const;
	AList_Model *goal_progress() const;
	AList_Model *measurements() const;

signals:
	void changed();

private:
	void Clear_Stats();
	void Clear_Error();
	void Set_Error();

	AProgram_Repository *Program_Repository_Instance;
	AWorkout_Repository *Workout_Repository_Instance;
	ARule_Repository *Rule_Repository_Instance;
	AMeasurement_Repository *Measurement_Repository_Instance;
	AStats_Repository *Stats_Repository_Instance;

	SProgram_Stats Stats;
	AsProgress_Calculator Calculator;

	AList_Model *Weeks;
	AList_Model *Rule_Stats_Model;
	AList_Model *Exercise_Strength_Model;
	AList_Model *Goal_Progress_Model;
	AList_Model *Measurements_Model;

	QString Today_Text;
	bool Today_Is_Pinned = false; // дата задана извне (тесты) и часами не обновляется
	QString Error_Key_Text;
};
//-------------------------------------------------------------------------------------------------------------
