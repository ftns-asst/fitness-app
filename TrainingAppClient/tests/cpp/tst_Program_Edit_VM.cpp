#include "Data/Database/Sql_Database.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Domain/Program_Service.h"
#include "Presentation/Program_Edit_VM.h"

#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

//----------------------------------------------------------------------------
// Avm_Program_Edit: создание программы с настройкой каждого дня недели.
// Валидация проверяется ДО записи: отказ не должен оставить программу в БД.
//----------------------------------------------------------------------------
class AProgram_Edit_VM_Test : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void resetProducesEmptyEditor();
	void canCreateRequiresUserDateAndTrainingDay();
	void rejectsInvalidStartDate();
	void createsDraftProgramWithSevenDays();
	void createCanActivateImmediately();
	void createWithoutExerciseFailsAndWritesNothing();
	void measurableGoalRequiresTarget();
	void dayKindChangeUpdatesSchedule();
	void addRulePersistsAfterCreate();
	void selectDaySwitchesExerciseModel();
	void removeDayExerciseReindexesOrder();
	void copyDayReplacesTargetDay();
	void addRuleDetailedKeepsCategoryAndPeriodicity();
	void removeRuleAndRemoveTargetClearDraftLists();
	void addDayExerciseRefreshesDayExerciseModel();
	void removeRejectsOutOfRangeIndex();
	void loadFillsEditorFromExistingProgram();
	void loadRefusesReadOnlyProgram();
	void createAfterLoadUpdatesSameProgramInsteadOfCreatingSecond();
	void saveFailureNotifiesScreen();

private:
	void Seed_Exercise();
	Avm_Program_Edit *Make_Editor();
	bool Fill_Minimal_Program(Avm_Program_Edit *in_editor);
	int Count_Programs() const;

	// Значение роли по имени: индекс роли — деталь реализации модели, и тест не
	// должен зависеть от её внутреннего порядка. Роли начинаются с
	// Qt::UserRole + 1, поэтому перебор начинается не с нуля.
	static QVariant Role_Value(const AList_Model *in_model, int in_row, const QString &in_role)
	{
		for (int role = Qt::UserRole + 1; role < Qt::UserRole + 1 + in_model->Role_Count(); ++role)
		{
			if (in_model->Role_Name(role) == in_role)
				return in_model->Data(in_row, role);
		}

		return QVariant();
	}

	QTemporaryDir Directory;
	AsSql_Database Database;
	AProgram_Repository *Program_Repository = 0;
	ARule_Repository *Rule_Repository = 0;
};
//----------------------------------------------------------------------------
void AProgram_Edit_VM_Test::initTestCase()
{
	QVERIFY(Directory.isValid());
	QVERIFY(Database.Open(Directory.filePath("program_edit_vm.sqlite3")));
	Program_Repository = new AProgram_Repository(&Database, this);
	Rule_Repository = new ARule_Repository(&Database, this);
	Seed_Exercise();
}
//----------------------------------------------------------------------------
void AProgram_Edit_VM_Test::Seed_Exercise()
{
	QSqlQuery query(Database.Sql_Connection());

	// Каталог приходит из AsSql_Database::Seed_Builtin_Exercises при Open(),
	// поэтому фикстура идемпотентна: тот же id уже есть в базе.
	query.prepare("INSERT OR IGNORE INTO exercises(id, title, muscle_group, equipment, is_custom, created_at) "
	              "VALUES('ex_squat', 'Присед', 'legs', 'barbell', 0, '2026-09-28T00:00:00Z')");
	QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}
//----------------------------------------------------------------------------
Avm_Program_Edit *AProgram_Edit_VM_Test::Make_Editor()
{
	AsProgram_Service *service = new AsProgram_Service(Program_Repository, this);

	return new Avm_Program_Edit(Program_Repository, Rule_Repository, service, this);
}
//----------------------------------------------------------------------------
// Загрузка существующей программы в редактор (срез S2). Без неё «Настроить»
// из каталога открывал бы пустой редактор создания, и правка была бы
// невозможна: сервис умеет только создавать новую программу.
void AProgram_Edit_VM_Test::loadFillsEditorFromExistingProgram()
{
	Avm_Program_Edit *editor = Make_Editor();
	QString program_id;

	QVERIFY(editor->reset("u1"));
	editor->setTitle("Сила 12 недель");
	editor->setStartDate("2026-09-28");
	editor->setWeeks(12);
	QVERIFY(editor->setDayKind(1, static_cast<int>(EDay_Kind::Workout)));
	QVERIFY(editor->addDayExercise(1, "ex_squat", 4, 5, 8, 100, true));
	QVERIFY(editor->create(false));
	program_id = editor->programId();
	QVERIFY(program_id.isEmpty() == false);

	// Счёт снимается после первого создания: правка не должна ни добавить
	// строку, ни удалить её.
	int programs_after_create = Count_Programs();

	// Второй редактор открывает ту же программу: поля и дни на месте.
	Avm_Program_Edit *opened = Make_Editor();

	QVERIFY(opened->load("u1", program_id));
	QCOMPARE(opened->title(), QString("Сила 12 недель"));
	QCOMPARE(opened->weeks(), 12);
	QCOMPARE(opened->startDate(), QString("2026-09-28"));
	QCOMPARE(opened->program_days()->Row_Count(), 7);

	// Состав выбранного дня доезжает: без него экран дня открылся бы пустым.
	QCOMPARE(opened->Select_Day(1), 1);
	QCOMPARE(opened->day_exercises()->Row_Count(), 1);
}
//----------------------------------------------------------------------------
void AProgram_Edit_VM_Test::loadRefusesReadOnlyProgram()
{
	AsSql_Database local_database;
	AProgram_Repository local_repository(&local_database);
	AsProgram_Service local_service(&local_repository, this);
	Avm_Program_Edit editor(&local_repository, Rule_Repository, &local_service, this);
	SProgram finished;

	QVERIFY(local_database.Open(Directory.filePath("load_readonly.sqlite3")));

	finished = SProgram();
	finished.ID = "p_done";
	finished.User_Id = "u1";
	finished.Title = "Сухая масса";
	finished.Source_Kind = EProgram_Source_Kind::Scratch;
	finished.Goal = EProgram_Goal::Muscle;
	finished.Level = EProgram_Level::Beginner;
	finished.Schedule_Kind = ESchedule_Kind::Weekday;
	finished.State = EProgram_State::Completed;
	finished.Revision = 1;
	finished.Term.Start_Date = "2026-01-05";
	finished.Term.Weeks_Total = 12;
	finished.Term.Mode = ETerm_Mode::FixedWeeks;
	QVERIFY(local_repository.Insert_Program(finished));

	// Завершённую программу нельзя править (§3.1): редактор обязан отказать,
	// а не показать форму, которая упрётся в отказ репозитория при сохранении.
	QVERIFY(editor.load("u1", "p_done") == false);
	QVERIFY(editor.errorKey().isEmpty() == false);
	QCOMPARE(editor.program_days()->Row_Count(), 0);
}
//----------------------------------------------------------------------------
// Правка существующей программы не должна создавать вторую. Редактор после
// Load() отдаёт тот же идентификатор, и Create обязан применить изменения к
// нему, а не завести дубликат: иначе каталог наполнялся бы копиями одной и той
// же программы.
void AProgram_Edit_VM_Test::createAfterLoadUpdatesSameProgramInsteadOfCreatingSecond()
{
	Avm_Program_Edit *editor = Make_Editor();
	QString program_id;

	QVERIFY(editor->reset("u1"));
	editor->setTitle("Сила 12 недель");
	editor->setStartDate("2026-09-28");
	editor->setWeeks(12);
	QVERIFY(editor->setDayKind(1, static_cast<int>(EDay_Kind::Workout)));
	QVERIFY(editor->addDayExercise(1, "ex_squat", 4, 5, 8, 100, true));
	QVERIFY(editor->create(false));
	program_id = editor->programId();

	// Счёт снимается после первого создания: правка не должна ни добавить
	// строку, ни удалить её.
	int programs_after_create = Count_Programs();

	// Правка загруженной программы: другое название и добавленный день.
	Avm_Program_Edit *opened = Make_Editor();

	QVERIFY(opened->load("u1", program_id));
	opened->setTitle("Сила 16 недель");
	QVERIFY(opened->setDayKind(3, static_cast<int>(EDay_Kind::Workout)));
	QVERIFY(opened->addDayExercise(3, "ex_squat", 3, 8, 12, 60, true));
	bool saved = opened->create(false);

	if (saved == false)
		qWarning("LAST ERROR: %s", qPrintable(Program_Repository->Get_Last_Error()));

	QVERIFY(saved);

	// Та же программа, а не новая.
	QCOMPARE(opened->programId(), program_id);
	QCOMPARE(Count_Programs(), programs_after_create);

	// И изменения действительно сохранены.
	Avm_Program_Edit *verify = Make_Editor();

	QVERIFY(verify->load("u1", program_id));
	QCOMPARE(verify->title(), QString("Сила 16 недель"));
	QCOMPARE(verify->Select_Day(3), 3);
	QCOMPARE(verify->day_exercises()->Row_Count(), 1);
}
//----------------------------------------------------------------------------
// Добавленное упражнение обязано появляться в списке дня сразу. Раньше
// добавление обновляло только модель дней, поэтому экран показывал «в этом дне
// пока нет упражнений» после успешного добавления.
void AProgram_Edit_VM_Test::addDayExerciseRefreshesDayExerciseModel()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(editor->Reset("u1"));
	QVERIFY(editor->Select_Day(1));
	QCOMPARE(editor->day_exercises()->Row_Count(), 0);

	// Добавление в открытый день: его список упражнений обязан пополниться.
	QVERIFY(editor->Add_Day_Exercise(1, "ex_squat", 4, 5, 8, 100, true));
	QCOMPARE(editor->day_exercises()->Row_Count(), 1);

	// Второе добавление не теряет первое: список не пересобирается с нуля.
	QVERIFY(editor->Add_Day_Exercise(1, "ex_squat", 3, 8, 12, 60, true));
	QCOMPARE(editor->day_exercises()->Row_Count(), 2);

	// Модель соответствует открытому дню, а не последнему изменённому.
	QVERIFY(editor->Select_Day(3));
	QCOMPARE(editor->day_exercises()->Row_Count(), 0);
}
//----------------------------------------------------------------------------
// assert-конструкция QtTest возвращает void, поэтому проверки внутри
// int-функции заменены на явный возврат: иначе тест не компилируется.
int AProgram_Edit_VM_Test::Count_Programs() const
{
	QSqlQuery query(Database.Sql_Connection());

	query.prepare("SELECT COUNT(*) FROM programs");
	if (query.exec() == false || query.next() == false)
		return -1;

	return query.value(0).toInt();
}
//----------------------------------------------------------------------------
// Хелперы возвращают bool: assert-конструкции QtTest возвращают void и не
// годятся для проверки результата внутри функции-хелпера.
bool AProgram_Edit_VM_Test::Fill_Minimal_Program(Avm_Program_Edit *in_editor)
{
	if (in_editor->Reset("u_edit") == false)
		return false;

	in_editor->Set_Title("Моя сила");
	in_editor->Set_Goal((int)EProgram_Goal::Strength);
	in_editor->Set_Start_Date("2026-11-02");
	in_editor->Set_Weeks(8);

	if (in_editor->Set_Day_Kind(1, (int)EDay_Kind::Workout) == false)
		return false;

	return in_editor->Add_Day_Exercise(1, "ex_squat", 4, 5, 8, 100, true);
}
//----------------------------------------------------------------------------
void AProgram_Edit_VM_Test::resetProducesEmptyEditor()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(editor->Reset("u_edit"));
	QCOMPARE(editor->program_days()->Row_Count(), 7);
	QCOMPARE(editor->created(), false);
	// Заголовок по умолчанию: редактор открывается с осмысленным названием,
	// а не с пустым полем.
	QCOMPARE(editor->title(), QString("Моя программа"));
	QCOMPARE(editor->trainingDays(), 0);
}
//----------------------------------------------------------------------------
// Минимально валидная программа: владелец, дата старта и хотя бы один рабочий
// день. Без этого создание бессмысленно (§3.4).
void AProgram_Edit_VM_Test::canCreateRequiresUserDateAndTrainingDay()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(editor->Reset("u_can"));
	QVERIFY(editor->canCreate() == false);

	editor->Set_Start_Date("2026-11-02");
	editor->Set_Weeks(4);
	QVERIFY(editor->canCreate() == false);

	QVERIFY(editor->Set_Day_Kind(2, (int)EDay_Kind::Workout));
	QVERIFY(editor->canCreate());
}
//----------------------------------------------------------------------------
void AProgram_Edit_VM_Test::rejectsInvalidStartDate()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(editor->Reset("u_date"));
	editor->Set_Start_Date("не-дата");
	QVERIFY(editor->canCreate() == false);
	QVERIFY(editor->errorKey().isEmpty() == false);
}
//----------------------------------------------------------------------------
void AProgram_Edit_VM_Test::createsDraftProgramWithSevenDays()
{
	Avm_Program_Edit *editor = Make_Editor();
	int before = Count_Programs();

	QVERIFY(Fill_Minimal_Program(editor));
	QVERIFY(editor->Create(false));
	QCOMPARE(Count_Programs(), before + 1);
	QVERIFY(editor->created());
	QVERIFY(editor->programId().isEmpty() == false);

	SProgram program;

	QVERIFY(Program_Repository->Load_Program(editor->programId(), program));
	QCOMPARE(program.State, EProgram_State::Draft);
	QCOMPARE(program.Term.Start_Date, QString("2026-11-02"));
	QCOMPARE(program.Term.Weeks_Total, 8);
}
//----------------------------------------------------------------------------
void AProgram_Edit_VM_Test::createCanActivateImmediately()
{
	Avm_Program_Edit *editor = Make_Editor();
	SProgram program;

	QVERIFY(Fill_Minimal_Program(editor));
	// Активная программа с измеримой целью обязана иметь цели (инвариант 8).
	QVERIFY(editor->Set_Target((int)EGoal_Metric::WorkingWeightKg, 80, true, 100, (int)EGoal_Direction::Increase));
	QVERIFY(editor->Create(true));

	QVERIFY(Program_Repository->Load_Program(editor->programId(), program));
	QCOMPARE(program.State, EProgram_State::Active);
}
//----------------------------------------------------------------------------
// Рабочий день без упражнений — опечатка пользователя, а не замысел: программа
// не создаётся и в БД ничего не попадает.
void AProgram_Edit_VM_Test::createWithoutExerciseFailsAndWritesNothing()
{
	Avm_Program_Edit *editor = Make_Editor();
	int before = Count_Programs();

	QVERIFY(editor->Reset("u_empty"));
	editor->Set_Start_Date("2026-11-02");
	editor->Set_Weeks(4);
	QVERIFY(editor->Set_Day_Kind(1, (int)EDay_Kind::Workout));
	QVERIFY(editor->Create(false) == false);
	QVERIFY(editor->errorKey().isEmpty() == false);
	QCOMPARE(Count_Programs(), before);
}
//----------------------------------------------------------------------------
void AProgram_Edit_VM_Test::measurableGoalRequiresTarget()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(Fill_Minimal_Program(editor));
	// Программа «сила» измерима: без целей её нельзя активировать (инвариант 8).
	QVERIFY(editor->Create(true) == false);
	QVERIFY(editor->errorKey() == QString("goal_required"));
}
//----------------------------------------------------------------------------
void AProgram_Edit_VM_Test::dayKindChangeUpdatesSchedule()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(Fill_Minimal_Program(editor));
	QVERIFY(editor->Set_Day_Kind(3, (int)EDay_Kind::Rest));
	QCOMPARE(editor->trainingDays(), 1);

	// Несуществующий день недели отвергается: ISO-неделя 1…7 (§3.4).
	QVERIFY(editor->Set_Day_Kind(9, (int)EDay_Kind::Workout) == false);
}
//----------------------------------------------------------------------------
void AProgram_Edit_VM_Test::addRulePersistsAfterCreate()
{
	Avm_Program_Edit *editor = Make_Editor();
	QVector<SProgram_Rule> rules;

	QVERIFY(Fill_Minimal_Program(editor));
	QVERIFY(editor->Add_Rule("protein", "Белок 140 г", (int)ERule_Metric::ProteinGrams, 140, true, 0, false));
	QCOMPARE(editor->rules()->Row_Count(), 1);
	QVERIFY(editor->Create(false));

	QVERIFY(Rule_Repository->Load_Rules(editor->programId(), rules));
	QCOMPARE(rules.size(), 1);
	QCOMPARE(rules.first().Code, QString("protein"));
	QVERIFY(rules.first().Has_Target_Min);
	QCOMPARE(rules.first().Target_Min, 140.0);
}
//----------------------------------------------------------------------------
// Выбор дня переключает модель упражнений: экран дня показывает упражнения
// именно выбранного дня, а не первого попавшегося.
void AProgram_Edit_VM_Test::selectDaySwitchesExerciseModel()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(editor->Reset("u_day"));
	QVERIFY(editor->Add_Day_Exercise(1, "ex_squat", 4, 5, 8, 100, true));
	QVERIFY(editor->Add_Day_Exercise(2, "ex_bench", 3, 8, 12, 60, true));

	QCOMPARE(editor->selectDay(1), 1);
	QCOMPARE(editor->day_exercises()->Row_Count(), 1);
	QCOMPARE(editor->selectDay(2), 2);
	QCOMPARE(editor->day_exercises()->Row_Count(), 1);

	// День вне ISO-недели не выбирается и оставляет открытым прежний.
	QCOMPARE(editor->selectDay(9), 2);
}
//----------------------------------------------------------------------------
// После удаления порядковые номера упражнений идут подряд: экран показывает их
// по порядку, и разрывы выглядели бы пропуском в программе.
void AProgram_Edit_VM_Test::removeDayExerciseReindexesOrder()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(editor->Reset("u_rm"));
	QVERIFY(editor->Add_Day_Exercise(1, "ex_a", 3, 5, 5, 0, false));
	QVERIFY(editor->Add_Day_Exercise(1, "ex_b", 3, 5, 5, 0, false));
	QVERIFY(editor->Add_Day_Exercise(1, "ex_c", 3, 5, 5, 0, false));
	QCOMPARE(editor->selectDay(1), 1);
	QCOMPARE(editor->day_exercises()->Row_Count(), 3);

	QVERIFY(editor->Remove_Day_Exercise(1, 1));
	QCOMPARE(editor->day_exercises()->Row_Count(), 2);
	// Удалён средний: порядок сохраняется, а не сдвигается и не дублируется.
	QCOMPARE(Role_Value(editor->day_exercises(), 0, QString("exerciseId")).toString(), QString("ex_a"));
	QCOMPARE(Role_Value(editor->day_exercises(), 1, QString("exerciseId")).toString(), QString("ex_c"));
	// Порядковые номера идут подряд, без разрыва на месте удалённого.
	QCOMPARE(Role_Value(editor->day_exercises(), 0, QString("orderIndex")).toInt(), 1);
	QCOMPARE(Role_Value(editor->day_exercises(), 1, QString("orderIndex")).toInt(), 2);
}
//----------------------------------------------------------------------------
// Копирование заменяет целевой день целиком: «сделать этот день таким».
void AProgram_Edit_VM_Test::copyDayReplacesTargetDay()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(editor->Reset("u_copy"));
	QVERIFY(editor->Add_Day_Exercise(1, "ex_a", 3, 5, 5, 0, false));
	QVERIFY(editor->Add_Day_Exercise(3, "ex_old", 2, 10, 10, 0, false));

	QVERIFY(editor->Copy_Day(1, 3));
	QCOMPARE(editor->selectDay(3), 3);
	// Старое упражнение исчезло, новое пришло из дня 1.
	QCOMPARE(editor->day_exercises()->Row_Count(), 1);
	QCOMPARE(Role_Value(editor->day_exercises(), 0, QString("exerciseId")).toString(), QString("ex_a"));

	// Копирование в самого себя и вне ISO-недели отвергается.
	QVERIFY(editor->Copy_Day(1, 1) == false);
	QVERIFY(editor->Copy_Day(1, 9) == false);
}
//----------------------------------------------------------------------------
// Полная форма правила сохраняет категорию, периодичность и единицу: короткая
// форма жёстко ставила Nutrition/в день/граммы, из-за чего сон и шаги выглядели
// как «граммы белка».
void AProgram_Edit_VM_Test::addRuleDetailedKeepsCategoryAndPeriodicity()
{
	Avm_Program_Edit *editor = Make_Editor();
	QVector<SProgram_Rule> rules;

	QVERIFY(Fill_Minimal_Program(editor));
	QVERIFY(editor->Add_Rule_Detailed("sleep", "Сон 8 часов", (int)ERule_Category::Recovery,
	                                  (int)ERule_Metric::SleepHours, (int)ERule_Periodicity::PerDay, 8, true, 0, false,
	                                  (int)ERule_Display_Unit::Hours, (int)ERule_Applies_To::AllDays,
	                                  (int)ERule_Enforcement::Hard));
	QCOMPARE(editor->rules()->Row_Count(), 1);
	QVERIFY(editor->Create(false));

	QVERIFY(Rule_Repository->Load_Rules(editor->programId(), rules));
	QCOMPARE(rules.size(), 1);
	QCOMPARE(rules.first().Category, ERule_Category::Recovery);
	QCOMPARE(rules.first().Metric, ERule_Metric::SleepHours);
	QCOMPARE(rules.first().Target_Unit, ERule_Display_Unit::Hours);
	QCOMPARE(rules.first().Enforcement, ERule_Enforcement::Hard);
}
//----------------------------------------------------------------------------
// Удаление обязательно: без него опечатку в правиле или цели не исправить.
void AProgram_Edit_VM_Test::removeRuleAndRemoveTargetClearDraftLists()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(editor->Reset("u_del"));
	QVERIFY(editor->Set_Target((int)EGoal_Metric::BodyWeightKg, 80, true, 75, (int)EGoal_Direction::Decrease));
	QVERIFY(editor->Add_Rule("protein", "Белок 140 г", (int)ERule_Metric::ProteinGrams, 140, true, 0, false));
	QCOMPARE(editor->goal_targets()->Row_Count(), 1);
	QCOMPARE(editor->rules()->Row_Count(), 1);

	QVERIFY(editor->Remove_Rule(0));
	QVERIFY(editor->Remove_Target(0));
	QCOMPARE(editor->rules()->Row_Count(), 0);
	QCOMPARE(editor->goal_targets()->Row_Count(), 0);
}
//----------------------------------------------------------------------------
// Индекс за пределами списка отвергается, а не роняет экран.
void AProgram_Edit_VM_Test::removeRejectsOutOfRangeIndex()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(editor->Reset("u_range"));
	QVERIFY(editor->Add_Rule("protein", "Белок 140 г", (int)ERule_Metric::ProteinGrams, 140, true, 0, false));
	QVERIFY(editor->Remove_Rule(5) == false);
	QVERIFY(editor->errorKey() == QString("rule_not_found"));
	QCOMPARE(editor->rules()->Row_Count(), 1);
}
//----------------------------------------------------------------------------
// Отказ записи обязан дойти до экрана: QML читает errorKey/errorDetail
// привязкой к changed(), поэтому без сигнала баннер ошибки не показывается и
// пользователь видит лишь «кнопка ничего не сделала». Сценарий — база без
// каталога упражнений: FK отвергает предписание, редактор обязан сообщить
// save_failed с деталью и оповестить экран.
void AProgram_Edit_VM_Test::saveFailureNotifiesScreen()
{
	AsSql_Database local_database;
	AProgram_Repository local_repository(&local_database);
	AsProgram_Service local_service(&local_repository, this);
	Avm_Program_Edit editor(&local_repository, Rule_Repository, &local_service, this);

	QVERIFY(local_database.Open(Directory.filePath("save_failed_fk.sqlite3")));

	// Сид заполняет каталог при Open(): убираем строки, чтобы повторить базу
	// пользователя, у которой каталог пуст.
	QSqlQuery cleanup(local_database.Sql_Connection());

	QVERIFY2(cleanup.exec("DELETE FROM exercises"), qPrintable(cleanup.lastError().text()));

	QVERIFY(editor.Reset("u1"));
	editor.Set_Title("Сила");
	editor.Set_Start_Date("2026-11-02");
	editor.Set_Weeks(4);
	QVERIFY(editor.Set_Day_Kind(1, (int)EDay_Kind::Workout));
	QVERIFY(editor.Add_Day_Exercise(1, "ex_squat", 4, 5, 8, 100, true));

	QSignalSpy spy(&editor, &Avm_Program_Edit::changed);

	QVERIFY(editor.Create(false) == false);
	QCOMPARE(editor.errorKey(), QString("save_failed"));
	QVERIFY2(editor.errorDetail().isEmpty() == false, "save_failed обязан нести деталь отказа");
	QVERIFY2(spy.count() > 0, "без changed() QML не перечитает errorKey/errorDetail");
}
//----------------------------------------------------------------------------

QTEST_GUILESS_MAIN(AProgram_Edit_VM_Test)
#include "tst_Program_Edit_VM.moc"
//----------------------------------------------------------------------------