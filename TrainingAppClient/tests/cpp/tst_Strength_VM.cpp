#include "Data/Database/Sql_Database.h"
#include "Presentation/Strength_VM.h"

#include <QTemporaryDir>
#include <QtTest>

//-------------------------------------------------------------------------------------------------------------
class AStrength_VM_Test : public QObject
{
	Q_OBJECT
private slots:
	void factsRefreshAndIsolation();
	void missingDependenciesClearData();
};
//-------------------------------------------------------------------------------------------------------------
void AStrength_VM_Test::factsRefreshAndIsolation()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QVERIFY(database.Open(directory.filePath("strength.sqlite3")));
	AProgram_Repository programs(&database);
	AWorkout_Repository workouts(&database);
	SProgram program;
	program.ID = "strength-program";
	program.User_Id = "u";
	program.Title = "Strength";
	program.Term.Start_Date = "2026-10-01";
	program.Term.Weeks_Total = 4;
	program.Rep_Tolerance = 0;
	QVERIFY(programs.Insert_Program(program));
	Avm_Strength vm(&programs, &workouts);
	QVERIFY(vm.reload("u", program.ID));
	QVERIFY(!vm.Has_Data());
	QCOMPARE(vm.repTolerance(), 0);
	QVERIFY(vm.programIndex().value("percent").isNull());
	for (int i = 0; i < 3; ++i)
	{
		SWorkout_Session session;
		session.ID = QString("session-%1").arg(i);
		session.User_Id = "u";
		session.Program_Id = program.ID;
		session.Date = QString("2026-10-0%1").arg(i + 1);
		session.State = i == 2 ? EWorkout_State::Draft : EWorkout_State::Completed;
		QVERIFY(workouts.Insert_Session(session));
		SSet_Log set;
		set.ID = QString("set-%1").arg(i);
		set.Session_Id = session.ID;
		set.Exercise_Id = "ex_squat";
		set.Done = true;
		set.Weight_Kg = 60 + 5 * i;
		set.Reps = 10 - i;
		QVERIFY(workouts.Save_Set(set));
	}
	QSignalSpy changed(&vm, &Avm_Strength::changed);
	QVERIFY(vm.reload("u", program.ID));
	QVERIFY(vm.Has_Data());
	QCOMPARE(vm.status_rows()->Row_Count(), 2);
	QCOMPARE(vm.exercises()->Row_Count(), 1);
	QCOMPARE(vm.exercises()->get(0).value("exerciseTitle").toString(), QString("Присед"));
	QCOMPARE(vm.exercises()->get(0).value("status").toString(), QString("worse"));
	QCOMPARE(vm.exercises()->get(0).value("startWeightKg").toDouble(), 60.0);
	QCOMPARE(vm.exercises()->get(0).value("currentWeightKg").toDouble(), 65.0);
	QCOMPARE(vm.programIndex().value("percent").toDouble(), -100.0);
	QCOMPARE(changed.count(), 1);
	QVERIFY(!vm.reload("foreign", program.ID));
	QVERIFY(!vm.Has_Data());
	QCOMPARE(vm.exercises()->Row_Count(), 0);
	QCOMPARE(vm.status_rows()->Row_Count(), 0);
	QVERIFY(!vm.errorKey().isEmpty());
	QVERIFY(vm.reload("u", program.ID));
	QVERIFY(vm.errorKey().isEmpty());
	QVERIFY(!vm.reload("u", "missing"));
	QCOMPARE(vm.programTitle(), QString());
}
//-------------------------------------------------------------------------------------------------------------
void AStrength_VM_Test::missingDependenciesClearData()
{
	Avm_Strength vm(0, 0);
	QVERIFY(!vm.reload("u", "p"));
	QVERIFY(!vm.Has_Data());
	QVERIFY(!vm.errorKey().isEmpty());
	QCOMPARE(vm.splits()->Row_Count(), 0);
}
//-------------------------------------------------------------------------------------------------------------
QTEST_GUILESS_MAIN(AStrength_VM_Test)
#include "tst_Strength_VM.moc"
