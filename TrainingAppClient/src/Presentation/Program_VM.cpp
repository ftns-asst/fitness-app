#include "Presentation/Program_VM.h"

#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Presentation/Program_Rows.h"

#include <QDate>
#include <QLoggingCategory>

Q_LOGGING_CATEGORY(PROGRAM_VM_Log, "trainingapp.program.vm")

//----------------------------------------------------------------------------
Avm_Program::Avm_Program(AProgram_Repository *in_program_repository, ARule_Repository *in_rule_repository,
                         QObject *in_parent)
    : QObject(in_parent), Program_Repository_Instance(in_program_repository),
      Rule_Repository_Instance(in_rule_repository), Week_Slots(new AList_Model(this)),
      Program_Days(new AList_Model(this)), Day_Exercises(new AList_Model(this)), Goal_Targets(new AList_Model(this)),
      Rules(new AList_Model(this))
{
	Week_Slots->Set_Roles(Program_Rows::Week_Slot_Roles());
	Program_Days->Set_Roles(Program_Rows::Program_Day_Roles());
	Day_Exercises->Set_Roles(Program_Rows::Day_Exercise_Roles());
	Goal_Targets->Set_Roles(Program_Rows::Goal_Target_Roles());
	Rules->Set_Roles(Program_Rows::Rule_Roles());
}
// — QML-мост (camelCase) —
// Обёртки отделены от методов предметной области: контракт экрана не должен
// повторять внутренний стиль имён (§2 CODE_STYLE) и зависеть от него.
bool Avm_Program::reload(const QString &in_user_id, const QString &in_program_id)
{
	return Reload(in_user_id, in_program_id);
}

void Avm_Program::selectDay(const QString &in_program_day_id)
{
	Select_Day(in_program_day_id);
}

//----------------------------------------------------------------------------
bool Avm_Program::Reload(const QString &in_user_id, const QString &in_program_id)
{
	QString program_id;

	Clear_Tree();
	Clear_Error();

	if (Program_Repository_Instance == 0)
	{
		Set_Error();

		return false;
	}

	// Пустой идентификатор — открыть активную программу: вход из вкладок
	// по-прежнему показывает текущую, а каталог передаёт конкретную.
	if (in_program_id.isEmpty())
	{
		if (Program_Repository_Instance->Get_Active_Program_Id(in_user_id, program_id) == false || program_id.isEmpty())
		{
			// Активной программы может не быть — законное состояние «нет
			// программы», но UI обязан отличать его от ошибки чтения.
			Set_Error();

			return false;
		}
	}
	else
	{
		program_id = in_program_id;
	}

	if (Load_Tree(program_id) == false)
	{
		Set_Error();

		return false;
	}

	// Владелец проверяется по загруженной программе, а не по идентификатору:
	// программа должна принадлежать тому, кто её открывает.
	if (Program.User_Id != in_user_id)
	{
		Clear_Tree();
		Set_Error(Reason_Not_Owned);

		return false;
	}

	emit changed();

	return true;
}
//----------------------------------------------------------------------------
void Avm_Program::Select_Day(const QString &in_program_day_id)
{
	Selected_Day_Id.clear();

	if (Load_Day_Exercises(in_program_day_id))
		Selected_Day_Id = in_program_day_id;

	emit changed();
}
//----------------------------------------------------------------------------
void Avm_Program::Clear_Tree()
{
	Program = Program_Model::SProgram();
	Selected_Day_Id.clear();
	Known_Day_Ids.clear();
	Exercise_Titles.clear();

	Week_Slots->Set_Rows(QVector<QVariantMap>());
	Program_Days->Set_Rows(QVector<QVariantMap>());
	Day_Exercises->Set_Rows(QVector<QVariantMap>());
	Goal_Targets->Set_Rows(QVector<QVariantMap>());
	Rules->Set_Rows(QVector<QVariantMap>());
}
//----------------------------------------------------------------------------
bool Avm_Program::Load_Tree(const QString &in_program_id)
{
	QVector<SGoal_Target> targets;
	QVector<SProgram_Day> days;
	QVector<SDay_Exercise> exercises;
	QVector<SWeek_Slot> week_slots;
	QVector<SProgram_Rule> rules;
	QHash<QString, QString> day_titles;

	if (Program_Repository_Instance->Load_Program_Full(in_program_id, Program, targets, days, exercises, week_slots) ==
	    false)
		return false;

	for (int i = 0; i < days.size(); ++i)
	{
		day_titles.insert(days.at(i).ID, days.at(i).Title);
		Known_Day_Ids.insert(days.at(i).ID, QString());
	}

	// Названия упражнений нужны обоим спискам (день и его предписания),
	// поэтому читаются один раз на всё дерево.
	QVector<QString> exercise_ids;

	for (int i = 0; i < exercises.size(); ++i)
	{
		if (exercises.at(i).Exercise_Id.isEmpty() == false)
			exercise_ids.append(exercises.at(i).Exercise_Id);
	}

	Program_Repository_Instance->Load_Exercise_Titles(exercise_ids, Exercise_Titles);

	Week_Slots->Set_Rows(Program_Rows::Build_Week_Slots(week_slots, day_titles, Today_Text()));
	// Число упражнений показывается одним числом на день: список дней не
	// разворачивает предписания в превью (§7).
	Program_Days->Set_Rows(Program_Rows::Build_Program_Days(days, exercises.size()));
	Goal_Targets->Set_Rows(Program_Rows::Build_Goal_Targets(targets, QHash<QString, double>()));

	if (Rule_Repository_Instance != 0)
	{
		QVector<SRule_Checkin> checkins;
		QHash<QString, QString> today_state;

		if (Rule_Repository_Instance->Load_Rules(in_program_id, rules) &&
		    Rule_Repository_Instance->Load_Program_Checkins_Range(in_program_id, Today_Text(), Today_Text(), checkins))
		{
			for (int i = 0; i < checkins.size(); ++i)
				today_state.insert(checkins.at(i).Rule_Id, Program_Model::To_String(checkins.at(i).State));
		}

		Rules->Set_Rows(Program_Rows::Build_Rules(rules, today_state));
	}
	else
		Rules->Set_Rows(QVector<QVariantMap>());

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Program::Load_Day_Exercises(const QString &in_program_day_id)
{
	QVector<SDay_Exercise> exercises;

	// День проверяется по загруженному списку: репозиторий читает
	// предписания запросом и для несуществующего дня возвращает пустой
	// список с true, поэтому выбрать «несуществующий день» нельзя.
	if (in_program_day_id.isEmpty() || Known_Day_Ids.contains(in_program_day_id) == false)
	{
		Day_Exercises->Set_Rows(QVector<QVariantMap>());

		return false;
	}

	if (Program_Repository_Instance->Load_Day_Exercises(in_program_day_id, exercises) == false)
	{
		Day_Exercises->Set_Rows(QVector<QVariantMap>());

		return false;
	}

	Day_Exercises->Set_Rows(Program_Rows::Build_Day_Exercises(exercises, Exercise_Titles));

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Program::Is_Read_Only() const
{
	// Без загруженной программы состояние неизвестно, и экран не должен
	// блокировать действия из-за пустого дерева.
	if (Has_Program() == false)
		return false;

	return Program_Model::Program_State_Is_Editable(Program.State) == false;
}
//----------------------------------------------------------------------------
QString Avm_Program::Today_Text() const
{
	return QDate::currentDate().toString("yyyy-MM-dd");
}
//----------------------------------------------------------------------------
void Avm_Program::Clear_Error()
{
	Error_Key_Text.clear();
}
//----------------------------------------------------------------------------
void Avm_Program::Set_Error(EReason in_reason)
{
	bool has_repository_error =
	    Program_Repository_Instance != 0 && Program_Repository_Instance->Get_Last_Error().isEmpty() == false;

	// Причину отказа различаем явно: «нет активной программы» и «программа не
	// найдена или чужая» — разные состояния экрана, и подменять их одним ключом
	// значило бы показывать пользователю неверную подпись.
	if (in_reason == Reason_Not_Owned)
		Error_Key_Text = "program_not_found";
	else
		Error_Key_Text = has_repository_error ? "undefined_error" : "no_active_program";

	qCInfo(PROGRAM_VM_Log).noquote() << "reload failed" << Error_Key_Text;

	// Отказ обязан дойти до экрана: QML читает errorKey привязкой к changed(),
	// без сигнала баннер состояния перечитывается только при успешном
	// перезагрузе, и пользователь не видит, почему ничего не открылось.
	emit changed();
}
//----------------------------------------------------------------------------
bool Avm_Program::Has_Program() const
{
	return Program.ID.isEmpty() == false;
}
//----------------------------------------------------------------------------
QString Avm_Program::programId() const
{
	return Program.ID;
}
//----------------------------------------------------------------------------
QString Avm_Program::title() const
{
	return Program.Title;
}
//----------------------------------------------------------------------------
QString Avm_Program::goal() const
{
	return Program_Model::To_String(Program.Goal);
}
//----------------------------------------------------------------------------
QString Avm_Program::state() const
{
	return Program_Model::To_String(Program.State);
}
//----------------------------------------------------------------------------
int Avm_Program::weeksTotal() const
{
	return Program.Term.Weeks_Total;
}
//----------------------------------------------------------------------------
int Avm_Program::revision() const
{
	return Program.Revision;
}
//----------------------------------------------------------------------------
QString Avm_Program::startDate() const
{
	return Program.Actual_Start_Date.isEmpty() ? Program.Term.Start_Date : Program.Actual_Start_Date;
}
//----------------------------------------------------------------------------
QString Avm_Program::selectedDayId() const
{
	return Selected_Day_Id;
}
//----------------------------------------------------------------------------
QString Avm_Program::errorKey() const
{
	return Error_Key_Text;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program::week_slots() const
{
	return Week_Slots;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program::program_days() const
{
	return Program_Days;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program::day_exercises() const
{
	return Day_Exercises;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program::goal_targets() const
{
	return Goal_Targets;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program::rules() const
{
	return Rules;
}
//----------------------------------------------------------------------------
