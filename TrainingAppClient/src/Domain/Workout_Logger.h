#pragma once

#include "Domain/Error_State.h"
#include "Domain/Program_Model.h"

#include <QString>
#include <QVector>

class AProgram_Repository;
class AWorkout_Repository;

using namespace Program_Model;

//-------------------------------------------------------------------------------------------------------------
// Запись фактов тренировки (срез W11, issue 27, §3.8).
//
// Логгер — единственная точка, где тренировка становится фактом: статистика
// W7 читает факты и ничего не пишет. Здесь же проверяются правила, которые
// иначе пришлось бы повторять в каждом вызывающем:
//
// - день открывается один раз: повторный Start_Session возвращает тот же
//   черновик, а не создаёт вторую сессию;
// - подход принадлежит своему черновику и только ему; чужой пользователь
//   не может дописать в чужую тренировку;
// - подход хранит снимок предписания (целевой вес и повторы), поэтому
//   последующая правка плана не переписывает выполненное;
// - завершение пересчитывает итоги сессии (упражнения, подходы, тоннаж,
//   средний RPE) — статистике они нужны готовыми.
//
// Черновик переживает перезапуск: тренировка в зале идёт без сети, и начатое
// не должно теряться (§5.4, офлайн-first).
//-------------------------------------------------------------------------------------------------------------
class AsWorkout_Logger
{
public:
	explicit AsWorkout_Logger(AWorkout_Repository *in_workout_repository,
	                          AProgram_Repository *in_program_repository = 0);

	// Открывает день. in_program_id и in_program_day_id пустые — соло-тренировка
	// вне программы (§4 инвариант 18). Непустая программа требует дня, который
	// действительно принадлежит ей.
	bool Start_Session(const QString &in_user_id, const QString &in_date, const QString &in_program_id,
	                   const QString &in_program_day_id, const QString &in_note, SWorkout_Session &out_session);

	// Сохраняет подход в своём черновике. Пустой идентификатор — новый подход;
	// непустой обновляет существующий. Снимок предписания подставляется по
	// day_exercise_id, если не задан явно.
	bool Save_Set(const QString &in_user_id, const SSet_Log &in_set);

	// Завершает тренировку и пересчитывает итоги.
	bool Finish_Session(const QString &in_user_id, const QString &in_session_id, SWorkout_Session &out_session);

	// Черновик пользователя на дату.
	bool Get_Draft(const QString &in_user_id, const QString &in_date, SWorkout_Session &out_session);

	QString Get_Last_Error() const;

private:
	bool Set_Last_Error(const QString &in_error);
	void Clear_Last_Error();

	// Проверки, общие для Start_Session и Save_Set.
	bool Is_Valid_Date(const QString &in_date) const;
	// Программа читается здесь, поэтому логгер получает и репозиторий
	// программ: без него день не проверить, а предписание не снять.
	bool Resolve_Program_Links(const QString &in_program_id, const QString &in_program_day_id, SProgram &out_program);
	bool Is_Own_Draft(const QString &in_user_id, const QString &in_session_id, SWorkout_Session &out_session);
	bool Is_Valid_Set(const SSet_Log &in_set);
	// Снимок предписания: план может измениться, а факт хранит то, что было.
	bool Apply_Plan_Snapshot(const QString &in_program_day_id, SSet_Log &in_out_set);
	// Итоги завершения считаются здесь, а не в статистике: она читает готовое.
	bool Recalculate_Summary(const QString &in_session_id, SWorkout_Session &in_out_session) const;

	AWorkout_Repository *Repository_Instance;
	AProgram_Repository *Program_Repository_Instance;
	AError_State Error_State;
};
//-------------------------------------------------------------------------------------------------------------