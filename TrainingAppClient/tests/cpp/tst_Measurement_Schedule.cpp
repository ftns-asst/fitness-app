#include "Domain/Measurement_Schedule.h"

#include <QtTest>

// Расписание замеров (FR-T7, docs/09): старт, шаг в днях, финиш. Книга-эталон
// меряет раз в 3 дня за 28 дней, то есть десять точек — от «Старт» до «Финиш».
class AMeasurement_Schedule_Test : public QObject
{
	Q_OBJECT

private slots:
	void trackerProgramHasTenPoints();
	void finishOffTheGridIsAppended();
	void invalidInputGivesNoDates();
	void nextDate_data();
	void nextDate();
	void previousDate_data();
	void previousDate();
	void dueFollowsLastMeasurement_data();
	void dueFollowsLastMeasurement();
	void openEndedProgramHasNoFinish();
};

//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Schedule_Test::trackerProgramHasTenPoints()
{
	const QStringList dates = AMeasurement_Schedule::Planned_Dates("2026-10-01", "2026-10-28", 3);

	QCOMPARE(dates.size(), 10);
	QCOMPARE(dates.first(), QString("2026-10-01"));
	QCOMPARE(dates.at(1), QString("2026-10-04"));
	QCOMPARE(dates.last(), QString("2026-10-28"));
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Schedule_Test::finishOffTheGridIsAppended()
{
	// 30 дней при шаге 3: сетка кончается 28-го, финиш 30-го добавляется.
	const QStringList dates = AMeasurement_Schedule::Planned_Dates("2026-10-01", "2026-10-30", 3);

	QCOMPARE(dates.size(), 11);
	QCOMPARE(dates.at(9), QString("2026-10-28"));
	QCOMPARE(dates.last(), QString("2026-10-30"));
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Schedule_Test::invalidInputGivesNoDates()
{
	QVERIFY(AMeasurement_Schedule::Planned_Dates("2026-10-01", "2026-10-28", 0).isEmpty());
	QVERIFY(AMeasurement_Schedule::Planned_Dates("2026-10-01", "2026-10-28", 31).isEmpty());
	QVERIFY(AMeasurement_Schedule::Planned_Dates("2026-10-28", "2026-10-01", 3).isEmpty());
	QVERIFY(AMeasurement_Schedule::Planned_Dates("not-a-date", "2026-10-28", 3).isEmpty());
	QVERIFY(!AMeasurement_Schedule::Is_Valid_Interval(0));
	QVERIFY(AMeasurement_Schedule::Is_Valid_Interval(AMeasurement_Schedule::Default_Interval_Days));
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Schedule_Test::nextDate_data()
{
	QTest::addColumn<QString>("end");
	QTest::addColumn<QString>("from");
	QTest::addColumn<bool>("found");
	QTest::addColumn<QString>("expected");

	QTest::newRow("inside-the-step") << "2026-10-28"
	                                 << "2026-10-05" << true << "2026-10-07";
	QTest::newRow("on-a-point-is-inclusive") << "2026-10-28"
	                                         << "2026-10-04" << true << "2026-10-04";
	QTest::newRow("before-start") << "2026-10-28"
	                              << "2026-09-20" << true << "2026-10-01";
	QTest::newRow("last-grid-point") << "2026-10-28"
	                                 << "2026-10-26" << true << "2026-10-28";
	QTest::newRow("after-finish") << "2026-10-28"
	                              << "2026-10-29" << false << "";
	QTest::newRow("finish-off-the-grid") << "2026-10-30"
	                                     << "2026-10-29" << true << "2026-10-30";
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Schedule_Test::nextDate()
{
	QFETCH(QString, end);
	QFETCH(QString, from);
	QFETCH(bool, found);
	QFETCH(QString, expected);
	QString date;

	QCOMPARE(AMeasurement_Schedule::Next_Date("2026-10-01", end, 3, from, date), found);
	QCOMPARE(date, expected);
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Schedule_Test::previousDate_data()
{
	QTest::addColumn<QString>("to");
	QTest::addColumn<bool>("found");
	QTest::addColumn<QString>("expected");

	QTest::newRow("between-points") << "2026-10-05" << true << "2026-10-04";
	QTest::newRow("on-a-point") << "2026-10-04" << true << "2026-10-04";
	QTest::newRow("start-day") << "2026-10-01" << true << "2026-10-01";
	QTest::newRow("before-start") << "2026-09-30" << false << "";
	QTest::newRow("after-finish") << "2026-11-15" << true << "2026-10-28";
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Schedule_Test::previousDate()
{
	QFETCH(QString, to);
	QFETCH(bool, found);
	QFETCH(QString, expected);
	QString date;

	QCOMPARE(AMeasurement_Schedule::Previous_Date("2026-10-01", "2026-10-28", 3, to, date), found);
	QCOMPARE(date, expected);
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Schedule_Test::dueFollowsLastMeasurement_data()
{
	QTest::addColumn<QString>("today");
	QTest::addColumn<QString>("last");
	QTest::addColumn<bool>("due");

	QTest::newRow("before-start") << "2026-09-30"
	                              << "" << false;
	QTest::newRow("start-day-never-measured") << "2026-10-01"
	                                          << "" << true;
	QTest::newRow("point-day-not-measured") << "2026-10-04"
	                                        << "2026-10-01" << true;
	QTest::newRow("point-day-measured") << "2026-10-04"
	                                    << "2026-10-04" << false;
	QTest::newRow("overdue-after-missed-point") << "2026-10-05"
	                                            << "2026-10-03" << true;
	QTest::newRow("measured-on-the-point-then-wait") << "2026-10-05"
	                                                 << "2026-10-04" << false;
	QTest::newRow("measured-early-covers-point") << "2026-10-04"
	                                             << "2026-10-05" << false;
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Schedule_Test::dueFollowsLastMeasurement()
{
	QFETCH(QString, today);
	QFETCH(QString, last);
	QFETCH(bool, due);

	QCOMPARE(AMeasurement_Schedule::Is_Due("2026-10-01", "2026-10-28", 3, today, last), due);
}
//-------------------------------------------------------------------------------------------------------------
void AMeasurement_Schedule_Test::openEndedProgramHasNoFinish()
{
	QString date;

	QVERIFY(AMeasurement_Schedule::Planned_Dates("2026-10-01", "", 3).isEmpty());
	QVERIFY(AMeasurement_Schedule::Next_Date("2026-10-01", "", 3, "2026-10-05", date));
	QCOMPARE(date, QString("2026-10-07"));
	QVERIFY(AMeasurement_Schedule::Previous_Date("2026-10-01", "", 3, "2027-01-01", date));
	QCOMPARE(date, QString("2026-12-30"));
}
//-------------------------------------------------------------------------------------------------------------

QTEST_GUILESS_MAIN(AMeasurement_Schedule_Test)
#include "tst_Measurement_Schedule.moc"
