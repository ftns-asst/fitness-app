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
	void plannedEndDateFollowsTerm();
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
	void addAdviceNeedsNoTarget();

	// — Рекомендация в редакторе —
	void addAdviceRulePersistsWithoutTarget();
	void addAdviceRejectsEmptyText();

	// — Каталог правил (§5.7) —
	// Экран выбора показывает каталог, а не захардкоженный в разметке набор:
	// добавление пресета идёт по его коду, а «уже добавлено» считается по коду.
	void catalogListsPresetsForCategory();
	void addCatalogPresetAddsRule();
	void catalogMarksAlreadyAddedPresets();

	// — Своё правило —
	// Метрика задаёт единицу и периодичность, пользователь — название и
	// сколько. Единица, взятая не из метрики, отвергается: «белок в мл» прошло
	// бы всю валидацию и не значило бы ничего.
	void customRuleTakesUnitFromMetric();
	void customRuleRejectsUnitForeignToMetric();
	void customRuleNeedsTargetAndTitle();
	void customRuleRejectsExactWhereRangeIsMeaningful();
	void removeRuleAndRemoveTargetClearDraftLists();
	void addDayExerciseRefreshesDayExerciseModel();
	void removeRejectsOutOfRangeIndex();
	void loadFillsEditorFromExistingProgram();
	void loadRefusesReadOnlyProgram();
	void createAfterLoadUpdatesSameProgramInsteadOfCreatingSecond();
	// — Повторное «Создать» на том же экране —
	// Кнопка остаётся нажимаемой до закрытия экрана, и второе нажатие приходит
	// в ту же ViewModel с тем же черновиком: сохранение обязано пройти и не
	// завести вторую программу.
	void repeatedCreateUpdatesSameProgramInsteadOfDuplicating();
	// — Активная программа у пользователя одна (§2.3) —
	void createReplacesActiveProgramInsteadOfFailing();
	void saveFailureNotifiesScreen();
	// — Черновик в БД: автосохранение и продолжение прерванного создания —
	void draftAutosavePersistsDayExercises();
	void resumeDraftContinuesSavedDraftAndDoesNotDuplicate();
	void resumeDraftWithoutSavedDraftStartsFresh();
	void deleteDraftRemovesStoredDraftAndResetsEditor();

private:
	void Seed_Exercise();
	Avm_Program_Edit *Make_Editor();
	bool Fill_Minimal_Program(Avm_Program_Edit *in_editor);
	int Count_Programs() const;
	int Count_Targets(const QString &in_program_id) const;
	// Состояние программы в базе: проверка, что прежняя активная ушла в паузу.
	QString Program_State(const QString &in_program_id) const;

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

	// Строка модели целиком: сверка пресета с созданным из него правилом идёт по
	// всем полям сразу, и собирать их по одному здесь было бы второй копией
	// контракта ролей.
	static QVariantMap Role_Map(const AList_Model *in_model, int in_row)
	{
		QVariantMap row;

		for (int role = Qt::UserRole + 1; role < Qt::UserRole + 1 + in_model->Role_Count(); ++role)
			row.insert(in_model->Role_Name(role), in_model->Data(in_row, role));

		return row;
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
// Дата окончания — производная от даты старта и срока (§3.3), и её считает
// домен. Подсказка под ползунком срока показывает именно эту строку: своя
// формула в QML разошлась бы с доменом, а слагаемые («старт + срок − 1 день»)
// достаточно неочевидны, чтобы проверять их тестом, а не глазами.
void AProgram_Edit_VM_Test::plannedEndDateFollowsTerm()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(editor->reset("u1"));

	// Ни даты старта, ни срока — окончание не определено, и показывать нечего.
	QCOMPARE(editor->plannedEndDate(), QString());

	editor->setStartDate("2026-09-28");
	// Одна дата старта окончания не даёт: без срока считать не от чего.
	QCOMPARE(editor->plannedEndDate(), QString());

	editor->setWeeks(12);
	// 2026-09-28 + 12 × 7 − 1 день = 2026-12-20.
	QCOMPARE(editor->plannedEndDate(), QString("2026-12-20"));

	// Срок — величина, за которой дата едет: ярлык «8 недель» на экране обязан
	// сдвинуть и окончание, а не только число под подписью.
	editor->setWeeks(8);
	QCOMPARE(editor->plannedEndDate(), QString("2026-11-22"));
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
// Повторное «Создать» после успешного создания обязано править ту же программу,
// а не заводить вторую. После успеха черновик оставался без идентификатора:
// вторая ветка создания вставляла те же цели ещё раз и упиралась в UNIQUE
// goal_targets.id — экран показывал «Не удалось сохранить цель», а программа
// без измеримых целей дублировалась бы целиком.
void AProgram_Edit_VM_Test::repeatedCreateUpdatesSameProgramInsteadOfDuplicating()
{
	Avm_Program_Edit *editor = Make_Editor();

	// Пользователь только этого теста: активная программа на пользователя
	// одна (§2.3), а общий u_edit из Fill_Minimal_Program занят проверками
	// активации, и первый же Create(true) упал бы на индексе, а не на повторе
	// сохранения — то есть тест проверял бы не то.
	QVERIFY(editor->Reset("u_repeat"));
	editor->Set_Title("Сила 12 недель");
	editor->Set_Start_Date("2026-09-28");
	editor->Set_Weeks(12);
	QVERIFY(editor->Set_Day_Kind(1, (int)EDay_Kind::Workout));
	QVERIFY(editor->Add_Day_Exercise(1, "ex_squat", 4, 5, 8, 100, true));
	QVERIFY(editor->Set_Target((int)EGoal_Metric::BodyWeightKg, 80, true, 75, (int)EGoal_Direction::Decrease));

	QVERIFY(editor->Create(true));

	const QString program_id = editor->programId();
	const int programs_after_first = Count_Programs();

	QVERIFY(program_id.isEmpty() == false);
	QCOMPARE(Count_Targets(program_id), 1);

	// Второе нажатие кнопки на том же экране: сохранение обязано пройти без
	// ошибки и без второй программы.
	QVERIFY(editor->Create(true));
	QCOMPARE(editor->errorKey(), QString(""));
	QCOMPARE(editor->programId(), program_id);
	QCOMPARE(Count_Programs(), programs_after_first);
	QCOMPARE(Count_Targets(program_id), 1);
}
//----------------------------------------------------------------------------
// Создание второй программы при уже активной — обычный путь пользователя:
// активная программа есть, он собирает новую. Сохранение обязано пройти, а
// прежняя — уйти в паузу (§2.3: активная программа у пользователя одна).
// Раньше экран показывал «Не удалось изменить состояние: UNIQUE constraint
// failed: programs.user_id» — то есть ошибку базы вместо результата.
void AProgram_Edit_VM_Test::createReplacesActiveProgramInsteadOfFailing()
{
	Avm_Program_Edit *first = Make_Editor();

	QVERIFY(first->Reset("u_switch"));
	first->Set_Title("Первая программа");
	first->Set_Start_Date("2026-10-05");
	first->Set_Weeks(8);
	QVERIFY(first->Set_Day_Kind(1, (int)EDay_Kind::Workout));
	QVERIFY(first->Add_Day_Exercise(1, "ex_squat", 4, 5, 8, 100, true));
	// Измеримая цель требует целевого значения: без него Create(true) откажет
	// по goal_required, и тест проверял бы не переключение активной программы.
	QVERIFY(first->Set_Target((int)EGoal_Metric::BodyWeightKg, 82, true, 78, (int)EGoal_Direction::Decrease));
	QVERIFY(first->Create(true));

	const QString first_id = first->programId();

	QCOMPARE(Program_State(first_id), QString("active"));

	Avm_Program_Edit *second = Make_Editor();

	QVERIFY(second->Reset("u_switch"));
	second->Set_Title("Вторая программа");
	second->Set_Start_Date("2026-10-19");
	second->Set_Weeks(12);
	QVERIFY(second->Set_Day_Kind(2, (int)EDay_Kind::Workout));
	QVERIFY(second->Add_Day_Exercise(2, "ex_squat", 3, 8, 12, 60, true));
	QVERIFY(second->Set_Target((int)EGoal_Metric::BodyWeightKg, 80, true, 76, (int)EGoal_Direction::Decrease));

	QVERIFY(second->Create(true));

	const QString second_id = second->programId();

	QVERIFY(second_id != first_id);
	// Признак для экрана: прежняя активная ушла в паузу — об этом сообщает
	// тост, иначе смена активной программы прошла бы молча.
	QVERIFY(second->Previous_Program_Paused());
	QCOMPARE(Program_State(second_id), QString("active"));
	QCOMPARE(Program_State(first_id), QString("paused"));
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
// Цели считаются по программе: конфликт идентификаторов целей виден именно
// здесь, а не в общем числе строк.
int AProgram_Edit_VM_Test::Count_Targets(const QString &in_program_id) const
{
	QSqlQuery query(Database.Sql_Connection());

	query.prepare("SELECT COUNT(*) FROM goal_targets WHERE program_id = ?");
	query.addBindValue(in_program_id);
	if (query.exec() == false || query.next() == false)
		return -1;

	return query.value(0).toInt();
}
//----------------------------------------------------------------------------
// Состояние программы в базе: «прежняя ушла в паузу» проверяется по факту
// записи, а не по ответу ViewModel.
QString AProgram_Edit_VM_Test::Program_State(const QString &in_program_id) const
{
	QSqlQuery query(Database.Sql_Connection());

	query.prepare("SELECT state FROM programs WHERE id = ?");
	query.addBindValue(in_program_id);
	if (query.exec() == false || query.next() == false)
		return QString();

	return query.value(0).toString();
}
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
// Рекомендация отвечает на вопрос «что делать», а не «сколько», поэтому цель
// ей не требуется. Раньше Add_Rule_Detailed отвергал любое правило без цели,
// и пользователь не мог записать в программу то, что модель и БД уже
// принимали: путь из интерфейса в базу обрывался на форме.
void AProgram_Edit_VM_Test::addAdviceNeedsNoTarget()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(Fill_Minimal_Program(editor));

	// Измеримое правило без цели по-прежнему мусор и отвергается.
	QVERIFY(editor->Add_Rule_Detailed("water", "Вода", (int)ERule_Category::Nutrition, (int)ERule_Metric::WaterMl,
	                                  (int)ERule_Periodicity::PerDay, 0, false, 0, false, (int)ERule_Display_Unit::Ml,
	                                  (int)ERule_Applies_To::AllDays, (int)ERule_Enforcement::Soft) == false);
	QCOMPARE(editor->errorKey(), QString("rule_target_required"));

	// Рекомендация с текстом проходит без цели.
	QVERIFY(editor->Add_Advice_Rule("Пей воду натощак", "Пей воду натощак"));
	QCOMPARE(editor->rules()->Row_Count(), 1);

	// «Другое» с числом остаётся измеримым: цель задана, значит вердикт есть.
	QVERIFY(editor->Add_Rule_Detailed(
	    "weight", "Вес", (int)ERule_Category::Other, (int)ERule_Metric::Custom, (int)ERule_Periodicity::PerWeek, 80,
	    true, 0, false, (int)ERule_Display_Unit::Count, (int)ERule_Applies_To::AllDays, (int)ERule_Enforcement::Soft));
	QCOMPARE(editor->rules()->Row_Count(), 2);
}
//----------------------------------------------------------------------------
void AProgram_Edit_VM_Test::addAdviceRulePersistsWithoutTarget()
{
	Avm_Program_Edit *editor = Make_Editor();
	QVector<SProgram_Rule> rules;

	QVERIFY(Fill_Minimal_Program(editor));
	QVERIFY(editor->Add_Advice_Rule("Ложись спать вовремя", "Отбой до 23:00, без экрана"));
	QVERIFY(editor->Create(false));

	QVERIFY(Rule_Repository->Load_Rules(editor->programId(), rules));
	QCOMPARE(rules.size(), 1);
	QCOMPARE(rules.first().Category, ERule_Category::Other);
	QCOMPARE(rules.first().Code, QString("custom"));
	QCOMPARE(rules.first().Title, QString("Ложись спать вовремя"));
	QCOMPARE(rules.first().Metric, ERule_Metric::Custom);

	// Текст пережил путь до БД: если бы он потерялся, в программе осталось бы
	// правило без содержания, а CHECK бы его не пропустил.
	QCOMPARE(rules.first().Note, QString("Отбой до 23:00, без экрана"));
	QCOMPARE(Get_Rule_Kind(rules.first()), ERule_Kind::Advice);

	// Единицы у рекомендации осмысленные: без них она выглядела бы как
	// «граммы белка» в общем списке.
	QCOMPARE(rules.first().Display_Unit, ERule_Display_Unit::Units);
	QVERIFY(rules.first().Has_Target_Min == false);
	QVERIFY(rules.first().Has_Target_Max == false);
	QVERIFY(rules.first().Has_Target_Exact == false);
}
//----------------------------------------------------------------------------
// Текст из пробелов не является содержанием. Без этой проверки «Другое» стало
// бы способом завести правило, в котором нечего прочитать, — то есть тем
// самым, ради чего требование цели и снималось.
void AProgram_Edit_VM_Test::addAdviceRejectsEmptyText()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(Fill_Minimal_Program(editor));

	// Текст из пробелов при нормальном заголовке — именно случай снятия цели.
	QVERIFY(editor->Add_Advice_Rule("Совет", "   ") == false);
	QCOMPARE(editor->errorKey(), QString("rule_advice_text_required"));
	QCOMPARE(editor->rules()->Row_Count(), 0);

	// Ошибки разные, потому что подсказки в UI разные: «введите текст» против
	// «введите название». Сведение их к одной строке заставило бы экран
	// подсвечивать не то поле.
	QVERIFY(editor->Add_Advice_Rule("   ", "Отбой до 23:00") == false);
	QCOMPARE(editor->errorKey(), QString("rule_title_required"));
	QCOMPARE(editor->rules()->Row_Count(), 0);

	QVERIFY(editor->Add_Advice_Rule(QString(), QString()) == false);
	QCOMPARE(editor->rules()->Row_Count(), 0);
}
//----------------------------------------------------------------------------
// Каталог отдаётся моделью, а не разметкой: пресеты объявленные в QML нечем
// ни отфильтровать, ни сверить с уже добавленными правилами.
void AProgram_Edit_VM_Test::catalogListsPresetsForCategory()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(Fill_Minimal_Program(editor));

	QVERIFY(editor->Set_Catalog_Filter(QString()));
	QVERIFY(editor->catalog()->Row_Count() >= 5);

	// Фильтр по питанию: остаются только питательные пресеты.
	QVERIFY(editor->Set_Catalog_Filter(QString("nutrition")));
	QVERIFY(editor->catalog()->Row_Count() > 0);
	QVERIFY(editor->catalog()->Row_Count() < 6);

	for (int row = 0; row < editor->catalog()->Row_Count(); ++row)
		QCOMPARE(Role_Value(editor->catalog(), row, QString("category")).toString(), QString("nutrition"));
}
//----------------------------------------------------------------------------
void AProgram_Edit_VM_Test::addCatalogPresetAddsRule()
{
	Avm_Program_Edit *editor = Make_Editor();
	AList_Model *catalog = 0;

	QVERIFY(Fill_Minimal_Program(editor));
	QVERIFY(editor->Set_Catalog_Filter(QString()));
	catalog = editor->catalog();
	QVERIFY(catalog->Row_Count() > 0);

	const QString code = Role_Value(catalog, 0, QString("code")).toString();

	QVERIFY(editor->Add_Catalog_Preset(code));
	QCOMPARE(editor->rules()->Row_Count(), 1);
	QVERIFY(editor->Create(false));

	QVector<SProgram_Rule> rules;

	QVERIFY(Rule_Repository->Load_Rules(editor->programId(), rules));
	QCOMPARE(rules.size(), 1);

	// Правило создалось с полями пресета, а не с Nutrition/граммами по
	// умолчанию: иначе «Сон 8 ч» сохранился бы как «8 граммов белка».
	const QVariantMap preset = Role_Map(editor->catalog(), 0);
	SProgram_Rule rule = rules.first();

	QCOMPARE(rule.Code, code);
	QCOMPARE(rule.Title, preset.value("title").toString());
	QCOMPARE(To_String(rule.Category), preset.value("category").toString());
	QCOMPARE(int(rule.Metric), preset.value("metric").toInt());
	QCOMPARE(int(rule.Periodicity), preset.value("periodicity").toInt());
	QCOMPARE(rule.Has_Target_Min, preset.value("hasTargetMin").toBool());
	QCOMPARE(rule.Has_Target_Max, preset.value("hasTargetMax").toBool());
	if (preset.value("hasTargetMin").toBool())
		QCOMPARE(rule.Target_Min, preset.value("targetMin").toDouble());
}
//----------------------------------------------------------------------------
// «Уже добавлено» считается по коду пресета: без отметки пользователь добавил
// бы «Сон 8 ч» второй раз и получил две одинаковые проверки в программе.
void AProgram_Edit_VM_Test::catalogMarksAlreadyAddedPresets()
{
	Avm_Program_Edit *editor = Make_Editor();
	AList_Model *catalog = 0;
	QString code = QString();

	QVERIFY(Fill_Minimal_Program(editor));
	QVERIFY(editor->Set_Catalog_Filter(QString()));
	catalog = editor->catalog();
	code = Role_Value(catalog, 0, QString("code")).toString();

	QVERIFY(Role_Value(catalog, 0, QString("isAdded")).toBool() == false);

	QVERIFY(editor->Add_Catalog_Preset(code));
	QVERIFY(Role_Value(catalog, 0, QString("isAdded")).toBool());

	// Повторное добавление того же пресета отвергается: в программе уже есть
	// такое правило, и второе ничего бы не изменило.
	QVERIFY(editor->Add_Catalog_Preset(code) == false);
	QCOMPARE(editor->rules()->Row_Count(), 1);
}
//----------------------------------------------------------------------------
// Единица и периодичность приходят из метрики, а не от пользователя: форма
// «своё правило» не должна требовать знать, что сон — в часах, а белок — в
// граммах.
void AProgram_Edit_VM_Test::customRuleTakesUnitFromMetric()
{
	Avm_Program_Edit *editor = Make_Editor();
	QVector<SProgram_Rule> rules;

	QVERIFY(Fill_Minimal_Program(editor));

	QVERIFY(editor->Add_Custom_Rule("Пей достаточно", int(ERule_Metric::WaterMl), int(ERule_Display_Unit::Ml), 2500,
	                                true, 0, false, 0, false));
	QVERIFY(editor->Create(false));
	QVERIFY(Rule_Repository->Load_Rules(editor->programId(), rules));
	QCOMPARE(rules.size(), 1);

	QCOMPARE(rules.first().Metric, ERule_Metric::WaterMl);
	QCOMPARE(rules.first().Display_Unit, ERule_Display_Unit::Ml);
	QCOMPARE(rules.first().Target_Unit, ERule_Display_Unit::Ml);
	// Периодичность метрики, а не «как получится»: вода считается каждый день.
	QCOMPARE(rules.first().Periodicity, ERule_Periodicity::PerDay);
	QCOMPARE(rules.first().Title, QString("Пей достаточно"));
	QVERIFY(rules.first().Has_Target_Min);
	QCOMPARE(rules.first().Target_Min, 2500.0);
}
//----------------------------------------------------------------------------
void AProgram_Edit_VM_Test::customRuleRejectsUnitForeignToMetric()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(Fill_Minimal_Program(editor));

	// Миллитры белку не подходят: без такой сверки модель приняла бы правило,
	// которое нельзя осмысленно отметить.
	QVERIFY(editor->Add_Custom_Rule("Белок", int(ERule_Metric::ProteinGrams), int(ERule_Display_Unit::Ml), 140, true, 0,
	                                false, 0, false) == false);
	QCOMPARE(editor->errorKey(), QString("rule_unit_mismatch"));
	QCOMPARE(editor->rules()->Row_Count(), 0);

	// Несуществующий enum отвергается так же, а не молча проходит.
	QVERIFY(editor->Add_Custom_Rule("Белок", int(ERule_Metric::ProteinGrams), int(ERule_Display_Unit::Max), 140, true,
	                                0, false, 0, false) == false);
	QCOMPARE(editor->rules()->Row_Count(), 0);

	// Метрика вне каталога: в форме её выбрать нельзя, значит и создать нельзя.
	QVERIFY(editor->Add_Custom_Rule("Вес", int(ERule_Metric::Max), int(ERule_Display_Unit::G), 75, true, 0, false, 0,
	                                false) == false);
	QCOMPARE(editor->rules()->Row_Count(), 0);

	// Произвольная метрика — это рекомендация, а не измеримое правило: в форме
	// своего правила её выбрать нельзя.
	QVERIFY(editor->Add_Custom_Rule("Своё", int(ERule_Metric::Custom), int(ERule_Display_Unit::Units), 1, true, 0,
	                                false, 0, false) == false);
	QCOMPARE(editor->rules()->Row_Count(), 0);
}
//----------------------------------------------------------------------------
void AProgram_Edit_VM_Test::customRuleNeedsTargetAndTitle()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(Fill_Minimal_Program(editor));

	// Без цели правило бессмысленно — это отказ уровня модели, а не UI.
	QVERIFY(editor->Add_Custom_Rule("Вода", int(ERule_Metric::WaterMl), int(ERule_Display_Unit::Ml), 0, false, 0, false,
	                                0, false) == false);
	QCOMPARE(editor->errorKey(), QString("rule_target_required"));

	QVERIFY(editor->Add_Custom_Rule("   ", int(ERule_Metric::WaterMl), int(ERule_Display_Unit::Ml), 2500, true, 0,
	                                false, 0, false) == false);
	QCOMPARE(editor->errorKey(), QString("rule_title_required"));

	QCOMPARE(editor->rules()->Row_Count(), 0);
}
//----------------------------------------------------------------------------
// «Ровно 7 часов сна» осмысленно, «ровно 8 RPE» — нет: у нагрузки важна
// граница сверху. Форма не должна предлагать точную цель там, где она не
// имеет смысла, и модель не должна такое сохранять.
void AProgram_Edit_VM_Test::customRuleRejectsExactWhereRangeIsMeaningful()
{
	Avm_Program_Edit *editor = Make_Editor();

	QVERIFY(Fill_Minimal_Program(editor));

	QVERIFY(editor->Add_Custom_Rule("Нагрузка", int(ERule_Metric::MaxRpe), int(ERule_Display_Unit::Rpe), 0, false, 0,
	                                false, 8, true) == false);
	QCOMPARE(editor->errorKey(), QString("rule_target_kind_invalid"));

	// У сна точная цель осмысленна, и максимум тоже.
	QVERIFY(editor->Add_Custom_Rule("Сон", int(ERule_Metric::SleepHours), int(ERule_Display_Unit::Hours), 0, false, 0,
	                                false, 7, true));
	QCOMPARE(editor->rules()->Row_Count(), 1);
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
// — Черновик в БД (отзыв: «добавил упражнение, нажал назад — ничего не
// сохранилось») —
// С включённым автосохранением каждое действие дня пишется в хранилище сразу:
// состояние Draft, полная структура дней и упражнений. Первая запись вставляет
// строку, следующие обновляют её же — копий программа не плодит.
void AProgram_Edit_VM_Test::draftAutosavePersistsDayExercises()
{
	Avm_Program_Edit editor(Program_Repository, Rule_Repository, new AsProgram_Service(Program_Repository, this), this);

	editor.setAutoSaveEnabled(true);
	QVERIFY(editor.Reset("u_draft_auto"));

	const int programs_before = Count_Programs();

	QVERIFY(editor.Set_Day_Kind(1, (int)EDay_Kind::Workout));
	QVERIFY(editor.Add_Day_Exercise(1, "ex_squat", 4, 5, 8, 100, true));

	// Первая запись создала строку программы в состоянии draft.
	QCOMPARE(Count_Programs(), programs_before + 1);
	QCOMPARE(Program_State(editor.programId()), QString("draft"));

	// Последующие действия обновляют ту же строку: копий в каталоге нет.
	QVERIFY(editor.Set_Day_Kind(2, (int)EDay_Kind::Workout));
	QCOMPARE(Count_Programs(), programs_before + 1);
	QCOMPARE(Program_State(editor.programId()), QString("draft"));
}
//----------------------------------------------------------------------------
// Повторное открытие редактора продолжает сохранённый черновик: упражнения
// дня на месте, и редактор правит ту же строку БД, а не создаёт вторую.
void AProgram_Edit_VM_Test::resumeDraftContinuesSavedDraftAndDoesNotDuplicate()
{
	Avm_Program_Edit first(Program_Repository, Rule_Repository, new AsProgram_Service(Program_Repository, this), this);

	first.setAutoSaveEnabled(true);
	QVERIFY(first.Reset("u_draft_resume"));

	const int programs_before = Count_Programs();

	QVERIFY(first.Set_Day_Kind(1, (int)EDay_Kind::Workout));
	QVERIFY(first.Add_Day_Exercise(1, "ex_squat", 4, 5, 8, 100, true));

	// «Перезапуск приложения»: новый редактор, та же БД.
	Avm_Program_Edit reopened(Program_Repository, Rule_Repository, new AsProgram_Service(Program_Repository, this),
	                          this);

	QVERIFY(reopened.resumeDraft("u_draft_resume"));
	QCOMPARE(reopened.program_days()->Row_Count(), 7);
	QCOMPARE(reopened.day_exercises()->Row_Count(), 1);

	// Правка продолженного черновика идёт в ту же строку: копий нет.
	reopened.setAutoSaveEnabled(true);
	QVERIFY(reopened.Add_Day_Exercise(1, "ex_bench", 3, 8, 12, 60, true));
	QCOMPARE(Count_Programs(), programs_before + 1);
}
//----------------------------------------------------------------------------
// Черновика нет (или хранилище недоступно) — resumeDraft даёт обычный пустой
// редактор и ничего не пишет в БД: «Новая программа» без прошлого.
void AProgram_Edit_VM_Test::resumeDraftWithoutSavedDraftStartsFresh()
{
	Avm_Program_Edit editor(Program_Repository, Rule_Repository, new AsProgram_Service(Program_Repository, this), this);

	const int programs_before = Count_Programs();

	QVERIFY(editor.resumeDraft("u_without_drafts"));
	QCOMPARE(editor.program_days()->Row_Count(), 7);
	QCOMPARE(editor.day_exercises()->Row_Count(), 0);
	QCOMPARE(Count_Programs(), programs_before);
}
//----------------------------------------------------------------------------
// «Удалить черновик»: брошенное создание стирается из БД целиком, а редактор
// остаётся открытым с чистой формой — начинать заново без выхода с экрана.
// Повторное удаление без сохранённого черновика — не отказ, а очистка.
void AProgram_Edit_VM_Test::deleteDraftRemovesStoredDraftAndResetsEditor()
{
	Avm_Program_Edit editor(Program_Repository, Rule_Repository, new AsProgram_Service(Program_Repository, this), this);

	editor.setAutoSaveEnabled(true);
	QVERIFY(editor.Reset("u_draft_delete"));

	const int programs_before = Count_Programs();

	QVERIFY(editor.Set_Day_Kind(1, (int)EDay_Kind::Workout));
	QVERIFY(editor.Add_Day_Exercise(1, "ex_squat", 4, 5, 8, 100, true));

	// Черновик реально в БД: строка со статусом draft.
	QCOMPARE(Count_Programs(), programs_before + 1);
	QCOMPARE(Program_State(editor.programId()), QString("draft"));

	QVERIFY(editor.deleteDraft());

	// БД чиста, редактор — как после reset(): тот же пользователь, пустые
	// списки, новый черновик ещё не записан.
	QCOMPARE(Count_Programs(), programs_before);
	QVERIFY(editor.programId().isEmpty());
	QCOMPARE(editor.program_days()->Row_Count(), 7);
	QCOMPARE(editor.day_exercises()->Row_Count(), 0);

	// Удалять больше нечего: второй вызов очищает форму и не падает.
	QVERIFY(editor.deleteDraft());
	QCOMPARE(Count_Programs(), programs_before);
}
//----------------------------------------------------------------------------

QTEST_GUILESS_MAIN(AProgram_Edit_VM_Test)
#include "tst_Program_Edit_VM.moc"
//----------------------------------------------------------------------------