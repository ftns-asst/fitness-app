#include "Data/Program/Measurement_Repository.h"

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

static const char *MEASUREMENT_Columns = "id, user_id, date, weight_kg, chest_cm, waist_cm, hip_cm, arm_cm, thigh_cm";
//----------------------------------------------------------------------------
static void Read_Measurement_Row(QSqlQuery &in_query, SBody_Measurement &out_measurement)

{
	out_measurement = SBody_Measurement();
	out_measurement.ID = in_query.value(0).toString();
	out_measurement.User_Id = in_query.value(1).toString();
	out_measurement.Date = in_query.value(2).toString();

	if (in_query.isNull(3) == false)
	{
		out_measurement.Has_Weight_Kg = true;
		out_measurement.Weight_Kg = in_query.value(3).toDouble();
	}

	if (in_query.isNull(4) == false)
	{
		out_measurement.Has_Chest_Cm = true;
		out_measurement.Chest_Cm = in_query.value(4).toDouble();
	}

	if (in_query.isNull(5) == false)
	{
		out_measurement.Has_Waist_Cm = true;
		out_measurement.Waist_Cm = in_query.value(5).toDouble();
	}

	if (in_query.isNull(6) == false)
	{
		out_measurement.Has_Hip_Cm = true;
		out_measurement.Hip_Cm = in_query.value(6).toDouble();
	}

	if (in_query.isNull(7) == false)
	{
		out_measurement.Has_Arm_Cm = true;
		out_measurement.Arm_Cm = in_query.value(7).toDouble();
	}

	if (in_query.isNull(8) == false)
	{
		out_measurement.Has_Thigh_Cm = true;
		out_measurement.Thigh_Cm = in_query.value(8).toDouble();
	}
}
//----------------------------------------------------------------------------

//----------------------------------------------------------------------------
// AMeasurement_Repository
//----------------------------------------------------------------------------
AMeasurement_Repository::AMeasurement_Repository(AsSql_Database *in_database, QObject *in_parent)
    : AProgram_Repository_Base(in_database, in_parent)
{
}
bool AMeasurement_Repository::Is_Valid_Date(const QString &in_date) const
{
	QDate date = QDate::fromString(in_date, Qt::ISODate);

	return in_date.size() == 10 && date.isValid() && date.toString("yyyy-MM-dd") == in_date;
}
//----------------------------------------------------------------------------

bool AMeasurement_Repository::Has_Any_Value(const SBody_Measurement &in_measurement) const
{
	if (in_measurement.Has_Weight_Kg || in_measurement.Has_Chest_Cm || in_measurement.Has_Waist_Cm ||
	    in_measurement.Has_Hip_Cm || in_measurement.Has_Arm_Cm || in_measurement.Has_Thigh_Cm)
		return true;

	return false;
}
//----------------------------------------------------------------------------
bool AMeasurement_Repository::Find_Measurement(const QString &in_user_id, const QString &in_date,
                                               SBody_Measurement &out_measurement)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();
	out_measurement = SBody_Measurement();

	if (Check_Open() == false)
		return false;

	query.prepare(QString("SELECT %1 FROM body_measurements WHERE user_id = ? AND date = ?").arg(MEASUREMENT_Columns));
	query.addBindValue(in_user_id);
	query.addBindValue(in_date);

	if (query.exec() == false)
		return Set_Last_Error(QString("Не удалось найти замер: %1").arg(query.lastError().text()));

	if (query.next() == false)
		return Set_Last_Error(QString("Замер пользователя %1 за %2 не найден").arg(in_user_id).arg(in_date));

	Read_Measurement_Row(query, out_measurement);

	return true;
}
//----------------------------------------------------------------------------
bool AMeasurement_Repository::Load_Range(const QString &in_user_id, const QString &in_date_from,
                                         const QString &in_date_to, QVector<SBody_Measurement> &out_measurements)
{
	QSqlQuery query(Connection());
	SBody_Measurement measurement;

	Clear_Last_Error();
	out_measurements.clear();

	if (Check_Open() == false)
		return false;

	query.prepare(QString("SELECT %1 FROM body_measurements WHERE user_id = ? AND date >= ? AND date <= ? "
	                      "ORDER BY date")
	                  .arg(MEASUREMENT_Columns));
	query.addBindValue(in_user_id);
	query.addBindValue(in_date_from);
	query.addBindValue(in_date_to);

	if (query.exec() == false)
		return Set_Last_Error(QString("Не удалось прочитать замеры: %1").arg(query.lastError().text()));

	while (query.next())
	{
		Read_Measurement_Row(query, measurement);
		out_measurements.append(measurement);
	}

	return true;
}
//----------------------------------------------------------------------------
bool AMeasurement_Repository::Load_Latest(const QString &in_user_id, SBody_Measurement &out_measurement)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();
	out_measurement = SBody_Measurement();

	if (Check_Open() == false)
		return false;

	query.prepare(QString("SELECT %1 FROM body_measurements WHERE user_id = ? ORDER BY date DESC LIMIT 1")
	                  .arg(MEASUREMENT_Columns));
	query.addBindValue(in_user_id);

	if (query.exec() == false)
		return Set_Last_Error(QString("Не удалось прочитать последний замер: %1").arg(query.lastError().text()));

	if (query.next() == false)
		return Set_Last_Error(QString("У пользователя %1 нет замеров").arg(in_user_id));

	Read_Measurement_Row(query, out_measurement);

	return true;
}
//----------------------------------------------------------------------------
bool AMeasurement_Repository::Save_Measurement(const SBody_Measurement &in_measurement)
{
	QSqlQuery query(Connection());
	QString measurement_id = in_measurement.ID;

	Clear_Last_Error();

	if (Check_Open() == false)
		return false;

	if (in_measurement.User_Id.isEmpty())
		return Set_Last_Error("Замеру нужен владелец (userId)");

	if (Is_Valid_Date(in_measurement.Date) == false)
		return Set_Last_Error(QString("Некорректная дата замера: %1").arg(in_measurement.Date));

	if (in_measurement.Has_Weight_Kg && (in_measurement.Weight_Kg <= 0 || in_measurement.Weight_Kg >= 1000))
		return Set_Last_Error("Масса тела должна быть больше 0 и меньше 1000 кг");

	if ((in_measurement.Has_Chest_Cm && in_measurement.Chest_Cm <= 0) ||
	    (in_measurement.Has_Waist_Cm && in_measurement.Waist_Cm <= 0) ||
	    (in_measurement.Has_Hip_Cm && in_measurement.Hip_Cm <= 0) ||
	    (in_measurement.Has_Arm_Cm && in_measurement.Arm_Cm <= 0) ||
	    (in_measurement.Has_Thigh_Cm && in_measurement.Thigh_Cm <= 0))
		return Set_Last_Error("Обхваты тела должны быть положительными");

	if (Has_Any_Value(in_measurement) == false)
		return Set_Last_Error("Замер без значений не сохраняется");

	if (measurement_id.isEmpty())
		measurement_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

	// Слияние значений дня: пустые поля не затирают уже записанные (§3.8),
	// идентификатор существующей строки сохраняется.
	query.prepare("INSERT INTO body_measurements(id, user_id, date, weight_kg, chest_cm, waist_cm, hip_cm, arm_cm, "
	              "thigh_cm) VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?) "
	              "ON CONFLICT(user_id, date) DO UPDATE SET "
	              "weight_kg = COALESCE(excluded.weight_kg, weight_kg), "
	              "chest_cm = COALESCE(excluded.chest_cm, chest_cm), "
	              "waist_cm = COALESCE(excluded.waist_cm, waist_cm), "
	              "hip_cm = COALESCE(excluded.hip_cm, hip_cm), "
	              "arm_cm = COALESCE(excluded.arm_cm, arm_cm), "
	              "thigh_cm = COALESCE(excluded.thigh_cm, thigh_cm)");
	query.addBindValue(measurement_id);
	query.addBindValue(in_measurement.User_Id);
	query.addBindValue(in_measurement.Date);
	query.addBindValue(Number_Column(in_measurement.Weight_Kg, in_measurement.Has_Weight_Kg));
	query.addBindValue(Number_Column(in_measurement.Chest_Cm, in_measurement.Has_Chest_Cm));
	query.addBindValue(Number_Column(in_measurement.Waist_Cm, in_measurement.Has_Waist_Cm));
	query.addBindValue(Number_Column(in_measurement.Hip_Cm, in_measurement.Has_Hip_Cm));
	query.addBindValue(Number_Column(in_measurement.Arm_Cm, in_measurement.Has_Arm_Cm));
	query.addBindValue(Number_Column(in_measurement.Thigh_Cm, in_measurement.Has_Thigh_Cm));

	if (query.exec())
		return true;

	return Set_Last_Error(QString("Не удалось сохранить замер: %1").arg(query.lastError().text()));
}
//----------------------------------------------------------------------------
bool AMeasurement_Repository::Delete_Measurement(const QString &in_user_id, const QString &in_date)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();

	if (Check_Open() == false)
		return false;

	query.prepare("DELETE FROM body_measurements WHERE user_id = ? AND date = ?");
	query.addBindValue(in_user_id);
	query.addBindValue(in_date);

	if (query.exec() == false)
		return Set_Last_Error(QString("Не удалось удалить замер: %1").arg(query.lastError().text()));

	if (query.numRowsAffected() != 1)
		return Set_Last_Error(QString("Замер пользователя %1 за %2 не найден").arg(in_user_id).arg(in_date));

	return true;
}
//----------------------------------------------------------------------------
