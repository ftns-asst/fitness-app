#pragma once

#include "Data/Program/Program_Repository_Base.h"
#include "Domain/Program_Model.h"

#include <QString>
#include <QVector>

using namespace Program_Model;

//-------------------------------------------------------------------------------------------------------------
// Хранит сессии и подходы. Подходы сохраняют снимки предписаний: правки
// программы не переписывают историю тренировок.
//-------------------------------------------------------------------------------------------------------------
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

	// Запись фактов. Сессия и подход пишутся отдельно: сессия создаётся один
	// раз, подходы добавляются и правятся по мере тренировки. Идентификатор
	// пустой — генерируется; непустой обновляет существующую строку, поэтому
	// повторная запись подхода с тем же id идемпотентна.
	bool Insert_Session(const SWorkout_Session &in_session, const QString &in_plan_json = QString());
	bool Update_Session(const SWorkout_Session &in_session);
	bool Complete_Session(const SWorkout_Session &in_session);
	bool Save_Set(const SSet_Log &in_set);

	// Черновик пользователя на дату: незавершённая сессия этого дня.
	bool Find_Draft(const QString &in_user_id, const QString &in_date, SWorkout_Session &out_session);
	bool Find_Session(const QString &in_session_id, SWorkout_Session &out_session);
	bool Save_Runtime(const QString &in_session_id, const QString &in_plan_json, qint64 in_rest_until_ms);
	bool Load_Runtime(const QString &in_session_id, QString &out_plan_json, qint64 &out_rest_until_ms);

private:
	// Общий разбор строки workouts; out_index — номер колонки с отступом,
	// чтобы Load_Sessions читала разные наборы колонок.
	bool Read_Session_Row(const QSqlQuery &in_query, int in_first_column, SWorkout_Session &out_session);
	// Привязка значений сессии в порядке колонок Session_Columns.
	void Bind_Session(QSqlQuery &in_query, const SWorkout_Session &in_session) const;
};
//-------------------------------------------------------------------------------------------------------------
