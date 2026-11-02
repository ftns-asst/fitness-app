#pragma once

#include "Data/Program/Day_Note_Repository.h"
#include "Data/Program/Measurement_Repository.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Data/Program/Stats_Repository.h"
#include "Data/Program/Workout_Repository.h"
#include "Domain/Diary_Calculator.h"
#include "Presentation/List_Model.h"

//-------------------------------------------------------------------------------------------------------------
// Дневник программы. Репозиториями владеет контекст; списки — дети ViewModel.
class Avm_Diary : public QObject
{
	Q_OBJECT
	Q_PROPERTY(AList_Model *days READ days CONSTANT)
	Q_PROPERTY(AList_Model *norms READ norms CONSTANT)
	Q_PROPERTY(AList_Model *summaries READ summaries CONSTANT)
	Q_PROPERTY(QVariantList selectedValues READ selectedValues NOTIFY changed)
	Q_PROPERTY(QString selectedNote READ selectedNote NOTIFY changed)
	Q_PROPERTY(QString selectedDate READ selectedDate NOTIFY changed)
	Q_PROPERTY(QVariant selectedWeight READ selectedWeight NOTIFY changed)
	Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)
	Q_PROPERTY(bool hasProgram READ Has_Program NOTIFY changed)
	Q_PROPERTY(QString programTitle READ programTitle NOTIFY changed)
public:
	Avm_Diary(AProgram_Repository *in_programs, ARule_Repository *in_rules, AWorkout_Repository *in_workouts,
	          AMeasurement_Repository *in_measurements, ADay_Note_Repository *in_notes, AStats_Repository *in_stats,
	          QObject *in_parent = 0);
	Q_INVOKABLE bool reload(const QString &in_user_id, const QString &in_program_id = QString());
	Q_INVOKABLE bool openDay(const QString &in_date);
	Q_INVOKABLE bool saveRuleValue(const QString &in_rule_id, const QVariant &in_value);
	Q_INVOKABLE bool saveWeight(const QVariant &in_value);
	Q_INVOKABLE bool saveNote(const QString &in_text);
	Q_INVOKABLE void setToday(const QString &in_date);
	AList_Model *days() const;
	AList_Model *norms() const;
	AList_Model *summaries() const;
	QVariantList selectedValues() const;
	QString selectedNote() const;
	QString selectedDate() const;
	QVariant selectedWeight() const;
	QString errorKey() const;
	bool Has_Program() const;
	QString programTitle() const;
signals:
	void changed();

private:
	QDate Today() const;
	bool Set_Error(const QString &in_key);
	void Clear_Data();
	bool Check_Editable();
	bool Refresh_Selection();
	bool Finish_Save();
	void Set_Rows(const QVector<SDay_Log> &in_days, const QVector<SDiary_Summary> &in_summaries);
	QVariantMap Rule_Row(const SProgram_Rule &in_rule) const;

	AProgram_Repository *Programs = 0;
	ARule_Repository *Rules = 0;
	AWorkout_Repository *Workouts = 0;
	AMeasurement_Repository *Measurements = 0;
	ADay_Note_Repository *Notes = 0;
	AStats_Repository *Stats = 0;
	AList_Model *Days = 0;
	AList_Model *Norms = 0;
	AList_Model *Summaries = 0;
	QString User_Id;
	SProgram Program;
	// Программы может не быть вовсе: тогда дневник показывает личные записи
	// (вес, заметки) за окно по умолчанию, а правила и дни остаются пустыми.
	bool Program_Loaded = false;
	QVector<SProgram_Rule> Loaded_Rules;
	QVector<SDay_Log> Day_Logs;
	QVariantMap Selection;
	QDate Pinned_Today;
	AError_State Error_State;
};
//-------------------------------------------------------------------------------------------------------------
