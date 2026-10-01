#include "Data/Program/Program_Repository_Base.h"
#include "Domain/Program_Model.h"

#include <QString>
#include <QVector>

using namespace Program_Model;

//----------------------------------------------------------------------------
// Репозиторий фактов тренировок (срез W9, issue 23). Читает workouts и sets —
// единственный источник статистики (§5.4: первичные данные — только факты).
//
// Запись тренировок в этом срезе не делается: её ведёт `AsWorkout_Logger`
// (фаза B, issue 27), а статистике W9 достаточно чтения. Поэтому класс
// асимметричен: чтение есть, записи нет.
//
// Правки структуры программы не переписывают историю: program_id и
// program_day_id ссылаются с ON DELETE SET NULL, а подход хранит снимок
// target_weight_kg/target_reps (§3.8).
//------------------------------------------------------------------------------
class AWorkout_Repository : public AProgram_Repository_Base
{
	Q_OBJECT

public:
	explicit AWorkout_Repository(AsSql_Database *in_database, QObject *in_parent = 0);

	// Сессии пользователя. Пустой in_program_id — все сессии пользователя,
	// включая соло-тренировки вне программы (§4 инвариант 18). Диапазон дат
	// пустой — без границ.
	bool Load_Sessions(const QString &in_user_id, const QString &in_program_id, const QString &in_date_from,
	                   const QString &in_date_to, QVector<SWorkout_Session> &out_sessions);

	// Подходы сессий. Список идентификаторов пустой — все подходы всех
	// переданных сессий; иначе читаются только указанные.
	bool Load_Sets(const QVector<QString> &in_session_ids, QVector<SSet_Log> &out_sets);

	// Факты программы для статистики одним вызовом: сессии программы плюс их
	// подходы. Соло-сессии в программу не входят, но попадают в «всё время»
	// через Load_Sessions с пустым program_id.
	bool Load_Program_Facts(const QString &in_user_id, const QString &in_program_id, const QString &in_date_from,
	                        const QString &in_date_to, QVector<SWorkout_Session> &out_sessions,
	                        QVector<SSet_Log> &out_sets);

private:
	// Общий разбор строки workouts; out_index — номер колонки с отступом,
	// чтобы Load_Sessions читала разные наборы колонок.
	bool Read_Session_Row(const QSqlQuery &in_query, int in_first_column, SWorkout_Session &out_session);
};
//----------------------------------------------------------------------------