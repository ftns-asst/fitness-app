#include "Data/Database/Sql_Database.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
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
	void emitsChangedOnReloadAndSave();

private:
	void Seed_Program(const QString &in_user_id, const QString &in_program_id);
	void Seed_Rules(const QString &in_program_id);
	// Отдельные пользователь и программа на тест: тесты не зависят от порядка.
	void Seed_User(const QString &in_tag);
	SProgram_Rule Make_Rule(const QString &in_program_id, const QString &in_id, const QString &in_code,
	                        ERule_Check_Source in_source);
	// Строка списка по имени роли: тест не должен знать числовых значений ролей.
	QVariant Row_By_Role(const AList_Model *in_model, int in_row, const QString &in_role) const;

	QTemporaryDir Directory;
	AsSql_Database Database;
	AProgram_Repository *Program_Repository = 0;
	ARule_Repository *Rule_Repository = 0;
};
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::initTestCase()
{
	QVERIFY(Directory.isValid());
	QVERIFY(Database.Open(Directory.filePath("program_rules_vm.sqlite3")));
	Program_Repository = new AProgram_Repository(&Database, this);
	Rule_Repository = new ARule_Repository(&Database, this);
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

QTEST_GUILESS_MAIN(AProgram_Rules_VM_Test)
#include "tst_Program_Rules_VM.moc"
//----------------------------------------------------------------------------
