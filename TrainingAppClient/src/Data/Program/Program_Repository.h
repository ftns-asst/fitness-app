#pragma once

#include "Data/Program/Program_Repository_Base.h"
#include "Domain/Program_Service.h"

#include <QHash>
#include <QSet>
#include <QString>
#include <QVector>

using namespace Program_Model;

//----------------------------------------------------------------------------
// Локальный репозиторий модели программы (срез W3, issue 23). Оркестрирует
// programs / goal_targets / program_schedule / program_days /
// program_day_exercises / program_revisions поверх AsSql_Database.
//
// Правки разделов (цели, срок, расписание, дни, состояние) транзакционны:
// валидируют вход, инкрементируют programs.revision и пишут строку
// program_revisions со снимком СТАРОГО состояния раздела. Снимок —
// единственное место, где сохраняется история плана: при замене дней FK
// переводит ссылки фактов (workouts/sets) в NULL, поэтому adherence и
// объяснение статистики считаются по журналам ревизий (`02-program-model.md`
// §3.9). Отсутствие снимка откатывает правку (Require_Snapshot): журнал
// без старого состояния бесполезен. Восстановление по снимку (откат) — W5.
//
// Read-only-состояния (completed/cancelled/archived) правки структуры
// отвергают (инвариант 24 §4, Is_Read_Only_State в базе). Ошибки —
// возврат false + Get_Last_Error(); журнал ревизий и оптимистическая
// блокировка — общие для всех репозиториев (AProgram_Repository_Base).
//----------------------------------------------------------------------------
class AProgram_Repository : public AProgram_Repository_Base, public IProgram_Repository
{
	Q_OBJECT

public:
	explicit AProgram_Repository(AsSql_Database *in_database, QObject *in_parent = 0);
	QString Get_Last_Error() const override
	{
		return AProgram_Repository_Base::Get_Last_Error();
	}

	// Чтение. Отсутствие строки — false, детали — Get_Last_Error().
	bool Load_Program(const QString &in_program_id, SProgram &out_program);
	bool Load_Program_Full(const QString &in_program_id, SProgram &out_program, QVector<SGoal_Target> &out_targets,
	                       QVector<SProgram_Day> &out_days, QVector<SDay_Exercise> &out_exercises,
	                       QVector<SWeek_Slot> &out_slots);
	bool Get_Active_Program_Id(const QString &in_user_id, QString &out_program_id);
	bool Load_Goal_Targets(const QString &in_program_id, QVector<SGoal_Target> &out_targets);
	bool Load_Days(const QString &in_program_id, QVector<SProgram_Day> &out_days);
	bool Load_Day_Exercises(const QString &in_program_day_id, QVector<SDay_Exercise> &out_exercises);
	bool Load_Schedule(const QString &in_program_id, QVector<SWeek_Slot> &out_slots);

	// Названия упражнений по их id. Карточка дня показывает название, а не
	// внутренний идентификатор; собственный репозиторий упражнений появится
	// в фазе B (issue 23), пока нужен только этот срез данных.
	bool Load_Exercise_Titles(const QVector<QString> &in_exercise_ids, QHash<QString, QString> &out_titles);

	// Запись. Created_At/Updated_At штампуются репозиторием, если пусты.
	// Insert_Program пишет журнал с ревизией in_program.Revision.
	bool Insert_Program(const SProgram &in_program);
	// Создание полной программы одной транзакцией и одной начальной ревизией.
	bool Insert_Program_Full(const SProgram &in_program, const QVector<SGoal_Target> &in_targets,
	                         const QVector<SProgram_Day> &in_days, const QVector<SDay_Exercise> &in_exercises,
	                         const QVector<SWeek_Slot> &in_slots) override;
	bool Replace_Goal_Targets(const QString &in_program_id, const QVector<SGoal_Target> &in_targets);
	bool Change_Term(const QString &in_program_id, const STerm &in_term);
	bool Replace_Schedule(const QString &in_program_id, const QVector<SWeek_Slot> &in_slots);
	bool Replace_Structure(const QString &in_program_id, const QVector<SProgram_Day> &in_days,
	                       const QVector<SDay_Exercise> &in_exercises, const QVector<SWeek_Slot> &in_slots,
	                       ERevision_Reason in_reason, const QString &in_summary);
	// Переходы состояний — только по §3.1: draft→active, active→paused→active,
	// active→completed|cancelled, completed|cancelled→archived. Активация
	// заполняет actual_start_date, если она пуста. Инкремент ревизии — с
	// оптимистичной проверкой (как во всех правках).
	bool Change_State(const QString &in_program_id, EProgram_State in_new_state) override;

private:
	bool Check_Editable(const QString &in_program_id, SProgram &out_program);
	// Предписания всех дней программы одним запросом: используется
	// Load_Program_Full, где цикл с запросом на каждый день давал N+1.
	bool Load_Day_Exercises_By_Days(const QVector<SProgram_Day> &in_days, QVector<SDay_Exercise> &out_exercises);
	bool Insert_Targets_Internal(const QString &in_program_id, const QVector<SGoal_Target> &in_targets);
	bool Insert_Days_Internal(const QString &in_program_id, const QVector<SProgram_Day> &in_days);
	bool Insert_Day_Exercises_Internal(const QVector<SDay_Exercise> &in_exercises);
	bool Insert_Slots_Internal(const QString &in_program_id, const QVector<SWeek_Slot> &in_slots);
	// Валидация по §4: инвариант 8 (непустые цели активной измеримой
	// программы), инвариант 9 (direction ↔ start/target), инварианты 2, 3,
	// 5, 6 для расписания и структуры.
	bool Validate_Targets(const SProgram &in_program, const QVector<SGoal_Target> &in_targets);
	bool Check_Goal_Direction(const SGoal_Target &in_target);
	bool Validate_Structure(ESchedule_Kind in_kind, const QVector<SProgram_Day> &in_days,
	                        const QVector<SDay_Exercise> &in_exercises, const QVector<SWeek_Slot> &in_slots);
	bool Validate_Days(const QVector<SProgram_Day> &in_days, QSet<QString> &out_day_ids);
	bool Validate_Day_Exercises(const QVector<SDay_Exercise> &in_exercises, const QSet<QString> &in_day_ids);
	bool Validate_Slots(ESchedule_Kind in_kind, const QVector<SWeek_Slot> &in_slots, const QSet<QString> &in_day_ids);
	bool Validate_Slot_Day(const SWeek_Slot &in_slot, const QSet<QString> &in_day_ids);
	// Позиция слота в расписании: ISO-день недели либо индекс ротации.
	int Slot_Position(ESchedule_Kind in_kind, const SWeek_Slot &in_slot) const;

	QString Build_Goal_Targets_Snapshot(const QString &in_program_id);
	QString Build_Schedule_Snapshot(const QString &in_program_id);
	QString Build_Structure_Snapshot(const QString &in_program_id);
	// Снимок обязателен для каждой правки с журналом: пустой (ошибка
	// чтения старого состояния) → откат, а не тихая запись без истории.
	bool Require_Snapshot(const QString &in_snapshot, const QString &in_section);
};
//----------------------------------------------------------------------------
