#include "Data/Program/Rule_Repository.h"

#include <QDate>
#include <QDateTime>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

//-------------------------------------------------------------------------------------------------------------
static QJsonObject Rule_Snapshot_Object(const SProgram_Rule &in_rule)
{
	QJsonObject object;
	QJsonArray applies_days;

	object.insert("id", in_rule.ID);
	object.insert("category", To_String(in_rule.Category));
	object.insert("code", in_rule.Code);
	object.insert("title", in_rule.Title);
	object.insert("metric", To_String(in_rule.Metric));
	object.insert("periodicity", To_String(in_rule.Periodicity));

	if (in_rule.Has_Target_Min)
		object.insert("targetMin", in_rule.Target_Min);
	if (in_rule.Has_Target_Max)
		object.insert("targetMax", in_rule.Target_Max);
	if (in_rule.Has_Target_Exact)
		object.insert("targetExact", in_rule.Target_Exact);

	object.insert("targetUnit", To_String(in_rule.Target_Unit));
	object.insert("appliesTo", To_String(in_rule.Applies_To));

	for (int i = 0; i < in_rule.Applies_Days.size(); ++i)
		applies_days.append(in_rule.Applies_Days.at(i));
	if (!applies_days.isEmpty())
		object.insert("appliesDays", applies_days);

	object.insert("checkSource", To_String(in_rule.Check_Source));
	object.insert("enforcement", To_String(in_rule.Enforcement));

	if (!in_rule.Active_From.isEmpty())
		object.insert("activeFrom", in_rule.Active_From);
	if (!in_rule.Active_To.isEmpty())
		object.insert("activeTo", in_rule.Active_To);

	object.insert("displayUnit", To_String(in_rule.Display_Unit));

	if (!in_rule.Note.isEmpty())
		object.insert("note", in_rule.Note);

	return object;
}

//-------------------------------------------------------------------------------------------------------------
bool ARule_Repository::Is_Valid_Date(const QString &in_date) const
{
	QDate date = QDate::fromString(in_date, Qt::ISODate);

	return date.isValid() && date.toString("yyyy-MM-dd") == in_date;
}
//-------------------------------------------------------------------------------------------------------------
ARule_Repository::ARule_Repository(AsSql_Database *in_database, QObject *in_parent)
    : AProgram_Repository_Base(in_database, in_parent)
{
}
//-------------------------------------------------------------------------------------------------------------
bool ARule_Repository::Read_Program_State(const QString &in_program_id, EProgram_State &out_state, int &out_revision)
{
	QSqlQuery query(Connection());
	EProgram_State state = EProgram_State::Draft;

	query.prepare("SELECT state, revision FROM programs WHERE id = ?");
	query.addBindValue(in_program_id);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось прочитать программу: %1").arg(query.lastError().text()));

	if (!query.next())
		return Set_Last_Error(QString("Программа %1 не найдена").arg(in_program_id));

	if (!From_String(query.value(0).toString(), state))
		return Set_Last_Error(QString("Неизвестное состояние программы: %1").arg(query.value(0).toString()));

	out_state = state;
	out_revision = query.value(1).toInt();

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool ARule_Repository::Load_Rules(const QString &in_program_id, QVector<SProgram_Rule> &out_rules)
{
	QSqlQuery query(Connection());
	SProgram_Rule rule;

	Clear_Last_Error();
	out_rules.clear();

	if (!Check_Open())
		return false;

	query.prepare("SELECT id, category, code, title, metric, periodicity, target_min, target_max, target_exact, "
	              "target_unit, applies_to, applies_days, check_source, enforcement, active_from, active_to, "
	              "display_unit, note FROM program_rules WHERE program_id = ? ORDER BY id");
	query.addBindValue(in_program_id);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось прочитать правила: %1").arg(query.lastError().text()));

	while (query.next())
	{
		rule = SProgram_Rule();
		rule.Program_Id = in_program_id;
		rule.ID = query.value(0).toString();
		From_String(query.value(1).toString(), rule.Category);
		rule.Code = query.value(2).toString();
		rule.Title = query.value(3).toString();
		From_String(query.value(4).toString(), rule.Metric);
		From_String(query.value(5).toString(), rule.Periodicity);

		if (!query.isNull(6))
		{
			rule.Has_Target_Min = true;
			rule.Target_Min = query.value(6).toDouble();
		}

		if (!query.isNull(7))
		{
			rule.Has_Target_Max = true;
			rule.Target_Max = query.value(7).toDouble();
		}

		if (!query.isNull(8))
		{
			rule.Has_Target_Exact = true;
			rule.Target_Exact = query.value(8).toDouble();
		}

		From_String(query.value(9).toString(), rule.Target_Unit);
		From_String(query.value(10).toString(), rule.Applies_To);

		if (!query.isNull(11))
			Parse_Json_Array(query.value(11).toString(), rule.Applies_Days);

		From_String(query.value(12).toString(), rule.Check_Source);
		From_String(query.value(13).toString(), rule.Enforcement);
		rule.Active_From = query.value(14).toString();
		rule.Active_To = query.value(15).toString();
		From_String(query.value(16).toString(), rule.Display_Unit);
		rule.Note = query.value(17).toString();
		out_rules.append(rule);
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
// Has_Advice_Text живёт в домене (Program_Model): та же проверка нужна и
// движку правил, и здесь, а две реализации «непустого текста» разошлись бы
// — одна приняла бы строку из пробелов, другая нет.
//-------------------------------------------------------------------------------------------------------------
bool ARule_Repository::Validate_Rules(const QVector<SProgram_Rule> &in_rules) const
{
	for (int i = 0; i < in_rules.size(); ++i)
	{
		const SProgram_Rule &rule = in_rules.at(i);

		if (rule.ID.isEmpty())
			return false;

		if (rule.Title.isEmpty())
			return false;

		// §3.6: правило отвечает на вопрос «сколько» — цель обязательна
		// (CHECK в схеме дублирует это на уровне БД). Исключение одно:
		// рекомендация (категория «Другое») отвечает на вопрос «что делать»,
		// поэтому вместо цели у неё текст, и он обязателен — иначе «Другое»
		// стало бы способом завести правило вообще без содержания.
		const bool has_target = rule.Has_Target_Min || rule.Has_Target_Max || rule.Has_Target_Exact;

		if (!has_target && Get_Rule_Kind(rule) != ERule_Kind::Advice)
			return false;
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool ARule_Repository::Insert_Rules_Internal(const QString &in_program_id, const QVector<SProgram_Rule> &in_rules)
{
	QSqlQuery query(Connection());

	if (in_rules.isEmpty())
		return true;

	query.prepare("INSERT INTO program_rules(id, program_id, category, code, title, metric, periodicity, target_min, "
	              "target_max, target_exact, target_unit, applies_to, applies_days, check_source, enforcement, "
	              "active_from, active_to, display_unit, note) "
	              "VALUES(?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?, ?)");

	for (int i = 0; i < in_rules.size(); ++i)
	{
		const SProgram_Rule &rule = in_rules.at(i);

		query.addBindValue(rule.ID);
		query.addBindValue(in_program_id);
		query.addBindValue(To_String(rule.Category));
		query.addBindValue(rule.Code);
		query.addBindValue(rule.Title);
		query.addBindValue(To_String(rule.Metric));
		query.addBindValue(To_String(rule.Periodicity));
		query.addBindValue(Number_Column(rule.Target_Min, rule.Has_Target_Min));
		query.addBindValue(Number_Column(rule.Target_Max, rule.Has_Target_Max));
		query.addBindValue(Number_Column(rule.Target_Exact, rule.Has_Target_Exact));
		query.addBindValue(To_String(rule.Target_Unit));
		query.addBindValue(To_String(rule.Applies_To));
		query.addBindValue(rule.Applies_Days.isEmpty() ? QVariant() : QVariant(To_Json_Array(rule.Applies_Days)));
		query.addBindValue(To_String(rule.Check_Source));
		query.addBindValue(To_String(rule.Enforcement));
		query.addBindValue(Text_Column(rule.Active_From));
		query.addBindValue(Text_Column(rule.Active_To));
		query.addBindValue(To_String(rule.Display_Unit));
		query.addBindValue(Text_Column(rule.Note));

		if (!query.exec())
			return Set_Last_Error(
			    QString("Не удалось сохранить правило %1: %2").arg(rule.ID).arg(query.lastError().text()));
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
QString ARule_Repository::Build_Rules_Snapshot(const QString &in_program_id)
{
	QVector<SProgram_Rule> rules;
	QJsonArray array;

	if (!Load_Rules(in_program_id, rules))
		return QString();

	for (int i = 0; i < rules.size(); ++i)
		array.append(Rule_Snapshot_Object(rules.at(i)));

	return QString::fromUtf8(QJsonDocument(array).toJson(QJsonDocument::Compact));
}
//-------------------------------------------------------------------------------------------------------------
bool ARule_Repository::Replace_Rules(const QString &in_program_id, const QVector<SProgram_Rule> &in_rules)
{
	EProgram_State state = EProgram_State::Draft;
	QSqlQuery query(Connection());
	QString snapshot;
	int old_revision = 0;

	Clear_Last_Error();

	if (!Check_Open())
		return false;

	if (!Read_Program_State(in_program_id, state, old_revision))
		return false;

	// Инвариант 24 (§4): read-only-состояния правку правил запрещают.
	if (state == EProgram_State::Completed || state == EProgram_State::Cancelled || state == EProgram_State::Archived)
		return Set_Last_Error(
		    QString("Программа %1 в состоянии %2: правка правил запрещена").arg(in_program_id).arg(To_String(state)));

	if (!Validate_Rules(in_rules))
		return Set_Last_Error(
		    "Каждое правило требует id, заголовок и цель; для рекомендации («Другое») вместо цели — текст");

	if (!Connection().transaction())
		return Set_Last_Error("Не удалось начать транзакцию");

	snapshot = Build_Rules_Snapshot(in_program_id);

	if (snapshot.isEmpty())
	{
		Connection().rollback();
		return false;
	}

	query.prepare("DELETE FROM program_rules WHERE program_id = ?");
	query.addBindValue(in_program_id);

	if (!query.exec() || !Insert_Rules_Internal(in_program_id, in_rules) ||
	    !Bump_Revision_Internal(in_program_id, old_revision) ||
	    !Write_Revision_Row(in_program_id, old_revision + 1, ERevision_Reason::Rules, "Правка правил программы",
	                        snapshot))
	{
		Connection().rollback();

		if (!Error_State.Has_Error())
			Set_Last_Error(QString("Не удалось заменить правила: %1").arg(query.lastError().text()));

		return false;
	}

	if (!Connection().commit())
		return Set_Last_Error("Не удалось зафиксировать правила");

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool ARule_Repository::Save_Checkin(const SRule_Checkin &in_checkin)
{
	QSqlQuery query(Connection());
	QString checkin_id = in_checkin.ID;
	QString current_timestamp;

	Clear_Last_Error();

	if (!Check_Open())
		return false;

	if (in_checkin.Rule_Id.isEmpty() || in_checkin.Program_Id.isEmpty() || in_checkin.Date.isEmpty() ||
	    in_checkin.Unit.isEmpty())
		return Set_Last_Error("Отметке правила нужны ruleId, programId, date и unit");

	if (!Is_Valid_Date(in_checkin.Date))
		return Set_Last_Error(QString("Некорректная дата отметки: %1").arg(in_checkin.Date));

	if (checkin_id.isEmpty())
		checkin_id = QUuid::createUuid().toString(QUuid::WithoutBraces);
	current_timestamp = QDateTime::currentDateTimeUtc().toString(Qt::ISODate);

	query.prepare("INSERT INTO rule_checkins(id, rule_id, program_id, date, value, unit, state, source, "
	              "session_id, meal_log_id, note, created_at, updated_at) "
	              "SELECT ?, id, program_id, ?, ?, ?, ?, ?, ?, ?, ?, ?, ? FROM program_rules "
	              "WHERE id = ? AND program_id = ? "
	              "ON CONFLICT(rule_id, date) DO UPDATE SET "
	              "value = excluded.value, unit = excluded.unit, state = excluded.state, source = excluded.source, "
	              "session_id = excluded.session_id, meal_log_id = excluded.meal_log_id, note = excluded.note, "
	              "updated_at = excluded.updated_at "
	              "WHERE rule_checkins.source != 'manual' OR excluded.source != 'computed'");
	query.addBindValue(checkin_id);
	query.addBindValue(in_checkin.Date);
	query.addBindValue(Number_Column(in_checkin.Value, in_checkin.Has_Value));
	query.addBindValue(in_checkin.Unit);
	query.addBindValue(To_String(in_checkin.State));
	query.addBindValue(To_String(in_checkin.Source));
	query.addBindValue(Text_Column(in_checkin.Session_Id));
	query.addBindValue(Text_Column(in_checkin.Meal_Log_Id));
	query.addBindValue(Text_Column(in_checkin.Note));
	query.addBindValue(current_timestamp);
	query.addBindValue(current_timestamp);
	query.addBindValue(in_checkin.Rule_Id);
	query.addBindValue(in_checkin.Program_Id);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось сохранить отметку: %1").arg(query.lastError().text()));
	if (query.numRowsAffected() == 0)
		return Set_Last_Error(
		    "Проверьте программу и правило: правило не принадлежит программе либо расчётная отметка заменяет ручную");

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool ARule_Repository::Delete_Checkin(const QString &in_rule_id, const QString &in_date)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();

	if (!Check_Open())
		return false;

	if (in_rule_id.isEmpty() || !Is_Valid_Date(in_date))
		return Set_Last_Error("Для снятия отметки нужны ruleId и корректная дата");

	query.prepare("DELETE FROM rule_checkins WHERE rule_id = ? AND date = ?");
	query.addBindValue(in_rule_id);
	query.addBindValue(in_date);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось снять отметку: %1").arg(query.lastError().text()));

	return true;
}
//-------------------------------------------------------------------------------------------------------------
static void Read_Checkin_Row(QSqlQuery &in_query, SRule_Checkin &out_checkin)
{
	out_checkin = SRule_Checkin();
	out_checkin.ID = in_query.value(0).toString();
	out_checkin.Rule_Id = in_query.value(1).toString();
	out_checkin.Program_Id = in_query.value(2).toString();
	out_checkin.Date = in_query.value(3).toString();

	if (!in_query.isNull(4))
	{
		out_checkin.Has_Value = true;
		out_checkin.Value = in_query.value(4).toDouble();
	}

	out_checkin.Unit = in_query.value(5).toString();
	From_String(in_query.value(6).toString(), out_checkin.State);
	From_String(in_query.value(7).toString(), out_checkin.Source);
	out_checkin.Session_Id = in_query.value(8).toString();
	out_checkin.Meal_Log_Id = in_query.value(9).toString();
	out_checkin.Note = in_query.value(10).toString();
	out_checkin.Created_At = in_query.value(11).toString();
	out_checkin.Updated_At = in_query.value(12).toString();
}
//-------------------------------------------------------------------------------------------------------------
// Чтение отметки по ключу (rule, date) без ошибки на «не найдено»: нужно и
// для публичного Find_Checkin, и для upsert в Save_Checkin.

//-------------------------------------------------------------------------------------------------------------
bool ARule_Repository::Read_Checkin_By_Key(const QString &in_rule_id, const QString &in_date,
                                           SRule_Checkin &out_checkin, bool &out_found)
{
	QSqlQuery query(Connection());

	out_checkin = SRule_Checkin();
	out_found = false;

	query.prepare("SELECT id, rule_id, program_id, date, value, unit, state, source, session_id, meal_log_id, note, "
	              "created_at, updated_at FROM rule_checkins WHERE rule_id = ? AND date = ?");
	query.addBindValue(in_rule_id);
	query.addBindValue(in_date);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось прочитать отметку: %1").arg(query.lastError().text()));

	if (!query.next())
		return true;

	Read_Checkin_Row(query, out_checkin);
	out_found = true;

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool ARule_Repository::Find_Checkin(const QString &in_rule_id, const QString &in_date, SRule_Checkin &out_checkin)
{
	bool found = false;

	Clear_Last_Error();
	out_checkin = SRule_Checkin();

	if (!Check_Open())
		return false;

	if (!Read_Checkin_By_Key(in_rule_id, in_date, out_checkin, found))
		return false;

	if (!found)
		return Set_Last_Error(QString("Отметка правила %1 за %2 не найдена").arg(in_rule_id).arg(in_date));

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool ARule_Repository::Load_Checkins_Range(const QString &in_rule_id, const QString &in_date_from,
                                           const QString &in_date_to, QVector<SRule_Checkin> &out_checkins)
{
	QSqlQuery query(Connection());
	SRule_Checkin checkin;

	Clear_Last_Error();
	out_checkins.clear();

	if (!Check_Open())
		return false;

	query.prepare("SELECT id, rule_id, program_id, date, value, unit, state, source, session_id, meal_log_id, note, "
	              "created_at, updated_at FROM rule_checkins WHERE rule_id = ? AND date >= ? AND date <= ? "
	              "ORDER BY date");
	query.addBindValue(in_rule_id);
	query.addBindValue(in_date_from);
	query.addBindValue(in_date_to);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось прочитать отметки: %1").arg(query.lastError().text()));

	while (query.next())
	{
		Read_Checkin_Row(query, checkin);
		out_checkins.append(checkin);
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool ARule_Repository::Load_Program_Checkins_Range(const QString &in_program_id, const QString &in_date_from,
                                                   const QString &in_date_to, QVector<SRule_Checkin> &out_checkins)
{
	QSqlQuery query(Connection());
	SRule_Checkin checkin;

	Clear_Last_Error();
	out_checkins.clear();

	if (!Check_Open())
		return false;

	query.prepare("SELECT id, rule_id, program_id, date, value, unit, state, source, session_id, meal_log_id, note, "
	              "created_at, updated_at FROM rule_checkins WHERE program_id = ? AND date >= ? AND date <= ? "
	              "ORDER BY rule_id, date");
	query.addBindValue(in_program_id);
	query.addBindValue(in_date_from);
	query.addBindValue(in_date_to);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось прочитать отметки программы: %1").arg(query.lastError().text()));

	while (query.next())
	{
		Read_Checkin_Row(query, checkin);
		out_checkins.append(checkin);
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
