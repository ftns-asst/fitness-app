#include "Data/Database/Sql_Database.h"
#include "Data/Program/Program_Repository.h"
#include "Domain/Program_Model.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class AProgram_Repository_Test : public QObject
{
	Q_OBJECT

private slots:
	void insertAndLoadProgram();
	void activeProgramLookup();
	void loadProgramsListsEveryStateNewestFirst();
	void loadProgramsIsEmptyForUnknownUser();
	void changeProfileUpdatesTitleGoalLevel();
	void changeProfileRejectsReadOnlyProgram();
	void changeStateValidatesTransitions();
	void editsAreRejectedForReadOnlyStates();
	void replaceStructureBumpsRevisionAndKeepsFacts();
	void replaceScheduleValidatesSlots();
	void goalAndTermEditsJournal();
	void loadFullAssemblesTree();
	void deleteDraftRemovesOnlyDrafts();

private:
	QString Make_Database_Path();
	void Seed_Exercise(AsSql_Database &in_database, const QString &in_id);
	SProgram Make_Program(const QString &in_id, EProgram_State in_state);
	void Stamp_Updated_At(AsSql_Database &in_database, const QString &in_program_id, const QString &in_stamp);
	void Seed_Structure(AProgram_Repository &in_repository, const QString &in_program_id);

	QTemporaryDir Directory;
	int Counter = 0;
};

//-------------------------------------------------------------------------------------------------------------
QString AProgram_Repository_Test::Make_Database_Path()
{
	++Counter;

	return Directory.filePath(QString("repo_%1.sqlite3").arg(Counter));
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Repository_Test::Seed_Exercise(AsSql_Database &in_database, const QString &in_id)
{
	QSqlQuery query(in_database.Sql_Connection());

	query.prepare("INSERT INTO exercises(id, title, muscle_group, equipment, is_custom, created_at) "
	              "VALUES(?, ?, ?, ?, 0, '2026-09-29T00:00:00Z')");
	query.addBindValue(in_id);
	query.addBindValue(QString("Упражнение %1").arg(in_id));
	query.addBindValue("legs");
	query.addBindValue("barbell");
	QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}
//-------------------------------------------------------------------------------------------------------------
SProgram AProgram_Repository_Test::Make_Program(const QString &in_id, EProgram_State in_state)
{
	SProgram program;

	program.ID = in_id;
	program.User_Id = "u1";
	program.Title = "Сила 12 недель";
	program.Source_Kind = EProgram_Source_Kind::Scratch;
	program.Goal = EProgram_Goal::Strength;
	program.Level = EProgram_Level::Beginner;
	program.Schedule_Kind = ESchedule_Kind::Weekday;
	program.State = in_state;
	program.Revision = 1;
	program.Term.Start_Date = "2026-09-29";
	program.Term.Weeks_Total = 12;
	program.Term.Mode = ETerm_Mode::FixedWeeks;

	return program;
}
//-------------------------------------------------------------------------------------------------------------
// Прямая запись updated_at в обход репозитория: штамп ставится самой записью,
// а для проверки порядка списка нужны заранее заданные, различимые значения.
void AProgram_Repository_Test::Stamp_Updated_At(AsSql_Database &in_database, const QString &in_program_id,
                                                const QString &in_stamp)
{
	QSqlQuery query(in_database.Sql_Connection());

	query.prepare("UPDATE programs SET updated_at = ? WHERE id = ?");
	query.addBindValue(in_stamp);
	query.addBindValue(in_program_id);
	QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Repository_Test::Seed_Structure(AProgram_Repository &in_repository, const QString &in_program_id)
{
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> exercises;
	QVector<SWeek_Slot> week_slots;
	SProgram_Day day;
	SDay_Exercise exercise;
	SWeek_Slot slot;

	day = SProgram_Day();
	day.ID = "d1";
	day.Program_Id = in_program_id;
	day.Title = "День A — Ноги";
	day.Kind = EDay_Kind::Workout;
	day.Focus << "legs";
	day.Estimated_Minutes = 60;
	day.Order_Index = 0;
	days.append(day);

	day = SProgram_Day();
	day.ID = "d2";
	day.Program_Id = in_program_id;
	day.Title = "День B — Жим";
	day.Kind = EDay_Kind::Workout;
	day.Focus << "push";
	day.Estimated_Minutes = 55;
	day.Order_Index = 1;
	days.append(day);

	exercise = SDay_Exercise();
	exercise.ID = "dx1";
	exercise.Program_Day_Id = "d1";
	exercise.Exercise_Id = "ex1";
	exercise.Order_Index = 0;
	exercise.Sets = 4;
	exercise.Reps_Min = 8;
	exercise.Reps_Max = 12;
	exercise.Weight_Mode = EWeight_Mode::Fixed;
	exercise.Target_Weight_Kg = 60;
	exercise.Has_Target_Weight_Kg = true;
	exercise.Rest_Seconds = 120;
	exercise.Progression.Type = EProgression_Type::LinearWeight;
	exercise.Progression.Increment_Kg = 2.5;
	exercise.Progression.Has_Increment_Kg = true;
	exercise.Has_Progression = true;
	exercise.Substitution_Ids << "ex1b";
	exercises.append(exercise);

	exercise = SDay_Exercise();
	exercise.ID = "dx2";
	exercise.Program_Day_Id = "d2";
	exercise.Exercise_Id = "ex2";
	exercise.Order_Index = 0;
	exercise.Sets = 3;
	exercise.Reps_Min = 6;
	exercise.Reps_Max = 6;
	exercise.Weight_Mode = EWeight_Mode::ByFeel;
	exercises.append(exercise);

	for (int day_of_week = 1; day_of_week <= 7; ++day_of_week)
	{
		slot = SWeek_Slot();
		slot.ID = QString("s%1").arg(day_of_week);
		slot.Program_Id = in_program_id;
		slot.Day_Of_Week = day_of_week;

		if (day_of_week == 1)
		{
			slot.Program_Day_Id = "d1";
			slot.Kind = ESlot_Kind::Workout;
			slot.Is_Anchor = true;
		}
		else if (day_of_week == 3)
		{
			slot.Program_Day_Id = "d2";
			slot.Kind = ESlot_Kind::Workout;
		}
		else
			slot.Kind = ESlot_Kind::Rest;

		week_slots.append(slot);
	}

	QVERIFY(in_repository.Replace_Structure(in_program_id, days, exercises, week_slots, ERevision_Reason::Days,
	                                        "Первичный план"));
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Repository_Test::insertAndLoadProgram()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	SProgram program;
	SProgram loaded;
	QSqlQuery query;
	QString path;

	path = Make_Database_Path();
	QVERIFY(database.Open(path));
	Seed_Exercise(database, "ex1");
	query = QSqlQuery(database.Sql_Connection());

	program = Make_Program("p1", EProgram_State::Draft);
	program.Note = "Заметка";
	QVERIFY(repository.Insert_Program(program));
	QVERIFY(repository.Load_Program("p1", loaded));
	QCOMPARE(loaded.Title, QString("Сила 12 недель"));
	QCOMPARE(loaded.Goal, EProgram_Goal::Strength);
	QCOMPARE(loaded.State, EProgram_State::Draft);
	QCOMPARE(loaded.Revision, 1);
	QCOMPARE(loaded.Term.Start_Date, QString("2026-09-29"));
	QCOMPARE(loaded.Term.Weeks_Total, 12);
	QVERIFY(!loaded.Created_At.isEmpty()); // репозиторий штампует время

	// Материализованное производное поле и журнал ревизии 1.
	QVERIFY(query.exec("SELECT end_date_generated FROM programs WHERE id = 'p1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toString(), QString("2026-12-21"));
	QVERIFY(query.exec("SELECT revision, reason, summary FROM program_revisions WHERE program_id = 'p1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 1);
	QCOMPARE(query.value(1).toString(), QString("state"));

	// Отсутствующая программа и закрытая база — ошибки.
	QVERIFY(!repository.Load_Program("missing", loaded));
	QVERIFY(!repository.Get_Last_Error().isEmpty());
	QVERIFY(!repository.Replace_Structure("missing", QVector<SProgram_Day>(), QVector<SDay_Exercise>(),
	                                      QVector<SWeek_Slot>(), ERevision_Reason::Days, QString()));

	AsSql_Database closed;
	AProgram_Repository closed_repository(&closed);
	QVERIFY(!closed_repository.Insert_Program(program));
	QVERIFY(!closed_repository.Get_Last_Error().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Repository_Test::activeProgramLookup()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	QString active_id;
	QString path;

	path = Make_Database_Path();
	QVERIFY(database.Open(path));

	QVERIFY(repository.Insert_Program(Make_Program("p1", EProgram_State::Active)));
	QVERIFY(repository.Get_Active_Program_Id("u1", active_id));
	QCOMPARE(active_id, QString("p1"));

	// Инвариант 1 §4: вторая активная невозможна
	// (частичный уникальный индекс).
	QVERIFY(!repository.Insert_Program(Make_Program("p2", EProgram_State::Active)));
	QVERIFY(!repository.Get_Last_Error().isEmpty());

	// Она же невозможна и переходом состояния.
	QVERIFY(repository.Insert_Program(Make_Program("p2", EProgram_State::Paused)));
	QVERIFY(!repository.Change_State("p2", EProgram_State::Active));

	QVERIFY(!repository.Get_Active_Program_Id("u2", active_id));
}
//-------------------------------------------------------------------------------------------------------------
// Список программ пользователя (каталог, срез S1). Особенность: список у
// несуществующего пользователя — это пустой список, а не ошибка. Каталог на
// первом запуске обязан показывать приглашение создать программу, поэтому
// «нет ни одной программы» — нормальное состояние, в отличие от Load_Program,
// где отсутствие строки означает промах по идентификатору.
void AProgram_Repository_Test::loadProgramsListsEveryStateNewestFirst()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	QVector<SProgram> programs;
	QString path;

	path = Make_Database_Path();
	QVERIFY(database.Open(path));

	QVERIFY(repository.Insert_Program(Make_Program("p_draft", EProgram_State::Draft)));
	QVERIFY(repository.Insert_Program(Make_Program("p_active", EProgram_State::Active)));
	QVERIFY(repository.Insert_Program(Make_Program("p_done", EProgram_State::Completed)));

	// Порядок определяется updated_at: он задаётся репозиторием при записи, и
	// последняя вставка должна оказаться первой в списке.
	Stamp_Updated_At(database, "p_draft", "2026-01-01T00:00:00Z");
	Stamp_Updated_At(database, "p_done", "2026-02-01T00:00:00Z");
	Stamp_Updated_At(database, "p_active", "2026-03-01T00:00:00Z");

	QVERIFY(repository.Load_Programs("u1", programs));
	QCOMPARE(programs.size(), 3);
	QCOMPARE(programs.at(0).ID, QString("p_active"));
	QCOMPARE(programs.at(1).ID, QString("p_done"));
	QCOMPARE(programs.at(2).ID, QString("p_draft"));

	// Состояние и название доезжают из своих колонок, а не из заглушки.
	QCOMPARE(programs.at(0).State, EProgram_State::Active);
	QCOMPARE(programs.at(0).Title, QString("Сила 12 недель"));
	QCOMPARE(programs.at(0).Term.Weeks_Total, 12);

	// Программа другого пользователя в список не попадает.
	SProgram foreign = Make_Program("p_foreign", EProgram_State::Draft);
	foreign.User_Id = "u2";
	QVERIFY(repository.Insert_Program(foreign));

	QVERIFY(repository.Load_Programs("u1", programs));
	QCOMPARE(programs.size(), 3);
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Repository_Test::loadProgramsIsEmptyForUnknownUser()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	QVector<SProgram> programs;
	QString path;

	path = Make_Database_Path();
	QVERIFY(database.Open(path));

	QVERIFY(repository.Insert_Program(Make_Program("p1", EProgram_State::Active)));

	QVERIFY(repository.Load_Programs("u_nobody", programs));
	QVERIFY(programs.isEmpty());
	QVERIFY(repository.Get_Last_Error().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
// Смена паспортных полей программы (название, цель, уровень). Редактор после
// Load() обязан сохранять их, иначе правка заголовка терялась бы молча.
void AProgram_Repository_Test::changeProfileUpdatesTitleGoalLevel()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	SProgram loaded;
	QString path;

	path = Make_Database_Path();
	QVERIFY(database.Open(path));
	QVERIFY(repository.Insert_Program(Make_Program("p1", EProgram_State::Active)));

	QVERIFY(repository.Change_Profile("p1", "Сила 16 недель", EProgram_Goal::Muscle, EProgram_Level::Intermediate));
	QVERIFY(repository.Load_Program("p1", loaded));
	QCOMPARE(loaded.Title, QString("Сила 16 недель"));
	QCOMPARE(loaded.Goal, EProgram_Goal::Muscle);
	QCOMPARE(loaded.Level, EProgram_Level::Intermediate);

	// Правка структуры увеличивает ревизию — иначе статистика не поняла бы,
	// что программа изменилась.
	QCOMPARE(loaded.Revision, 2);
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Repository_Test::changeProfileRejectsReadOnlyProgram()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	SProgram loaded;
	QString path;

	path = Make_Database_Path();
	QVERIFY(database.Open(path));
	QVERIFY(repository.Insert_Program(Make_Program("p1", EProgram_State::Completed)));

	QVERIFY(!repository.Change_Profile("p1", "Другое", EProgram_Goal::Strength, EProgram_Level::Beginner));
	QVERIFY(!repository.Get_Last_Error().isEmpty());
	QVERIFY(repository.Load_Program("p1", loaded));
	QCOMPARE(loaded.Title, QString("Сила 12 недель"));
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Repository_Test::changeStateValidatesTransitions()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	SProgram loaded;
	QString path;

	path = Make_Database_Path();
	QVERIFY(database.Open(path));
	QVERIFY(repository.Insert_Program(Make_Program("p1", EProgram_State::Draft)));

	// Активация штампует actual_start_date и пишет ревизию.
	QVERIFY(repository.Change_State("p1", EProgram_State::Active));
	QVERIFY(repository.Load_Program("p1", loaded));
	QCOMPARE(loaded.State, EProgram_State::Active);
	QCOMPARE(loaded.Revision, 2);
	QVERIFY(!loaded.Actual_Start_Date.isEmpty());

	// Запрещённые переходы.
	QVERIFY(!repository.Change_State("p1", EProgram_State::Draft));
	QVERIFY(!repository.Change_State("p1", EProgram_State::Archived));
	QVERIFY(!repository.Get_Last_Error().isEmpty());

	// Пауза и возврат.
	QVERIFY(repository.Change_State("p1", EProgram_State::Paused));
	QVERIFY(repository.Change_State("p1", EProgram_State::Active));

	// Завершение → архив; из архива хода нет.
	QVERIFY(repository.Change_State("p1", EProgram_State::Completed));
	QVERIFY(!repository.Change_State("p1", EProgram_State::Active));
	QVERIFY(repository.Change_State("p1", EProgram_State::Archived));
	QVERIFY(!repository.Change_State("p1", EProgram_State::Active));

	QVERIFY(repository.Load_Program("p1", loaded));
	QCOMPARE(loaded.Revision, 6); // 6 успешных переходов = 6 ревизий
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Repository_Test::editsAreRejectedForReadOnlyStates()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	STerm term;
	QString path;

	path = Make_Database_Path();
	QVERIFY(database.Open(path));
	QVERIFY(repository.Insert_Program(Make_Program("p1", EProgram_State::Active)));
	QVERIFY(repository.Change_State("p1", EProgram_State::Completed));

	// Инвариант 24 §4: completed/cancelled/archived — read-only по
	// структуре.
	QVERIFY(!repository.Replace_Structure("p1", QVector<SProgram_Day>(), QVector<SDay_Exercise>(),
	                                      QVector<SWeek_Slot>(), ERevision_Reason::Days, QString()));
	QVERIFY(!repository.Replace_Schedule("p1", QVector<SWeek_Slot>()));
	QVERIFY(!repository.Replace_Goal_Targets("p1", QVector<SGoal_Target>()));

	term = STerm();
	term.Start_Date = "2026-10-06";
	term.Weeks_Total = 16;
	term.Mode = ETerm_Mode::FixedWeeks;
	QVERIFY(!repository.Change_Term("p1", term));
	QVERIFY(!repository.Get_Last_Error().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Repository_Test::replaceStructureBumpsRevisionAndKeepsFacts()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> exercises;
	QVector<SWeek_Slot> week_slots;
	QVector<SProgram_Day> loaded_days;
	QVector<SWeek_Slot> loaded_slots;
	SProgram_Day day;
	SDay_Exercise exercise;
	SWeek_Slot slot;
	SProgram loaded;
	QSqlQuery query;
	QString path;
	QString snapshot;

	path = Make_Database_Path();
	QVERIFY(database.Open(path));
	Seed_Exercise(database, "ex1");
	Seed_Exercise(database, "ex2");
	query = QSqlQuery(database.Sql_Connection());

	QVERIFY(repository.Insert_Program(Make_Program("p1", EProgram_State::Active)));
	Seed_Structure(repository, "p1");

	// Факт: завершённая тренировка по дню d1.
	QVERIFY(query.exec("INSERT INTO workouts(id, user_id, program_id, program_day_id, date, state, program_revision) "
	                   "VALUES('w1', 'u1', 'p1', 'd1', '2026-09-29', 'completed', 2)"));

	// Правка на ходу (UC-8): полностью новый состав — один день d3.
	day = SProgram_Day();
	day.ID = "d3";
	day.Title = "День C — Всё тело";
	day.Kind = EDay_Kind::Workout;
	day.Order_Index = 0;
	days.append(day);

	exercise = SDay_Exercise();
	exercise.ID = "dx3";
	exercise.Program_Day_Id = "d3";
	exercise.Exercise_Id = "ex1";
	exercise.Sets = 5;
	exercise.Reps_Min = 5;
	exercise.Reps_Max = 5;
	exercise.Weight_Mode = EWeight_Mode::Fixed;
	exercise.Target_Weight_Kg = 80;
	exercise.Has_Target_Weight_Kg = true;
	exercises.append(exercise);

	for (int day_of_week = 1; day_of_week <= 7; ++day_of_week)
	{
		slot = SWeek_Slot();
		slot.ID = QString("n%1").arg(day_of_week);
		slot.Day_Of_Week = day_of_week;
		slot.Kind = day_of_week == 2 ? ESlot_Kind::Workout : ESlot_Kind::Rest;

		if (day_of_week == 2)
			slot.Program_Day_Id = "d3";

		week_slots.append(slot);
	}

	QVERIFY(repository.Replace_Structure("p1", days, exercises, week_slots, ERevision_Reason::ExercisePlan,
	                                     "Смена состава"));
	QVERIFY(repository.Load_Program("p1", loaded));
	QCOMPARE(loaded.Revision, 3); // 1 создание + 2 правки структуры

	// Новый состав на месте, слоты перелинкованы.
	QVERIFY(repository.Load_Days("p1", loaded_days));
	QCOMPARE(loaded_days.size(), 1);
	QCOMPARE(loaded_days.at(0).ID, QString("d3"));
	QVERIFY(repository.Load_Schedule("p1", loaded_slots));
	QCOMPARE(loaded_slots.size(), 7);
	QCOMPARE(loaded_slots.at(1).Program_Day_Id, QString("d3"));

	// Факт остался, ссылка на удалённый день снята (FK ON DELETE SET NULL).
	QVERIFY(query.exec("SELECT program_day_id FROM workouts WHERE id = 'w1'"));
	QVERIFY(query.next());
	QVERIFY(query.isNull(0));

	// Журнал: снимок старой структуры сохраняет план до правки (§3.9).
	QVERIFY(query.exec("SELECT revision, reason, snapshot_json FROM program_revisions "
	                   "WHERE program_id = 'p1' AND revision = 3"));
	QVERIFY(query.next());
	QCOMPARE(query.value(1).toString(), QString("exercisePlan"));
	snapshot = query.value(2).toString();
	QVERIFY(snapshot.contains("d1"));
	QVERIFY(snapshot.contains("День A — Ноги"));
	QVERIFY(snapshot.contains("dx1"));
	QVERIFY(snapshot.contains("\"targetWeightKg\":60"));
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Repository_Test::replaceScheduleValidatesSlots()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	QVector<SWeek_Slot> week_slots;
	QVector<SWeek_Slot> loaded_slots;
	SWeek_Slot slot;
	SProgram loaded;
	QString path;
	int day_of_week = 0;

	path = Make_Database_Path();
	QVERIFY(database.Open(path));
	Seed_Exercise(database, "ex1");
	Seed_Exercise(database, "ex2");
	QVERIFY(repository.Insert_Program(Make_Program("p1", EProgram_State::Active)));
	Seed_Structure(repository, "p1");

	// Слот-тренировка без ссылки на день.
	slot = SWeek_Slot();
	slot.ID = "bad1";
	slot.Day_Of_Week = 2;
	slot.Kind = ESlot_Kind::Workout;
	week_slots.append(slot);
	QVERIFY(!repository.Replace_Schedule("p1", week_slots));
	QVERIFY(repository.Get_Last_Error().contains("день"));

	// Ссылка на несуществующий день.
	week_slots.clear();
	slot = SWeek_Slot();
	slot.ID = "bad2";
	slot.Day_Of_Week = 2;
	slot.Program_Day_Id = "ghost";
	slot.Kind = ESlot_Kind::Workout;
	week_slots.append(slot);
	QVERIFY(!repository.Replace_Schedule("p1", week_slots));

	// День отдыха со ссылкой на день.
	week_slots.clear();
	slot = SWeek_Slot();
	slot.ID = "bad3";
	slot.Day_Of_Week = 2;
	slot.Program_Day_Id = "d1";
	slot.Kind = ESlot_Kind::Rest;
	week_slots.append(slot);
	QVERIFY(!repository.Replace_Schedule("p1", week_slots));

	// Дубликат day_of_week.
	week_slots.clear();

	for (day_of_week = 1; day_of_week <= 2; ++day_of_week)
	{
		slot = SWeek_Slot();
		slot.ID = QString("dup%1").arg(day_of_week);
		slot.Day_Of_Week = 2;
		slot.Kind = ESlot_Kind::Rest;
		week_slots.append(slot);
	}

	QVERIFY(!repository.Replace_Schedule("p1", week_slots));

	// Отказ ничего не меняет: ревизия и слоты прежние.
	QVERIFY(repository.Load_Program("p1", loaded));
	QCOMPARE(loaded.Revision, 2);
	QVERIFY(repository.Load_Schedule("p1", loaded_slots));
	QCOMPARE(loaded_slots.size(), 7);
	QCOMPARE(loaded_slots.at(0).Program_Day_Id, QString("d1"));
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Repository_Test::goalAndTermEditsJournal()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	QVector<SGoal_Target> targets;
	QVector<SGoal_Target> loaded_targets;
	SGoal_Target target;
	STerm term;
	SProgram loaded;
	QSqlQuery query;
	QString path;
	QString snapshot;

	path = Make_Database_Path();
	QVERIFY(database.Open(path));
	Seed_Exercise(database, "ex1");
	query = QSqlQuery(database.Sql_Connection());
	QVERIFY(repository.Insert_Program(Make_Program("p1", EProgram_State::Active)));

	target = SGoal_Target();
	target.ID = "t1";
	target.Program_Id = "p1";
	target.Metric = EGoal_Metric::BodyWeightKg;
	target.Unit = EGoal_Unit::Kg;
	target.Start_Value = 80;
	target.Has_Start_Value = true;
	target.Target_Value = 75;
	target.Direction = EGoal_Direction::Decrease;
	targets.append(target);

	target = SGoal_Target();
	target.ID = "t2";
	target.Program_Id = "p1";
	target.Metric = EGoal_Metric::RepsAtWeight;
	target.Exercise_Id = "ex1";
	target.Unit = EGoal_Unit::Kg;
	target.Target_Value = 100;
	target.Target_Weight_Kg = 100;
	target.Has_Target_Weight_Kg = true;
	target.Target_Reps = 5;
	target.Has_Target_Reps = true;
	targets.append(target);

	QVERIFY(repository.Replace_Goal_Targets("p1", targets));
	QVERIFY(repository.Load_Program("p1", loaded));
	QCOMPARE(loaded.Revision, 2);
	QVERIFY(repository.Load_Goal_Targets("p1", loaded_targets));
	QCOMPARE(loaded_targets.size(), 2);
	QCOMPARE(loaded_targets.at(0).Metric, EGoal_Metric::BodyWeightKg);
	QCOMPARE(loaded_targets.at(0).Target_Value, 75.0);
	QCOMPARE(loaded_targets.at(1).Target_Weight_Kg, 100.0);
	QVERIFY(loaded_targets.at(1).Has_Target_Reps);
	QCOMPARE(loaded_targets.at(1).Target_Reps, 5);

	// Срок: правка пересчитывает end_date_generated и пишет снимок старого.
	term = STerm();
	term.Start_Date = "2026-10-06";
	term.Weeks_Total = 16;
	term.Mode = ETerm_Mode::FixedWeeks;
	QVERIFY(repository.Change_Term("p1", term));
	QVERIFY(repository.Load_Program("p1", loaded));
	QCOMPARE(loaded.Revision, 3);
	QCOMPARE(loaded.Term.Weeks_Total, 16);
	QVERIFY(query.exec("SELECT end_date_generated FROM programs WHERE id = 'p1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toString(), QString("2027-01-25"));

	// Повторная правка целей: журнал хранит прежние цели.
	targets.clear();
	target = SGoal_Target();
	target.ID = "t3";
	target.Program_Id = "p1";
	target.Metric = EGoal_Metric::TonnagePerWeekKg;
	target.Unit = EGoal_Unit::Kg;
	target.Target_Value = 20000;
	targets.append(target);
	QVERIFY(repository.Replace_Goal_Targets("p1", targets));

	QVERIFY(query.exec("SELECT revision, reason, snapshot_json FROM program_revisions "
	                   "WHERE program_id = 'p1' AND revision = 4"));
	QVERIFY(query.next());
	QCOMPARE(query.value(1).toString(), QString("goal"));
	snapshot = query.value(2).toString();
	QVERIFY(snapshot.contains("t1"));
	QVERIFY(snapshot.contains("t2"));
	QVERIFY(snapshot.contains("\"startValue\":80"));
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Repository_Test::loadFullAssemblesTree()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	QVector<SGoal_Target> targets;
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> exercises;
	QVector<SWeek_Slot> week_slots;
	SProgram program;
	QString path;

	path = Make_Database_Path();
	QVERIFY(database.Open(path));
	Seed_Exercise(database, "ex1");
	Seed_Exercise(database, "ex2");
	QVERIFY(repository.Insert_Program(Make_Program("p1", EProgram_State::Active)));
	Seed_Structure(repository, "p1");

	QVERIFY(repository.Load_Program_Full("p1", program, targets, days, exercises, week_slots));
	QCOMPARE(program.ID, QString("p1"));
	QCOMPARE(program.State, EProgram_State::Active);
	QVERIFY(targets.isEmpty());

	// Дни по порядку order_index, у каждого — свои предписания.
	QCOMPARE(days.size(), 2);
	QCOMPARE(days.at(0).ID, QString("d1"));
	QCOMPARE(days.at(0).Title, QString("День A — Ноги"));
	QCOMPARE(days.at(1).ID, QString("d2"));
	QCOMPARE(exercises.size(), 2);
	QCOMPARE(exercises.at(0).ID, QString("dx1"));
	QVERIFY(exercises.at(0).Has_Progression);
	QCOMPARE(exercises.at(0).Progression.Type, EProgression_Type::LinearWeight);
	QVERIFY(exercises.at(0).Progression.Has_Increment_Kg);
	QCOMPARE(exercises.at(0).Progression.Increment_Kg, 2.5);
	QCOMPARE(exercises.at(0).Substitution_Ids, QStringList() << "ex1b");
	QCOMPARE(exercises.at(0).Weight_Mode, EWeight_Mode::Fixed);
	QCOMPARE(exercises.at(1).ID, QString("dx2"));

	// Слоты по дням недели, якорный понедельник.
	QCOMPARE(week_slots.size(), 7);
	QCOMPARE(week_slots.at(0).Day_Of_Week, 1);
	QCOMPARE(week_slots.at(0).Kind, ESlot_Kind::Workout);
	QVERIFY(week_slots.at(0).Is_Anchor);
	QCOMPARE(week_slots.at(0).Program_Day_Id, QString("d1"));
	QCOMPARE(week_slots.at(1).Kind, ESlot_Kind::Rest);
	QCOMPARE(week_slots.at(2).Program_Day_Id, QString("d2"));

	// Точечная загрузка предписаний одного дня.
	exercises.clear();
	QVERIFY(repository.Load_Day_Exercises("d2", exercises));
	QCOMPARE(exercises.size(), 1);
	QCOMPARE(exercises.at(0).Exercise_Id, QString("ex2"));
}
//-------------------------------------------------------------------------------------------------------------
// Удаление черновика: брошенное создание стирается целиком — дни, предписания
// и расписание снимает каскад FK, сирот в таблицах не остаётся. Программа в
// «живом» состоянии удалению не поддаётся: отказ с ошибкой, а не тихие
// «0 строк».
void AProgram_Repository_Test::deleteDraftRemovesOnlyDrafts()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	QVector<SProgram> programs;
	QVector<SProgram_Day> days;
	QString path;

	path = Make_Database_Path();
	QVERIFY(database.Open(path));

	Seed_Exercise(database, "ex1");
	Seed_Exercise(database, "ex2");

	QVERIFY(repository.Insert_Program(Make_Program("p_draft", EProgram_State::Draft)));
	Seed_Structure(repository, "p_draft");
	QVERIFY(repository.Insert_Program(Make_Program("p_active", EProgram_State::Active)));

	// Активная программа редактором не удаляется: её состояния меняются
	// переходами §3.1 (Change_State/Activate_Exclusive), и Delete_Draft
	// обязан отказаться с ошибкой.
	QVERIFY(!repository.Delete_Draft("p_active"));
	QVERIFY(!repository.Get_Last_Error().isEmpty());

	// Промах по идентификатору — тоже отказ, а не молчаливый успех.
	QVERIFY(!repository.Delete_Draft("p_missing"));

	QVERIFY(repository.Delete_Draft("p_draft"));

	// В каталоге осталась только активная программа.
	QVERIFY(repository.Load_Programs("u1", programs));
	QCOMPARE(programs.size(), 1);
	QCOMPARE(programs.at(0).ID, QString("p_active"));

	// Дни ушли вместе с программой (каскад FK).
	QVERIFY(repository.Load_Days("p_draft", days));
	QCOMPARE(days.size(), 0);
}
//-------------------------------------------------------------------------------------------------------------

QTEST_MAIN(AProgram_Repository_Test)

#include "tst_Program_Repository.moc"
