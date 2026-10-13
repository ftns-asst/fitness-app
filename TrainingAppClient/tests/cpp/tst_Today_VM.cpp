#include "Data/Auth/Auth_Session.h"
#include "Data/Auth/Token_Store.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Network/Http_Client.h"
#include "Data/Program/Measurement_Repository.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Data/Program/Stats_Repository.h"
#include "Data/Program/Workout_Repository.h"
#include "Data/Users/Users_Repository.h"
#include "Presentation/Today_VM.h"

#include <QSignalSpy>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

//------------------------------------------------------------------------------
// Avm_Today: главный экран на данных программы (issue 27, §5.1-§5.2).
//
// Дата фиксируется через Set_Today, поэтому результат не зависит от часов: день
// недели вычисляется из даты, а слот ищется по нему (§3.4).
//------------------------------------------------------------------------------
class AToday_VM_Test : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void withoutActiveProgramIsEmptyState();
	void todayDayComesFromScheduleSlot();
	void dayKindFollowsDayPlanNotSlot();
	void restDayHasNoExercises();
	void rulesShowTodayCheckinState();
	void completedSessionMarksDayDone();
	void weekOfTermFollowsActualStart();
	// — Ввод значений (спринт S6, issue 51) —
	void measurementIsSavedOncePerDay();
	void measurementWithoutValueIsRejected();
	void measurementIsReadableWithoutProgram();
	void checkinUpdatesRuleStateOnce();
	void measurementInvalidatesStatsCache();
	void checkinInvalidatesStatsCache();
	// — Авторасчёт КБЖУ на главной (спринт S7, миграция v6) —
	void macroPlanAppearsOnToday();

private:
	// Программа на семь дней: понедельник и вторник — кардио, четверг и
	// воскресенье — отдых, остальные — силовая. У каждого дня свой слот.
	void Seed_Program(const QString &in_user_id, const QString &in_program_id);
	void Seed_Rules(const QString &in_program_id);
	void Seed_Session(const QString &in_id, const QString &in_date);
	// Строка списка по имени роли: тест не должен знать числовых значений ролей.
	QVariant Row_By_Role(const AList_Model *in_model, int in_row, const QString &in_role) const;

	QTemporaryDir Directory;
	AsSql_Database Database;
	// Профиль пользователя читается репозиторием с сессией: результат КБЖУ
	// лежит в profile_local рядом с параметрами расчёта (миграция v6).
	AsHttp_Client HTTP_Client = AsHttp_Client(QUrl());
	AsToken_Store Token_Store = AsToken_Store(&Database);
	AsAuth_Session Auth_Session = AsAuth_Session(&HTTP_Client, &Token_Store);
	AProgram_Repository *Program_Repository = 0;
	ARule_Repository *Rule_Repository = 0;
	AWorkout_Repository *Workout_Repository = 0;
	AMeasurement_Repository *Measurement_Repository = 0;
	AStats_Repository *Stats_Repository = 0;
	AUsers_Repository *Users_Repository = 0;
};
//------------------------------------------------------------------------------
void AToday_VM_Test::initTestCase()
{
	QVERIFY(Directory.isValid());
	QVERIFY(Database.Open(Directory.filePath("today_vm.sqlite3")));
	Program_Repository = new AProgram_Repository(&Database, this);
	Rule_Repository = new ARule_Repository(&Database, this);
	Workout_Repository = new AWorkout_Repository(&Database, this);
	Measurement_Repository = new AMeasurement_Repository(&Database, this);
	Stats_Repository = new AStats_Repository(&Database, this);
	Users_Repository = new AUsers_Repository(&Auth_Session, &Database, this);
}
//------------------------------------------------------------------------------
QVariant AToday_VM_Test::Row_By_Role(const AList_Model *in_model, int in_row, const QString &in_role) const
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
//------------------------------------------------------------------------------
void AToday_VM_Test::Seed_Program(const QString &in_user_id, const QString &in_program_id)
{
	SProgram program;
	QVector<SGoal_Target> targets;
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> day_exercises;
	QVector<SWeek_Slot> week_slots;

	// Посев идемпотентен: каждый тест готовит одну и ту же программу, а вставка
	// второй копии того же идентификатора отвергается базой.
	SProgram existing;

	if (Program_Repository->Load_Program(in_program_id, existing))
		return;

	program.ID = in_program_id;
	program.User_Id = in_user_id;
	program.Title = "Сила 4 недели";
	program.Source_Kind = EProgram_Source_Kind::Scratch;
	program.Goal = EProgram_Goal::Strength;
	program.Schedule_Kind = ESchedule_Kind::Weekday;
	program.State = EProgram_State::Draft;
	program.Revision = 1;
	program.Term.Start_Date = "2026-09-28";
	program.Term.Weeks_Total = 4;
	// Фактический старт задаётся явно: активация проставляет его по системной
	// дате, и без этого номер недели зависел бы от того, когда запущен тест.
	program.Actual_Start_Date = "2026-09-28";

	// Активная программа с измеримой целью обязана иметь цели (инвариант 8).
	SGoal_Target weight;

	weight.ID = in_program_id + "_g";
	weight.Program_Id = in_program_id;
	weight.Metric = EGoal_Metric::BodyWeightKg;
	weight.Unit = EGoal_Unit::Kg;
	weight.Start_Value = 82;
	weight.Has_Start_Value = true;
	weight.Target_Value = 78;
	weight.Direction = EGoal_Direction::Decrease;
	weight.State = EGoal_Target_State::Active;
	targets.append(weight);

	for (int day_of_week = 1; day_of_week <= 7; ++day_of_week)
	{
		SProgram_Day day;
		SWeek_Slot slot;
		const QString day_id = QString("%1_d%2").arg(in_program_id).arg(day_of_week);

		day.ID = day_id;
		day.Program_Id = in_program_id;
		day.Title = QString("День %1").arg(day_of_week);
		day.Order_Index = day_of_week;

		// Вид дня: 1 и 2 — кардио, 4 и 7 — отдых, остальные — силовая.
		if (day_of_week == 1 || day_of_week == 2)
			day.Kind = EDay_Kind::Cardio;
		else if (day_of_week == 4 || day_of_week == 7)
			day.Kind = EDay_Kind::Rest;
		else
			day.Kind = EDay_Kind::Workout;

		day.Estimated_Minutes = day.Kind == EDay_Kind::Rest ? 0 : 55;
		days.append(day);

		slot.ID = QString("%1_slot_%2").arg(in_program_id).arg(day_of_week);
		slot.Program_Id = in_program_id;
		slot.Day_Of_Week = day_of_week;
		// Слот отдыха по инварианту не ссылается на день плана (§4), а у всех
		// остальных вид слота намеренно «тренировка»: экран обязан брать вид у
		// дня плана, иначе кардио-день показывался бы силовой.
		slot.Kind = day.Kind == EDay_Kind::Rest ? ESlot_Kind::Rest : ESlot_Kind::Workout;
		slot.Program_Day_Id = day.Kind == EDay_Kind::Rest ? QString() : day_id;
		week_slots.append(slot);

		if (day.Kind == EDay_Kind::Rest)
			continue;

		for (int index = 0; index < 2; ++index)
		{
			SDay_Exercise exercise;

			exercise.ID = QString("%1_ex_%2").arg(day_id).arg(index + 1);
			exercise.Program_Day_Id = day_id;
			exercise.Exercise_Id = index == 0 ? "ex_squat" : "ex_bench";
			exercise.Order_Index = index;
			exercise.Sets = 4;
			exercise.Reps_Min = 5;
			exercise.Reps_Max = 8;
			exercise.Weight_Mode = EWeight_Mode::Fixed;
			exercise.Target_Weight_Kg = index == 0 ? 60 : 70;
			exercise.Has_Target_Weight_Kg = true;
			day_exercises.append(exercise);
		}
	}

	QVERIFY2(Program_Repository->Insert_Program_Full(program, targets, days, day_exercises, week_slots),
	         qPrintable(Program_Repository->Get_Last_Error()));
	QVERIFY(Program_Repository->Change_State(in_program_id, EProgram_State::Active));
}
//------------------------------------------------------------------------------
void AToday_VM_Test::Seed_Rules(const QString &in_program_id)
{
	QVector<SProgram_Rule> rules;
	SProgram_Rule protein;
	SProgram_Rule sleep;

	protein.ID = in_program_id + "_r_protein";
	protein.Program_Id = in_program_id;
	protein.Category = ERule_Category::Nutrition;
	protein.Code = "protein";
	protein.Title = "Белок 140 г";
	protein.Metric = ERule_Metric::ProteinGrams;
	protein.Periodicity = ERule_Periodicity::PerDay;
	protein.Target_Min = 140;
	protein.Has_Target_Min = true;
	protein.Target_Unit = ERule_Display_Unit::G;
	protein.Display_Unit = ERule_Display_Unit::G;
	protein.Applies_To = ERule_Applies_To::AllDays;
	protein.Check_Source = ERule_Check_Source::SelfReport;
	protein.Enforcement = ERule_Enforcement::Soft;

	sleep = protein;
	sleep.ID = in_program_id + "_r_sleep";
	sleep.Code = "sleep";
	sleep.Title = "Сон 7–9 часов";
	sleep.Category = ERule_Category::Recovery;
	sleep.Metric = ERule_Metric::SleepHours;
	sleep.Periodicity = ERule_Periodicity::PerDay;
	sleep.Target_Min = 7;
	sleep.Has_Target_Min = true;
	sleep.Target_Max = 9;
	sleep.Has_Target_Max = true;
	sleep.Target_Unit = ERule_Display_Unit::Hours;
	sleep.Display_Unit = ERule_Display_Unit::Hours;

	QVector<SProgram_Rule> existing_rules;

	// Посев идемпотентен: тесты делят одну базу, а повторная вставка тех же
	// правил отвергается первичным ключом.
	if (Rule_Repository->Load_Rules(in_program_id, existing_rules) && existing_rules.isEmpty() == false)
		return;

	rules.append(protein);
	rules.append(sleep);
	QVERIFY(Rule_Repository->Replace_Rules(in_program_id, rules));
}
//------------------------------------------------------------------------------
void AToday_VM_Test::Seed_Session(const QString &in_id, const QString &in_date)
{
	QSqlQuery query(Database.Sql_Connection());

	query.prepare("INSERT INTO workouts(id, user_id, program_id, date, started_at, completed_at, duration_seconds, "
	              "state, exercise_count, set_count, tonnage_kg, sync_state) "
	              "VALUES(?, 'u_today', 'p_today', ?, ?, ?, 3600, 'completed', 2, 8, 1200, 'local')");
	query.addBindValue(in_id);
	query.addBindValue(in_date);
	query.addBindValue(in_date + "T18:00:00Z");
	query.addBindValue(in_date + "T19:00:00Z");
	QVERIFY2(query.exec(), qPrintable(query.lastError().text()));
}
//------------------------------------------------------------------------------
// Без программы главный экран пуст, и это не ошибка: гость не должен видеть на
// главной сообщение об ошибке вместо приглашения создать программу (§5.5).
void AToday_VM_Test::withoutActiveProgramIsEmptyState()
{
	Avm_Today vm(Program_Repository, Rule_Repository, Workout_Repository, Measurement_Repository, Stats_Repository,
	             Users_Repository);

	vm.Set_Today("2026-10-05");
	QVERIFY(vm.Load("u_guest"));
	QVERIFY(vm.Has_Program() == false);
	QVERIFY(vm.errorKey().isEmpty());
	QCOMPARE(vm.programId(), QString());
	QCOMPARE(vm.exercises()->Row_Count(), 0);
	QCOMPARE(vm.rules()->Row_Count(), 0);
}
//------------------------------------------------------------------------------
void AToday_VM_Test::todayDayComesFromScheduleSlot()
{
	Avm_Today vm(Program_Repository, Rule_Repository, Workout_Repository, Measurement_Repository, Stats_Repository,
	             Users_Repository);

	Seed_Program("u_today", "p_today");
	// 2026-10-05 — понедельник, ISO 1 (§3.4).
	vm.Set_Today("2026-10-05");
	QVERIFY(vm.Load("u_today"));

	QVERIFY(vm.Has_Program());
	QCOMPARE(vm.programId(), QString("p_today"));
	QCOMPARE(vm.programTitle(), QString("Сила 4 недели"));
	QCOMPARE(vm.goal(), QString("strength"));
	QCOMPARE(vm.today(), QString("2026-10-05"));
	QCOMPARE(vm.weekday(), 1);
	QCOMPARE(vm.dayTitle(), QString("День 1"));
	QVERIFY(vm.Is_Rest_Day() == false);
	QCOMPARE(vm.estimated_minutes(), 55);
	QCOMPARE(vm.exercises()->Row_Count(), 2);

	// Название упражнения приходит из каталога: без него главная показывала бы
	// идентификатор вместо «Приседа».
	QCOMPARE(Row_By_Role(vm.exercises(), 0, "title").toString(), QString("Присед"));
	QCOMPARE(Row_By_Role(vm.exercises(), 1, "title").toString(), QString("Жим лёжа"));
}
//------------------------------------------------------------------------------
// Вид дня берётся у дня плана: слот может называть кардио-день тренировкой, и
// тогда главная показала бы «тренировка» вместо кардио.
void AToday_VM_Test::dayKindFollowsDayPlanNotSlot()
{
	Avm_Today vm(Program_Repository, Rule_Repository, Workout_Repository, Measurement_Repository, Stats_Repository,
	             Users_Repository);

	Seed_Program("u_today", "p_today");
	Seed_Rules("p_today");

	vm.Set_Today("2026-10-05");
	QVERIFY(vm.Load("u_today"));
	QCOMPARE(vm.dayKind(), int(EDay_Kind::Cardio));
}
//------------------------------------------------------------------------------
void AToday_VM_Test::restDayHasNoExercises()
{
	Avm_Today vm(Program_Repository, Rule_Repository, Workout_Repository, Measurement_Repository, Stats_Repository,
	             Users_Repository);

	Seed_Program("u_today", "p_today");

	// 2026-10-08 — четверг, ISO 4: в фикстуре это день отдыха.
	vm.Set_Today("2026-10-08");
	QVERIFY(vm.Load("u_today"));

	QCOMPARE(vm.weekday(), 4);
	QCOMPARE(vm.dayKind(), int(EDay_Kind::Rest));
	QVERIFY(vm.Is_Rest_Day());
	QCOMPARE(vm.estimated_minutes(), 0);
	QCOMPARE(vm.exercises()->Row_Count(), 0);
}
//------------------------------------------------------------------------------
void AToday_VM_Test::rulesShowTodayCheckinState()
{
	Avm_Today vm(Program_Repository, Rule_Repository, Workout_Repository, Measurement_Repository, Stats_Repository,
	             Users_Repository);
	SRule_Checkin checkin;

	Seed_Program("u_today", "p_today");
	Seed_Rules("p_today");

	vm.Set_Today("2026-10-05");
	QVERIFY(vm.Load("u_today"));
	QCOMPARE(vm.rules()->Row_Count(), 2);
	// Отметки за сегодня нет — «неизвестно», а не «выполнено» (§3.6).
	QCOMPARE(Row_By_Role(vm.rules(), 0, "todayState").toString(), QString("unknown"));

	checkin.Rule_Id = "p_today_r_protein";
	checkin.Program_Id = "p_today";
	checkin.Date = "2026-10-05";
	checkin.Value = 152;
	checkin.Has_Value = true;
	checkin.Unit = "g";
	checkin.State = ECheckin_State::Done;
	checkin.Source = ECheckin_Source::Manual;
	QVERIFY(Rule_Repository->Save_Checkin(checkin));

	vm.Set_Today("2026-10-05");
	QVERIFY(vm.Load("u_today"));
	QCOMPARE(Row_By_Role(vm.rules(), 0, "todayState").toString(), QString("done"));

	// Отметка за другой день состояния сегодняшнего не меняет.
	vm.Set_Today("2026-10-06");
	QVERIFY(vm.Load("u_today"));
	QCOMPARE(Row_By_Role(vm.rules(), 0, "todayState").toString(), QString("unknown"));
}
//------------------------------------------------------------------------------
void AToday_VM_Test::completedSessionMarksDayDone()
{
	Avm_Today vm(Program_Repository, Rule_Repository, Workout_Repository, Measurement_Repository, Stats_Repository,
	             Users_Repository);

	Seed_Program("u_today", "p_today");
	Seed_Session("w_today", "2026-10-05");
	vm.Set_Today("2026-10-05");
	QVERIFY(vm.Load("u_today"));
	QVERIFY(vm.Session_Completed());

	// Тренировка вчерашнего дня сегодняшним днём не считается.
	vm.Set_Today("2026-10-06");
	QVERIFY(vm.Load("u_today"));
	QVERIFY(vm.Session_Completed() == false);
}
//------------------------------------------------------------------------------

// Неделя срока считается от фактического старта, а не от даты создания:
// программа может стартовать с задержкой, и тогда неделя «не соответствует».
void AToday_VM_Test::weekOfTermFollowsActualStart()
{
	Avm_Today vm(Program_Repository, Rule_Repository, Workout_Repository, Measurement_Repository, Stats_Repository,
	             Users_Repository);

	Seed_Program("u_today", "p_today");
	// Старт программы в фикстуре — 2026-09-28, то есть неделя 1 длится до
	// 2026-10-04 включительно.
	vm.Set_Today("2026-10-04");
	QVERIFY(vm.Load("u_today"));
	QCOMPARE(vm.weeks_total(), 4);
	QCOMPARE(vm.Current_Week(), 1);

	vm.Set_Today("2026-10-05");
	QVERIFY(vm.Load("u_today"));
	QCOMPARE(vm.Current_Week(), 2);

	// День до старта — неделя 0: показывать первую неделю программы, которой ещё
	// не было, значило бы врать.
	vm.Set_Today("2026-09-27");
	QVERIFY(vm.Load("u_today"));
	QCOMPARE(vm.Current_Week(), 0);

	// Вне срока номер не превышает последнюю неделю: программа на четыре недели не
	// попадает на «нед. 5».
	vm.Set_Today("2027-01-11");
	QVERIFY(vm.Load("u_today"));
	QCOMPARE(vm.Current_Week(), 4);
}
//------------------------------------------------------------------------------
// Ввод значений (спринт S6, issue 51)
//------------------------------------------------------------------------------
void AToday_VM_Test::measurementIsSavedOncePerDay()
{
	// Свой пользователь на каждый тест: база общая, и замер, оставленный
	// соседним тестом на ту же дату, сделал бы проверку неоднозначной —
	// «замер сохранился один раз» не отличалось бы от «запись уже была, и
	// новый ввод её не создал».
	Avm_Today vm(Program_Repository, Rule_Repository, Workout_Repository, Measurement_Repository, Stats_Repository,
	             Users_Repository);
	QVector<SBody_Measurement> measurements;
	SBody_Measurement stored;

	vm.Set_Today("2026-10-05");
	QVERIFY(vm.Load("u_meas_once"));

	QVERIFY(vm.saveMeasurement(82.5, true, 104, true, 86, true, 98, true, 37.5, true, 58, true));
	QVERIFY(vm.Has_Measurement());
	QCOMPARE(vm.weightKg(), 82.5);

	// Второй ввод за тот же день обновляет запись, а не создаёт вторую: пара
	// (user, date) уникальна, и без слияния форма наслаивала бы дубли.
	QVERIFY(vm.saveMeasurement(82.3, true, -1, false, -1, false, -1, false, -1, false, -1, false));
	QCOMPARE(vm.weightKg(), 82.3);
	// Обхваты утренней записи пережили вечерний ввод только веса.
	QCOMPARE(vm.chestCm(), 104.0);

	QVERIFY(Measurement_Repository->Load_Range("u_meas_once", "2026-10-05", "2026-10-05", measurements));
	QCOMPARE(measurements.size(), 1);
	// Запись читается обратно из базы, а не остаётся во временном состоянии
	// ViewModel: после перезапуска приложения форма должна увидеть те же числа.
	QVERIFY(Measurement_Repository->Find_Measurement("u_meas_once", "2026-10-05", stored));
	QCOMPARE(stored.Weight_Kg, 82.3);
	QCOMPARE(stored.Chest_Cm, 104.0);
}
//------------------------------------------------------------------------------
void AToday_VM_Test::measurementWithoutValueIsRejected()
{
	Avm_Today vm(Program_Repository, Rule_Repository, Workout_Repository, Measurement_Repository, Stats_Repository,
	             Users_Repository);

	vm.Set_Today("2026-10-06");
	QVERIFY(vm.Load("u_meas_empty"));

	// Пустая форма — не ошибка ввода и не запись: показывать пользователю
	// сообщение об ошибке после нажатия «Сохранить» без единого числа значило бы
	// ругаться на то, что он и не вводил.
	QVERIFY(vm.saveMeasurement(-1, false, -1, false, -1, false, -1, false, -1, false, -1, false) == false);
	QVERIFY(vm.Has_Measurement() == false);
}
//------------------------------------------------------------------------------
void AToday_VM_Test::measurementIsReadableWithoutProgram()
{
	Avm_Today vm(Program_Repository, Rule_Repository, Workout_Repository, Measurement_Repository, Stats_Repository,
	             Users_Repository);

	vm.Set_Today("2026-10-05");

	// У пользователя без активной программы замер остаётся осмысленным
	// (таблица замеров от программы не зависит, инвариант 20 §4).
	QVERIFY(vm.Load("u_solo"));
	QVERIFY(vm.Has_Program() == false);
	QVERIFY(vm.saveMeasurement(70.0, true, -1, false, -1, false, -1, false, -1, false, -1, false));
	QVERIFY(vm.Has_Measurement());
	QCOMPARE(vm.weightKg(), 70.0);
}
//------------------------------------------------------------------------------
void AToday_VM_Test::checkinUpdatesRuleStateOnce()
{
	Avm_Today vm(Program_Repository, Rule_Repository, Workout_Repository, Measurement_Repository, Stats_Repository,
	             Users_Repository);
	QVector<SRule_Checkin> checkins;

	Seed_Program("u_checkin", "p_checkin");
	Seed_Rules("p_checkin");

	vm.Set_Today("2026-10-06");
	QVERIFY(vm.Load("u_checkin"));
	QCOMPARE(Row_By_Role(vm.rules(), 0, "todayState").toString(), QString("unknown"));

	QVERIFY(vm.saveCheckin("p_checkin_r_protein", "done", -1, false));
	// Состояние пришло на экран сразу: точка и процент не должны ждать
	// перезагрузки вкладки.
	QCOMPARE(Row_By_Role(vm.rules(), 0, "todayState").toString(), QString("done"));

	QVERIFY(vm.saveCheckin("p_checkin_r_protein", "done", -1, false));
	// Повторная отметка тем же состоянием не создаёт второй строки за день.
	QVERIFY(Rule_Repository->Load_Program_Checkins_Range("p_checkin", "2026-10-06", "2026-10-06", checkins));
	QCOMPARE(checkins.size(), 1);
	// Единица берётся у правила, а не из заглушки «г»: у белка это граммы,
	// у шагов их не было бы вовсе.
	QCOMPARE(checkins.at(0).Unit, QString("g"));
}
//------------------------------------------------------------------------------
void AToday_VM_Test::measurementInvalidatesStatsCache()
{
	Avm_Today vm(Program_Repository, Rule_Repository, Workout_Repository, Measurement_Repository, Stats_Repository,
	             Users_Repository);
	SStats_Cache_Entry entry;
	bool fresh = false;

	Seed_Program("u_meas_cache", "p_meas_cache");
	QVERIFY(Stats_Repository->Save_Program_Stats("p_meas_cache", 1, "{}"));
	QVERIFY(Stats_Repository->Save_Lifetime_Stats("u_meas_cache", "{}"));
	QVERIFY(Stats_Repository->Load_Program_Stats("p_meas_cache", entry));
	QVERIFY(Stats_Repository->Is_Program_Stats_Fresh("p_meas_cache", 1, fresh) && fresh);

	vm.Set_Today("2026-10-05");
	QVERIFY(vm.Load("u_meas_cache"));
	QVERIFY(vm.saveMeasurement(82.5, true, -1, false, -1, false, -1, false, -1, false, -1, false));

	// Новый замер делает прежние числа статистики недействительными (§5.4):
	// экран статистики не должен продолжать показывать вес, которого нет.
	QVERIFY(Stats_Repository->Load_Program_Stats("p_meas_cache", entry) == false);
	QVERIFY(Stats_Repository->Load_Lifetime_Stats("u_meas_cache", entry) == false);
}
//------------------------------------------------------------------------------
void AToday_VM_Test::checkinInvalidatesStatsCache()
{
	Avm_Today vm(Program_Repository, Rule_Repository, Workout_Repository, Measurement_Repository, Stats_Repository,
	             Users_Repository);
	SStats_Cache_Entry entry;

	Seed_Program("u_checkin_cache", "p_checkin_cache");
	Seed_Rules("p_checkin_cache");
	QVERIFY(Stats_Repository->Save_Lifetime_Stats("u_checkin_cache", "{}"));

	vm.Set_Today("2026-10-05");
	QVERIFY(vm.Load("u_checkin_cache"));
	QVERIFY(vm.saveCheckin("p_checkin_cache_r_protein", "done", -1, false));

	// Процент выполнения входит в кэш, поэтому отметка инвалидирует его так же,
	// как запись факта: иначе «выполнено» появилось бы на главной, а процент на
	// экране статистики остался бы прежним.
	QVERIFY(Stats_Repository->Load_Lifetime_Stats("u_checkin_cache", entry) == false);
}
//------------------------------------------------------------------------------

//------------------------------------------------------------------------------
// Результат авторасчёта КБЖУ показывается на главной (спринт S7, миграция v6):
// числа читаются из profile_local, а не пересчитываются экраном. Расчёт не
// зависит от программы, поэтому проверка идёт без активной программы — так же,
// как у гостя, который считает КБЖУ до создания программы.
//------------------------------------------------------------------------------
void AToday_VM_Test::macroPlanAppearsOnToday()
{
	Avm_Today vm(Program_Repository, Rule_Repository, Workout_Repository, Measurement_Repository, Stats_Repository,
	             Users_Repository);
	AsMacro_Calculator::SResult result;
	const QString user_id = "macro-user";

	result.Bmr_Kcal = 1700;
	result.Tdee_Kcal = 2380;
	result.Calories_Kcal = 2023;
	result.Protein_G = 132;
	result.Fat_G = 56;
	result.Carbs_G = 210;

	// До расчёта карточка пустая: нули выглядели бы как честный результат.
	QVERIFY(vm.Load(user_id));
	QVERIFY(vm.Has_Macro_Plan() == false);
	QCOMPARE(vm.macro_calories_kcal(), 0.0);

	QVERIFY(Users_Repository->Save_Macro_Result(user_id, result));
	QVERIFY(vm.Load(user_id));

	QVERIFY(vm.Has_Macro_Plan());
	QCOMPARE(vm.macro_calories_kcal(), 2023.0);
	QCOMPARE(vm.macro_protein_g(), 132.0);
	QCOMPARE(vm.macro_fat_g(), 56.0);
	QCOMPARE(vm.macro_carbs_g(), 210.0);
	QVERIFY(vm.macro_calculated_at().isEmpty() == false);
}
//------------------------------------------------------------------------------
QTEST_GUILESS_MAIN(AToday_VM_Test)
#include "tst_Today_VM.moc"
