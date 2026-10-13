#include "Data/Database/Sql_Database.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Domain/Program_Model.h"

#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

class ARule_Repository_Test : public QObject
{
	Q_OBJECT

private slots:
	void replaceRulesAndReload();
	void replaceRulesValidatesInput();
	void replaceRulesKeepsPreviousSnapshot();
	void replaceRulesRejectsReadOnlyPrograms();
	void replaceRulesRollsBackOnInvalidSet();
	void checkinUpsertAndManualWins();

	void checkinRangesUseProgramAndRule();

	// — Рекомендация: текстовое правило без измеримой цели —
	// Слой репозитория повторяет правило схемы, но с внятным сообщением:
	// отказ без причины в UI выглядит как «FOREIGN KEY constraint failed».
	void replaceRulesAcceptsAdviceWithoutTarget();
	void replaceRulesRejectsAdviceWithoutText();
	void loadRulesKeepsAdviceText();

private:
	QString Make_Database_Path();
	SProgram Make_Program(const QString &in_id, EProgram_State in_state);
	SProgram_Rule Make_Rule(const QString &in_id, const QString &in_code);
	// Рекомендация: то, что ради неё и снимается требование цели. Заводится
	// отдельно от Make_Rule, потому что отличается по сути, а не по значению:
	// у него нет цели, метрика custom, а текст лежит в Note.
	SProgram_Rule Make_Advice_Rule(const QString &in_id, const QString &in_text);
	void Seed_Program(AProgram_Repository &in_repository, const QString &in_id, EProgram_State in_state);
	SRule_Checkin Make_Checkin(const QString &in_rule_id, const QString &in_date, double in_value,
	                           ECheckin_Source in_source);

	QTemporaryDir Directory;
	int Counter = 0;
};

//----------------------------------------------------------------------------
QString ARule_Repository_Test::Make_Database_Path()
{
	++Counter;

	return Directory.filePath(QString("rules_%1.sqlite3").arg(Counter));
}
//----------------------------------------------------------------------------
SProgram ARule_Repository_Test::Make_Program(const QString &in_id, EProgram_State in_state)
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
//----------------------------------------------------------------------------
void ARule_Repository_Test::Seed_Program(AProgram_Repository &in_repository, const QString &in_id,
                                         EProgram_State in_state)
{
	QVERIFY(in_repository.Insert_Program(Make_Program(in_id, in_state)));
}
//----------------------------------------------------------------------------
SProgram_Rule ARule_Repository_Test::Make_Rule(const QString &in_id, const QString &in_code)
{
	SProgram_Rule rule;

	rule.ID = in_id;
	rule.Program_Id = "p1";
	rule.Category = ERule_Category::Nutrition;
	rule.Code = in_code;
	rule.Title = QString("Белок не ниже 1,8 г/кг (%1)").arg(in_code);
	rule.Metric = ERule_Metric::ProteinGrams;
	rule.Periodicity = ERule_Periodicity::PerDay;
	rule.Target_Min = 145;
	rule.Has_Target_Min = true;
	rule.Target_Unit = ERule_Display_Unit::G;
	rule.Applies_To = ERule_Applies_To::AllDays;
	rule.Check_Source = ERule_Check_Source::Mixed;
	rule.Enforcement = ERule_Enforcement::Soft;
	rule.Display_Unit = ERule_Display_Unit::G;
	rule.Note = "1,8 г на кг веса";

	return rule;
}
//----------------------------------------------------------------------------
SProgram_Rule ARule_Repository_Test::Make_Advice_Rule(const QString &in_id, const QString &in_text)
{
	SProgram_Rule rule;

	rule.ID = in_id;
	rule.Program_Id = "p1";
	rule.Category = ERule_Category::Other;
	rule.Code = "custom";
	rule.Title = in_text;
	rule.Metric = ERule_Metric::Custom;
	rule.Periodicity = ERule_Periodicity::PerDay;
	rule.Target_Unit = ERule_Display_Unit::Units;
	rule.Applies_To = ERule_Applies_To::AllDays;
	rule.Check_Source = ERule_Check_Source::SelfReport;
	rule.Enforcement = ERule_Enforcement::Soft;
	rule.Display_Unit = ERule_Display_Unit::Units;
	rule.Note = in_text;

	return rule;
}
//----------------------------------------------------------------------------
SRule_Checkin ARule_Repository_Test::Make_Checkin(const QString &in_rule_id, const QString &in_date, double in_value,
                                                  ECheckin_Source in_source)
{
	SRule_Checkin checkin;

	checkin.Rule_Id = in_rule_id;
	checkin.Program_Id = "p1";
	checkin.Date = in_date;
	checkin.Value = in_value;
	checkin.Has_Value = true;
	checkin.Unit = "g";
	checkin.State = ECheckin_State::Done;
	checkin.Source = in_source;
	checkin.Note = "Ручная отметка";

	return checkin;
}
//----------------------------------------------------------------------------
void ARule_Repository_Test::replaceRulesAndReload()
{
	AsSql_Database database;
	AProgram_Repository program_repository(&database);
	ARule_Repository repository(&database);
	QVector<SProgram_Rule> rules;
	QVector<SProgram_Rule> loaded;
	SProgram_Rule rule;
	SProgram_Rule water;
	QSqlQuery query;

	QVERIFY(database.Open(Make_Database_Path()));
	Seed_Program(program_repository, "p1", EProgram_State::Draft);
	query = QSqlQuery(database.Sql_Connection());

	rules.append(Make_Rule("r1", "protein"));

	water = Make_Rule("r2", "water");
	water.Category = ERule_Category::Lifestyle;
	water.Metric = ERule_Metric::WaterMl;
	water.Target_Min = -1;
	water.Has_Target_Min = false;
	water.Target_Max = 3000;
	water.Has_Target_Max = true;
	water.Target_Unit = ERule_Display_Unit::Ml;
	water.Display_Unit = ERule_Display_Unit::Ml;
	water.Periodicity = ERule_Periodicity::PerWeek;
	water.Applies_To = ERule_Applies_To::CustomDays;
	water.Applies_Days << "1";
	water.Applies_Days << "3";
	water.Check_Source = ERule_Check_Source::SelfReport;
	water.Enforcement = ERule_Enforcement::Hard;
	water.Active_From = "2026-10-05";
	water.Active_To = "2026-11-02";
	water.Note = QString();
	rules.append(water);

	QVERIFY(repository.Replace_Rules("p1", rules));
	QVERIFY(repository.Load_Rules("p1", loaded));
	QCOMPARE(loaded.size(), 2);
	QCOMPARE(loaded.at(0).ID, QString("r1"));
	QCOMPARE(loaded.at(0).Program_Id, QString("p1"));
	QCOMPARE(loaded.at(0).Category, ERule_Category::Nutrition);
	QCOMPARE(loaded.at(0).Metric, ERule_Metric::ProteinGrams);
	QCOMPARE(loaded.at(0).Periodicity, ERule_Periodicity::PerDay);
	QVERIFY(loaded.at(0).Has_Target_Min);
	QCOMPARE(loaded.at(0).Target_Min, 145.0);
	QVERIFY(loaded.at(0).Has_Target_Max == false);
	QCOMPARE(loaded.at(0).Target_Unit, ERule_Display_Unit::G);
	QCOMPARE(loaded.at(0).Check_Source, ERule_Check_Source::Mixed);
	QCOMPARE(loaded.at(0).Enforcement, ERule_Enforcement::Soft);
	QVERIFY(loaded.at(0).Active_From.isEmpty());

	QCOMPARE(loaded.at(1).ID, QString("r2"));
	QVERIFY(loaded.at(1).Has_Target_Min == false);
	QVERIFY(loaded.at(1).Has_Target_Max);
	QCOMPARE(loaded.at(1).Target_Max, 3000.0);
	QCOMPARE(loaded.at(1).Applies_To, ERule_Applies_To::CustomDays);
	QCOMPARE(loaded.at(1).Applies_Days.size(), 2);
	QCOMPARE(loaded.at(1).Applies_Days.at(1), QString("3"));
	QCOMPARE(loaded.at(1).Enforcement, ERule_Enforcement::Hard);
	QCOMPARE(loaded.at(1).Active_From, QString("2026-10-05"));
	QCOMPARE(loaded.at(1).Active_To, QString("2026-11-02"));
	QVERIFY(loaded.at(1).Note.isEmpty());

	// Правка раздела правил растёт как любая другая: revision++ и запись
	// в журнал со снимком СТАРОГО набора (§3.9).
	QVERIFY(query.exec("SELECT revision FROM programs WHERE id = 'p1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 2);
	QVERIFY(query.exec("SELECT revision, reason, snapshot_json FROM program_revisions WHERE program_id = 'p1' "
	                   "ORDER BY revision"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 1);
	QCOMPARE(query.value(1).toString(), QString("state"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 2);
	QCOMPARE(query.value(1).toString(), QString("rules"));
	QCOMPARE(query.value(2).toString(), QString("[]"));

	// Пустой набор — легальная правка: правила просто удаляются.
	rules.clear();
	QVERIFY(repository.Replace_Rules("p1", rules));
	QVERIFY(repository.Load_Rules("p1", loaded));
	QVERIFY(loaded.isEmpty());
	QVERIFY(query.exec("SELECT revision FROM programs WHERE id = 'p1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 3);
}
//----------------------------------------------------------------------------
void ARule_Repository_Test::replaceRulesValidatesInput()
{
	AsSql_Database database;
	AProgram_Repository program_repository(&database);
	ARule_Repository repository(&database);
	QVector<SProgram_Rule> rules;
	SProgram_Rule rule;

	QVERIFY(database.Open(Make_Database_Path()));
	Seed_Program(program_repository, "p1", EProgram_State::Draft);

	// Правило без цели «сколько» не сохраняется (§3.6).
	rule = Make_Rule("r1", "protein");
	rule.Has_Target_Min = false;
	rules.append(rule);
	QVERIFY(repository.Replace_Rules("p1", rules) == false);
	QVERIFY(repository.Get_Last_Error().isEmpty() == false);

	// Правило без идентификатора и без заголовка — тоже.
	rules.clear();
	rules.append(Make_Rule(QString(), "protein"));
	QVERIFY(repository.Replace_Rules("p1", rules) == false);

	rules.clear();
	rule = Make_Rule("r1", "protein");
	rule.Title = QString();
	rules.append(rule);
	QVERIFY(repository.Replace_Rules("p1", rules) == false);

	// Несуществующая программа и закрытая база.
	rules.clear();
	rules.append(Make_Rule("r1", "protein"));
	QVERIFY(repository.Replace_Rules("missing", rules) == false);

	AsSql_Database closed;
	ARule_Repository closed_repository(&closed);
	QVERIFY(closed_repository.Replace_Rules("p1", rules) == false);
	QVERIFY(closed_repository.Load_Rules("p1", rules) == false);
	QVERIFY(closed_repository.Get_Last_Error().isEmpty() == false);
}
//----------------------------------------------------------------------------
void ARule_Repository_Test::replaceRulesKeepsPreviousSnapshot()
{
	AsSql_Database database;
	AProgram_Repository program_repository(&database);
	ARule_Repository repository(&database);
	QVector<SProgram_Rule> rules;
	SProgram_Rule rule;
	QSqlQuery query;
	QString snapshot;

	QVERIFY(database.Open(Make_Database_Path()));
	Seed_Program(program_repository, "p1", EProgram_State::Draft);
	query = QSqlQuery(database.Sql_Connection());

	rules.append(Make_Rule("r1", "protein"));
	QVERIFY(repository.Replace_Rules("p1", rules));

	rules.clear();
	rule = Make_Rule("r2", "sleep");
	rule.Category = ERule_Category::Recovery;
	rule.Metric = ERule_Metric::SleepHours;
	rule.Target_Min = 7;
	rule.Target_Max = 9;
	rule.Has_Target_Max = true;
	rule.Target_Unit = ERule_Display_Unit::Hours;
	rule.Display_Unit = ERule_Display_Unit::Hours;
	rules.append(rule);
	QVERIFY(repository.Replace_Rules("p1", rules));

	QVERIFY(query.exec("SELECT snapshot_json FROM program_revisions WHERE program_id = 'p1' AND revision = 3"));
	QVERIFY(query.next());
	snapshot = query.value(0).toString();
	QVERIFY(snapshot.contains("r1"));
	QVERIFY(snapshot.contains("protein"));
	QVERIFY(snapshot.contains("\"targetMin\":145"));
	QVERIFY(snapshot.contains("r2") == false);
}
//----------------------------------------------------------------------------
void ARule_Repository_Test::replaceRulesRejectsReadOnlyPrograms()
{
	AsSql_Database database;
	AProgram_Repository program_repository(&database);
	ARule_Repository repository(&database);
	QVector<SProgram_Rule> rules;

	QVERIFY(database.Open(Make_Database_Path()));
	Seed_Program(program_repository, "p1", EProgram_State::Draft);

	rules.append(Make_Rule("r1", "protein"));
	QVERIFY(repository.Replace_Rules("p1", rules));

	QVERIFY(program_repository.Change_State("p1", EProgram_State::Active));
	QVERIFY(program_repository.Change_State("p1", EProgram_State::Completed));

	// Инвариант 24 (§4): завершённая программа правку правил запрещает...
	rules.clear();
	rules.append(Make_Rule("r2", "sleep"));
	QVERIFY(repository.Replace_Rules("p1", rules) == false);
	QVERIFY(repository.Get_Last_Error().contains("запрещена"));

	// ...но отметки в ней доступны: ретроспектива правил не теряется (§3.7).
	QVERIFY(repository.Save_Checkin(Make_Checkin("r1", "2026-09-29", 150, ECheckin_Source::Manual)));

	SRule_Checkin checkin;
	QVERIFY(repository.Find_Checkin("r1", "2026-09-29", checkin));
	QCOMPARE(checkin.Value, 150.0);
}
//----------------------------------------------------------------------------
void ARule_Repository_Test::replaceRulesRollsBackOnInvalidSet()
{
	AsSql_Database database;
	AProgram_Repository program_repository(&database);
	ARule_Repository repository(&database);
	QVector<SProgram_Rule> rules;
	QVector<SProgram_Rule> loaded;
	SProgram_Rule rule;
	QSqlQuery query;

	QVERIFY(database.Open(Make_Database_Path()));
	Seed_Program(program_repository, "p1", EProgram_State::Draft);
	query = QSqlQuery(database.Sql_Connection());

	rules.append(Make_Rule("r1", "protein"));
	QVERIFY(repository.Replace_Rules("p1", rules));

	// Два правила с одним id: вставка падает на первичном ключе, набор и
	// ревизия остаются прежними — правка транзакционна.
	rules.clear();
	rules.append(Make_Rule("r2", "water"));
	rule = Make_Rule("r2", "water");
	rule.Title = "Дубль";
	rules.append(rule);
	QVERIFY(repository.Replace_Rules("p1", rules) == false);
	QVERIFY(repository.Get_Last_Error().isEmpty() == false);

	QVERIFY(repository.Load_Rules("p1", loaded));
	QCOMPARE(loaded.size(), 1);
	QCOMPARE(loaded.at(0).ID, QString("r1"));

	QVERIFY(query.exec("SELECT revision FROM programs WHERE id = 'p1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 2);
	QVERIFY(query.exec("SELECT COUNT(*) FROM program_revisions WHERE program_id = 'p1'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 2);
}
//----------------------------------------------------------------------------
// Целое требование снимается с рекомендации: она отвечает не «сколько», а
// «что делать», и измеримой цели у неё нет по природе.
void ARule_Repository_Test::replaceRulesAcceptsAdviceWithoutTarget()
{
	AsSql_Database database;
	AProgram_Repository program_repository(&database);
	ARule_Repository repository(&database);
	QVector<SProgram_Rule> rules;

	QVERIFY(database.Open(Make_Database_Path()));
	Seed_Program(program_repository, "p1", EProgram_State::Draft);

	rules.append(Make_Advice_Rule("r1", "Спать не меньше 7 часов"));
	QVERIFY2(repository.Replace_Rules("p1", rules), qPrintable(repository.Get_Last_Error()));
	QVERIFY(repository.Get_Last_Error().isEmpty());

	// Смешанный набор: измеримое и рекомендация вместе — так пользователь и
	// задаёт программу.
	rules.clear();
	rules.append(Make_Rule("r2", "protein"));
	rules.append(Make_Advice_Rule("r3", "Пей воду натощак"));
	QVERIFY2(repository.Replace_Rules("p1", rules), qPrintable(repository.Get_Last_Error()));
}
//----------------------------------------------------------------------------
// Снятие требования не должно превращать «Другое» в мусор: пустой текст —
// не рекомендация.
void ARule_Repository_Test::replaceRulesRejectsAdviceWithoutText()
{
	AsSql_Database database;
	AProgram_Repository program_repository(&database);
	ARule_Repository repository(&database);
	QVector<SProgram_Rule> rules;
	SProgram_Rule rule;

	QVERIFY(database.Open(Make_Database_Path()));
	Seed_Program(program_repository, "p1", EProgram_State::Draft);

	rule = Make_Advice_Rule("r1", "Совет");
	rule.Note = QString();
	rules.append(rule);
	QVERIFY(repository.Replace_Rules("p1", rules) == false);
	QVERIFY(repository.Get_Last_Error().isEmpty() == false);

	// Измеримое правило без цели по-прежнему отвергается: «Другое» — это
	// про текст, а не послабление для всех.
	rules.clear();
	rule = Make_Rule("r2", "protein");
	rule.Has_Target_Min = false;
	rules.append(rule);
	QVERIFY(repository.Replace_Rules("p1", rules) == false);
}
//----------------------------------------------------------------------------
// Текст рекомендации обязан пережить цикл сохранения: иначе пользователь
// вернётся к записанному совету и не найдёт его.
void ARule_Repository_Test::loadRulesKeepsAdviceText()
{
	AsSql_Database database;
	AProgram_Repository program_repository(&database);
	ARule_Repository repository(&database);
	QVector<SProgram_Rule> rules;
	QVector<SProgram_Rule> loaded;

	QVERIFY(database.Open(Make_Database_Path()));
	Seed_Program(program_repository, "p1", EProgram_State::Draft);

	rules.append(Make_Advice_Rule("r1", "Пей воду натощак каждое утро"));
	rules.append(Make_Rule("r2", "protein"));
	QVERIFY2(repository.Replace_Rules("p1", rules), qPrintable(repository.Get_Last_Error()));

	QVERIFY(repository.Load_Rules("p1", loaded));
	QCOMPARE(loaded.size(), 2);

	// Метрика и категория у рекомендации свои — иначе фильтр по категориям
	// в UI не сможет её показать.
	QCOMPARE(loaded.at(0).Category, ERule_Category::Other);
	QCOMPARE(loaded.at(0).Metric, ERule_Metric::Custom);
	QCOMPARE(loaded.at(0).Note, QString("Пей воду натощак каждое утро"));
	QVERIFY(loaded.at(0).Has_Target_Min == false);
}
//----------------------------------------------------------------------------
void ARule_Repository_Test::checkinUpsertAndManualWins()
{
	AsSql_Database database;
	AProgram_Repository program_repository(&database);
	ARule_Repository repository(&database);
	QVector<SProgram_Rule> rules;
	SRule_Checkin checkin;
	SRule_Checkin loaded;
	QSqlQuery query;
	QString first_id;

	QVERIFY(database.Open(Make_Database_Path()));
	Seed_Program(program_repository, "p1", EProgram_State::Draft);
	rules.append(Make_Rule("r1", "protein"));
	QVERIFY(repository.Replace_Rules("p1", rules));
	query = QSqlQuery(database.Sql_Connection());
	QVERIFY(repository.Save_Checkin(Make_Checkin("r1", "2026-09-29", 120, ECheckin_Source::Computed)));
	QVERIFY(repository.Find_Checkin("r1", "2026-09-29", loaded));
	QCOMPARE(loaded.Source, ECheckin_Source::Computed);
	QCOMPARE(loaded.Program_Id, QString("p1"));
	QVERIFY(loaded.Created_At.isEmpty() == false);
	first_id = loaded.ID;

	checkin = Make_Checkin("r1", "2026-09-29", 150, ECheckin_Source::Manual);
	checkin.State = ECheckin_State::Partial;
	checkin.Note = "Ужин ещё не закрыт";
	QVERIFY(repository.Save_Checkin(checkin));
	QVERIFY(repository.Find_Checkin("r1", "2026-09-29", loaded));
	QCOMPARE(loaded.ID, first_id);
	QCOMPARE(loaded.Source, ECheckin_Source::Manual);
	QCOMPARE(loaded.Value, 150.0);

	QVERIFY(repository.Save_Checkin(Make_Checkin("r1", "2026-09-29", 155, ECheckin_Source::Manual)));
	QVERIFY(repository.Find_Checkin("r1", "2026-09-29", loaded));
	QCOMPARE(loaded.Value, 155.0);
	QCOMPARE(loaded.ID, first_id);
	QVERIFY(query.exec("SELECT COUNT(*) FROM rule_checkins WHERE rule_id = 'r1' AND date = '2026-09-29'"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 1);

	QVERIFY(repository.Save_Checkin(Make_Checkin("r1", "2026-09-29", 90, ECheckin_Source::Computed)) == false);
	QVERIFY(repository.Get_Last_Error().contains("ручную"));
	QVERIFY(repository.Find_Checkin("r1", "2026-09-29", loaded));
	QCOMPARE(loaded.Value, 155.0);

	checkin = Make_Checkin("r1", "2026-09-30", 100, ECheckin_Source::Manual);
	checkin.Program_Id = "different-program";
	QVERIFY(repository.Save_Checkin(checkin) == false);
	QVERIFY(repository.Find_Checkin("r1", "2026-09-30", loaded) == false);
	QVERIFY(repository.Save_Checkin(Make_Checkin("r1", "2026-02-30", 100, ECheckin_Source::Manual)) == false);
	QVERIFY(repository.Save_Checkin(Make_Checkin("r1", QString(), 1, ECheckin_Source::Manual)) == false);
	checkin = Make_Checkin("r1", "2026-09-30", 1, ECheckin_Source::Manual);
	checkin.Unit.clear();
	QVERIFY(repository.Save_Checkin(checkin) == false);
	QVERIFY(repository.Save_Checkin(Make_Checkin("missing", "2026-09-30", 1, ECheckin_Source::Manual)) == false);

	AsSql_Database closed;
	ARule_Repository closed_repository(&closed);
	QVERIFY(closed_repository.Save_Checkin(Make_Checkin("r1", "2026-09-30", 1, ECheckin_Source::Manual)) == false);
	QVERIFY(closed_repository.Get_Last_Error().isEmpty() == false);
}
//----------------------------------------------------------------------------
void ARule_Repository_Test::checkinRangesUseProgramAndRule()
{
	AsSql_Database database;
	AProgram_Repository program_repository(&database);
	ARule_Repository repository(&database);
	QVector<SProgram_Rule> rules;
	QVector<SRule_Checkin> checkins;
	SRule_Checkin missing;

	QVERIFY(database.Open(Make_Database_Path()));
	Seed_Program(program_repository, "p1", EProgram_State::Draft);
	rules.append(Make_Rule("r1", "protein"));
	rules.append(Make_Rule("r2", "water"));
	QVERIFY(repository.Replace_Rules("p1", rules));
	QVERIFY(repository.Save_Checkin(Make_Checkin("r1", "2026-09-27", 140, ECheckin_Source::Manual)));
	QVERIFY(repository.Save_Checkin(Make_Checkin("r1", "2026-09-28", 150, ECheckin_Source::Manual)));
	QVERIFY(repository.Save_Checkin(Make_Checkin("r2", "2026-09-28", 2400, ECheckin_Source::Computed)));
	QVERIFY(repository.Load_Checkins_Range("r1", "2026-09-27", "2026-09-27", checkins));
	QCOMPARE(checkins.size(), 1);
	QCOMPARE(checkins.at(0).Date, QString("2026-09-27"));
	QVERIFY(repository.Load_Checkins_Range("r1", "2026-09-01", "2026-09-30", checkins));
	QCOMPARE(checkins.size(), 2);
	QCOMPARE(checkins.at(0).Date, QString("2026-09-27"));
	QCOMPARE(checkins.at(1).Date, QString("2026-09-28"));
	QVERIFY(repository.Load_Program_Checkins_Range("p1", "2026-09-01", "2026-09-30", checkins));
	QCOMPARE(checkins.size(), 3);
	QCOMPARE(checkins.at(0).Rule_Id, QString("r1"));
	QCOMPARE(checkins.at(1).Rule_Id, QString("r1"));
	QCOMPARE(checkins.at(2).Rule_Id, QString("r2"));
	QCOMPARE(checkins.at(2).Value, 2400.0);
	QCOMPARE(checkins.at(2).Source, ECheckin_Source::Computed);
	QVERIFY(repository.Load_Checkins_Range("r1", "2026-10-01", "2026-10-31", checkins));
	QVERIFY(checkins.isEmpty());
	QVERIFY(repository.Load_Program_Checkins_Range("missing", "2026-09-01", "2026-09-30", checkins));
	QVERIFY(checkins.isEmpty());
	QVERIFY(repository.Find_Checkin("r1", "2026-09-29", missing) == false);
	QVERIFY(repository.Get_Last_Error().isEmpty() == false);
}
//----------------------------------------------------------------------------
QTEST_MAIN(ARule_Repository_Test)

#include "tst_Rule_Repository.moc"
