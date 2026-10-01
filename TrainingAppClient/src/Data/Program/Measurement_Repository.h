#pragma once

#include "Data/Program/Program_Repository_Base.h"
#include "Domain/Program_Model.h"

#include <QString>
#include <QVector>

//----------------------------------------------------------------------------
// Локальный репозиторий замеров тела (срез W4, issue 23).
//
// `body_measurements` — таблица, независимая от программы (§3.8, инвариант 20):
// одна строка на пару (user, date), обязательны только пользователь и дата,
// остальные поля необязательные. Сохранение — upsert с слиянием: заполненные
// поля (`Has_*`) перекрывают значения дня, пустые остаются как есть, поэтому
// утренний вес не теряется при вечерней записи обхватов.
//----------------------------------------------------------------------------
class AMeasurement_Repository : public AProgram_Repository_Base
{
	Q_OBJECT

public:
	explicit AMeasurement_Repository(AsSql_Database *in_database, QObject *in_parent = 0);

	// Чтение. Отсутствие строки — false, детали — Get_Last_Error().
	bool Find_Measurement(const QString &in_user_id, const QString &in_date,
	                      Program_Model::SBody_Measurement &out_measurement);
	bool Load_Range(const QString &in_user_id, const QString &in_date_from, const QString &in_date_to,
	                QVector<Program_Model::SBody_Measurement> &out_measurements);
	bool Load_Latest(const QString &in_user_id, Program_Model::SBody_Measurement &out_measurement);

	// Запись. ID пустой — генерируется; дата обязательна и должна быть
	// корректной календарной датой YYYY-MM-DD, иначе false.
	bool Save_Measurement(const Program_Model::SBody_Measurement &in_measurement);
	bool Delete_Measurement(const QString &in_user_id, const QString &in_date);

private:
	bool Is_Valid_Date(const QString &in_date) const;
	bool Has_Any_Value(const Program_Model::SBody_Measurement &in_measurement) const;
};
//----------------------------------------------------------------------------
