#pragma once

#include "Data/Program/Program_Repository_Base.h"
#include "Domain/Program_Model.h"

#include <QString>
#include <QVector>

//----------------------------------------------------------------------------
// Локальный репозиторий правил программы и отметок (срез W4, issue 23).
////
// `Replace_Rules` — редактирование набора правил: валидирует вход и
// состояние программы (инвариант 24 §4), транзакционно заменяет набор,
// инкрементирует programs.revision и пишет строку program_revisions со
// снимком старых правил.
//
// `Save_Checkin` — upsert на пару (rule, date) с защитой §3.7: отметка
// `source = computed` не может перезаписать `manual`; ручная правка
// перекрывает расчётную отметку этого дня. Отметки доступны и в
// read-only-состояниях программы (инвариант 24: ретроспектива).
//----------------------------------------------------------------------------
class ARule_Repository : public AProgram_Repository_Base
{
	Q_OBJECT

public:
	explicit ARule_Repository(AsSql_Database *in_database, QObject *in_parent = 0);

	// Чтение правил и отметок.
	bool Load_Rules(const QString &in_program_id, QVector<SProgram_Rule> &out_rules);
	bool Find_Checkin(const QString &in_rule_id, const QString &in_date, SRule_Checkin &out_checkin);
	bool Load_Checkins_Range(const QString &in_rule_id, const QString &in_date_from, const QString &in_date_to,
	                         QVector<SRule_Checkin> &out_checkins);
	bool Load_Program_Checkins_Range(const QString &in_program_id, const QString &in_date_from,
	                                 const QString &in_date_to, QVector<SRule_Checkin> &out_checkins);

	// Запись.
	bool Replace_Rules(const QString &in_program_id, const QVector<SProgram_Rule> &in_rules);
	bool Save_Checkin(const SRule_Checkin &in_checkin);
	// Снятие отметки за день: строка удаляется, правило возвращается в «нет
	// данных» (Unknown сохранять нельзя — отметка без факта не имеет смысла).
	bool Delete_Checkin(const QString &in_rule_id, const QString &in_date);

private:
	bool Read_Program_State(const QString &in_program_id, EProgram_State &out_state, int &out_revision);
	bool Insert_Rules_Internal(const QString &in_program_id, const QVector<SProgram_Rule> &in_rules);
	bool Validate_Rules(const QVector<SProgram_Rule> &in_rules) const;
	QString Build_Rules_Snapshot(const QString &in_program_id);
	bool Is_Valid_Date(const QString &in_date) const;

	bool Read_Checkin_By_Key(const QString &in_rule_id, const QString &in_date, SRule_Checkin &out_checkin,
	                         bool &out_found);
};
//----------------------------------------------------------------------------
