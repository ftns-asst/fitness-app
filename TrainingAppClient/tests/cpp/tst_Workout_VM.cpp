#include "Data/Database/Sql_Database.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Workout_Repository.h"
#include "Domain/Workout_Logger.h"
#include "Presentation/Workout_VM.h"

#include <QTemporaryDir>
#include <QtTest>

class AWorkout_VM_Test : public QObject
{
	Q_OBJECT
private slots:
	void restoresFactsPlanAndRestAfterRestart();
	void rejectsForeignProgramAndEmptyFinish();
};
//-------------------------------------------------------------------------------------------------------------
void AWorkout_VM_Test::restoresFactsPlanAndRestAfterRestart()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QVERIFY(database.Open(directory.filePath("workout.sqlite")));
	AProgram_Repository programs(&database);
	AWorkout_Repository workouts(&database);
	AsWorkout_Logger logger(&workouts, &programs);
	SProgram program;
	program.ID = "p";
	program.User_Id = "u";
	program.Title = "Программа";
	program.Term.Start_Date = "2026-11-02";
	program.Actual_Start_Date = "2026-11-02";
	program.Term.Weeks_Total = 4;
	SProgram_Day day;
	day.ID = "d";
	day.Title = "Верх";
	day.Kind = EDay_Kind::Workout;
	SDay_Exercise exercise;
	exercise.ID = "de";
	exercise.Program_Day_Id = day.ID;
	exercise.Exercise_Id = "ex_bench";
	exercise.Sets = 2;
	exercise.Reps_Min = 6;
	exercise.Reps_Max = 8;
	exercise.Rest_Seconds = 90;
	SWeek_Slot slot;
	slot.ID = "s";
	slot.Program_Id = program.ID;
	slot.Program_Day_Id = day.ID;
	slot.Day_Of_Week = 1;
	slot.Kind = ESlot_Kind::Workout;
	QVERIFY(programs.Insert_Program_Full(program, {}, {day}, {exercise}, {slot}));
	QVERIFY(programs.Change_State("p", EProgram_State::Active));
	Avm_Workout vm(&logger, &workouts, &programs);
	const QDateTime now = QDateTime::fromString("2026-11-02T12:00:00Z", Qt::ISODate);
	vm.Set_Now(now);
	QVERIFY(vm.startOrResume("u", "p", "2026-11-02"));
	QVERIFY(vm.saveSet(0, 1, 60, 8));
	QVERIFY(vm.saveSet(0, 1, 62.5, 7));
	QCOMPARE(vm.completedSets(), 1);
	QCOMPARE(vm.restRemaining(), 90);
	QVERIFY(vm.adjustRest(15));
	QCOMPARE(vm.restRemaining(), 105);
	exercise.Reps_Min = 12;
	exercise.Reps_Max = 15;
	exercise.Rest_Seconds = 30;
	QVERIFY(programs.Replace_Structure("p", {day}, {exercise}, {slot}, ERevision_Reason::Days, "Правка плана"));
	Avm_Workout restored(&logger, &workouts, &programs);
	restored.Set_Now(now.addSecs(30));
	QVERIFY(restored.checkPending("u"));
	QVERIFY(restored.hasPending());
	QVERIFY(restored.startOrResume("u", "p", "2026-11-03"));
	QCOMPARE(restored.sessionId(), vm.sessionId());
	QCOMPARE(restored.completedSets(), 1);
	QCOMPARE(restored.restRemaining(), 75);
	QCOMPARE(restored.exercises().first().toMap().value("repsMax").toInt(), 8);
	QVERIFY(restored.saveSet(0, 2, 62.5, 6, 8, "Изменил хват"));
	QCOMPARE(restored.exercises().first().toMap().value("facts").toList().at(1).toMap().value("note").toString(),
	         QString("Изменил хват"));
	QCOMPARE(restored.exercises().first().toMap().value("facts").toList().at(1).toMap().value("rpe").toDouble(), 8.0);
	QVERIFY(restored.finish());
	QVERIFY(!restored.hasPending());
	QVector<SSet_Log> sets;
	QVERIFY(workouts.Load_Sets({vm.sessionId()}, sets));
	QCOMPARE(sets.size(), 2);
	QVERIFY(sets.first().Has_Target_Reps_Range);
	QCOMPARE(sets.first().Target_Reps_Max, 8);
	SWorkout_Session session;
	QVERIFY(workouts.Find_Session(vm.sessionId(), session));
	QCOMPARE(session.State, EWorkout_State::Completed);
	QCOMPARE(session.Tonnage_Kg, 812.5);
	QCOMPARE(sets.at(1).Note, QString("Изменил хват"));
	QCOMPARE(vm.completedExercises(), 1);
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_VM_Test::rejectsForeignProgramAndEmptyFinish()
{
	QTemporaryDir directory;
	AsSql_Database database;
	QVERIFY(database.Open(directory.filePath("workout.sqlite")));
	AProgram_Repository programs(&database);
	AWorkout_Repository workouts(&database);
	AsWorkout_Logger logger(&workouts, &programs);
	Avm_Workout vm(&logger, &workouts, &programs);
	QVERIFY(!vm.startOrResume("u", "missing", "2026-11-02"));
	QVERIFY(!vm.finish());
	QVERIFY(!vm.saveSet(0, 1, 60, 8));
	QVERIFY(!vm.checkPending(""));
	QVERIFY(!vm.adjustRest(15));
}
//-------------------------------------------------------------------------------------------------------------
QTEST_GUILESS_MAIN(AWorkout_VM_Test)
#include "tst_Workout_VM.moc"
