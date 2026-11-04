#include "Presentation/Program_Rules_VM.h"

#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Data/Users/Users_Repository.h"
#include "Presentation/Program_Rows.h"

#include <QDate>
#include <QLoggingCategory>
#include <QUuid>

Q_LOGGING_CATEGORY(PROGRAM_Rules_VM_Log, "trainingapp.program.rules_vm")

//----------------------------------------------------------------------------
// Допустимые периоды отметок (§5.1): неделя, месяц, квартал.
//----------------------------------------------------------------------------
static bool Is_Supported_Period(int in_days)
{
	return in_days == 7 || in_days == 30 || in_days == 90;
}
//----------------------------------------------------------------------------

// Avm_Program_Rules
//----------------------------------------------------------------------------
Avm_Program_Rules::Avm_Program_Rules(AProgram_Repository *in_program_repository, ARule_Repository *in_rule_repository,
                                     AUsers_Repository *in_users_repository, QObject *in_parent)
    : QObject(in_parent), Program_Repository_Instance(in_program_repository),
      Rule_Repository_Instance(in_rule_repository), Users_Repository_Instance(in_users_repository),
      Rules(new AList_Model(this)), Checkins(new AList_Model(this))
{
	Rules->Set_Roles(Program_Rows::Rule_Roles());
	Checkins->Set_Roles(Program_Rows::Checkin_Roles());
}
//----------------------------------------------------------------------------
// — QML-мост (camelCase) —
// Обёртки отделены от методов предметной области: контракт экрана не должен
// повторять внутренний стиль имён (§2 CODE_STYLE) и зависеть от него.
bool Avm_Program_Rules::reload(const QString &in_user_id)
{
	return Reload(in_user_id);
}
//----------------------------------------------------------------------------
bool Avm_Program_Rules::setPeriodDays(int in_days)
{
	return Set_Period_Days(in_days);
}
//----------------------------------------------------------------------------
bool Avm_Program_Rules::saveCheckin(const QString &in_rule_id, const QString &in_state, double in_value,
                                    bool in_has_value, const QString &in_note)
{
	return Save_Checkin(in_rule_id, in_state, in_value, in_has_value, in_note);
}
//----------------------------------------------------------------------------
bool Avm_Program_Rules::generateRulesFromMacro(const QString &in_user_id)
{
	return Generate_Rules_From_Macro(in_user_id);
}
//----------------------------------------------------------------------------
bool Avm_Program_Rules::Reload(const QString &in_user_id)
{
	Clear_Error();

	if (Load_Program_Id(in_user_id) == false)
	{
		Clear_All();
		Set_Error("no_active_program");

		return false;
	}

	// Окно пересчитывается перед чтением отметок. Раньше оно заполнялось только
	// из Set_Period_Days и Set_Today, поэтому первый reload — а он и есть
	// обычное состояние экрана — читал отметки с пустыми границами и получал
	// пустую выборку: процент показывался прочерком, а подпись «Отметок за это
	// окно нет» появлялась при полной базе.
	Apply_Period();

	// Сохранённый расчёт КБЖУ: кнопка генерации правил питания активна только
	// при наличии расчёта. Пустой результат — обычное состояние («ещё не
	// считали»), поэтому чтение не ошибка.
	Has_Macro_Result_Value = false;

	if (Users_Repository_Instance != 0)
	{
		AsMacro_Calculator::SResult macro;
		QString calculated_at;

		if (Users_Repository_Instance->Load_Macro_Result(in_user_id, macro, calculated_at) && macro.Calories_Kcal > 0)
			Has_Macro_Result_Value = true;
	}

	// Отметки читаются раньше правил: роль todayState строится из них, иначе
	// состояние за сегодня всегда оставалось бы «неизвестно».
	if (Load_Checkins() == false || Load_Rules() == false)
	{
		Clear_All();
		Set_Error("undefined_error");

		return false;
	}

	// Процент считается только здесь, после обеих загрузок: движок правил
	// выводит вердикты по целям, поэтому на пустом наборе правил метрика
	// отсутствует. Расчёт внутри Load_Checkins давал прочерк при полной базе —
	// правила на тот момент ещё не были прочитаны.
	Recalculate_Adherence();

	emit changed();

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Program_Rules::Load_Program_Id(const QString &in_user_id)
{
	if (Program_Repository_Instance == 0)
		return false;

	return Program_Repository_Instance->Get_Active_Program_Id(in_user_id, Program_Id_Text);
}
//----------------------------------------------------------------------------
bool Avm_Program_Rules::Set_Period_Days(int in_days)
{
	if (Is_Supported_Period(in_days) == false)
	{
		Set_Error("invalid_period");
		emit changed();

		return false;
	}

	Period_Days_Number = in_days;
	Apply_Period();

	if (Program_Id_Text.isEmpty() == false)
	{
		Load_Checkins();
		// Окно сменилось, значит сменился и набор отметок, по которому считается
		// процент: без пересчёта метрика осталась бы от прежнего периода.
		Recalculate_Adherence();
	}

	emit changed();

	return true;
}
//----------------------------------------------------------------------------
void Avm_Program_Rules::Set_Today(const QString &in_today)
{
	Today_Text_Override = in_today;
	Apply_Period();
}
//----------------------------------------------------------------------------
// Ручная отметка пользователя. Состояние приходит текстом, потому что QML
// сопоставляет строки (§7), а расчётные отметки пользователь не ставит:
// репозиторий всё равно не даст им переписать ручные (§3.7).
//----------------------------------------------------------------------------
bool Avm_Program_Rules::Save_Checkin(const QString &in_rule_id, const QString &in_state, double in_value,
                                     bool in_has_value, const QString &in_note)
{
	ECheckin_State state = ECheckin_State::Unknown;
	SRule_Checkin checkin;

	Clear_Error();

	if (From_String(in_state, state) == false || state == ECheckin_State::Unknown)
	{
		Set_Error("invalid_checkin_state");
		emit changed();

		return false;
	}

	checkin.Rule_Id = in_rule_id;
	checkin.Program_Id = Program_Id_Text;
	checkin.Date = Today();
	checkin.Value = in_value;
	checkin.Has_Value = in_has_value;
	checkin.Unit = "g";
	checkin.State = state;
	checkin.Source = ECheckin_Source::Manual;
	checkin.Note = in_note;

	if (Rule_Repository_Instance == 0 || Rule_Repository_Instance->Save_Checkin(checkin) == false)
	{
		Set_Error("checkin_save_failed");
		emit changed();

		return false;
	}

	// Порядок тот же, что в Reload: сначала отметки, затем правила, а метрика —
	// после обоих наборов, иначе новая отметка не попадёт в процент.
	Load_Checkins();
	Load_Rules();
	Recalculate_Adherence();
	emit changed();

	return true;
}
//----------------------------------------------------------------------------
// Генерация правил питания из расчёта КБЖУ (спринт «КБЖУ в правилах»).
//
// Четыре правила: белок/углеводы/жиры — «не ниже N г», калории — «ровно C
// ккал». Цели берутся из сохранённого расчёта (миграции v5–v6), поэтому
// программу можно отмечать по нормам макросов.
//
// Повторное нажатие по тому же расчёту ничего не меняет, по новому — обновляет
// цели существующих правил по коду: adherence считал бы по двум противоречащим
// нормам белка, а список засорялся бы дублями. Название правила при обновлении
// сохраняется: переименовать «Белок» — право пользователя.
//
// Без сохранённого расчёта отказ с macro_calc_required: «углеводы ≥ 0 г» в
// программе бессмысленны и только запутали бы.
//----------------------------------------------------------------------------
bool Avm_Program_Rules::Generate_Rules_From_Macro(const QString &in_user_id)
{
	AsMacro_Calculator::SResult macro;
	QString calculated_at;

	Clear_Error();

	if (Users_Repository_Instance == 0 ||
	    Users_Repository_Instance->Load_Macro_Result(in_user_id, macro, calculated_at) == false ||
	    macro.Calories_Kcal <= 0)
	{
		Set_Error("macro_calc_required");
		emit changed();

		return false;
	}

	// Генерация привязана к активной программе: правила живут в ней.
	if (Program_Id_Text.isEmpty())
	{
		Set_Error("no_active_program");
		emit changed();

		return false;
	}

	if (Rule_Repository_Instance == 0)
	{
		Set_Error("undefined_error");
		emit changed();

		return false;
	}

	// Макро-правила описываются таблицей: код — ключ идемпотентности, форма
	// цели задаётся метрикой (г — «не ниже», ккал — «ровно»).
	struct SMacro_Rule
	{
		const char *Code;
		const char *Title;
		ERule_Metric Metric;
		ERule_Display_Unit Unit;
		bool Is_Exact;
	};

	static const SMacro_Rule macro_rules[] = {
	    {"macro_protein", "Белок", ERule_Metric::ProteinGrams, ERule_Display_Unit::G, false},
	    {"macro_carbs", "Углеводы", ERule_Metric::CarbGrams, ERule_Display_Unit::G, false},
	    {"macro_fat", "Жиры", ERule_Metric::FatGrams, ERule_Display_Unit::G, false},
	    {"macro_calories", "Калории", ERule_Metric::CaloriesKcal, ERule_Display_Unit::Kcal, true},
	};

	const double values[] = {macro.Protein_G, macro.Carbs_G, macro.Fat_G, macro.Calories_Kcal};

	for (int i = 0; i < int(sizeof(macro_rules) / sizeof(macro_rules[0])); ++i)
	{
		const SMacro_Rule &entry = macro_rules[i];
		const QString code = QString::fromLatin1(entry.Code);
		int index = -1;

		for (int j = 0; j < Loaded_Rules.size(); ++j)
		{
			if (Loaded_Rules.at(j).Code == code)
			{
				index = j;
				break;
			}
		}

		SProgram_Rule rule;

		if (index >= 0)
		{
			// Существующее правило: название и прочие поля остаются за
			// пользователем, целями обновляются только числа.
			rule = Loaded_Rules.at(index);
		}
		else
		{
			rule.ID = QUuid::createUuid().toString(QUuid::WithoutBraces);
			rule.Program_Id = Program_Id_Text;
			rule.Category = ERule_Category::Nutrition;
			rule.Code = code;
			rule.Title = QString::fromUtf8(entry.Title);
			rule.Metric = entry.Metric;
			rule.Periodicity = ERule_Periodicity::PerDay;
			rule.Target_Unit = entry.Unit;
			rule.Display_Unit = entry.Unit;
			rule.Applies_To = ERule_Applies_To::AllDays;
			rule.Check_Source = ERule_Check_Source::SelfReport;
			rule.Enforcement = ERule_Enforcement::Soft;
		}

		// Цель поедет по расчёту в любом случае: «не ниже N г» для макросов,
		// «ровно C ккал» для калорийности.
		rule.Target_Min = values[i];
		rule.Has_Target_Min = entry.Is_Exact == false;
		rule.Target_Exact = values[i];
		rule.Has_Target_Exact = entry.Is_Exact;

		if (index >= 0)
			Loaded_Rules[index] = rule;
		else
			Loaded_Rules.append(rule);
	}

	if (Rule_Repository_Instance->Replace_Rules(Program_Id_Text, Loaded_Rules) == false)
	{
		Set_Error("undefined_error");
		emit changed();

		return false;
	}

	// Правила перечитываются из хранилища: модель обязана показывать то, что
	// сохранено, а не то, что собиралось в памяти.
	Load_Rules();
	emit changed();

	return true;
}
//----------------------------------------------------------------------------
void Avm_Program_Rules::Clear_All()
{
	Program_Id_Text.clear();
	Loaded_Rules.clear();
	Loaded_Checkins.clear();
	Rules->Set_Rows(QVector<QVariantMap>());
	Checkins->Set_Rows(QVector<QVariantMap>());
	Has_Adherence = false;
	Adherence_Number = 0;
	Has_Macro_Result_Value = false;
}
//----------------------------------------------------------------------------
bool Avm_Program_Rules::Load_Rules()
{
	if (Rule_Repository_Instance == 0)
		return false;

	if (Rule_Repository_Instance->Load_Rules(Program_Id_Text, Loaded_Rules) == false)
		return false;

	Rules->Set_Rows(Program_Rows::Build_Rules(Loaded_Rules, Today_States()));

	return true;
}
//----------------------------------------------------------------------------
bool Avm_Program_Rules::Load_Checkins()
{
	if (Rule_Repository_Instance == 0)
		return false;

	if (Rule_Repository_Instance->Load_Program_Checkins_Range(Program_Id_Text, Range_From_Text, Range_To_Text,
	                                                          Loaded_Checkins) == false)
		return false;

	Checkins->Set_Rows(Program_Rows::Build_Checkins(Loaded_Checkins));

	return true;
}
//----------------------------------------------------------------------------
// Окно отметок заканчивается сегодня и уходит в прошлое на periodDays.
//----------------------------------------------------------------------------
void Avm_Program_Rules::Apply_Period()
{
	QDate end = QDate::fromString(Today(), Qt::ISODate);

	if (end.isValid() == false)
	{
		Range_From_Text.clear();
		Range_To_Text.clear();

		return;
	}

	Range_To_Text = end.toString("yyyy-MM-dd");
	Range_From_Text = end.addDays(-(Period_Days_Number - 1)).toString("yyyy-MM-dd");
}
//----------------------------------------------------------------------------
QHash<QString, QString> Avm_Program_Rules::Today_States() const
{
	QHash<QString, QString> states;
	const QString today_text = Today();

	for (int i = 0; i < Loaded_Checkins.size(); ++i)
	{
		if (Loaded_Checkins.at(i).Date != today_text)
			continue;

		states.insert(Loaded_Checkins.at(i).Rule_Id, Program_Model::To_String(Loaded_Checkins.at(i).State));
	}

	return states;
}
//----------------------------------------------------------------------------
// Adherence считает доменный движок правил: «неизвестно» не входит ни в
// успех, ни в провал (§5.5), поэтому метрика без вердиктов отсутствует.
//----------------------------------------------------------------------------
void Avm_Program_Rules::Recalculate_Adherence()
{
	AsRule_Engine engine;
	SRule_Stats overall;

	Has_Adherence = false;
	Adherence_Number = 0;

	if (engine.Compute_Overall_Stats(Loaded_Rules, Loaded_Checkins, overall) == false)
		return;

	if (overall.Has_Adherence_Percent == false)
		return;

	Has_Adherence = true;
	Adherence_Number = overall.Adherence_Percent;
}
//----------------------------------------------------------------------------
QString Avm_Program_Rules::Today() const
{
	if (Today_Text_Override.isEmpty() == false)
		return Today_Text_Override;

	return QDate::currentDate().toString("yyyy-MM-dd");
}
//----------------------------------------------------------------------------
void Avm_Program_Rules::Clear_Error()
{
	Error_Key_Text.clear();
}
//----------------------------------------------------------------------------
void Avm_Program_Rules::Set_Error(const QString &in_key)
{
	Error_Key_Text = in_key;

	qCInfo(PROGRAM_Rules_VM_Log).noquote() << "rules vm failed" << in_key;
}
//----------------------------------------------------------------------------
bool Avm_Program_Rules::Has_Program() const
{
	return Program_Id_Text.isEmpty() == false;
}
//----------------------------------------------------------------------------
QString Avm_Program_Rules::programId() const
{
	return Program_Id_Text;
}
//----------------------------------------------------------------------------
QString Avm_Program_Rules::errorKey() const
{
	return Error_Key_Text;
}
//----------------------------------------------------------------------------
QString Avm_Program_Rules::today() const
{
	return Today();
}
//----------------------------------------------------------------------------
int Avm_Program_Rules::periodDays() const
{
	return Period_Days_Number;
}
//----------------------------------------------------------------------------
QString Avm_Program_Rules::rangeFrom() const
{
	return Range_From_Text;
}
//----------------------------------------------------------------------------
QString Avm_Program_Rules::rangeTo() const
{
	return Range_To_Text;
}
//----------------------------------------------------------------------------
int Avm_Program_Rules::checkinCount() const
{
	return Loaded_Checkins.size();
}
//----------------------------------------------------------------------------
double Avm_Program_Rules::adherencePercent() const
{
	return Adherence_Number;
}
//----------------------------------------------------------------------------
bool Avm_Program_Rules::hasAdherencePercent() const
{
	return Has_Adherence;
}
//----------------------------------------------------------------------------
bool Avm_Program_Rules::Has_Macro_Result() const
{
	return Has_Macro_Result_Value;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program_Rules::rules() const
{
	return Rules;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program_Rules::checkins() const
{
	return Checkins;
}
//----------------------------------------------------------------------------
