#include "Demo/Demo_Seeder.h"

#include "Data/Program/Measurement_Repository.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Data/Program/Workout_Repository.h"
#include "Domain/Rule_Engine.h"

#include <QDate>
#include <QLoggingCategory>
#include <QUuid>

Q_LOGGING_CATEGORY(DEMO_Seeder_Log, "trainingapp.demo.seeder")

namespace
{
// Каталог упражнений сидится в AsSql_Database::Seed_Builtin_Exercises, поэтому
// сидер берёт те же четыре id: свои он вставил бы второй раз, и предписания
// ссылались бы на упражнения, которых нет ни в одном каталоге.
// Содержит ли is_in — своя обёртка, потому что QList::contains в Qt 6
// обобщён и в QStringList, и в QVector: одна функция на оба случая.
bool Is_In(const int *in_values, int in_count, int in_value)
{
	for (int i = 0; i < in_count; ++i)
	{
		if (in_values[i] == in_value)
			return true;
	}

	return false;
}

const char *const Demo_Exercise_Ids[] = {"ex_squat", "ex_bench", "ex_row", "ex_deadlift"};

// Дни недели с упражнениями: 4 тренировки и 3 дня отдыха, как в макете
// «Мои программы». Понедельник — силовая, средина — кардио, пятница — силовая,
// суббота — силовая; вторник, четверг, воскресенье — отдых.
const int Workout_Days[] = {1, 3, 5, 6};
const int Workout_Day_Count = 4;
const int Cardio_Day = 2;
const int Rest_Day = 7;

const int Demo_Week_Count = 8;

// Глубина демо-истории в днях. Три недели при сроке в восемь недель: «сегодня»
// попадает внутрь программы, а график статистики уже не пуст.
const int Demo_History_Days = 20;

// Окно отметок правил совпадает с окном экрана по умолчанию (неделя, §5.1):
// иначе посеянные отметки оказались бы вне окна и процент остался бы «—».
const int Demo_Checkin_Days = 6;

// День окна, значение которого не попадает в цель. Без него экран показывает
// только «выполнено», и провальный путь отметки не проверить: процент равнялся
// бы 100 % независимо от того, умеет ли экран показывать «не выполнено».
const int Demo_Checkin_Miss_Offset = 6;

// Вес тела на начало демо-истории и шаг за неделю: минус означает снижение,
// и метрика «Тело, кг» показывает дельту со знаком. Недельный шаг 0,3 кг даёт
// −0,9 кг за три недели — правдоподобно и заметно на экране.
const double Demo_Start_Weight_Kg = 82.6;
const double Demo_Weekly_Weight_Step_Kg = -0.3;

// Обхваты для метрики «Тело, кг»: без них показывается только вес, а карточка
// изменения замеров остаётся пустой.
const double Demo_Chest_Cm = 104.0;
const double Demo_Waist_Cm = 86.0;
const double Demo_Hip_Cm = 98.0;
const double Demo_Arm_Cm = 37.5;
const double Demo_Thigh_Cm = 58.0;

// Признак демо-программы и её заголовок. Заголовок проверяется при повторном
// посеве, поэтому он же служит признаком: данные демо узнаваемы в базе.
const char *const Demo_Program_Title = "Сила 8 недель";
const char *const Demo_Program_Note = "Демо-программа: наполнение для просмотра экранов";
const char *const Demo_Structure_Summary = "Посев демо-данных: неделя, упражнения и расписание";

// Правила демо-программы: два измеримых (у них есть цель и они попадают в
// adherence) и одна рекомендация текстом (цели нет, в процент не входит).
struct SDemo_Rule
{
	const char *Code;
	const char *Title;
	ERule_Category Category;
	ERule_Metric Metric;
	ERule_Periodicity Periodicity;
	ERule_Display_Unit Unit;
	double Target_Min;
	double Target_Max;
	bool Has_Min;
	bool Has_Max;
	const char *Note;
	// Значение отметки за обычный день и за день промаха. Посев отметок идёт по
	// этим числам, а состояние оставляет движок правил: он один знает, выполнена
	// цель или нет, и посев не должен дублировать это решение.
	double Checkin_Value;
	double Checkin_Miss_Value;
};

const SDemo_Rule Demo_Rules[] = {
    {"protein", "Белок 140 г", ERule_Category::Nutrition, ERule_Metric::ProteinGrams, ERule_Periodicity::PerDay,
     ERule_Display_Unit::G, 140, -1, true, false, "Каждый день", 152, 90},
    {"sleep", "Сон 7–9 часов", ERule_Category::Recovery, ERule_Metric::SleepHours, ERule_Periodicity::PerDay,
     ERule_Display_Unit::Hours, 7, 9, true, true, "Диапазон, а не точка", 8, 5},
    {"noAlcohol", "Без алкоголя", ERule_Category::Other, ERule_Metric::Custom, ERule_Periodicity::PerWeek,
     ERule_Display_Unit::Units, -1, -1, false, false, "Рекомендация текстом: цели нет, в процент выполнения не входит",
     -1, -1},
};
} // namespace

//----------------------------------------------------------------------------
ADemo_Seeder::ADemo_Seeder(QObject *in_parent) : QObject(in_parent), Error_State(&DEMO_Seeder_Log())
{
}
//----------------------------------------------------------------------------
QString ADemo_Seeder::Get_Last_Error() const
{
	return Error_State.Get_Last_Error();
}
//----------------------------------------------------------------------------
QString ADemo_Seeder::Program_Id() const
{
	return Program_Id_Text;
}
//----------------------------------------------------------------------------
bool ADemo_Seeder::Set_Last_Error(const QString &in_error)
{
	return Error_State.Set_Last_Error(in_error);
}
//----------------------------------------------------------------------------
void ADemo_Seeder::Clear_Last_Error()
{
	Error_State.Clear_Last_Error();
}
//----------------------------------------------------------------------------
bool ADemo_Seeder::Seed(AProgram_Repository *in_programs, ARule_Repository *in_rules, const QString &in_user_id,
                        AWorkout_Repository *in_workouts, AMeasurement_Repository *in_measurements)
{
	Clear_Last_Error();

	if (in_programs == 0 || in_rules == 0 || in_workouts == 0 || in_measurements == 0)
		return Set_Last_Error("Сидеру демо-данных нужны репозитории программ, правил, тренировок и замеров");

	if (in_user_id.trimmed().isEmpty())
		return Set_Last_Error("Сидеру демо-данных нужен идентификатор пользователя");

	// Повторный посев — не ошибка: приложение стартует многократно, и каждый
	// запуск не должен добавлять «Моя программа» в каталог. По той же
	// причине посев не выполняется поверх уже существующей программы.
	if (Has_Any_Program(in_programs, in_user_id))
		return true;

	if (Seed_Program(in_programs, in_user_id) == false || Seed_Structure(in_programs) == false ||
	    Seed_Rules(in_rules) == false || Seed_Workouts(in_workouts, in_user_id) == false ||
	    Seed_Checkins(in_rules) == false || Seed_Measurements(in_measurements, in_user_id) == false)
		return false;

	return true;
}
//----------------------------------------------------------------------------
// Есть ли у пользователя хоть одна программа. Проверяется любая программа, а не
// только демо: индекс programs_one_active_per_user допускает одну активную
// программу на пользователя, и посев поверх уже созданной упал бы с UNIQUE.
// Наполнять чужую программу демо-упражнениями всё равно нельзя, поэтому при её
// наличии посев пропускается.
bool ADemo_Seeder::Has_Any_Program(AProgram_Repository *in_programs, const QString &in_user_id)
{
	QVector<SProgram> programs;

	// Отказ трактуем как «данные есть»: посеять поверх неизвестного состояния
	// хуже, чем не посеять вовсе.
	if (in_programs->Load_Programs(in_user_id, programs) == false)
		return true;

	if (programs.isEmpty() == false)
	{
		// Идентификатор запоминается: вызывающий должен знать, какую программу
		// открывать, даже если посев был пропущен.
		Program_Id_Text = programs.at(0).ID;

		qCInfo(DEMO_Seeder_Log) << "у пользователя уже есть программа" << Program_Id_Text << programs.at(0).Title
		                        << ", посев пропущен";

		return true;
	}

	return false;
}
//----------------------------------------------------------------------------
bool ADemo_Seeder::Seed_Program(AProgram_Repository *in_programs, const QString &in_user_id)
{
	SProgram program;

	program.ID = QUuid::createUuid().toString(QUuid::WithoutBraces);
	program.User_Id = in_user_id;
	program.Title = QString::fromUtf8(Demo_Program_Title);
	program.Source_Kind = EProgram_Source_Kind::Scratch;
	program.Goal = EProgram_Goal::Strength;
	program.Level = EProgram_Level::Intermediate;
	program.Schedule_Kind = ESchedule_Kind::Weekday;
	// Старт ретроспективно, чтобы «сегодня» попадал внутрь срока: программа,
	// начавшаяся в будущем, показывала бы неделю без прошедших тренировок и
	// статистика выглядела бы пустой.
	program.Term.Start_Date = Recent_Date(21);
	program.Term.Weeks_Total = Demo_Week_Count;
	program.Term.Mode = ETerm_Mode::FixedWeeks;
	program.Actual_Start_Date = program.Term.Start_Date;
	program.Note = QString::fromUtf8(Demo_Program_Note);

	if (in_programs->Insert_Program_Full(program, QVector<SGoal_Target>(), QVector<SProgram_Day>(),
	                                     QVector<SDay_Exercise>(), QVector<SWeek_Slot>()) == false)
		return Set_Last_Error(in_programs->Get_Last_Error());

	// Программа активируется после вставки структуры: Create_From_Scratch так же
	// требует draft на входе, а Activate проверяет наличие дней.
	Program_Id_Text = program.ID;

	return true;
}
//----------------------------------------------------------------------------
bool ADemo_Seeder::Seed_Structure(AProgram_Repository *in_programs)
{
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> exercises;
	QVector<SWeek_Slot> week_slots;
	const QString prefix = Program_Id_Text + "_";

	// Семь дней недели. Дни тренировок получают упражнения, дни отдыха — нет:
	// пустой день отдыха это норма, а не недостаток данных.
	for (int weekday = 1; weekday <= 7; ++weekday)
	{
		SProgram_Day day;

		day.ID = prefix + QString("day_%1").arg(weekday);
		day.Program_Id = Program_Id_Text;
		day.Order_Index = weekday;
		day.Estimated_Minutes = 55;

		if (Is_In(Workout_Days, Workout_Day_Count, weekday))
		{
			day.Kind = EDay_Kind::Workout;
			day.Title = QString("Силовая %1").arg(weekday);
			day.Focus << "legs";
		}
		else if (weekday == Cardio_Day)
		{
			day.Kind = EDay_Kind::Cardio;
			day.Title = QString("Кардио");
			day.Focus << "fullBody";
			day.Estimated_Minutes = 35;
		}
		else
		{
			day.Kind = EDay_Kind::Rest;
			day.Title = QString("Отдых");
			day.Estimated_Minutes = 0;
		}

		days.append(day);

		SWeek_Slot slot;

		slot.ID = prefix + QString("slot_%1").arg(weekday);
		slot.Program_Id = Program_Id_Text;
		slot.Day_Of_Week = weekday;
		slot.Kind = day.Kind == EDay_Kind::Rest ? ESlot_Kind::Rest : ESlot_Kind::Workout;

		// Слот отдыха не ссылается на день плана: отдых — это отсутствие
		// нагрузки, а не день с пустым списком упражнений. Репозиторий такое
		// отвергает, и правильно: иначе «отдых» стал бы днём с программой.
		if (slot.Kind == ESlot_Kind::Rest)
		{
			week_slots.append(slot);

			continue;
		}

		slot.Program_Day_Id = day.ID;
		week_slots.append(slot);

		// У кардио и силовой разные упражнения: иначе все дни выглядели бы
		// одинаково и экран дня не показывал бы, чем день отличается.
		if (day.Kind == EDay_Kind::Rest)
			continue;

		const int exercise_count = day.Kind == EDay_Kind::Cardio ? 2 : 3;

		for (int index = 0; index < exercise_count; ++index)
		{
			SDay_Exercise exercise;

			exercise.ID = day.ID + QString("_ex_%1").arg(index + 1);
			exercise.Program_Day_Id = day.ID;
			exercise.Exercise_Id = Demo_Exercise_Ids[index % 4];
			exercise.Order_Index = index + 1;
			exercise.Sets = day.Kind == EDay_Kind::Cardio ? 1 : 4;
			exercise.Reps_Min = day.Kind == EDay_Kind::Cardio ? 20 : 5;
			exercise.Reps_Max = day.Kind == EDay_Kind::Cardio ? 25 : 8;
			exercise.Weight_Mode = EWeight_Mode::Fixed;
			exercise.Target_Weight_Kg = day.Kind == EDay_Kind::Cardio ? -1 : 60 + index * 10;
			exercise.Has_Target_Weight_Kg = day.Kind != EDay_Kind::Cardio;
			exercise.Rest_Seconds = 120;
			exercises.append(exercise);
		}
	}

	if (in_programs->Replace_Structure(Program_Id_Text, days, exercises, week_slots, ERevision_Reason::Days,
	                                   QString::fromUtf8(Demo_Structure_Summary)) == false)
		return Set_Last_Error(in_programs->Get_Last_Error());

	if (in_programs->Change_State(Program_Id_Text, EProgram_State::Active) == false)
		return Set_Last_Error(in_programs->Get_Last_Error());

	return true;
}
//----------------------------------------------------------------------------
// Правила пишутся через ARule_Repository::Replace_Rules — тот же путь, что и
// у редактора. Прямой вставкой сидер мог бы записать то, чего редактор не
// создаст, и расхождение осталось бы незамеченным до первого запуска экрана.
bool ADemo_Seeder::Seed_Rules(ARule_Repository *in_rules)
{
	QVector<SProgram_Rule> rules;
	const int total = int(sizeof(Demo_Rules) / sizeof(Demo_Rules[0]));

	for (int i = 0; i < total; ++i)
	{
		const SDemo_Rule &preset = Demo_Rules[i];
		SProgram_Rule rule;

		rule.ID = Program_Id_Text + QString("_rule_%1").arg(i + 1);
		rule.Program_Id = Program_Id_Text;
		rule.Code = QString::fromLatin1(preset.Code);
		rule.Title = QString::fromUtf8(preset.Title);
		rule.Category = preset.Category;
		rule.Metric = preset.Metric;
		rule.Periodicity = preset.Periodicity;
		rule.Target_Min = preset.Target_Min;
		rule.Has_Target_Min = preset.Has_Min;
		rule.Target_Max = preset.Target_Max;
		rule.Has_Target_Max = preset.Has_Max;
		rule.Target_Unit = preset.Unit;
		rule.Display_Unit = preset.Unit;
		rule.Applies_To = ERule_Applies_To::AllDays;
		rule.Check_Source = ERule_Check_Source::SelfReport;
		rule.Enforcement = ERule_Enforcement::Soft;
		rule.Note = QString::fromUtf8(preset.Note);
		rules.append(rule);
	}

	if (in_rules->Replace_Rules(Program_Id_Text, rules) == false)
		return Set_Last_Error(in_rules->Get_Last_Error());

	return true;
}
//----------------------------------------------------------------------------
// Дата демо-тренировки: отсчёт от сегодняшней, а не фиксированная строка.
// Фиксированная дата через несколько недель оказалась бы в прошлом, и весь
// набор тренировок — тоже: статистика показывала бы пустоту.
QString ADemo_Seeder::Recent_Date(int in_days_ago)
{
	return QDate::currentDate().addDays(-in_days_ago).toString("yyyy-MM-dd");
}
//----------------------------------------------------------------------------
QString ADemo_Seeder::Recent_Stamp(const QString &in_date, int in_hour)
{
	return in_date + QString("T%1:00:00Z").arg(in_hour, 2, 10, QChar('0'));
}
//----------------------------------------------------------------------------
// Завершённые тренировки нужны экрану статистики: без них он показывает
// «Пока нет данных» и половина экрана остаётся непроверяемой. Подходы пишутся
// вместе с сессиями — без них тоннаж и подходы остались бы нулевыми.
bool ADemo_Seeder::Seed_Workouts(AWorkout_Repository *in_workouts, const QString &in_user_id)
{
	if (in_workouts == 0)
		return Set_Last_Error("Сидеру демо-данных нужен репозиторий тренировок");

	// Одна тренировка каждые два дня за последние три недели: получается
	// процент выполнения около 80 %, а не 0 или 100 — оба края не проверяют
	// округление и деление.
	// Обход каждого дня, а не выборка через день: иначе за месяц попадало
	// считанное число рабочих дней, и график тоннажа оставался почти пустым.
	//
	// Пропуск выбирается по номеру дня, а не случайно: данные должны быть
	// воспроизводимы между запусками, иначе дефекты статистики нельзя
	// отличить от разброса.
	for (int offset = Demo_History_Days; offset >= 0; --offset)
	{
		const QString date = Recent_Date(offset);
		const int weekday = QDate::fromString(date, Qt::ISODate).dayOfWeek();

		// Тренировка ставится только в день, который по расписанию рабочий,
		// и часть из них пропускается: процент выполнения получается около
		// 75 %, а не 0 или 100 — оба края не проверяют округление и деление.
		const bool scheduled = Is_In(Workout_Days, Workout_Day_Count, weekday);
		const bool planned = scheduled && ((offset % 4) != 0);

		SWorkout_Session session;

		session.ID = Program_Id_Text + QString("_wo_%1").arg(offset);
		session.User_Id = in_user_id;
		session.Program_Id = Program_Id_Text;
		session.Planned_Date = date;
		session.Planned_Week_Index = 1;
		session.Date = date;
		session.Started_At = Recent_Stamp(date, 18);
		session.Completed_At = Recent_Stamp(date, 19);
		session.Duration_Seconds = 3300;
		session.Exercise_Count = 3;
		session.Set_Count = 12;
		session.Tonnage_Kg = 4800;
		session.Avg_Rpe = 7.5;
		session.Has_Avg_Rpe = true;
		session.Program_Revision = 1;
		session.Sync_State = ESync_State::Local;

		// Пропущенный день отмечается как отменённая сессия, а не как
		// завершённая: иначе процент выполнения не отличал бы «не делал» от
		// «делал».
		if (planned == false)
		{
			session.State = EWorkout_State::Cancelled;
			session.Deviation = EDeviation::MovedDay;
			session.Has_Deviation = true;
			session.Completed_At.clear();
			session.Tonnage_Kg = 0;
			session.Avg_Rpe = -1;
			session.Has_Avg_Rpe = false;
		}
		else
		{
			session.State = EWorkout_State::Completed;
		}

		if (in_workouts->Insert_Session(session) == false)
			return Set_Last_Error(in_workouts->Get_Last_Error());

		// Внеплановых тренировок посев не делает: иначе adherence считал бы
		// лишние дни, а экран показывал бы их как часть программы.
		if (scheduled == false)
			continue;

		// Подходы силовой плитки: тоннаж и подходы на экране статистики
		// считаются по ним, без них оставались бы нули.
		QVector<SSet_Log> sets;

		for (int set_index = 1; set_index <= 4; ++set_index)
		{
			SSet_Log set;

			set.ID = session.ID + QString("_set_%1").arg(set_index);
			set.Session_Id = session.ID;
			set.Exercise_Id = QString::fromLatin1(Demo_Exercise_Ids[0]);
			set.Set_Number = set_index;
			set.Weight_Kg = 60 + set_index * 5;
			set.Reps = 5;
			set.Rpe = 7 + set_index * 0.5;
			set.Has_Rpe = true;
			set.Done = true;
			sets.append(set);
		}

		for (int i = 0; i < sets.size(); ++i)
		{
			if (in_workouts->Save_Set(sets.at(i)) == false)
				return Set_Last_Error(in_workouts->Get_Last_Error());
		}
	}

	return true;
}
//----------------------------------------------------------------------------
// Отметки правил за неделю. Без них экран «Правила» показывает процент «—» и
// подпись «Отметок за это окно нет»: обычное состояние экрана оказывается
// непроверяемым.
//
// Состояние отметки не выводится здесь руками: вердикт `done`/`failed` по
// значению считает движок правил (Evaluate_Value), и посев лишь передаёт ему
// то же значение, что показано на экране. Пока состояние оставалось
// «неизвестно», заполненная отметка выглядела как «Нет отметки», а процент
// выполнения отсутствовал: в метрику входят только вердикты (§5.5).
//
// Окно недели совпадает с окном экрана по умолчанию (periodDays = 7, §5.1),
// поэтому отметки попадают в выборку экрана, а не остаются за её пределами.
bool ADemo_Seeder::Seed_Checkins(ARule_Repository *in_rules)
{
	QVector<SProgram_Rule> rules;
	const int total = int(sizeof(Demo_Rules) / sizeof(Demo_Rules[0]));
	AsRule_Engine engine;

	if (in_rules->Load_Rules(Program_Id_Text, rules) == false)
		return Set_Last_Error(in_rules->Get_Last_Error());

	for (int rule_index = 0; rule_index < rules.size(); ++rule_index)
	{
		const SProgram_Rule &rule = rules.at(rule_index);
		double value = -1;
		double miss_value = -1;

		// Рекомендация текстом не отмечается: у неё нечего измерить, и её
		// отметка не должна попадать в процент выполнения (§3.6).
		if (Get_Rule_Kind(rule) == ERule_Kind::Advice)
			continue;

		// Значение для правила берётся из того же пресета, которым заведено
		// само правило: рассинхрон между ними означал бы, что экран показывает
		// выполнение цели, которой в правиле нет.
		for (int i = 0; i < total; ++i)
		{
			if (rule.Code == QString::fromLatin1(Demo_Rules[i].Code))
			{
				value = Demo_Rules[i].Checkin_Value;
				miss_value = Demo_Rules[i].Checkin_Miss_Value;
			}
		}

		for (int offset = Demo_Checkin_Days; offset >= 0; --offset)
		{
			SRule_Checkin checkin;
			const double checkin_value = (offset == Demo_Checkin_Miss_Offset) ? miss_value : value;
			ECheckin_State state = ECheckin_State::Unknown;

			checkin.Rule_Id = rule.ID;
			checkin.Program_Id = Program_Id_Text;
			checkin.Date = Recent_Date(offset);
			checkin.Unit = To_String(rule.Display_Unit);
			// Один день из шести остаётся без отметки: «нет данных» должно
			// отличаться от «не выполнено» (§5.5), иначе состояние отметки
			// никогда не показывается.
			if (offset == 3)
				continue;

			if (checkin_value >= 0 && engine.Evaluate_Value(rule, checkin_value, state) == false)
				return Set_Last_Error(engine.Get_Last_Error());

			// Правило без значения в пресете отмечается «выполнено»: измерить
			// его нечем, а пустая отметка не показала бы ничего.
			checkin.State = (checkin_value >= 0) ? state : ECheckin_State::Done;
			checkin.Source = ECheckin_Source::Manual;
			checkin.Value = checkin_value;
			checkin.Has_Value = checkin_value >= 0;

			if (in_rules->Save_Checkin(checkin) == false)
				return Set_Last_Error(in_rules->Get_Last_Error());
		}
	}

	return true;
}
//----------------------------------------------------------------------------
// Замеры тела: вес раз в неделю плюс обхваты в последний день. Без них метрика
// «Тело, кг» остаётся прочерком, а блок изменения замеров пуст.
//
// Вес задан линейным шагом от начала истории к сегодняшнему дню, поэтому
// дельта на экране отрицательная и стабильная между запусками: сравнивать её
// вручную не нужно, значение одно и то же при любой дате запуска.
bool ADemo_Seeder::Seed_Measurements(AMeasurement_Repository *in_measurements, const QString &in_user_id)
{
	// Взвешивание раз в неделю шагом от начала истории к сегодняшнему дню, и
	// замер «за сегодня» обязателен: последний день цикла — это 0, и именно
	// он несёт обхваты. Начало округляется вниз до целой недели, иначе
	// Demo_History_Days = 20 дала бы смещения 20, 13, 6, −1 и нулевого замера
	// не существовало бы вовсе.
	const int first_offset = Demo_History_Days - (Demo_History_Days % 7);

	for (int offset = first_offset; offset >= 0; offset -= 7)
	{
		SBody_Measurement measurement;
		const int weeks = (first_offset - offset) / 7;

		measurement.User_Id = in_user_id;
		measurement.Date = Recent_Date(offset);
		measurement.Weight_Kg = Demo_Start_Weight_Kg + Demo_Weekly_Weight_Step_Kg * weeks;
		measurement.Has_Weight_Kg = true;

		// Обхваты — только в последний день окна: иначе каждый замер тащил бы
		// за собой пять лишних значений без видимой пользы.
		if (offset == 0)
		{
			measurement.Chest_Cm = Demo_Chest_Cm;
			measurement.Has_Chest_Cm = true;
			measurement.Waist_Cm = Demo_Waist_Cm;
			measurement.Has_Waist_Cm = true;
			measurement.Hip_Cm = Demo_Hip_Cm;
			measurement.Has_Hip_Cm = true;
			measurement.Arm_Cm = Demo_Arm_Cm;
			measurement.Has_Arm_Cm = true;
			measurement.Thigh_Cm = Demo_Thigh_Cm;
			measurement.Has_Thigh_Cm = true;
		}

		if (in_measurements->Save_Measurement(measurement) == false)
			return Set_Last_Error(in_measurements->Get_Last_Error());
	}

	return true;
}