#include "Data/Network/Network_Monitor.h"

#include <QSignalSpy>
#include <QtTest>

class ANetwork_Monitor_Test : public QObject
{
	Q_OBJECT

private slots:
	void unknownDoesNotBlockRequests();
	void transitionsAreDeduplicated();
	void captivePortalIsNotOnline();
};
//-------------------------------------------------------------------------------------------------------------
void ANetwork_Monitor_Test::unknownDoesNotBlockRequests()
{
	AsNetwork_Monitor monitor(0, false);
	QCOMPARE(monitor.status(), QString("unknown"));
	QVERIFY(!monitor.offline());
	QVERIFY(!monitor.available());
}
//-------------------------------------------------------------------------------------------------------------
void ANetwork_Monitor_Test::transitionsAreDeduplicated()
{
	AsNetwork_Monitor monitor(0, false);
	QSignalSpy changed(&monitor, &AsNetwork_Monitor::changed);
	QSignalSpy restored(&monitor, &AsNetwork_Monitor::connectionRestored);
	monitor.Update_State(QNetworkInformation::Reachability::Disconnected, false, true);
	QVERIFY(monitor.offline());
	monitor.Update_State(QNetworkInformation::Reachability::Disconnected, false, true);
	QCOMPARE(changed.count(), 1);
	monitor.Update_State(QNetworkInformation::Reachability::Online, false, true);
	QVERIFY(!monitor.offline());
	QCOMPARE(restored.count(), 1);
	monitor.Update_State(QNetworkInformation::Reachability::Online, false, true);
	QCOMPARE(changed.count(), 2);
	QCOMPARE(restored.count(), 1);
}
//-------------------------------------------------------------------------------------------------------------
void ANetwork_Monitor_Test::captivePortalIsNotOnline()
{
	AsNetwork_Monitor monitor(0, false);
	monitor.Update_State(QNetworkInformation::Reachability::Online, true, true);
	QVERIFY(monitor.offline());
	QCOMPARE(monitor.status(), QString("captive"));
	monitor.Update_State(QNetworkInformation::Reachability::Local, false, true);
	QVERIFY(monitor.offline());
	QCOMPARE(monitor.status(), QString("limited"));
}
//-------------------------------------------------------------------------------------------------------------
QTEST_GUILESS_MAIN(ANetwork_Monitor_Test)
#include "tst_Network_Monitor.moc"
