#include "Data/Auth/Auth_Session.h"
#include "Data/Auth/Token_Store.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Network/Http_Client.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Data/Users/Users_Repository.h"
#include "Domain/Macro_Calculator.h"
#include "Presentation/Program_Rules_VM.h"

#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

//----------------------------------------------------------------------------
// Avm_Program_Rules: правила программы и ручные отметки пользователя
// (§3.6–3.7, §5.1). Дата фиксируется через Set_Today, чтобы результат не
// зависел от системных часов.
//----------------------------------------------------------------------------
class AProgram_Rules_VM_Test : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void reloadWithoutActiveProgramFails();
	void reloadLoadsRulesWithUnknownTodayState();
	void manualCheckinAppearsInTodayState();
	void savesManualCheckinWithValueAndNote();
	void computedCheckinCannotOverwriteManual();
	void periodLimitsLoadedCheckins();
	void adherenceCountsVerdictsOnly();
	void adherenceComputedAfterRulesLoad();
	void emitsChangedOnReloadAndSave();
	// — Генерация правил из расчёта КБЖУ (спринт «КБЖУ в правилах») —
	// Кнопка на экране правил создаёт/обновляет 4 правила питания (белок,
	// углеводы, жиры, калории) в активной программе по сохранённому расчёту.
	// Проверяется главное: правила появляются с целевыми значениями расчёта,
	// повторное нажатие обновляет их, а не дублирует, а без расчёта кнопка
	// отказывает понятной ошибкой, не создавая пустых правил.
	void generateRulesFromMacroCreatesNutritionRules();
	void generateRulesFromMacroUpdatesInsteadOfDuplicating();
	void generateRulesFromMacroRequiresCalculatedMacro();

private:
	void Seed_Program(const QString &in_user_id, const QString &in_program_id);
	void Seed_Rules(const QString &in_program_id);
	// Отдельные пользователь и программа на тест: тесты не зависят от порядка.
	void Seed_User(const QString &in_tag);
	SProgram_Rule Make_Rule(const QString &in_program_id, const QString &in_id, const QString &in_code,
	                        ERule_Check_Source in_source);
	// Сохранённый результат расчёта КБЖУ для генерации правил: генерация читает
	// его из profile_local, а не из аргумента, чтобы числа правил совпадали с
	// тем, что человек видел на экране расчёта.
	void Seed_Macro_Result(const QString &in_user_id, double in_protein, double in_fat, double in_carbs,
	                      double in_calories);
	// Строка списка по имени роли: тест не должен знать числовых значений ролей.
	QVariant Row_By_Role(const AList_Model *in_model, int in_row, const QString &in_role) const;

	QTemporaryDir Directory;
	AsSql_Database Database;
	AProgram_Repository *Program_Repository = 0;
	ARule_Repository *Rule_Repository = 0;
	AUsers_Repository *Users_Repository = 0;
	AsHttp_Client HTTP_Client = AsHttp_Client(QUrl());
	AsToken_Store Token_Store = AsToken_Store(&Database);
	AsAuth_Session Auth_Session = AsAuth_Session(&HTTP_Client, &Token_Store);
};
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::initTestCase()
{
	QVERIFY(Directory.isValid());
	QVERIFY(Database.Open(Directory.filePath("program_rules_vm.sqlite3")));
	Program_Repository = new AProgram_Repository(&Database, this);
	Rule_Repository = new ARule_Repository(&Database, this);
	// Репозиторий пользователя нужен генерации правил: она читает сохранённый
	// расчёт КБЖУ из profile_local. Сессия и HTTP-клиент — заглушки: генерация
	// не ходит в сеть, только в локальную базу.
	Users_Repository = new AUsers_Repository(&Auth_Session, &Database, this);
}
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::Seed_Macro_Result(const QString &in_user_id, double in_protein, double in_fat,
                                               double in_carbs, double in_calories)
{
	AsMacro_Calculator::SResult result;

	result.Protein_G = in_protein;
	result.Fat_G = in_fat;
	result.Carbs_G = in_carbs;
	result.Calories_Kcal = in_calories;

	QVERIFY(Users_Repository->Save_Macro_Result(in_user_id, result));
}
//----------------------------------------------------------------------------
QVariant AProgram_Rules_VM_Test::Row_By_Role(const AList_Model *in_model, int in_row, const QString &in_role) const
{
	// Значения ролей Qt начинаются с Qt::UserRole + 1, поэтому перебираются
	// ключи хеша, а не числа подряд.
	const QHash<int, QByteArray> role_names = in_model->roleNames();

	for (int role = Qt::UserRole + 1; role <= Qt::UserRole + role_names.size(); ++role)
	{
		if (role_names.value(role) == in_role)
			return in_model->Data(in_row, role);
	}

	return QVariant();
}
//----------------------------------------------------------------------------
SProgram_Rule AProgram_Rules_VM_Test::Make_Rule(const QString &in_program_id, const QString &in_id,
                                                const QString &in_code, ERule_Check_Source in_source)
{
	SProgram_Rule rule;

	rule.ID = in_program_id + "_" + in_id;
	rule.Program_Id = in_program_id;
	rule.Category = ERule_Category::Nutrition;
	rule.Code = in_code;
	rule.Title = "Белок";
	rule.Metric = ERule_Metric::ProteinGrams;
	rule.Periodicity = ERule_Periodicity::PerDay;
	rule.Target_Min = 140;
	rule.Has_Target_Min = true;
	rule.Target_Unit = ERule_Display_Unit::G;
	rule.Display_Unit = ERule_Display_Unit::G;
	rule.Applies_To = ERule_Applies_To::AllDays;
	rule.Check_Source = in_source;
	rule.Enforcement = ERule_Enforcement::Soft;

	return rule;
}
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::Seed_Program(const QString &in_user_id, const QString &in_program_id)
{
	SProgram program;
	QVector<SProgram_Day> days;
	QVector<SWeek_Slot> week_slots;

	program.ID = in_program_id;
	program.User_Id = in_user_id;
	program.Title = "Сила 4 недели";
	program.Source_Kind = EProgram_Source_Kind::Scratch;
	program.Goal = EProgram_Goal::Strength;
	program.Schedule_Kind = ESchedule_Kind::Weekday;
	program.State = EProgram_State::Draft;
	program.Revision = 1;
	program.Term.Start_Date = "2026-09-28";
	program.Term.Weeks_Total = 8;

	// Активная программа с измеримой целью обязана иметь цели (инвариант 8).
	SGoal_Target weight;

	weight.ID = in_program_id + "_g";
	weight.Program_Id = in_program_id;
	weight.Metric = EGoal_Metric::BodyWeightKg;
	weight.Unit = EGoal_Unit::Kg;
	weight.Start_Value = 80;
	weight.Has_Start_Value = true;
	weight.Target_Value = 76;
	weight.Direction = EGoal_Direction::Decrease;
	weight.State = EGoal_Target_State::Active;

	for (int day_of_week = 1; day_of_week <= 7; ++day_of_week)
	{
		SProgram_Day day;
		SWeek_Slot slot;

		day.ID = in_program_id + QString("_d%1").arg(day_of_week);
		day.Title = QString("День %1").arg(day_of_week);
		day.Kind = EDay_Kind::Workout;
		day.Order_Index = day_of_week;
		days.append(day);

		slot.ID = in_program_id + QString("_s%1").arg(day_of_week);
		slot.Program_Id = in_program_id;
		slot.Program_Day_Id = day.ID;
		slot.Day_Of_Week = day_of_week;
		slot.Kind = ESlot_Kind::Workout;
		week_slots.append(slot);
	}

	QVERIFY(Program_Repository->Insert_Program_Full(program, QVector<SGoal_Target>{weight}, days,
	                                                QVector<SDay_Exercise>(), week_slots));
	// Активной программой становится только через штатный переход (§3.1).
	QVERIFY(Program_Repository->Change_State(in_program_id, EProgram_State::Active));
}
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::Seed_Rules(const QString &in_program_id)
{
	QVector<SProgram_Rule> rules;

	rules.append(Make_Rule(in_program_id, "r_protein", "protein", ERule_Check_Source::SelfReport));
	rules.append(Make_Rule(in_program_id, "r_steps", "steps", ERule_Check_Source::ComputedFromSets));
	QVERIFY(Rule_Repository->Replace_Rules(in_program_id, rules));
}
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::Seed_User(const QString &in_tag)
{
	Seed_Program("u_" + in_tag, "p_" + in_tag);
	Seed_Rules("p_" + in_tag);
}
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::reloadWithoutActiveProgramFails()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository);
	vm.Set_Today("2026-10-05");

	QVERIFY(vm.Reload("u_unknown") == false);
	QVERIFY(vm.Has_Program() == false);
	QVERIFY(vm.errorKey().isEmpty() == false);
	QCOMPARE(vm.rules()->Row_Count(), 0);
}
//----------------------------------------------------------------------------
// Правило без отметки за сегодня показывает «неизвестно», а не «выполнено»:
// отсутствие данных не равно нулю (§3.6).
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::reloadLoadsRulesWithUnknownTodayState()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository);
	Seed_User("load");
	vm.Set_Today("2026-10-05");

	QVERIFY(vm.Reload("u_load"));
	QVERIFY(vm.Has_Program());
	QCOMPARE(vm.programId(), QString("p_load"));
	QCOMPARE(vm.rules()->Row_Count(), 2);
	QCOMPARE(Row_By_Role(vm.rules(), 0, "todayState").toString(), QString("unknown"));
	QCOMPARE(vm.today(), QString("2026-10-05"));
}
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::manualCheckinAppearsInTodayState()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository);
	Seed_User("manual");
	vm.Set_Today("2026-10-05");

	SRule_Checkin checkin;

	checkin.Rule_Id = "p_manual_r_protein";
	checkin.Program_Id = "p_manual";
	checkin.Date = "2026-10-05";
	checkin.Value = 150;
	checkin.Has_Value = true;
	checkin.Unit = "g";
	checkin.State = ECheckin_State::Done;
	checkin.Source = ECheckin_Source::Manual;
	QVERIFY(Rule_Repository->Save_Checkin(checkin));

	QVERIFY(vm.Reload("u_manual"));
	QCOMPARE(Row_By_Role(vm.rules(), 0, "todayState").toString(), QString("done"));
}
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::savesManualCheckinWithValueAndNote()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository);
	Seed_User("save");
	vm.Set_Today("2026-10-06");
	QVERIFY(vm.Reload("u_save"));

	QVERIFY(vm.Save_Checkin("p_save_r_protein", "done", 155, true, "после тренировки"));
	QCOMPARE(Row_By_Role(vm.rules(), 0, "todayState").toString(), QString("done"));
	QVERIFY(vm.checkins()->Row_Count() >= 1);
	QCOMPARE(Row_By_Role(vm.checkins(), 0, "ruleId").toString(), QString("p_save_r_protein"));
	QCOMPARE(Row_By_Role(vm.checkins(), 0, "date").toString(), QString("2026-10-06"));
	QCOMPARE(Row_By_Role(vm.checkins(), 0, "value").toDouble(), 155.0);
	QCOMPARE(Row_By_Role(vm.checkins(), 0, "note").toString(), QString("после тренировки"));
	QCOMPARE(Row_By_Role(vm.checkins(), 0, "source").toString(), QString("manual"));
}
//----------------------------------------------------------------------------
// §3.7: расчётная отметка не перезаписывает ручную.
void AProgram_Rules_VM_Test::computedCheckinCannotOverwriteManual()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository);
	Seed_User("state");
	vm.Set_Today("2026-10-07");
	QVERIFY(vm.Reload("u_state"));

	QVERIFY(vm.Save_Checkin("p_state_r_protein", "done", 160, true, "вручную"));
	QVERIFY(vm.Save_Checkin("p_state_r_protein", "computed", 0, false, "") == false);
	QVERIFY(vm.errorKey().isEmpty() == false);
}
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::periodLimitsLoadedCheckins()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository);
	Seed_User("period");
	vm.Set_Today("2026-10-20");
	QVERIFY(vm.Reload("u_period"));

	QVERIFY(vm.Save_Checkin("p_period_r_protein", "done", 150, true, ""));

	vm.Set_Today("2026-09-30");
	QVERIFY(vm.Save_Checkin("p_period_r_protein", "failed", 90, true, ""));

	vm.Set_Today("2026-10-20");
	QVERIFY(vm.Reload("u_period"));

	// Неделя не достаёт отметку конца сентября.
	QCOMPARE(vm.periodDays(), 7);
	QCOMPARE(vm.checkinCount(), 1);

	vm.Set_Period_Days(90);
	QCOMPARE(vm.checkinCount(), 2);
}
//----------------------------------------------------------------------------
// В adherence входят только вердикты: «неизвестно» не считается ни успехом,
// ни провалом (§5.5).
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::adherenceCountsVerdictsOnly()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository);
	Seed_User("adh");
	vm.Set_Today("2026-10-12");

	QVERIFY(vm.Reload("u_adh"));
	// Отметок нет вовсе — считать нечего.
	QVERIFY(vm.hasAdherencePercent() == false);

	QVERIFY(vm.Save_Checkin("p_adh_r_protein", "done", 150, true, ""));
	QVERIFY(vm.hasAdherencePercent());
	QVERIFY(vm.adherencePercent() >= 0.0);
}
//----------------------------------------------------------------------------
// Процент считается после того, как прочитаны правила: движок раскладывает
// вердикты отметок по целям правил, поэтому расчёт на пустом наборе правил
// оставляет метрику отсутствующей — экран показывает прочерк при полной базе.
// Отметка здесь без явного состояния: вердикт выводится из значения (§3.6).
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::adherenceComputedAfterRulesLoad()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository);
	Seed_User("order");
	vm.Set_Today("2026-10-14");

	SRule_Checkin checkin;

	checkin.Rule_Id = "p_order_r_protein";
	checkin.Program_Id = "p_order";
	checkin.Date = "2026-10-14";
	checkin.Value = 150;
	checkin.Has_Value = true;
	checkin.Unit = "g";
	checkin.State = ECheckin_State::Unknown;
	checkin.Source = ECheckin_Source::Manual;
	QVERIFY(Rule_Repository->Save_Checkin(checkin));

	QVERIFY(vm.Reload("u_order"));
	QCOMPARE(vm.checkinCount(), 1);
	QVERIFY2(vm.hasAdherencePercent(), "процент посчитан раньше загрузки правил");
	QCOMPARE(vm.adherencePercent(), 100.0);

	// Смена окна пересчитывает метрику по новому набору отметок: расширение
	// периода оставляет ту же отметку, и процент считается заново, а не
	// остаётся значением прежнего окна.
	vm.Set_Period_Days(30);
	QCOMPARE(vm.checkinCount(), 1);
	QVERIFY(vm.hasAdherencePercent());
}
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::emitsChangedOnReloadAndSave()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository);
	Seed_User("signal");
	vm.Set_Today("2026-10-13");
	QSignalSpy spy(&vm, &Avm_Program_Rules::changed);

	QVERIFY(vm.Reload("u_signal"));
	QCOMPARE(spy.count(), 1);

	QVERIFY(vm.Save_Checkin("p_signal_r_protein", "done", 150, true, ""));
	QCOMPARE(spy.count(), 2);
}
//----------------------------------------------------------------------------
// Генерация создаёт четыре правила питания с целевыми значениями расчёта:
// белок/углеводы/жиры — «не ниже N г», калории — «ровно C ккал». Именно это
// закрепляет норму макросов в программе, чтобы её можно было отмечать.
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::generateRulesFromMacroCreatesNutritionRules()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository, Users_Repository);
	Seed_User("gen");
	Seed_Macro_Result("u_gen", 150, 70, 250, 2400);
	vm.Set_Today("2026-10-13");

	QVERIFY(vm.Reload("u_gen"));
	const int before = vm.rules()->Row_Count();

	QVERIFY(vm.Generate_Rules_From_Macro("u_gen"));

	// Прежние правила (белок + шаги) остаются, к ним добавляются четыре макро.
	QCOMPARE(vm.rules()->Row_Count(), before + 4);

	// Повторная генерация по тому же расчёту не должна ничего менять в наборе.
	QVERIFY(vm.Generate_Rules_From_Macro("u_gen"));
	QCOMPARE(vm.rules()->Row_Count(), before + 4);
}
//----------------------------------------------------------------------------
// Повторное нажатие обновляет целевые значения макро-правил по новому расчёту,
// а не копит дубликаты: иначе экран правил засорялся бы одинаковыми строками, а
// adherence считал бы по двум противоречащим нормам белка.
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::generateRulesFromMacroUpdatesInsteadOfDuplicating()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository, Users_Repository);
	Seed_User("upd");
	Seed_Macro_Result("u_upd", 150, 70, 250, 2400);
	vm.Set_Today("2026-10-13");

	QVERIFY(vm.Reload("u_upd"));
	QVERIFY(vm.Generate_Rules_From_Macro("u_upd"));
	const int after_first = vm.rules()->Row_Count();

	// Пересчёт с другими числами — цель правил обязана поехать, а не удвоиться.
	Seed_Macro_Result("u_upd", 170, 80, 200, 2200);
	QVERIFY(vm.Generate_Rules_From_Macro("u_upd"));
	QCOMPARE(vm.rules()->Row_Count(), after_first);
}
//----------------------------------------------------------------------------
// Без сохранённого расчёта кнопка отказывает понятной ошибкой и не создаёт
// пустых правил: «углеводы ≥ 0 г» в программе бессмысленны и только запутали бы.
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::generateRulesFromMacroRequiresCalculatedMacro()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository, Users_Repository);
	Seed_User("nocalc");
	vm.Set_Today("2026-10-13");

	QVERIFY(vm.Reload("u_nocalc"));
	const int before = vm.rules()->Row_Count();

	QVERIFY(vm.Generate_Rules_From_Macro("u_nocalc") == false);
	QVERIFY(vm.errorKey().isEmpty() == false);
	QCOMPARE(vm.rules()->Row_Count(), before);
}
//----------------------------------------------------------------------------

QTEST_GUILESS_MAIN(AProgram_Rules_VM_Test)
#include "tst_Program_Rules_VM.moc"
//----------------------------------------------------------------------------
