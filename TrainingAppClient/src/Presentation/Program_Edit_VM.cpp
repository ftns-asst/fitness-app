#include "Presentation/Program_Edit_VM.h"

#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Presentation/Program_Rows.h"

#include <QDate>
#include <QLoggingCategory>
#include <QUuid>

Q_LOGGING_CATEGORY(PROGRAM_Edit_VM_Log, "trainingapp.program.edit_vm")

//----------------------------------------------------------------------------
QStringList Edit_Day_Roles()
{
	return QStringList{"id", "weekday", "title", "kind", "exerciseCount", "note"};
}
//----------------------------------------------------------------------------
// Avm_Program_Edit
//----------------------------------------------------------------------------
Avm_Program_Edit::Avm_Program_Edit(AProgram_Repository *in_program_repository, ARule_Repository *in_rule_repository,
                                   AsProgram_Service *in_service, QObject *in_parent)
    : QObject(in_parent), Program_Repository_Instance(in_program_repository),
      Rule_Repository_Instance(in_rule_repository), Service_Instance(in_service), Program_Days(new AList_Model(this)),
      Goal_Targets(new AList_Model(this)), Rules(new AList_Model(this)), Day_Exercises(new AList_Model(this))
{
	Program_Days->Set_Roles(Edit_Day_Roles());
	Goal_Targets->Set_Roles(Program_Rows::Goal_Target_Roles());
	Rules->Set_Roles(Program_Rows::Rule_Roles());
	Day_Exercises->Set_Roles(Program_Rows::Day_Exercise_Roles());
}
//----------------------------------------------------------------------------
// Семь дней создаются сразу: редактор показывает неделю целиком, и пользователь
// настраивает каждый день отдельно (§3.4).
// — QML-мост (camelCase) —
// Обёртки живут отдельно от методов предметной области, чтобы контракт экрана
// не повторял внутренний стиль имён (§2 CODE_STYLE) и правка разметки QML не
// зависела от переименования в C++.
bool Avm_Program_Edit::reset(const QString &in_user_id)
{
	return Reset(in_user_id);
}

void Avm_Program_Edit::setTitle(const QString &in_title)
{
	Set_Title(in_title);
}

void Avm_Program_Edit::setGoal(int in_goal)
{
	Set_Goal(in_goal);
}

void Avm_Program_Edit::setLevel(int in_level)
{
	Set_Level(in_level);
}

void Avm_Program_Edit::setStartDate(const QString &in_date)
{
	Set_Start_Date(in_date);
}

void Avm_Program_Edit::setWeeks(int in_weeks)
{
	Set_Weeks(in_weeks);
}

bool Avm_Program_Edit::setDayKind(int in_weekday, int in_kind)
{
	return Set_Day_Kind(in_weekday, in_kind);
}

bool Avm_Program_Edit::addDayExercise(int in_weekday, const QString &in_exercise_id, int in_sets, int in_reps_min,
                                      int in_reps_max, double in_target_weight_kg, bool in_has_target_weight)
{
	return Add_Day_Exercise(in_weekday, in_exercise_id, in_sets, in_reps_min, in_reps_max, in_target_weight_kg,
	                        in_has_target_weight);
}

bool Avm_Program_Edit::setTarget(int in_metric, double in_start_value, bool in_has_start_value, double in_target_value,
                                 int in_direction)
{
	return Set_Target(in_metric, in_start_value, in_has_start_value, in_target_value, in_direction);
}

bool Avm_Program_Edit::addRule(const QString &in_code, const QString &in_title, int in_metric, double in_target_min,
                               bool in_has_target_min, double in_target_max, bool in_has_target_max)
{
	return Add_Rule(in_code, in_title, in_metric, in_target_min, in_has_target_min, in_target_max, in_has_target_max);
}

bool Avm_Program_Edit::create(bool in_activate)
{
	return Create(in_activate);
}

// — Экран дня —
// День выбирается в ViewModel, потому что модель упражнений принадлежит ему, а не
// экрану: QML лишь показывает то, что уже выбрано.
int Avm_Program_Edit::Select_Day(int in_weekday)
{
	if (in_weekday < 1 || in_weekday > 7)
	{
		Set_Error("invalid_weekday");

		return Selected_Day;
	}

	Selected_Day = in_weekday;
	Refresh_Exercise_Model();
	emit changed();

	return Selected_Day;
}

bool Avm_Program_Edit::Remove_Day_Exercise(int in_weekday, int in_index)
{
	const QString day_id = Day_Id_For(in_weekday);

	if (day_id.isEmpty())
		return Set_Error("invalid_weekday");

	// Упражнения хранятся в общем списке программы, поэтому удаление идёт по
	// позиции именно внутри дня, а не по индексу всего списка.
	int position = -1;
	int seen = 0;

	for (int i = 0; i < Draft_Exercises.size(); ++i)
	{
		if (Draft_Exercises.at(i).Program_Day_Id != day_id)
			continue;

		if (seen == in_index)
		{
			position = i;

			break;
		}

		++seen;
	}

	if (position < 0)
		return Set_Error("exercise_not_found");

	Draft_Exercises.remove(position);
	Reindex_Day_Exercises(day_id);
	Refresh_Day_Model();
	Refresh_Exercise_Model();
	emit changed();

	return true;
}

bool Avm_Program_Edit::Copy_Day(int in_from_weekday, int in_to_weekday)
{
	const QString from_id = Day_Id_For(in_from_weekday);
	const QString to_id = Day_Id_For(in_to_weekday);

	if (from_id.isEmpty() || to_id.isEmpty())
		return Set_Error("invalid_weekday");

	if (in_from_weekday == in_to_weekday)
		return Set_Error("copy_day_itself");

	// Прежнее содержимое целевого дня заменяется целиком: копирование — это
	// «сделать этот день таким», а не добавить упражнения поверх старых.
	for (int i = Draft_Exercises.size() - 1; i >= 0; --i)
	{
		if (Draft_Exercises.at(i).Program_Day_Id == to_id)
			Draft_Exercises.remove(i);
	}

	int order = 0;

	for (int i = 0; i < Draft_Exercises.size(); ++i)
	{
		if (Draft_Exercises.at(i).Program_Day_Id != from_id)
			continue;

		SDay_Exercise copy = Draft_Exercises.at(i);

		copy.ID = New_Id();
		copy.Program_Day_Id = to_id;
		copy.Order_Index = ++order;
		Draft_Exercises.append(copy);
	}

	Reindex_Day_Exercises(from_id);
	Refresh_Day_Model();
	Refresh_Exercise_Model();
	emit changed();

	return true;
}

// — Цели и правила —
// Правило несёт все поля, а не фиксированный набор: короткая форма ставила
// Nutrition/в день/граммы, из-за чего сон и шаги выглядели как граммы белка.
bool Avm_Program_Edit::Add_Rule_Detailed(const QString &in_code, const QString &in_title, int in_category,
                                         int in_metric, int in_periodicity, double in_target_min,
                                         bool in_has_target_min, double in_target_max, bool in_has_target_max,
                                         int in_unit, int in_applies_to, int in_enforcement)
{
	SProgram_Rule rule;

	Clear_Error();

	if (in_title.isEmpty())
		return Set_Error("rule_title_required");

	// §3.6: правило отвечает на вопрос «сколько» — без цели оно бессмысленно.
	if (in_has_target_min == false && in_has_target_max == false)
		return Set_Error("rule_target_required");

	rule.ID = New_Id();
	rule.Program_Id = Created_Program_Id;
	rule.Category = (ERule_Category)in_category;
	rule.Code = in_code.isEmpty() ? QString("custom") : in_code;
	rule.Title = in_title;
	rule.Metric = (ERule_Metric)in_metric;
	rule.Periodicity = (ERule_Periodicity)in_periodicity;
	rule.Target_Min = in_target_min;
	rule.Has_Target_Min = in_has_target_min;
	rule.Target_Max = in_target_max;
	rule.Has_Target_Max = in_has_target_max;
	rule.Target_Unit = (ERule_Display_Unit)in_unit;
	rule.Display_Unit = (ERule_Display_Unit)in_unit;
	rule.Applies_To = (ERule_Applies_To)in_applies_to;
	rule.Enforcement = (ERule_Enforcement)in_enforcement;

	Draft_Rules.append(rule);
	Refresh_Rule_Model();
	emit changed();

	return true;
}

// Удаление обязательно: правило с опечаткой иначе осталось бы в программе и
// пользователь не смог бы его исправить — интерфейс без удаления в тупике.
bool Avm_Program_Edit::Remove_Rule(int in_index)
{
	Clear_Error();

	if (Remove_By_Index(Draft_Rules, in_index) == false)
		return Set_Error("rule_not_found");

	Refresh_Rule_Model();
	emit changed();

	return true;
}

bool Avm_Program_Edit::Remove_Target(int in_index)
{
	Clear_Error();

	if (Remove_By_Index(Draft_Targets, in_index) == false)
		return Set_Error("target_not_found");

	Refresh_Target_Model();
	emit changed();

	return true;
}

bool Avm_Program_Edit::load(const QString &in_user_id, const QString &in_program_id)
{
	return Load(in_user_id, in_program_id);
}
//----------------------------------------------------------------------------
bool Avm_Program_Edit::Reset(const QString &in_user_id)
{
	Clear_Error();

	Draft = SProgram();
	Draft_Targets.clear();
	Draft_Days.clear();
	Draft_Exercises.clear();
	Draft_Rules.clear();
	Created_Program_Id.clear();

	Draft.User_Id = in_user_id;
	Draft.Title = "Моя программа";
	Draft.Source_Kind = EProgram_Source_Kind::Scratch;
	Draft.State = EProgram_State::Draft;
	Draft.Revision = 1;
	Draft.Term.Mode = ETerm_Mode::FixedWeeks;
	Draft.Term.Weeks_Total = 0;
	// Открытый день сбрасывается вместе с черновиком: иначе экран дня открылся бы
	// на дне, которого в новой программе ещё нет.
	Selected_Day = 1;

	for (int weekday = 1; weekday <= 7; ++weekday)
	{
		SProgram_Day day;

		day.ID = New_Id();
		day.Title = QString("День %1").arg(weekday);
		day.Kind = EDay_Kind::Rest;
		day.Order_Index = weekday;
		Draft_Days.append(day);
	}

	Refresh_Day_Model();
	// Новый черновик — пустые списки; без обновления модели целей, правил и
	// упражнений на экране остались бы строки предыдущей программы.
	Refresh_Target_Model();
	Refresh_Rule_Model();
	Refresh_Exercise_Model();
	emit changed();

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Program_Edit::Load(const QString &in_user_id, const QString &in_program_id)
{
	SProgram program;
	QVector<SGoal_Target> targets;
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> exercises;
	QVector<SWeek_Slot> week_slots;
	QVector<SProgram_Rule> rules;

	Clear_Error();

	// Существующая программа читается целиком: редактору нужны и цели, и
	// правила, и предписания дней — по частям форма показала бы половину.
	if (Program_Repository_Instance == 0 || Program_Repository_Instance->Load_Program_Full(
	                                            in_program_id, program, targets, days, exercises, week_slots) == false)
	{
		Set_Error("program_not_found");

		return false;
	}

	if (program.User_Id != in_user_id)
	{
		Set_Error("program_not_found");

		return false;
	}

	// Правка структуры запрещена в read-only состояниях (§3.1): форма, которая
	// упрётся в отказ репозитория при сохранении, хуже явного отказа.
	if (Program_Model::Program_State_Is_Editable(program.State) == false)
	{
		Set_Error("program_read_only");

		return false;
	}

	if (Rule_Repository_Instance != 0)
		Rule_Repository_Instance->Load_Rules(in_program_id, rules);

	// Черновик заменяется целиком: иначе семь дней из reset() остались бы
	// поверх загруженных и экран показал бы дни не той программы.
	Draft = program;
	Draft_Targets = targets;
	Draft_Days = days;
	Draft_Exercises = exercises;
	Draft_Rules = rules;

	// Семь дней обязательны для редактора, даже если программа хранит их
	// иначе: экран недели показывает ровно семь ячеек.
	while (Draft_Days.size() < 7)
	{
		SProgram_Day day;

		day.ID = New_Id();
		day.Program_Id = program.ID;
		day.Title = QString("День %1").arg(Draft_Days.size() + 1);
		day.Kind = EDay_Kind::Rest;
		day.Order_Index = Draft_Days.size() + 1;
		Draft_Days.append(day);
	}

	// Черновик идёт с тем же идентификатором: create() применит правки к уже
	// существующей программе, а не создаст вторую.
	Created_Program_Id = program.ID;
	Selected_Day = 1;

	Refresh_Day_Model();
	Refresh_Exercise_Model();
	Refresh_Target_Model();
	Refresh_Rule_Model();

	emit changed();

	return true;
}
//----------------------------------------------------------------------------
QString Avm_Program_Edit::New_Id() const
{
	return QUuid::createUuid().toString(QUuid::WithoutBraces);
}
//----------------------------------------------------------------------------
void Avm_Program_Edit::Set_Title(const QString &in_title)
{
	Draft.Title = in_title;
	emit changed();
}
//----------------------------------------------------------------------------
void Avm_Program_Edit::Set_Goal(int in_goal)
{
	Draft.Goal = (EProgram_Goal)in_goal;
	emit changed();
}
//----------------------------------------------------------------------------
void Avm_Program_Edit::Set_Level(int in_level)
{
	Draft.Level = (EProgram_Level)in_level;
	emit changed();
}
//----------------------------------------------------------------------------
void Avm_Program_Edit::Set_Start_Date(const QString &in_date)
{
	Clear_Error();
	Draft.Term.Start_Date = in_date;

	if (QDate::fromString(in_date, Qt::ISODate).isValid() == false)
		Set_Error("invalid_start_date");

	emit changed();
}
//----------------------------------------------------------------------------
void Avm_Program_Edit::Set_Weeks(int in_weeks)
{
	Clear_Error();

	if (in_weeks < 1 || in_weeks > 104)
	{
		Set_Error("invalid_term");

		return;
	}

	Draft.Term.Weeks_Total = in_weeks;
	emit changed();
}
//----------------------------------------------------------------------------
bool Avm_Program_Edit::Set_Day_Kind(int in_weekday, int in_kind)
{
	if (in_weekday < 1 || in_weekday > 7)
		return Set_Error("invalid_weekday");

	Draft_Days[in_weekday - 1].Kind = (EDay_Kind)in_kind;

	// Рабочий день без упражнений допустим на время редактирования, но
	// создание без упражнений отвергается — см. Create.
	Refresh_Day_Model();
	emit changed();

	return true;
}
// — Перезаполнение моделей и служебные операции —
// Общие функции обновления: добавление и удаление меняют одни и те же списки, и
// каждая точка изменения должна вести себя одинаково.
void Avm_Program_Edit::Refresh_Target_Model()
{
	Goal_Targets->Set_Rows(Program_Rows::Build_Goal_Targets(Draft_Targets, QHash<QString, double>()));
}

void Avm_Program_Edit::Refresh_Rule_Model()
{
	Rules->Set_Rows(Program_Rows::Build_Rules(Draft_Rules, QHash<QString, QString>()));
}

void Avm_Program_Edit::Refresh_Exercise_Model()
{
	const QString day_id = Day_Id_For(Selected_Day);
	QVector<SDay_Exercise> visible;

	for (int i = 0; i < Draft_Exercises.size(); ++i)
	{
		if (Draft_Exercises.at(i).Program_Day_Id == day_id)
			visible.append(Draft_Exercises.at(i));
	}

	// Названия упражнений редактору неизвестны (каталога ещё нет), поэтому
	// подпись строится из идентификатора: экран дня обязан что-то показать.
	Day_Exercises->Set_Rows(Program_Rows::Build_Day_Exercises(visible, QHash<QString, QString>()));
}

QString Avm_Program_Edit::Day_Id_For(int in_weekday) const
{
	if (in_weekday < 1 || in_weekday > Draft_Days.size())
		return QString();

	return Draft_Days.at(in_weekday - 1).ID;
}

void Avm_Program_Edit::Reindex_Day_Exercises(const QString &in_day_id)
{
	int order = 0;

	for (int i = 0; i < Draft_Exercises.size(); ++i)
	{
		if (Draft_Exercises.at(i).Program_Day_Id == in_day_id)
			Draft_Exercises[i].Order_Index = ++order;
	}
}

bool Avm_Program_Edit::Remove_By_Index(QVector<SGoal_Target> &io_targets, int in_index)
{
	if (in_index < 0 || in_index >= io_targets.size())
		return false;

	io_targets.remove(in_index);

	return true;
}

bool Avm_Program_Edit::Remove_By_Index(QVector<SProgram_Rule> &io_rules, int in_index)
{
	if (in_index < 0 || in_index >= io_rules.size())
		return false;

	io_rules.remove(in_index);

	return true;
}

int Avm_Program_Edit::Goal_Value() const
{
	return (int)Draft.Goal;
}

int Avm_Program_Edit::Level_Value() const
{
	return (int)Draft.Level;
}

int Avm_Program_Edit::Selected_Weekday() const
{
	return Selected_Day;
}

AList_Model *Avm_Program_Edit::day_exercises() const
{
	return Day_Exercises;
}

// — QML-мост: экран дня и цели с правилами —
// Обёртки отделены от доменных методов по той же причине, что и остальные:
// контракт экрана не должен повторять внутренний стиль имён (§2 CODE_STYLE).
int Avm_Program_Edit::selectDay(int in_weekday)
{
	return Select_Day(in_weekday);
}

bool Avm_Program_Edit::removeDayExercise(int in_weekday, int in_index)
{
	return Remove_Day_Exercise(in_weekday, in_index);
}

bool Avm_Program_Edit::copyDay(int in_from_weekday, int in_to_weekday)
{
	return Copy_Day(in_from_weekday, in_to_weekday);
}

bool Avm_Program_Edit::addRuleDetailed(const QString &in_code, const QString &in_title, int in_category, int in_metric,
                                       int in_periodicity, double in_target_min, bool in_has_target_min,
                                       double in_target_max, bool in_has_target_max, int in_unit, int in_applies_to,
                                       int in_enforcement)
{
	return Add_Rule_Detailed(in_code, in_title, in_category, in_metric, in_periodicity, in_target_min,
	                         in_has_target_min, in_target_max, in_has_target_max, in_unit, in_applies_to,
	                         in_enforcement);
}

bool Avm_Program_Edit::removeRule(int in_index)
{
	return Remove_Rule(in_index);
}

bool Avm_Program_Edit::removeTarget(int in_index)
{
	return Remove_Target(in_index);
}

//----------------------------------------------------------------------------
bool Avm_Program_Edit::Add_Day_Exercise(int in_weekday, const QString &in_exercise_id, int in_sets, int in_reps_min,
                                        int in_reps_max, double in_target_weight_kg, bool in_has_target_weight)
{
	SDay_Exercise exercise;

	Clear_Error();

	if (in_weekday < 1 || in_weekday > 7)
		return Set_Error("invalid_weekday");

	if (in_exercise_id.isEmpty())
		return Set_Error("exercise_required");

	if (in_sets < 1 || in_sets > 20)
		return Set_Error("invalid_sets");

	if (in_reps_min < 0 || in_reps_max < in_reps_min)
		return Set_Error("invalid_reps");

	exercise.ID = New_Id();
	exercise.Program_Day_Id = Draft_Days[in_weekday - 1].ID;
	exercise.Exercise_Id = in_exercise_id;
	exercise.Order_Index = Count_Day_Exercises(in_weekday) + 1;
	exercise.Sets = in_sets;
	exercise.Reps_Min = in_reps_min;
	exercise.Reps_Max = in_reps_max;
	exercise.Weight_Mode = in_has_target_weight ? EWeight_Mode::Fixed : EWeight_Mode::FromLastSession;
	exercise.Target_Weight_Kg = in_target_weight_kg;
	exercise.Has_Target_Weight_Kg = in_has_target_weight;

	Draft_Exercises.append(exercise);
	Refresh_Day_Model();
	// Модель упражнений открытого дня обновляется здесь же: экран дня
	// показывает её, а не модель дней, и без этого добавленное упражнение
	// оставалось невидимым до следующего выбора дня.
	Refresh_Exercise_Model();
	emit changed();

	return true;
}
//----------------------------------------------------------------------------
int Avm_Program_Edit::Count_Day_Exercises(int in_weekday) const
{
	int count = 0;
	const QString day_id = Draft_Days.at(in_weekday - 1).ID;

	for (int i = 0; i < Draft_Exercises.size(); ++i)
	{
		if (Draft_Exercises.at(i).Program_Day_Id == day_id)
			count += 1;
	}

	return count;
}
//----------------------------------------------------------------------------
bool Avm_Program_Edit::Set_Target(int in_metric, double in_start_value, bool in_has_start_value, double in_target_value,
                                  int in_direction)
{
	SGoal_Target target;

	Clear_Error();
	target.ID = New_Id();
	target.Program_Id = Created_Program_Id;
	target.Metric = (EGoal_Metric)in_metric;
	target.Unit = (in_metric == (int)EGoal_Metric::BodyWeightKg) ? EGoal_Unit::Kg : EGoal_Unit::Kg;
	target.Start_Value = in_start_value;
	target.Has_Start_Value = in_has_start_value;
	target.Target_Value = in_target_value;
	target.Direction = (EGoal_Direction)in_direction;
	target.State = EGoal_Target_State::Active;

	Draft_Targets.append(target);
	Goal_Targets->Set_Rows(Program_Rows::Build_Goal_Targets(Draft_Targets, QHash<QString, double>()));
	emit changed();

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Program_Edit::Add_Rule(const QString &in_code, const QString &in_title, int in_metric, double in_target_min,
                                bool in_has_target_min, double in_target_max, bool in_has_target_max)
{
	SProgram_Rule rule;

	Clear_Error();

	if (in_code.isEmpty() || in_title.isEmpty())
		return Set_Error("rule_title_required");

	// §3.6: правило отвечает на вопрос «сколько» — без цели оно бессмысленно.
	if (in_has_target_min == false && in_has_target_max == false)
		return Set_Error("rule_target_required");

	rule.ID = New_Id();
	rule.Program_Id = Created_Program_Id;
	rule.Category = ERule_Category::Nutrition;
	rule.Code = in_code;
	rule.Title = in_title;
	rule.Metric = (ERule_Metric)in_metric;
	rule.Periodicity = ERule_Periodicity::PerDay;
	rule.Target_Min = in_target_min;
	rule.Has_Target_Min = in_has_target_min;
	rule.Target_Max = in_target_max;
	rule.Has_Target_Max = in_has_target_max;
	rule.Target_Unit = ERule_Display_Unit::G;
	rule.Display_Unit = ERule_Display_Unit::G;
	rule.Applies_To = ERule_Applies_To::AllDays;
	rule.Check_Source = ERule_Check_Source::SelfReport;
	rule.Enforcement = ERule_Enforcement::Soft;

	Draft_Rules.append(rule);
	Rules->Set_Rows(Program_Rows::Build_Rules(Draft_Rules, QHash<QString, QString>()));
	emit changed();

	return true;
}
//----------------------------------------------------------------------------
// Создание. Проверки идут ДО записи: отказ не должен оставлять в БД половину
// программы. Идентификаторы программы выдаёт сервис, а идентификаторы целей и
// правил проставляются здесь, когда уже известен идентификатор программы.
//----------------------------------------------------------------------------
bool Avm_Program_Edit::Create(bool in_activate)
{
	SProgram created;
	bool all_days_have_exercises = true;

	Clear_Error();

	// Частая причина ложного «invalid_draft» — пустая дата или нулевой срок:
	// экран создавал программу, не заполнив обязательные поля. Раздельные
	// ключи позволяют экрану сказать, чего именно не хватает.
	if (Draft.User_Id.isEmpty())
		return Set_Error("invalid_draft");

	if (Draft.Term.Start_Date.isEmpty())
		return Set_Error("invalid_start_date");

	if (Draft.Term.Weeks_Total < 1)
		return Set_Error("invalid_term");

	if (trainingDays() == 0)
		return Set_Error("empty_training_day");

	for (int weekday = 1; weekday <= 7; ++weekday)
	{
		if (Draft_Days.at(weekday - 1).Kind == EDay_Kind::Rest)
			continue;

		if (Count_Day_Exercises(weekday) > 0)
			continue;

		all_days_have_exercises = false;
	}

	if (all_days_have_exercises == false)
		return Set_Error("empty_training_day");

	if (in_activate && Draft_Targets.isEmpty() && Is_Measurable_Goal(Draft.Goal))
		return Set_Error("goal_required");

	if (Service_Instance == 0)
		return Set_Error("service_unavailable");

	// Загруженная программа правится, а не создаётся заново: после Load() в
	// черновике уже есть её идентификатор, и попытка вставить ту же строку
	// нарушала UNIQUE programs.id — каталог получал бы дубликат.
	if (Draft.ID.isEmpty() == false && Draft.ID == Created_Program_Id)
	{
		if (Program_Repository_Instance == 0)
			return Set_Error("service_unavailable");

		if (Program_Repository_Instance->Replace_Structure(Draft.ID, Draft_Days, Draft_Exercises, Build_Schedule(),
		                                                   ERevision_Reason::Days, "Правка из редактора") == false)
			return Fail_Save(Program_Repository_Instance->Get_Last_Error());

		if (Program_Repository_Instance->Replace_Goal_Targets(Draft.ID, Draft_Targets) == false)
			return Fail_Save(Program_Repository_Instance->Get_Last_Error());

		if (Program_Repository_Instance->Change_Term(Draft.ID, Draft.Term) == false)
			return Fail_Save(Program_Repository_Instance->Get_Last_Error());

		// Название, цель и уровень правятся отдельным вызовом: структурная
		// правка их не трогает.
		if (Program_Repository_Instance->Change_Profile(Draft.ID, Draft.Title, Draft.Goal, Draft.Level) == false)
			return Fail_Save(Program_Repository_Instance->Get_Last_Error());
	}
	else if (Service_Instance->Create_From_Scratch(Draft, Draft_Targets, Draft_Days, Draft_Exercises, Build_Schedule(),
	                                               created) == false)
	{
		return Fail_Save(Service_Instance->Get_Last_Error());
	}
	else
	{
		Created_Program_Id = created.ID;
	}

	// Цели и правила уже собраны в редакторе, поэтому после создания им
	// проставляется настоящий идентификатор программы.
	if (Link_Drafts_To_Program() == false)
		return Fail_Save(Rule_Repository_Instance != 0 ? Rule_Repository_Instance->Get_Last_Error() : QString());

	if (in_activate && Service_Instance->Activate(Created_Program_Id) == false)
		return Fail_Save(Service_Instance->Get_Last_Error());

	emit changed();

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Program_Edit::Link_Drafts_To_Program()
{
	if (Rule_Repository_Instance == 0 || Draft_Rules.isEmpty())
		return true;

	for (int i = 0; i < Draft_Rules.size(); ++i)
		Draft_Rules[i].Program_Id = Created_Program_Id;

	if (Rule_Repository_Instance->Replace_Rules(Created_Program_Id, Draft_Rules) == false)
		return false;

	Rules->Set_Rows(Program_Rows::Build_Rules(Draft_Rules, QHash<QString, QString>()));

	return true;
}
//----------------------------------------------------------------------------
// Расписание из видов дней: рабочий день даёт слот-тренировку со ссылкой на свой
// день, день отдыха — слот отдыха без ссылки (§3.4, инвариант 4).
QVector<SWeek_Slot> Avm_Program_Edit::Build_Schedule() const
{
	QVector<SWeek_Slot> week_slots;

	for (int weekday = 1; weekday <= 7; ++weekday)
	{
		const SProgram_Day &day = Draft_Days.at(weekday - 1);
		SWeek_Slot slot;

		slot.ID = New_Id();
		slot.Program_Id = Created_Program_Id;
		slot.Day_Of_Week = weekday;
		slot.Program_Day_Id = day.ID;

		if (day.Kind == EDay_Kind::Rest)
		{
			slot.Kind = ESlot_Kind::Rest;
			slot.Program_Day_Id.clear();
		}
		else
			slot.Kind = ESlot_Kind::Workout;

		week_slots.append(slot);
	}

	return week_slots;
}
//----------------------------------------------------------------------------
bool Avm_Program_Edit::Is_Measurable_Goal(EProgram_Goal in_goal)
{
	return in_goal == EProgram_Goal::Strength || in_goal == EProgram_Goal::FatLoss || in_goal == EProgram_Goal::Muscle;
}
//----------------------------------------------------------------------------
// Роли дней редактора отличаются от ролей дней программы: редактору нужен
// номер дня недели и число упражнений, экран же программы показывает готовое
// дерево. Формат месяца остаётся одинаковым.
void Avm_Program_Edit::Refresh_Day_Model()
{
	QHash<QString, int> counts;

	for (int i = 0; i < Draft_Exercises.size(); ++i)
		counts[Draft_Exercises.at(i).Program_Day_Id] += 1;

	QVector<QVariantMap> rows;

	for (int weekday = 1; weekday <= 7; ++weekday)
	{
		const SProgram_Day &day = Draft_Days.at(weekday - 1);
		QVariantMap row;

		row.insert("id", day.ID);
		row.insert("weekday", weekday);
		row.insert("title", day.Title);
		row.insert("kind", To_String(day.Kind));
		row.insert("exerciseCount", counts.value(day.ID));
		row.insert("note", day.Note);
		rows.append(row);
	}

	Program_Days->Set_Rows(rows);
}
//----------------------------------------------------------------------------
bool Avm_Program_Edit::canCreate() const
{
	if (Draft.User_Id.isEmpty() || Draft.Term.Start_Date.isEmpty() || Draft.Term.Weeks_Total < 1)
		return false;

	if (trainingDays() == 0)
		return false;

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Program_Edit::created() const
{
	return Created_Program_Id.isEmpty() == false;
}
//----------------------------------------------------------------------------
QString Avm_Program_Edit::programId() const
{
	return Created_Program_Id;
}
//----------------------------------------------------------------------------
QString Avm_Program_Edit::errorKey() const
{
	return Error_Key_Text;
}
//----------------------------------------------------------------------------
QString Avm_Program_Edit::title() const
{
	return Draft.Title;
}
//----------------------------------------------------------------------------
int Avm_Program_Edit::weeks() const
{
	return Draft.Term.Weeks_Total;
}
//----------------------------------------------------------------------------
QString Avm_Program_Edit::startDate() const
{
	return Draft.Term.Start_Date;
}
//----------------------------------------------------------------------------
int Avm_Program_Edit::trainingDays() const
{
	int count = 0;

	for (int i = 0; i < Draft_Days.size(); ++i)
	{
		if (Draft_Days.at(i).Kind != EDay_Kind::Rest)
			count += 1;
	}

	return count;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program_Edit::program_days() const
{
	return Program_Days;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program_Edit::goal_targets() const
{
	return Goal_Targets;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program_Edit::rules() const
{
	return Rules;
}
//----------------------------------------------------------------------------
void Avm_Program_Edit::Clear_Error()
{
	Error_Key_Text.clear();
	Error_Detail_Text.clear();
}
//----------------------------------------------------------------------------
bool Avm_Program_Edit::Fail_Save(const QString &in_detail)
{
	Error_Detail_Text = in_detail;

	return Set_Error("save_failed");
}
//----------------------------------------------------------------------------
bool Avm_Program_Edit::Set_Error(const QString &in_key)
{
	Error_Key_Text = in_key;

	qCInfo(PROGRAM_Edit_VM_Log).noquote() << "program edit failed" << in_key;

	// Экран читает errorKey/errorDetail привязкой к changed(): без сигнала
	// отказ записи остаётся в журнале, а на дисплее пользователь видит только
	// «кнопка ничего не сделала». Валидация и сбой записи обязаны оповестить
	// QML, иначе сообщение об ошибке не появится.
	emit changed();

	return false;
}
//----------------------------------------------------------------------------
