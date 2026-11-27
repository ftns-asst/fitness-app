#include "Data/Program/Measurement_Repository.h"

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

#include <cmath>

// Столбцы обхватов в порядке EMeasurement_Key: индекс в запросе = 4 + ключ
// (после id, user_id, date, weight_kg).
static const char *const MEASUREMENT_Circumference_Columns[] = {
    "chest_cm", "waist_cm", "hip_cm", "arm_cm", "thigh_cm", "belly_cm", "shoulders_cm", "calves_cm", "neck_cm"};
static const int MEASUREMENT_First_Circumference_Index = 4;
static const char *MEASUREMENT_Columns =
    "id, user_id, date, weight_kg, chest_cm, waist_cm, hip_cm, arm_cm, thigh_cm, belly_cm, shoulders_cm, calves_cm, "
    "neck_cm";
//-------------------------------------------------------------------------------------------------------------
static void Read_Measurement_Row(QSqlQuery &in_query, SBody_Measurement &out_measurement)
{
	out_measurement = SBody_Measurement();
	out_measurement.ID = in_query.value(0).toString();
	out_measurement.User_Id = in_query.value(1).toString();
	out_measurement.Date = in_query.value(2).toString();

	if (!in_query.isNull(3))
	{
		out_measurement.Has_Weight_Kg = true;
		out_measurement.Weight_Kg = in_query.value(3).toDouble();
	}

	for (int i = 0; i < (int)EMeasurement_Key::Max; ++i)
	{
		if (!in_query.isNull(MEASUREMENT_First_Circumference_Index + i))
			Set_Measurement_Value(out_measurement, (EMeasurement_Key)i,
			                      in_query.value(MEASUREMENT_First_Circumference_Index + i).toDouble());
	}
}
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// AMeasurement_Repository
//-------------------------------------------------------------------------------------------------------------
AMeasurement_Repository::AMeasurement_Repository(AsSql_Database *in_database, QObject *in_parent)
    : AProgram_Repository_Base(in_database, in_parent)
{
}
bool AMeasurement_Repository::Is_Valid_Date(const QString &in_date) const
{
	QDate date = QDate::fromString(in_date, Qt::ISODate);

	return in_date.size() == 10 && date.isValid() && date.toString("yyyy-MM-dd") == in_date;
}
//-------------------------------------------------------------------------------------------------------------

bool AMeasurement_Repository::Has_Any_Value(const SBody_Measurement &in_measurement) const
{
	return Has_Any_Measurement_Value(in_measurement);
}
//-------------------------------------------------------------------------------------------------------------
bool AMeasurement_Repository::Find_Measurement(const QString &in_user_id, const QString &in_date,
                                               SBody_Measurement &out_measurement)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();
	out_measurement = SBody_Measurement();

	if (!Check_Open())
		return false;

	query.prepare(QString("SELECT %1 FROM body_measurements WHERE user_id = ? AND date = ?").arg(MEASUREMENT_Columns));
	query.addBindValue(in_user_id);
	query.addBindValue(in_date);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось найти замер: %1").arg(query.lastError().text()));

	if (!query.next())
		return Set_Last_Error(QString("Замер пользователя %1 за %2 не найден").arg(in_user_id).arg(in_date));

	Read_Measurement_Row(query, out_measurement);

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AMeasurement_Repository::Load_Range(const QString &in_user_id, const QString &in_date_from,
                                         const QString &in_date_to, QVector<SBody_Measurement> &out_measurements)
{
	QSqlQuery query(Connection());
	SBody_Measurement measurement;

	Clear_Last_Error();
	out_measurements.clear();

	if (!Check_Open())
		return false;

	query.prepare(QString("SELECT %1 FROM body_measurements WHERE user_id = ? AND date >= ? AND date <= ? "
	                      "ORDER BY date")
	                  .arg(MEASUREMENT_Columns));
	query.addBindValue(in_user_id);
	query.addBindValue(in_date_from);
	query.addBindValue(in_date_to);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось прочитать замеры: %1").arg(query.lastError().text()));

	while (query.next())
	{
		Read_Measurement_Row(query, measurement);
		out_measurements.append(measurement);
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AMeasurement_Repository::Load_Latest(const QString &in_user_id, SBody_Measurement &out_measurement)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();
	out_measurement = SBody_Measurement();

	if (!Check_Open())
		return false;

	query.prepare(QString("SELECT %1 FROM body_measurements WHERE user_id = ? ORDER BY date DESC LIMIT 1")
	                  .arg(MEASUREMENT_Columns));
	query.addBindValue(in_user_id);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось прочитать последний замер: %1").arg(query.lastError().text()));

	if (!query.next())
		return Set_Last_Error(QString("У пользователя %1 нет замеров").arg(in_user_id));

	Read_Measurement_Row(query, out_measurement);

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AMeasurement_Repository::Latest_Measurement_Date(const QString &in_user_id, QString &out_date)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();
	out_date.clear();

	if (!Check_Open())
		return false;

	query.prepare("SELECT MAX(date) FROM body_measurements WHERE user_id = ?");
	query.addBindValue(in_user_id);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось прочитать дату последнего замера: %1").arg(query.lastError().text()));

	if (query.next() && !query.isNull(0))
		out_date = query.value(0).toString();

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AMeasurement_Repository::Load_Program_Measurement_Keys(const QString &in_program_id, QStringList &out_keys)
{
	QSqlQuery query(Connection());
	QStringList muscles;

	Clear_Last_Error();
	out_keys.clear();

	if (!Check_Open())
		return false;

	if (in_program_id.isEmpty())
		return true;

	// Кэш хранит все группы, FK-зеркало exercises — обычно только первую.
	// Fallback только при отсутствии кэша: пустой список в кэше тоже факт.
	query.prepare("SELECT DISTINCT CASE WHEN c.id IS NOT NULL THEN c.muscle_groups ELSE e.muscle_group END "
	              "FROM program_days d JOIN program_day_exercises p ON p.program_day_id = d.id "
	              "JOIN exercises e ON e.id = p.exercise_id "
	              "LEFT JOIN exercises_cache c ON c.id = e.id WHERE d.program_id = ?");
	query.addBindValue(in_program_id);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось выбрать обхваты программы: %1").arg(query.lastError().text()));

	while (query.next())
		muscles.append(query.value(0).toString().split(',', Qt::SkipEmptyParts));

	out_keys = Measurement_Keys_For_Muscles(muscles);

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool AMeasurement_Repository::Save_Measurement(const SBody_Measurement &in_measurement)
{
	QSqlQuery query(Connection());
	QString measurement_id = in_measurement.ID;

	Clear_Last_Error();

	if (!Check_Open())
		return false;

	if (in_measurement.User_Id.isEmpty())
		return Set_Last_Error("Замеру нужен владелец (userId)");

	if (!Is_Valid_Date(in_measurement.Date))
		return Set_Last_Error(QString("Некорректная дата замера: %1").arg(in_measurement.Date));

	if (in_measurement.Has_Weight_Kg && (in_measurement.Weight_Kg <= 0 || in_measurement.Weight_Kg >= 1000))
		return Set_Last_Error("Масса тела должна быть больше 0 и меньше 1000 кг");

	if (in_measurement.Has_Weight_Kg && !std::isfinite(in_measurement.Weight_Kg))
		return Set_Last_Error("Масса тела должна быть числом");

	for (int i = 0; i < (int)EMeasurement_Key::Max; ++i)
	{
		double circumference;

		if (Get_Measurement_Value(in_measurement, (EMeasurement_Key)i, circumference) &&
		    (!std::isfinite(circumference) || circumference <= 0))
			return Set_Last_Error("Обхваты тела должны быть положительными");
	}

	if (!Has_Any_Value(in_measurement))
		return Set_Last_Error("Замер без значений не сохраняется");

	if (measurement_id.isEmpty())
		measurement_id = QUuid::createUuid().toString(QUuid::WithoutBraces);

	// Слияние значений дня: пустые поля не затирают уже записанные (§3.8),
	// идентификатор существующей строки сохраняется.
	QString columns = "id, user_id, date, weight_kg";
	QString placeholders = "?, ?, ?, ?";
	QString updates = "weight_kg = COALESCE(excluded.weight_kg, weight_kg)";

	for (int i = 0; i < (int)EMeasurement_Key::Max; ++i)
	{
		const QString column = QString::fromLatin1(MEASUREMENT_Circumference_Columns[i]);

		columns += ", " + column;
		placeholders += ", ?";
		updates += ", " + column + " = COALESCE(excluded." + column + ", " + column + ")";
	}

	query.prepare("INSERT INTO body_measurements(" + columns + ") VALUES(" + placeholders +
	              ") ON CONFLICT(user_id, date) DO UPDATE SET " + updates);
	query.addBindValue(measurement_id);
	query.addBindValue(in_measurement.User_Id);
	query.addBindValue(in_measurement.Date);
	query.addBindValue(Number_Column(in_measurement.Weight_Kg, in_measurement.Has_Weight_Kg));

	for (int i = 0; i < (int)EMeasurement_Key::Max; ++i)
	{
		double circumference = -1;
		const bool has_value = Get_Measurement_Value(in_measurement, (EMeasurement_Key)i, circumference);

		query.addBindValue(Number_Column(circumference, has_value));
	}

	if (query.exec())
		return true;

	return Set_Last_Error(QString("Не удалось сохранить замер: %1").arg(query.lastError().text()));
}
//-------------------------------------------------------------------------------------------------------------
bool AMeasurement_Repository::Delete_Measurement(const QString &in_user_id, const QString &in_date)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();

	if (!Check_Open())
		return false;

	query.prepare("DELETE FROM body_measurements WHERE user_id = ? AND date = ?");
	query.addBindValue(in_user_id);
	query.addBindValue(in_date);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось удалить замер: %1").arg(query.lastError().text()));

	if (query.numRowsAffected() != 1)
		return Set_Last_Error(QString("Замер пользователя %1 за %2 не найден").arg(in_user_id).arg(in_date));

	return true;
}
//-------------------------------------------------------------------------------------------------------------
