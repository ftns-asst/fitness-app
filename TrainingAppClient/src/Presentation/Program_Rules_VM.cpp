#include "Presentation/Program_Rules_VM.h"

#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Presentation/Program_Rows.h"

#include <QDate>
#include <QLoggingCategory>

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
                                     QObject *in_parent)
    : QObject(in_parent), Program_Repository_Instance(in_program_repository),
      Rule_Repository_Instance(in_rule_repository), Rules(new AList_Model(this)), Checkins(new AList_Model(this))
{
	Rules->Set_Roles(Program_Rows::Rule_Roles());
	Checkins->Set_Roles(Program_Rows::Checkin_Roles());
}
// — QML-мост (camelCase) —
// Обёртки отделены от методов предметной области: контракт экрана не должен
// повторять внутренний стиль имён (§2 CODE_STYLE) и зависеть от него.
bool Avm_Program_Rules::reload(const QString &in_user_id)
{
	return Reload(in_user_id);
}

bool Avm_Program_Rules::setPeriodDays(int in_days)
{
	return Set_Period_Days(in_days);
}

bool Avm_Program_Rules::saveCheckin(const QString &in_rule_id, const QString &in_state, double in_value,
                                    bool in_has_value, const QString &in_note)
{
	return Save_Checkin(in_rule_id, in_state, in_value, in_has_value, in_note);
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

	// Отметки читаются раньше правил: роль todayState строится из них, иначе
	// состояние за сегодня всегда оставалось бы «неизвестно».
	if (Load_Checkins() == false || Load_Rules() == false)
	{
		Clear_All();
		Set_Error("undefined_error");

		return false;
	}

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
		Load_Checkins();

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

	// Порядок тот же, что в Reload: сначала отметки, затем правила.
	Load_Checkins();
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
	Recalculate_Adherence();

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
