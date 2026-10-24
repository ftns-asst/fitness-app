#include "Data/Database/Sql_Database.h"
#include "Data/Program/Measurement_Repository.h"
#include "Domain/Program_Model.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class AMeasurement_Repository_Test : public QObject
{
	Q_OBJECT

private slots:
	void saveAndMergeSameDay();
	void saveValidatesInput();
	void rangeAndLatest();
	void deleteMeasurement();
	void measurementsArePerUser();

private:
	QString Make_Database_Path();
	SBody_Measurement Make_Measurement(const QString &in_user_id, const QString &in_date);

	QTemporaryDir Directory;
	int Counter = 0;
};
//----------------------------------------------------------------------------
QString AMeasurement_Repository_Test::Make_Database_Path()
{
	++Counter;

	return Directory.filePath(QString("measurements_%1.sqlite3").arg(Counter));
}
//----------------------------------------------------------------------------
SBody_Measurement AMeasurement_Repository_Test::Make_Measurement(const QString &in_user_id, const QString &in_date)
{
	SBody_Measurement measurement;

	measurement.User_Id = in_user_id;
	measurement.Date = in_date;

	return measurement;
}
//----------------------------------------------------------------------------
void AMeasurement_Repository_Test::saveAndMergeSameDay()
{
	AsSql_Database database;
	AMeasurement_Repository repository(&database);
	SBody_Measurement measurement;
	SBody_Measurement loaded;
	QSqlQuery query;
	QString first_id;

	QVERIFY(database.Open(Make_Database_Path()));
	query = QSqlQuery(database.Sql_Connection());

	// Утренний вес.
	measurement = Make_Measurement("u1", "2026-09-29");
	measurement.Weight_Kg = 82.5;
	measurement.Has_Weight_Kg = true;
	QVERIFY(repository.Save_Measurement(measurement));
	QVERIFY(repository.Find_Measurement("u1", "2026-09-29", loaded));
	QVERIFY(loaded.ID.isEmpty() == false);
	first_id = loaded.ID;
	QCOMPARE(loaded.Weight_Kg, 82.5);
	QVERIFY(loaded.Has_Waist_Cm == false);

	// Вечерние обхваты не затирают утренний вес (§3.8).
	measurement = Make_Measurement("u1", "2026-09-29");
	measurement.Waist_Cm = 84;
	measurement.Has_Waist_Cm = true;
	measurement.Chest_Cm = 104;
	measurement.Has_Chest_Cm = true;
	QVERIFY(repository.Save_Measurement(measurement));

	QVERIFY(repository.Find_Measurement("u1", "2026-09-29", loaded));
	QCOMPARE(loaded.ID, first_id);
	QCOMPARE(loaded.Weight_Kg, 82.5);
	QCOMPARE(loaded.Waist_Cm, 84.0);
	QCOMPARE(loaded.Chest_Cm, 104.0);
	QVERIFY(loaded.Has_Hip_Cm == false);

	// Повторный вес того же дня обновляет значение, а не создаёт строку.
	measurement = Make_Measurement("u1", "2026-09-29");
	measurement.Weight_Kg = 82;
	measurement.Has_Weight_Kg = true;
	measurement.Thigh_Cm = 58;
	measurement.Has_Thigh_Cm = true;
	QVERIFY(repository.Save_Measurement(measurement));

	QVERIFY(repository.Find_Measurement("u1", "2026-09-29", loaded));
	QCOMPARE(loaded.ID, first_id);
	QCOMPARE(loaded.Weight_Kg, 82.0);
	QCOMPARE(loaded.Thigh_Cm, 58.0);
	QCOMPARE(loaded.Waist_Cm, 84.0);
	QVERIFY(query.exec("SELECT COUNT(*) FROM body_measurements WHERE user_id = 'u1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 1);
}
//----------------------------------------------------------------------------
void AMeasurement_Repository_Test::saveValidatesInput()
{
	AsSql_Database database;
	AMeasurement_Repository repository(&database);
	SBody_Measurement measurement;
	SBody_Measurement loaded;
	QVector<SBody_Measurement> empty_range;

	QVERIFY(database.Open(Make_Database_Path()));
	measurement = Make_Measurement("u1", "2026-09-29");
	QVERIFY(repository.Save_Measurement(measurement) == false);
	QVERIFY(repository.Get_Last_Error().isEmpty() == false);

	measurement = Make_Measurement(QString(), "2026-09-29");
	measurement.Weight_Kg = 80;
	measurement.Has_Weight_Kg = true;
	QVERIFY(repository.Save_Measurement(measurement) == false);

	measurement = Make_Measurement("u1", "29.09.2026");
	measurement.Weight_Kg = 80;
	measurement.Has_Weight_Kg = true;
	QVERIFY(repository.Save_Measurement(measurement) == false);
	measurement.Date = "2026-13-01";
	QVERIFY(repository.Save_Measurement(measurement) == false);
	measurement.Date = QString();
	QVERIFY(repository.Save_Measurement(measurement) == false);

	measurement = Make_Measurement("u1", "2026-09-29");
	measurement.Weight_Kg = 1000;
	measurement.Has_Weight_Kg = true;
	QVERIFY(repository.Save_Measurement(measurement) == false);
	measurement.Weight_Kg = 80;
	measurement.Waist_Cm = -1;
	measurement.Has_Waist_Cm = true;
	QVERIFY(repository.Save_Measurement(measurement) == false);

	AsSql_Database closed;
	AMeasurement_Repository closed_repository(&closed);
	measurement = Make_Measurement("u1", "2026-09-29");
	measurement.Weight_Kg = 80;
	measurement.Has_Weight_Kg = true;
	QVERIFY(closed_repository.Save_Measurement(measurement) == false);
	QVERIFY(closed_repository.Find_Measurement("u1", "2026-09-29", loaded) == false);
	QVERIFY(closed_repository.Load_Range("u1", "2026-09-01", "2026-09-30", empty_range) == false);
	QVERIFY(closed_repository.Load_Latest("u1", loaded) == false);
	QVERIFY(closed_repository.Get_Last_Error().isEmpty() == false);
}
//----------------------------------------------------------------------------
void AMeasurement_Repository_Test::rangeAndLatest()
{
	AsSql_Database database;
	AMeasurement_Repository repository(&database);
	SBody_Measurement measurement;
	SBody_Measurement latest;
	QVector<SBody_Measurement> range;

	QVERIFY(database.Open(Make_Database_Path()));
	measurement = Make_Measurement("u1", "2026-09-14");
	measurement.Weight_Kg = 84;
	measurement.Has_Weight_Kg = true;
	QVERIFY(repository.Save_Measurement(measurement));
	measurement = Make_Measurement("u1", "2026-09-21");
	measurement.Weight_Kg = 83.5;
	measurement.Has_Weight_Kg = true;
	QVERIFY(repository.Save_Measurement(measurement));
	measurement = Make_Measurement("u1", "2026-09-28");
	measurement.Weight_Kg = 83;
	measurement.Has_Weight_Kg = true;
	measurement.Waist_Cm = 82;
	measurement.Has_Waist_Cm = true;
	QVERIFY(repository.Save_Measurement(measurement));

	QVERIFY(repository.Load_Range("u1", "2026-09-14", "2026-09-28", range));
	QCOMPARE(range.size(), 3);
	QCOMPARE(range.at(0).Date, QString("2026-09-14"));
	QCOMPARE(range.at(2).Date, QString("2026-09-28"));
	QVERIFY(repository.Load_Range("u1", "2026-09-21", "2026-09-21", range));
	QCOMPARE(range.size(), 1);
	QCOMPARE(range.at(0).Weight_Kg, 83.5);
	QVERIFY(range.at(0).Has_Waist_Cm == false);
	QVERIFY(repository.Load_Range("u1", "2026-08-01", "2026-08-31", range));
	QVERIFY(range.isEmpty());
	QVERIFY(repository.Load_Latest("u1", latest));
	QCOMPARE(latest.Date, QString("2026-09-28"));
	QCOMPARE(latest.Weight_Kg, 83.0);
	QCOMPARE(latest.Waist_Cm, 82.0);
	QVERIFY(repository.Load_Latest("u2", latest) == false);
	QVERIFY(repository.Get_Last_Error().isEmpty() == false);
	QVERIFY(repository.Find_Measurement("u2", "2026-09-28", latest) == false);
}
//----------------------------------------------------------------------------
void AMeasurement_Repository_Test::deleteMeasurement()
{
	AsSql_Database database;
	AMeasurement_Repository repository(&database);
	SBody_Measurement measurement;
	SBody_Measurement loaded;
	QVector<SBody_Measurement> range;

	QVERIFY(database.Open(Make_Database_Path()));
	measurement = Make_Measurement("u1", "2026-09-29");
	measurement.Weight_Kg = 82;
	measurement.Has_Weight_Kg = true;
	QVERIFY(repository.Save_Measurement(measurement));
	QVERIFY(repository.Delete_Measurement("u1", "2026-09-29"));
	QVERIFY(repository.Find_Measurement("u1", "2026-09-29", loaded) == false);
	QVERIFY(repository.Load_Range("u1", "2026-09-01", "2026-09-30", range));
	QVERIFY(range.isEmpty());
	QVERIFY(repository.Delete_Measurement("u1", "2026-09-29") == false);
	QVERIFY(repository.Delete_Measurement("u2", "2026-09-29") == false);
	QVERIFY(repository.Get_Last_Error().isEmpty() == false);
}
//----------------------------------------------------------------------------
void AMeasurement_Repository_Test::measurementsArePerUser()
{
	AsSql_Database database;
	AMeasurement_Repository repository(&database);
	SBody_Measurement measurement;
	SBody_Measurement loaded;
	QVector<SBody_Measurement> range;

	QVERIFY(database.Open(Make_Database_Path()));
	measurement = Make_Measurement("u1", "2026-09-29");
	measurement.Weight_Kg = 82;
	measurement.Has_Weight_Kg = true;
	QVERIFY(repository.Save_Measurement(measurement));
	measurement = Make_Measurement("u2", "2026-09-29");
	measurement.Weight_Kg = 95;
	measurement.Has_Weight_Kg = true;
	QVERIFY(repository.Save_Measurement(measurement));
	QVERIFY(repository.Find_Measurement("u1", "2026-09-29", loaded));
	QCOMPARE(loaded.Weight_Kg, 82.0);
	QVERIFY(repository.Find_Measurement("u2", "2026-09-29", loaded));
	QCOMPARE(loaded.Weight_Kg, 95.0);
	QVERIFY(repository.Load_Range("u1", "2026-09-01", "2026-09-30", range));
	QCOMPARE(range.size(), 1);
	QCOMPARE(range.at(0).User_Id, QString("u1"));
}
//----------------------------------------------------------------------------
QTEST_MAIN(AMeasurement_Repository_Test)

#include "tst_Measurement_Repository.moc"
