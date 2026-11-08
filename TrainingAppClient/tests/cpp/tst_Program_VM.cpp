#include "Data/Database/Sql_Database.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Presentation/Program_Catalog_VM.h"
#include "Presentation/Program_VM.h"

#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

// Avm_Program: собирает дерево активной программы и отдаёт его QML через
// модели-списки и скалярные свойства. Проверяем связку «БД → роли».
class AProgram_VM_Test : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void withoutProgramExposesEmptyState();
	void loadsProgramTreeIntoModels();
	void exposesScalarProgramProperties();
	void daySelectionFiltersExercises();
	void reloadEmitsChanged();
	void reloadLoadsRequestedProgram();
	void reloadRejectsProgramOfAnotherUser();
	void reloadReportsReadOnlyForFinishedProgram();
	void catalogListsAllStates();
	void catalogEmptyStateIsNotError();
	void catalogFailsWithoutRepository();

private:
	void Seed_Exercise(const QString &in_id);
	void Seed_Program();
	QVariant Role_Of(AList_Model &in_model, int in_row, const QString &in_role);
	void Seed_Program_State(AProgram_Repository &in_repository, const QString &in_id, const QString &in_title,
	                        EProgram_State in_state, const QString &in_user_id);

	QTemporaryDir Directory;
	AsSql_Database Database;
	AProgram_Repository *Program_Repository = 0;
	ARule_Repository *Rule_Repository = 0;
};
//-------------------------------------------------------------------------------------------------------------
void AProgram_VM_Test::initTestCase()
{
	QVERIFY(Directory.isValid());
	QVERIFY(Database.Open(Directory.filePath("program_vm.sqlite3")));
	Program_Repository = new AProgram_Repository(&Database, this);
	Rule_Repository = new ARule_Repository(&Database, this);
	// Название упражнения экран берёт из каталога, поэтому фикстура заводит
	// своё упражнение: базовый каталог (ex_squat и остальные) заполняется
	// AsSql_Database при открытии базы и не должен задавать ожидаемый текст.
	Seed_Exercise("ex_fixture");
	Seed_Program();
}
//-------------------------------------------------------------------------------------------------------------
QVariant AProgram_VM_Test::Role_Of(AList_Model &in_model, int in_row, const QString &in_role)
{
	for (int i = 0; i < in_model.Role_Count(); ++i)
	{
		if (in_model.Role_Name(Qt::UserRole + 1 + i) == in_role)
			return in_model.Data(in_row, Qt::UserRole + 1 + i);
	}

	return QVariant();
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_VM_Test::Seed_Exercise(const QString &in_id)
{
	QSqlQuery query(Database.Sql_Connection());

	query.prepare("INSERT INTO exercises(id, title, muscle_group, equipment, is_custom, created_at) "
	              "VALUES(?, ?, ?, ?, 0, '2026-09-29T00:00:00Z')");
	query.addBindValue(in_id);
	query.addBindValue(QString("Упражнение %1").arg(in_id));
	query.addBindValue("legs");
	query.addBindValue("barbell");
	QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_VM_Test::Seed_Program()
{
	SProgram program;
	QVector<SGoal_Target> targets;
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> exercises;
	QVector<SWeek_Slot> week_slots;

	program.ID = "p1";
	program.User_Id = "u1";
	program.Title = "Сила 4 недели";
	program.Source_Kind = EProgram_Source_Kind::Scratch;
	program.Goal = EProgram_Goal::Strength;
	program.Schedule_Kind = ESchedule_Kind::Weekday;
	// Создание программы идёт из состояния draft (§3.1); перевод в active —
	// отдельный переход с записью actual_start_date.
	program.State = EProgram_State::Draft;
	program.Term.Start_Date = "2026-09-28";
	program.Term.Weeks_Total = 4;
	program.Actual_Start_Date = "2026-09-28";

	SGoal_Target goal;

	goal.ID = "g1";
	goal.Program_Id = "p1";
	goal.Metric = EGoal_Metric::WorkingWeightKg;
	goal.Exercise_Id = "ex_fixture";
	goal.Start_Value = 100;
	goal.Has_Start_Value = true;
	goal.Target_Value = 110;
	goal.Direction = EGoal_Direction::Increase;
	targets.append(goal);

	SProgram_Day day;

	day.ID = "d1";
	day.Program_Id = "p1";
	day.Title = "Ноги";
	day.Kind = EDay_Kind::Workout;
	day.Order_Index = 0;
	days.append(day);

	SDay_Exercise exercise;

	exercise.ID = "de1";
	exercise.Program_Day_Id = "d1";
	exercise.Exercise_Id = "ex_fixture";
	exercise.Order_Index = 0;
	exercise.Sets = 4;
	exercise.Reps_Min = 5;
	exercise.Reps_Max = 5;
	exercise.Weight_Mode = EWeight_Mode::Fixed;
	exercise.Target_Weight_Kg = 100;
	exercise.Has_Target_Weight_Kg = true;
	exercises.append(exercise);

	SWeek_Slot slot;

	slot.ID = "slot_1";
	slot.Program_Id = "p1";
	slot.Day_Of_Week = 1;
	slot.Kind = ESlot_Kind::Workout;
	slot.Program_Day_Id = "d1";
	week_slots.append(slot);

	QVERIFY2(Program_Repository->Insert_Program_Full(program, targets, days, exercises, week_slots),
	         qPrintable(Program_Repository->Get_Last_Error()));
	// Активация — отдельный переход: он же проставляет actual_start_date.
	QVERIFY2(Program_Repository->Change_State("p1", EProgram_State::Active),
	         qPrintable(Program_Repository->Get_Last_Error()));
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_VM_Test::withoutProgramExposesEmptyState()
{
	Avm_Program vm(Program_Repository, Rule_Repository, this);

	QVERIFY(!vm.Reload("nobody"));
	QVERIFY(!vm.Has_Program());
	QCOMPARE(vm.week_slots()->Row_Count(), 0);
	QCOMPARE(vm.program_days()->Row_Count(), 0);
	QVERIFY(!vm.errorKey().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_VM_Test::loadsProgramTreeIntoModels()
{
	Avm_Program vm(Program_Repository, Rule_Repository, this);

	QVERIFY(vm.Reload("u1"));
	QVERIFY(vm.Has_Program());
	QCOMPARE(vm.programId(), QString("p1"));
	QCOMPARE(vm.title(), QString("Сила 4 недели"));

	AList_Model *slot_model = vm.week_slots();
	AList_Model *days = vm.program_days();

	QCOMPARE(slot_model->Row_Count(), 1);
	QCOMPARE(days->Row_Count(), 1);
	QCOMPARE(Role_Of(*slot_model, 0, "id").toString(), QString("slot_1"));
	QCOMPARE(Role_Of(*slot_model, 0, "dayTitle").toString(), QString("Ноги"));
	QCOMPARE(Role_Of(*days, 0, "title").toString(), QString("Ноги"));

	// Предписания дня выбираются явно: пока день не выбран — список пуст.
	QCOMPARE(vm.day_exercises()->Row_Count(), 0);
	vm.Select_Day("d1");

	QCOMPARE(vm.day_exercises()->Row_Count(), 1);
	QCOMPARE(Role_Of(*vm.day_exercises(), 0, "title").toString(), QString("Упражнение ex_fixture"));
	QCOMPARE(vm.selectedDayId(), QString("d1"));
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_VM_Test::exposesScalarProgramProperties()
{
	Avm_Program vm(Program_Repository, Rule_Repository, this);

	QVERIFY(vm.Reload("u1"));
	QCOMPARE(vm.goal(), QString("strength"));
	QCOMPARE(vm.state(), QString("active"));
	QCOMPARE(vm.weeksTotal(), 4);
	// Ревизия 2: создание программы пишет ревизию 1, активация инкрементит.
	QCOMPARE(vm.revision(), 2);
	QCOMPARE(vm.goal_targets()->Row_Count(), 1);
	QCOMPARE(vm.rules()->Row_Count(), 0);
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_VM_Test::daySelectionFiltersExercises()
{
	Avm_Program vm(Program_Repository, Rule_Repository, this);

	QVERIFY(vm.Reload("u1"));

	// Неизвестный день не оставляет прежние предписания в модели.
	vm.Select_Day("нет-такого-дня");

	QCOMPARE(vm.day_exercises()->Row_Count(), 0);
	QVERIFY(vm.selectedDayId().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_VM_Test::reloadEmitsChanged()
{
	Avm_Program vm(Program_Repository, Rule_Repository, this);
	QSignalSpy spy(&vm, &Avm_Program::changed);

	QVERIFY(vm.Reload("u1"));
	QCOMPARE(spy.count(), 1);
}

//-------------------------------------------------------------------------------------------------------------
// Программа заданного состояния для сценариев каталога. Отдельный хелпер, а не
// переиспользование Seed_Program: каталогу не нужны дни, цели и расписание, а
// программа нужна конкретная — с заданным названием и владельцем.
void AProgram_VM_Test::Seed_Program_State(AProgram_Repository &in_repository, const QString &in_id,
                                          const QString &in_title, EProgram_State in_state, const QString &in_user_id)
{
	SProgram program;

	program.ID = in_id;
	program.User_Id = in_user_id;
	program.Title = in_title;
	program.Source_Kind = EProgram_Source_Kind::Scratch;
	program.Goal = EProgram_Goal::Strength;
	program.Level = EProgram_Level::Beginner;
	program.Schedule_Kind = ESchedule_Kind::Weekday;
	program.State = in_state;
	program.Revision = 1;
	program.Term.Start_Date = "2026-09-28";
	program.Term.Weeks_Total = 12;
	program.Term.Mode = ETerm_Mode::FixedWeeks;

	QVERIFY(in_repository.Insert_Program(program));
}
//-------------------------------------------------------------------------------------------------------------
// Открытие конкретной программы из каталога (срез S2). Пока reload умел только
// активную, каталог показывал все программы, а открыть можно было лишь одну.
void AProgram_VM_Test::reloadLoadsRequestedProgram()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	ARule_Repository rule_repository(&database);
	Avm_Program vm(&repository, &rule_repository, this);

	QVERIFY(database.Open(Directory.filePath("open_program.sqlite3")));

	Seed_Program_State(repository, "p_first", "Сила 12 недель", EProgram_State::Active, "u1");
	Seed_Program_State(repository, "p_done", "Масса 8 недель", EProgram_State::Completed, "u1");

	// Открытие неактивной программы: она читается так же, как активная, но
	// выбранной может быть любая из каталога.
	QVERIFY(vm.Reload("u1", "p_done"));
	QCOMPARE(vm.title(), QString("Масса 8 недель"));
	QCOMPARE(vm.programId(), QString("p_done"));
	QCOMPARE(vm.state(), QString("completed"));

	// Программа принадлежит пользователю: hasProgram остаётся истинным.
	QCOMPARE(vm.Has_Program(), true);
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_VM_Test::reloadRejectsProgramOfAnotherUser()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	ARule_Repository rule_repository(&database);
	Avm_Program vm(&repository, &rule_repository, this);

	QVERIFY(database.Open(Directory.filePath("open_foreign.sqlite3")));

	Seed_Program_State(repository, "p_mine", "Моя", EProgram_State::Active, "u1");
	Seed_Program_State(repository, "p_foreign", "Чужая", EProgram_State::Active, "u2");

	// Чужую программу открыть нельзя: иначе каталог одного пользователя
	// показывал бы и правил чужие тренировки. Отказ обязан оповестить экран
	// (changed), иначе QML-привязка errorKey не увидит новое состояние.
	QSignalSpy spy(&vm, &Avm_Program::changed);

	QVERIFY(!vm.Reload("u1", "p_foreign"));
	QCOMPARE(vm.Has_Program(), false);
	QVERIFY(!vm.errorKey().isEmpty());
	QVERIFY2(spy.count() > 0, "без changed() QML не перечитает errorKey после отказа");
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_VM_Test::reloadReportsReadOnlyForFinishedProgram()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	ARule_Repository rule_repository(&database);
	Avm_Program vm(&repository, &rule_repository, this);

	QVERIFY(database.Open(Directory.filePath("open_readonly.sqlite3")));

	Seed_Program_State(repository, "p_active", "Сила", EProgram_State::Active, "u1");
	Seed_Program_State(repository, "p_done", "Масса", EProgram_State::Completed, "u1");
	Seed_Program_State(repository, "p_arch", "Старая", EProgram_State::Archived, "u1");

	QVERIFY(vm.Reload("u1", "p_active"));
	QCOMPARE(vm.Is_Read_Only(), false);

	// Завершённая программа читается, но править её структуру нельзя (§3.1):
	// экран обязан знать это и не показывать пункт настройки.
	QVERIFY(vm.Reload("u1", "p_done"));
	QCOMPARE(vm.Has_Program(), true);
	QCOMPARE(vm.Is_Read_Only(), true);

	QVERIFY(vm.Reload("u1", "p_arch"));
	QCOMPARE(vm.Is_Read_Only(), true);
}
//-------------------------------------------------------------------------------------------------------------
// Каталог программ (срез S1). Отдельный сценарий от активной программы:
// пользователю нужны ВСЕ его программы, а не только активная. ViewModel
// раскладывает их по группам состояний; SQL-порядок проверяет
// tst_Program_Repository.
void AProgram_VM_Test::catalogListsAllStates()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	Avm_Program_Catalog catalog(&repository, this);

	QVERIFY(database.Open(Directory.filePath("catalog.sqlite3")));

	// Три программы разных состояний плюс программа чужого пользователя.
	Seed_Program_State(repository, "p_draft", "Черновик силы", EProgram_State::Draft, "u1");
	Seed_Program_State(repository, "p_active", "Сила 12 недель", EProgram_State::Active, "u1");
	Seed_Program_State(repository, "p_done", "Масса 8 недель", EProgram_State::Completed, "u1");
	Seed_Program_State(repository, "p_foreign", "Чужая", EProgram_State::Draft, "u2");

	QVERIFY(catalog.Reload("u1"));
	QCOMPARE(catalog.Has_Programs(), true);
	QCOMPARE(catalog.active_list()->Row_Count(), 1);
	QCOMPARE(catalog.draft_list()->Row_Count(), 1);
	QCOMPARE(catalog.finished_list()->Row_Count(), 1);

	// Чтение прошло без ошибки: пустой каталог — это не сбой (см. следующий тест).
	QVERIFY(catalog.Error_Key().isEmpty());

	// Роли строки несут то, что показывает карточка каталога: заголовок,
	// состояние строкой и срок.
	QCOMPARE(Role_Of(*catalog.active_list(), 0, "title").toString(), QString("Сила 12 недель"));
	QCOMPARE(Role_Of(*catalog.active_list(), 0, "state").toString(), QString("active"));
	QCOMPARE(Role_Of(*catalog.active_list(), 0, "weeksTotal").toInt(), 12);

	// Активная программа правится, завершённая — нет (§3.1). Каталог сообщает
	// это ролью canEdit, иначе экран показал бы пункт меню, который упрётся
	// в отказ репозитория.
	QCOMPARE(Role_Of(*catalog.active_list(), 0, "canEdit").toBool(), true);
	QCOMPARE(Role_Of(*catalog.finished_list(), 0, "canEdit").toBool(), false);

	// Повторная загрузка не дублирует строки: счётчики групп считаются заново
	// по новому набору, а не добавляются к предыдущему.
	QVERIFY(catalog.Reload("u1"));
	QCOMPARE(catalog.active_list()->Row_Count(), 1);
	QCOMPARE(catalog.finished_list()->Row_Count(), 1);
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_VM_Test::catalogEmptyStateIsNotError()
{
	AsSql_Database database;
	AProgram_Repository repository(&database);
	Avm_Program_Catalog catalog(&repository, this);

	QVERIFY(database.Open(Directory.filePath("catalog_empty.sqlite3")));

	// Пустой каталог — законное состояние первого запуска, а не сбой: ошибка
	// чтения и «программ нет» на экране выглядят по-разному.
	QVERIFY(catalog.Reload("u_nobody"));
	QCOMPARE(catalog.Has_Programs(), false);
	QCOMPARE(catalog.active_list()->Row_Count(), 0);
	QVERIFY(catalog.Error_Key().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_VM_Test::catalogFailsWithoutRepository()
{
	Avm_Program_Catalog catalog(0, this);

	// Без хранилища каталог не читается: обязан отдать ошибку, а не молча
	// показать пустоту — иначе экран стёр бы настоящие программы.
	QVERIFY(!catalog.Reload("u1"));
	QVERIFY(!catalog.Error_Key().isEmpty());
	QCOMPARE(catalog.active_list()->Row_Count(), 0);
}
QTEST_GUILESS_MAIN(AProgram_VM_Test)
#include "tst_Program_VM.moc"
