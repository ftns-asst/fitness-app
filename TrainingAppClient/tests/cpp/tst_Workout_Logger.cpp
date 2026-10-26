#include "Data/Database/Sql_Database.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Workout_Repository.h"
#include "Domain/Workout_Logger.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

//-------------------------------------------------------------------------------------------------------------
// AsWorkout_Logger: запись фактов тренировки — черновик, подходы, завершение
// (§3.8). Факты — единственный источник статистики (§5.4), поэтому проверяется
// именно то, что потом посчитает AsProgress_Calculator.
//-------------------------------------------------------------------------------------------------------------
class AWorkout_Logger_Test : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void startCreatesDraftForDate();
	void startIsIdempotentForSameDay();
	void startRejectsUnknownExerciseDay();
	void savesSetWithPlanSnapshot();
	void setMustBelongToOwnDraft();
	void setValidationRejectsBadValues();
	void finishRecalculatesSummary();
	void finishRejectsForeignSession();
	void draftRecoversAfterRestart();
	void soloSessionHasNoProgramLinks();

private:
	void Seed_Exercise();
	void Seed_Program();
	SSet_Log Make_Set(const QString &in_session_id, const QString &in_exercise_id, int in_number) const;

	QTemporaryDir Directory;
	AsSql_Database Database;
	AWorkout_Repository *Workout_Repository = 0;
	AProgram_Repository *Program_Repository = 0;
};
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Logger_Test::initTestCase()
{
	QVERIFY(Directory.isValid());
	QVERIFY(Database.Open(Directory.filePath("workout_logger.sqlite3")));
	Workout_Repository = new AWorkout_Repository(&Database, this);
	Program_Repository = new AProgram_Repository(&Database, this);
	Seed_Exercise();
	Seed_Program();
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Logger_Test::Seed_Exercise()
{
	QSqlQuery query(Database.Sql_Connection());

	// Каталог приходит из AsSql_Database::Seed_Builtin_Exercises при Open(),
	// поэтому фикстура идемпотентна: тот же id уже есть в базе.
	query.prepare("INSERT OR IGNORE INTO exercises(id, title, muscle_group, equipment, is_custom, created_at) "
	              "VALUES('ex_squat', 'Присед', 'legs', 'barbell', 0, '2026-09-28T00:00:00Z')");
	QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Logger_Test::Seed_Program()
{
	SProgram program;
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> exercises;
	QVector<SWeek_Slot> week_slots;

	program.ID = "p1";
	program.User_Id = "u1";
	program.Title = "Сила";
	program.Source_Kind = EProgram_Source_Kind::Scratch;
	program.Goal = EProgram_Goal::Strength;
	program.Schedule_Kind = ESchedule_Kind::Weekday;
	program.State = EProgram_State::Draft;
	program.Revision = 1;
	program.Term.Start_Date = "2026-10-01";
	program.Term.Weeks_Total = 8;

	for (int day_of_week = 1; day_of_week <= 7; ++day_of_week)
	{
		SProgram_Day day;
		SWeek_Slot slot;

		day.ID = QString("d%1").arg(day_of_week);
		day.Title = QString("День %1").arg(day_of_week);
		day.Kind = EDay_Kind::Workout;
		day.Order_Index = day_of_week;
		days.append(day);

		slot.ID = QString("s%1").arg(day_of_week);
		slot.Program_Id = "p1";
		slot.Program_Day_Id = day.ID;
		slot.Day_Of_Week = day_of_week;
		slot.Kind = ESlot_Kind::Workout;
		week_slots.append(slot);
	}

	SDay_Exercise exercise;

	exercise.ID = "de1";
	exercise.Program_Day_Id = "d3";
	exercise.Exercise_Id = "ex_squat";
	exercise.Order_Index = 1;
	exercise.Sets = 4;
	exercise.Reps_Min = 5;
	exercise.Reps_Max = 8;
	exercise.Weight_Mode = EWeight_Mode::Fixed;
	exercise.Target_Weight_Kg = 100;
	exercise.Has_Target_Weight_Kg = true;
	exercises.append(exercise);

	QVERIFY(Program_Repository->Insert_Program_Full(program, QVector<SGoal_Target>(), days, exercises, week_slots));
	QVERIFY(Program_Repository->Change_State("p1", EProgram_State::Active));
	// Активация поднимает ревизию до 2 — её и запоминает тренировка (§3.8).
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Logger_Test::startCreatesDraftForDate()
{
	AsWorkout_Logger logger(Workout_Repository, Program_Repository);
	SWorkout_Session session;

	QVERIFY(logger.Start_Session("u1", "2026-10-07", "p1", "d3", "заметка", session));
	QVERIFY(!session.ID.isEmpty());
	QCOMPARE(session.State, EWorkout_State::Draft);
	QCOMPARE(session.Program_Id, QString("p1"));
	QCOMPARE(session.Program_Day_Id, QString("d3"));
	QCOMPARE(session.Date, QString("2026-10-07"));
	QCOMPARE(session.Note, QString("заметка"));
	QCOMPARE(session.Program_Revision, 2);
}
//-------------------------------------------------------------------------------------------------------------
// Повторное открытие дня возвращает тот же черновик, а не плодит сессии.
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Logger_Test::startIsIdempotentForSameDay()
{
	AsWorkout_Logger logger(Workout_Repository, Program_Repository);
	SWorkout_Session first;
	SWorkout_Session second;

	QVERIFY(logger.Start_Session("u1", "2026-10-08", "p1", "d3", "", first));
	QVERIFY(logger.Start_Session("u1", "2026-10-08", "p1", "d3", "уточнение", second));
	QCOMPARE(second.ID, first.ID);
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Logger_Test::startRejectsUnknownExerciseDay()
{
	AsWorkout_Logger logger(Workout_Repository, Program_Repository);
	SWorkout_Session session;

	// День не принадлежит программе — привязка была бы фиктивной.
	QVERIFY(!logger.Start_Session("u1", "2026-10-09", "p1", "ghost", "", session));
	QVERIFY(!logger.Get_Last_Error().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
// §3.8: подход хранит снимок предписания, поэтому правка плана потом не
// перепишет историю выполненного.
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Logger_Test::savesSetWithPlanSnapshot()
{
	AsWorkout_Logger logger(Workout_Repository, Program_Repository);
	SWorkout_Session session;
	SSet_Log set;
	QVector<SSet_Log> sets;

	QVERIFY(logger.Start_Session("u1", "2026-10-10", "p1", "d3", "", session));

	set = Make_Set(session.ID, "ex_squat", 1);
	set.Day_Exercise_Id = "de1";
	QVERIFY(logger.Save_Set("u1", set));

	QVERIFY(Workout_Repository->Load_Sets(QStringList{session.ID}, sets));
	QCOMPARE(sets.size(), 1);
	QCOMPARE(sets.first().Target_Weight_Kg, 100.0);
	QVERIFY(sets.first().Has_Target_Weight_Kg);
	QCOMPARE(sets.first().Target_Reps, 5);
	QVERIFY(sets.first().Has_Target_Reps);
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Logger_Test::setMustBelongToOwnDraft()
{
	AsWorkout_Logger logger(Workout_Repository, Program_Repository);
	SWorkout_Session session;
	SSet_Log set;

	QVERIFY(logger.Start_Session("u1", "2026-10-11", "p1", "d3", "", session));
	set = Make_Set(session.ID, "ex_squat", 1);

	// Чужой пользователь не может дописать в чужую сессию.
	QVERIFY(!logger.Save_Set("u_other", set));
	QVERIFY(!logger.Get_Last_Error().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Logger_Test::setValidationRejectsBadValues()
{
	AsWorkout_Logger logger(Workout_Repository, Program_Repository);
	SWorkout_Session session;
	SSet_Log set;

	QVERIFY(logger.Start_Session("u1", "2026-10-12", "p1", "d3", "", session));

	set = Make_Set(session.ID, "ex_squat", 1);
	set.Rpe = 12;
	set.Has_Rpe = true;
	QVERIFY(!logger.Save_Set("u1", set));

	set = Make_Set(session.ID, "ex_squat", 1);
	set.Weight_Kg = -5;
	QVERIFY(!logger.Save_Set("u1", set));

	set = Make_Set(session.ID, "ex_squat", 0);
	QVERIFY(!logger.Save_Set("u1", set));
}
//-------------------------------------------------------------------------------------------------------------
// Итоги сессии считает сам логгер: статистике W7 они нужны готовыми.
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Logger_Test::finishRecalculatesSummary()
{
	AsWorkout_Logger logger(Workout_Repository, Program_Repository);
	SWorkout_Session session;
	SSet_Log set;

	QVERIFY(logger.Start_Session("u1", "2026-10-13", "p1", "d3", "", session));

	set = Make_Set(session.ID, "ex_squat", 1);
	set.Rpe = 8;
	set.Has_Rpe = true;
	QVERIFY(logger.Save_Set("u1", set));

	set = Make_Set(session.ID, "ex_squat", 2);
	set.Weight_Kg = 110;
	set.Rpe = 9;
	set.Has_Rpe = true;
	QVERIFY(logger.Save_Set("u1", set));

	QVERIFY(logger.Finish_Session("u1", session.ID, session));
	QCOMPARE(session.State, EWorkout_State::Completed);
	QCOMPARE(session.Set_Count, 2);
	QCOMPARE(session.Exercise_Count, 1);
	QCOMPARE(session.Tonnage_Kg, 1050.0);
	QVERIFY(session.Has_Avg_Rpe);
	QCOMPARE(session.Avg_Rpe, 8.5);
	QVERIFY(!session.Completed_At.isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Logger_Test::finishRejectsForeignSession()
{
	AsWorkout_Logger logger(Workout_Repository, Program_Repository);
	SWorkout_Session session;

	QVERIFY(logger.Start_Session("u1", "2026-10-14", "p1", "d3", "", session));
	QVERIFY(!logger.Finish_Session("u_other", session.ID, session));
}
//-------------------------------------------------------------------------------------------------------------
// Черновик переживает перезапуск: пользователь не теряет начатое (офлайн).
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Logger_Test::draftRecoversAfterRestart()
{
	AsWorkout_Logger writer(Workout_Repository, Program_Repository);
	SWorkout_Session started;

	QVERIFY(writer.Start_Session("u1", "2026-10-15", "p1", "d3", "черновик", started));

	AsWorkout_Logger reader(Workout_Repository, Program_Repository);
	SWorkout_Session recovered;

	QVERIFY(reader.Get_Draft("u1", "2026-10-15", recovered));
	QCOMPARE(recovered.ID, started.ID);
	QCOMPARE(recovered.Note, QString("черновик"));
}
//-------------------------------------------------------------------------------------------------------------
// Соло-тренировка вне программы — законный случай (§4 инвариант 18).
//-------------------------------------------------------------------------------------------------------------
void AWorkout_Logger_Test::soloSessionHasNoProgramLinks()
{
	AsWorkout_Logger logger(Workout_Repository, Program_Repository);
	SWorkout_Session session;

	QVERIFY(logger.Start_Session("u1", "2026-10-16", "", "", "", session));
	QVERIFY(session.Program_Id.isEmpty());
	QVERIFY(session.Program_Day_Id.isEmpty());
	QCOMPARE(session.Program_Revision, 0);
}
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
SSet_Log AWorkout_Logger_Test::Make_Set(const QString &in_session_id, const QString &in_exercise_id,
                                        int in_number) const
{
	SSet_Log set;

	set.Session_Id = in_session_id;
	set.Exercise_Id = in_exercise_id;
	set.Set_Number = in_number;
	set.Weight_Kg = 100;
	set.Reps = 5;
	set.Done = true;

	return set;
}
//-------------------------------------------------------------------------------------------------------------

QTEST_GUILESS_MAIN(AWorkout_Logger_Test)
#include "tst_Workout_Logger.moc"
//-------------------------------------------------------------------------------------------------------------
