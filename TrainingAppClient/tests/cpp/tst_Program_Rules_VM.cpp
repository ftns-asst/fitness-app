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
// Avm_Program_Rules: РїСЂР°РІРёР»Р° РїСЂРѕРіСЂР°РјРјС‹ Рё СЂСѓС‡РЅС‹Рµ РѕС‚РјРµС‚РєРё РїРѕР»СЊР·РѕРІР°С‚РµР»СЏ
// (В§3.6вЂ“3.7, В§5.1). Р”Р°С‚Р° С„РёРєСЃРёСЂСѓРµС‚СЃСЏ С‡РµСЂРµР· Set_Today, С‡С‚РѕР±С‹ СЂРµР·СѓР»СЊС‚Р°С‚ РЅРµ
// Р·Р°РІРёСЃРµР» РѕС‚ СЃРёСЃС‚РµРјРЅС‹С… С‡Р°СЃРѕРІ.
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
	// вЂ” Р“РµРЅРµСЂР°С†РёСЏ РїСЂР°РІРёР» РёР· СЂР°СЃС‡С‘С‚Р° РљР‘Р–РЈ (СЃРїСЂРёРЅС‚ В«РљР‘Р–РЈ РІ РїСЂР°РІРёР»Р°С…В»)
	// вЂ” РљРЅРѕРїРєР° РЅР° СЌРєСЂР°РЅРµ РїСЂР°РІРёР» СЃРѕР·РґР°С‘С‚/РѕР±РЅРѕРІР»СЏРµС‚ 4 РїСЂР°РІРёР»Р° РїРёС‚Р°РЅРёСЏ
	// (Р±РµР»РѕРє, СѓРіР»РµРІРѕРґС‹, Р¶РёСЂС‹, РєР°Р»РѕСЂРёРё) РІ Р°РєС‚РёРІРЅРѕР№ РїСЂРѕРіСЂР°РјРјРµ РїРѕ
	// СЃРѕС…СЂР°РЅС‘РЅРЅРѕРјСѓ СЂР°СЃС‡С‘С‚Сѓ. РџСЂРѕРІРµСЂСЏРµС‚СЃСЏ РіР»Р°РІРЅРѕРµ: РїСЂР°РІРёР»Р°
	// РїРѕСЏРІР»СЏСЋС‚СЃСЏ СЃ С†РµР»РµРІС‹РјРё Р·РЅР°С‡РµРЅРёСЏРјРё СЂР°СЃС‡С‘С‚Р°, РїРѕРІС‚РѕСЂРЅРѕРµ РЅР°Р¶Р°С‚РёРµ
	// РѕР±РЅРѕРІР»СЏРµС‚ РёС…, Р° РЅРµ РґСѓР±Р»РёСЂСѓРµС‚, Р° Р±РµР· СЂР°СЃС‡С‘С‚Р° РєРЅРѕРїРєР° РѕС‚РєР°Р·С‹РІР°РµС‚
	// РїРѕРЅСЏС‚РЅРѕР№ РѕС€РёР±РєРѕР№, РЅРµ СЃРѕР·РґР°РІР°СЏ РїСѓСЃС‚С‹С… РїСЂР°РІРёР».
	void generateRulesFromMacroCreatesNutritionRules();
	void generateRulesFromMacroUpdatesInsteadOfDuplicating();
	void generateRulesFromMacroRequiresCalculatedMacro();

private:
	void Seed_Program(const QString &in_user_id, const QString &in_program_id);
	void Seed_Rules(const QString &in_program_id);
	// РћС‚РґРµР»СЊРЅС‹Рµ РїРѕР»СЊР·РѕРІР°С‚РµР»СЊ Рё РїСЂРѕРіСЂР°РјРјР° РЅР° С‚РµСЃС‚: С‚РµСЃС‚С‹ РЅРµ Р·Р°РІРёСЃСЏС‚
	// РѕС‚ РїРѕСЂСЏРґРєР°.
	void Seed_User(const QString &in_tag);
	SProgram_Rule Make_Rule(const QString &in_program_id, const QString &in_id, const QString &in_code,
	                        ERule_Check_Source in_source);
	// РЎРѕС…СЂР°РЅС‘РЅРЅС‹Р№ СЂРµР·СѓР»СЊС‚Р°С‚ СЂР°СЃС‡С‘С‚Р° РљР‘Р–РЈ РґР»СЏ РіРµРЅРµСЂР°С†РёРё РїСЂР°РІРёР»:
	// РіРµРЅРµСЂР°С†РёСЏ С‡РёС‚Р°РµС‚ РµРіРѕ РёР· profile_local, Р° РЅРµ РёР· Р°СЂРіСѓРјРµРЅС‚Р°, С‡С‚РѕР±С‹ С‡РёСЃР»Р°
	// РїСЂР°РІРёР» СЃРѕРІРїР°РґР°Р»Рё СЃ С‚РµРј, С‡С‚Рѕ С‡РµР»РѕРІРµРє РІРёРґРµР» РЅР° СЌРєСЂР°РЅРµ СЂР°СЃС‡С‘С‚Р°.
	void Seed_Macro_Result(const QString &in_user_id, double in_protein, double in_fat, double in_carbs,
	                       double in_calories);
	// РЎС‚СЂРѕРєР° СЃРїРёСЃРєР° РїРѕ РёРјРµРЅРё СЂРѕР»Рё: С‚РµСЃС‚ РЅРµ РґРѕР»Р¶РµРЅ Р·РЅР°С‚СЊ С‡РёСЃР»РѕРІС‹С…
	// Р·РЅР°С‡РµРЅРёР№ СЂРѕР»РµР№.
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
	// Р РµРїРѕР·РёС‚РѕСЂРёР№ РїРѕР»СЊР·РѕРІР°С‚РµР»СЏ РЅСѓР¶РµРЅ РіРµРЅРµСЂР°С†РёРё РїСЂР°РІРёР»: РѕРЅР° С‡РёС‚Р°РµС‚
	// СЃРѕС…СЂР°РЅС‘РЅРЅС‹Р№ СЂР°СЃС‡С‘С‚ РљР‘Р–РЈ РёР· profile_local. РЎРµСЃСЃРёСЏ Рё HTTP-РєР»РёРµРЅС‚ вЂ”
	// Р·Р°РіР»СѓС€РєРё: РіРµРЅРµСЂР°С†РёСЏ РЅРµ С…РѕРґРёС‚ РІ СЃРµС‚СЊ, С‚РѕР»СЊРєРѕ РІ Р»РѕРєР°Р»СЊРЅСѓСЋ Р±Р°Р·Сѓ.
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
	// Р—РЅР°С‡РµРЅРёСЏ СЂРѕР»РµР№ Qt РЅР°С‡РёРЅР°СЋС‚СЃСЏ СЃ Qt::UserRole + 1, РїРѕСЌС‚РѕРјСѓ РїРµСЂРµР±РёСЂР°СЋС‚СЃСЏ
	// РєР»СЋС‡Рё С…РµС€Р°, Р° РЅРµ С‡РёСЃР»Р° РїРѕРґСЂСЏРґ.
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
	rule.Title = "Р‘РµР»РѕРє";
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
	program.Title = "РЎРёР»Р° 4 РЅРµРґРµР»Рё";
	program.Source_Kind = EProgram_Source_Kind::Scratch;
	program.Goal = EProgram_Goal::Strength;
	program.Schedule_Kind = ESchedule_Kind::Weekday;
	program.State = EProgram_State::Draft;
	program.Revision = 1;
	program.Term.Start_Date = "2026-09-28";
	program.Term.Weeks_Total = 8;

	// РђРєС‚РёРІРЅР°СЏ РїСЂРѕРіСЂР°РјРјР° СЃ РёР·РјРµСЂРёРјРѕР№ С†РµР»СЊСЋ РѕР±СЏР·Р°РЅР° РёРјРµС‚СЊ С†РµР»Рё
	// (РёРЅРІР°СЂРёР°РЅС‚ 8).
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
		day.Title = QString("Р”РµРЅСЊ %1").arg(day_of_week);
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
	// РђРєС‚РёРІРЅРѕР№ РїСЂРѕРіСЂР°РјРјРѕР№ СЃС‚Р°РЅРѕРІРёС‚СЃСЏ С‚РѕР»СЊРєРѕ С‡РµСЂРµР· С€С‚Р°С‚РЅС‹Р№ РїРµСЂРµС…РѕРґ
	// (В§3.1).
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
	Avm_Program_Rules vm(Program_Repository, Rule_Repository, Users_Repository);
	vm.Set_Today("2026-10-05");

	QVERIFY(vm.Reload("u_unknown") == false);
	QVERIFY(vm.Has_Program() == false);
	QVERIFY(vm.errorKey().isEmpty() == false);
	QCOMPARE(vm.rules()->Row_Count(), 0);
}
//----------------------------------------------------------------------------
// РџСЂР°РІРёР»Рѕ Р±РµР· РѕС‚РјРµС‚РєРё Р·Р° СЃРµРіРѕРґРЅСЏ РїРѕРєР°Р·С‹РІР°РµС‚ В«РЅРµРёР·РІРµСЃС‚РЅРѕВ», Р° РЅРµ
// В«РІС‹РїРѕР»РЅРµРЅРѕВ»: РѕС‚СЃСѓС‚СЃС‚РІРёРµ РґР°РЅРЅС‹С… РЅРµ СЂР°РІРЅРѕ РЅСѓР»СЋ (В§3.6).
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::reloadLoadsRulesWithUnknownTodayState()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository, Users_Repository);
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
	Avm_Program_Rules vm(Program_Repository, Rule_Repository, Users_Repository);
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
	Avm_Program_Rules vm(Program_Repository, Rule_Repository, Users_Repository);
	Seed_User("save");
	vm.Set_Today("2026-10-06");
	QVERIFY(vm.Reload("u_save"));

	QVERIFY(vm.Save_Checkin("p_save_r_protein", "done", 155, true, "РїРѕСЃР»Рµ С‚СЂРµРЅРёСЂРѕРІРєРё"));
	QCOMPARE(Row_By_Role(vm.rules(), 0, "todayState").toString(), QString("done"));
	QVERIFY(vm.checkins()->Row_Count() >= 1);
	QCOMPARE(Row_By_Role(vm.checkins(), 0, "ruleId").toString(), QString("p_save_r_protein"));
	QCOMPARE(Row_By_Role(vm.checkins(), 0, "date").toString(), QString("2026-10-06"));
	QCOMPARE(Row_By_Role(vm.checkins(), 0, "value").toDouble(), 155.0);
	QCOMPARE(Row_By_Role(vm.checkins(), 0, "note").toString(), QString("РїРѕСЃР»Рµ С‚СЂРµРЅРёСЂРѕРІРєРё"));
	QCOMPARE(Row_By_Role(vm.checkins(), 0, "source").toString(), QString("manual"));
}
//----------------------------------------------------------------------------
// В§3.7: СЂР°СЃС‡С‘С‚РЅР°СЏ РѕС‚РјРµС‚РєР° РЅРµ РїРµСЂРµР·Р°РїРёСЃС‹РІР°РµС‚ СЂСѓС‡РЅСѓСЋ.
void AProgram_Rules_VM_Test::computedCheckinCannotOverwriteManual()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository, Users_Repository);
	Seed_User("state");
	vm.Set_Today("2026-10-07");
	QVERIFY(vm.Reload("u_state"));

	QVERIFY(vm.Save_Checkin("p_state_r_protein", "done", 160, true, "РІСЂСѓС‡РЅСѓСЋ"));
	QVERIFY(vm.Save_Checkin("p_state_r_protein", "computed", 0, false, "") == false);
	QVERIFY(vm.errorKey().isEmpty() == false);
}
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::periodLimitsLoadedCheckins()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository, Users_Repository);
	Seed_User("period");
	vm.Set_Today("2026-10-20");
	QVERIFY(vm.Reload("u_period"));

	QVERIFY(vm.Save_Checkin("p_period_r_protein", "done", 150, true, ""));

	vm.Set_Today("2026-09-30");
	QVERIFY(vm.Save_Checkin("p_period_r_protein", "failed", 90, true, ""));

	vm.Set_Today("2026-10-20");
	QVERIFY(vm.Reload("u_period"));

	// РќРµРґРµР»СЏ РЅРµ РґРѕСЃС‚Р°С‘С‚ РѕС‚РјРµС‚РєСѓ РєРѕРЅС†Р° СЃРµРЅС‚СЏР±СЂСЏ.
	QCOMPARE(vm.periodDays(), 7);
	QCOMPARE(vm.checkinCount(), 1);

	vm.Set_Period_Days(90);
	QCOMPARE(vm.checkinCount(), 2);
}
//----------------------------------------------------------------------------
// Р’ adherence РІС…РѕРґСЏС‚ С‚РѕР»СЊРєРѕ РІРµСЂРґРёРєС‚С‹: В«РЅРµРёР·РІРµСЃС‚РЅРѕВ» РЅРµ СЃС‡РёС‚Р°РµС‚СЃСЏ РЅРё
// СѓСЃРїРµС…РѕРј, РЅРё РїСЂРѕРІР°Р»РѕРј (В§5.5).
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::adherenceCountsVerdictsOnly()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository, Users_Repository);
	Seed_User("adh");
	vm.Set_Today("2026-10-12");

	QVERIFY(vm.Reload("u_adh"));
	// РћС‚РјРµС‚РѕРє РЅРµС‚ РІРѕРІСЃРµ вЂ” СЃС‡РёС‚Р°С‚СЊ РЅРµС‡РµРіРѕ.
	QVERIFY(vm.hasAdherencePercent() == false);

	QVERIFY(vm.Save_Checkin("p_adh_r_protein", "done", 150, true, ""));
	QVERIFY(vm.hasAdherencePercent());
	QVERIFY(vm.adherencePercent() >= 0.0);
}
//----------------------------------------------------------------------------
// РџСЂРѕС†РµРЅС‚ СЃС‡РёС‚Р°РµС‚СЃСЏ РїРѕСЃР»Рµ С‚РѕРіРѕ, РєР°Рє РїСЂРѕС‡РёС‚Р°РЅС‹ РїСЂР°РІРёР»Р°: РґРІРёР¶РѕРє
// СЂР°СЃРєР»Р°РґС‹РІР°РµС‚ РІРµСЂРґРёРєС‚С‹ РѕС‚РјРµС‚РѕРє РїРѕ С†РµР»СЏРј РїСЂР°РІРёР», РїРѕСЌС‚РѕРјСѓ СЂР°СЃС‡С‘С‚
// РЅР° РїСѓСЃС‚РѕРј РЅР°Р±РѕСЂРµ РїСЂР°РІРёР» РѕСЃС‚Р°РІР»СЏРµС‚ РјРµС‚СЂРёРєСѓ РѕС‚СЃСѓС‚СЃС‚РІСѓСЋС‰РµР№ вЂ”
// СЌРєСЂР°РЅ РїРѕРєР°Р·С‹РІР°РµС‚ РїСЂРѕС‡РµСЂРє РїСЂРё РїРѕР»РЅРѕР№ Р±Р°Р·Рµ. РћС‚РјРµС‚РєР° Р·РґРµСЃСЊ Р±РµР·
// СЏРІРЅРѕРіРѕ СЃРѕСЃС‚РѕСЏРЅРёСЏ: РІРµСЂРґРёРєС‚ РІС‹РІРѕРґРёС‚СЃСЏ РёР· Р·РЅР°С‡РµРЅРёСЏ (В§3.6).
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::adherenceComputedAfterRulesLoad()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository, Users_Repository);
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
	QVERIFY2(vm.hasAdherencePercent(), "РїСЂРѕС†РµРЅС‚ РїРѕСЃС‡РёС‚Р°РЅ СЂР°РЅСЊС€Рµ Р·Р°РіСЂСѓР·РєРё РїСЂР°РІРёР»");
	QCOMPARE(vm.adherencePercent(), 100.0);

	// РЎРјРµРЅР° РѕРєРЅР° РїРµСЂРµСЃС‡РёС‚С‹РІР°РµС‚ РјРµС‚СЂРёРєСѓ РїРѕ РЅРѕРІРѕРјСѓ РЅР°Р±РѕСЂСѓ РѕС‚РјРµС‚РѕРє:
	// СЂР°СЃС€РёСЂРµРЅРёРµ РїРµСЂРёРѕРґР° РѕСЃС‚Р°РІР»СЏРµС‚ С‚Сѓ Р¶Рµ РѕС‚РјРµС‚РєСѓ, Рё РїСЂРѕС†РµРЅС‚
	// СЃС‡РёС‚Р°РµС‚СЃСЏ Р·Р°РЅРѕРІРѕ, Р° РЅРµ РѕСЃС‚Р°С‘С‚СЃСЏ Р·РЅР°С‡РµРЅРёРµРј РїСЂРµР¶РЅРµРіРѕ РѕРєРЅР°.
	vm.Set_Period_Days(30);
	QCOMPARE(vm.checkinCount(), 1);
	QVERIFY(vm.hasAdherencePercent());
}
//----------------------------------------------------------------------------
void AProgram_Rules_VM_Test::emitsChangedOnReloadAndSave()
{
	Avm_Program_Rules vm(Program_Repository, Rule_Repository, Users_Repository);
	Seed_User("signal");
	vm.Set_Today("2026-10-13");
	QSignalSpy spy(&vm, &Avm_Program_Rules::changed);

	QVERIFY(vm.Reload("u_signal"));
	QCOMPARE(spy.count(), 1);

	QVERIFY(vm.Save_Checkin("p_signal_r_protein", "done", 150, true, ""));
	QCOMPARE(spy.count(), 2);
}
//----------------------------------------------------------------------------
// Р“РµРЅРµСЂР°С†РёСЏ СЃРѕР·РґР°С‘С‚ С‡РµС‚С‹СЂРµ РїСЂР°РІРёР»Р° РїРёС‚Р°РЅРёСЏ СЃ С†РµР»РµРІС‹РјРё Р·РЅР°С‡РµРЅРёСЏРјРё
// СЂР°СЃС‡С‘С‚Р°: Р±РµР»РѕРє/СѓРіР»РµРІРѕРґС‹/Р¶РёСЂС‹ вЂ” В«РЅРµ РЅРёР¶Рµ N РіВ», РєР°Р»РѕСЂРёРё вЂ” В«СЂРѕРІРЅРѕ C
// РєРєР°Р»В». РРјРµРЅРЅРѕ СЌС‚Рѕ Р·Р°РєСЂРµРїР»СЏРµС‚ РЅРѕСЂРјСѓ РјР°РєСЂРѕСЃРѕРІ РІ
// РїСЂРѕРіСЂР°РјРјРµ, С‡С‚РѕР±С‹ РµС‘ РјРѕР¶РЅРѕ Р±С‹Р»Рѕ РѕС‚РјРµС‡Р°С‚СЊ.
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

	// РџСЂРµР¶РЅРёРµ РїСЂР°РІРёР»Р° (Р±РµР»РѕРє + С€Р°РіРё) РѕСЃС‚Р°СЋС‚СЃСЏ, Рє РЅРёРј РґРѕР±Р°РІР»СЏСЋС‚СЃСЏ
	// С‡РµС‚С‹СЂРµ РјР°РєСЂРѕ.
	QCOMPARE(vm.rules()->Row_Count(), before + 4);

	// РџРѕРІС‚РѕСЂРЅР°СЏ РіРµРЅРµСЂР°С†РёСЏ РїРѕ С‚РѕРјСѓ Р¶Рµ СЂР°СЃС‡С‘С‚Сѓ РЅРµ РґРѕР»Р¶РЅР° РЅРёС‡РµРіРѕ
	// РјРµРЅСЏС‚СЊ РІ РЅР°Р±РѕСЂРµ.
	QVERIFY(vm.Generate_Rules_From_Macro("u_gen"));
	QCOMPARE(vm.rules()->Row_Count(), before + 4);
}
//----------------------------------------------------------------------------
// РџРѕРІС‚РѕСЂРЅРѕРµ РЅР°Р¶Р°С‚РёРµ РѕР±РЅРѕРІР»СЏРµС‚ С†РµР»РµРІС‹Рµ Р·РЅР°С‡РµРЅРёСЏ РјР°РєСЂРѕ-РїСЂР°РІРёР» РїРѕ
// РЅРѕРІРѕРјСѓ СЂР°СЃС‡С‘С‚Сѓ, Р° РЅРµ РєРѕРїРёС‚ РґСѓР±Р»РёРєР°С‚С‹: РёРЅР°С‡Рµ СЌРєСЂР°РЅ РїСЂР°РІРёР»
// Р·Р°СЃРѕСЂСЏР»СЃСЏ Р±С‹ РѕРґРёРЅР°РєРѕРІС‹РјРё СЃС‚СЂРѕРєР°РјРё, Р° adherence СЃС‡РёС‚Р°Р» Р±С‹ РїРѕ РґРІСѓРј
// РїСЂРѕС‚РёРІРѕСЂРµС‡Р°С‰РёРј РЅРѕСЂРјР°Рј Р±РµР»РєР°.
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

	// РџРµСЂРµСЃС‡С‘С‚ СЃ РґСЂСѓРіРёРјРё С‡РёСЃР»Р°РјРё вЂ” С†РµР»СЊ РїСЂР°РІРёР» РѕР±СЏР·Р°РЅР° РїРѕРµС…Р°С‚СЊ, Р°
	// РЅРµ СѓРґРІРѕРёС‚СЊСЃСЏ.
	Seed_Macro_Result("u_upd", 170, 80, 200, 2200);
	QVERIFY(vm.Generate_Rules_From_Macro("u_upd"));
	QCOMPARE(vm.rules()->Row_Count(), after_first);
}
//----------------------------------------------------------------------------
// Р‘РµР· СЃРѕС…СЂР°РЅС‘РЅРЅРѕРіРѕ СЂР°СЃС‡С‘С‚Р° РєРЅРѕРїРєР° РѕС‚РєР°Р·С‹РІР°РµС‚ РїРѕРЅСЏС‚РЅРѕР№ РѕС€РёР±РєРѕР№ Рё
// РЅРµ СЃРѕР·РґР°С‘С‚ РїСѓСЃС‚С‹С… РїСЂР°РІРёР»: В«СѓРіР»РµРІРѕРґС‹ в‰Ґ 0 РіВ» РІ РїСЂРѕРіСЂР°РјРјРµ
// Р±РµСЃСЃРјС‹СЃР»РµРЅРЅС‹ Рё С‚РѕР»СЊРєРѕ Р·Р°РїСѓС‚Р°Р»Рё Р±С‹.
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
