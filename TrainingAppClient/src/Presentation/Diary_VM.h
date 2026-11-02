#pragma once

#include "Data/Program/Day_Note_Repository.h"
#include "Data/Program/Measurement_Repository.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Data/Program/Stats_Repository.h"
#include "Data/Program/Workout_Repository.h"
#include "Domain/Diary_Calculator.h"
#include "Presentation/List_Model.h"

#include <QStringList>
#include <QVariantMap>

//-------------------------------------------------------------------------------------------------------------
// Дневник программы. Репозиториями владеет контекст; списки — дети ViewModel.
class Avm_Diary : public QObject
{
	Q_OBJECT
	Q_PROPERTY(AList_Model *days READ days CONSTANT)
	Q_PROPERTY(AList_Model *norms READ norms CONSTANT)
	Q_PROPERTY(AList_Model *summaries READ summaries CONSTANT)
	// Прошедшие тренировки: дневник — единственное место, откуда видно историю
	// занятий (§13 «Дневник»), отдельного раздела «История» в профиле нет.
	Q_PROPERTY(AList_Model *sessions READ sessions CONSTANT)
	// Поля формы замеров: вес плюс обхваты, нужные упражнениям активной программы
	// (без программы — грудь, талия, ягодицы). Форма одна на вес и обхваты:
	// «где сохранить замер» должен иметь один ответ.
	Q_PROPERTY(QStringList measurementKeys READ measurementKeys NOTIFY changed)
	// Замер выбранной даты и последний замер с обхватами: первый заполняет форму,
	// второй — подпись «обхваты измерены <дата>».
	Q_PROPERTY(QVariantMap selectedMeasurement READ selectedMeasurement NOTIFY changed)
	Q_PROPERTY(QVariantMap lastMeasurement READ lastMeasurement NOTIFY changed)
	// Тренировка выбранной даты: пустая карта — занятия в этот день не было.
	Q_PROPERTY(QVariantMap selectedSession READ selectedSession NOTIFY changed)
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
	// Замер дня одним объектом: ключи — «weight» и ключи обхватов (§3.8).
	// Заполненные поля перекрывают значения дня, пустые остаются как есть, поэтому
	// утренний вес не теряется при вечерней записи обхватов.
	Q_INVOKABLE bool saveMeasurements(const QVariantMap &in_values);
	Q_INVOKABLE bool saveNote(const QString &in_text);
	Q_INVOKABLE void setToday(const QString &in_date);
	AList_Model *days() const;
	AList_Model *norms() const;
	AList_Model *summaries() const;
	AList_Model *sessions() const;
	QStringList measurementKeys() const;
	QVariantMap selectedMeasurement() const;
	QVariantMap lastMeasurement() const;
	QVariantMap selectedSession() const;
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
	// Замер по дате; nullptr — записей этого дня нет.
	const SBody_Measurement *Find_Measurement(const QString &in_date) const;
	// Замер как карта «ключ → значение»: вес и заданные обхваты.
	QVariantMap Measurement_Map(const SBody_Measurement &in_measurement) const;
	// Поля формы замеров: вес плюс обхваты программы либо базовые три.
	void Prepare_Measurement_Keys();
	void Fill_Measurement_Values(SBody_Measurement &io_measurement, const QVariantMap &in_values, bool &out_any) const;
	void Set_Rows(const QVector<SDay_Log> &in_days, const QVector<SDiary_Summary> &in_summaries);
	void Set_Session_Rows();
	QVariantMap Rule_Row(const SProgram_Rule &in_rule) const;
	QVariantMap Session_Row(const SWorkout_Session &in_session) const;

	AProgram_Repository *Programs = 0;
	ARule_Repository *Rules = 0;
	AWorkout_Repository *Workouts = 0;
	AMeasurement_Repository *Measurements = 0;
	ADay_Note_Repository *Notes = 0;
	AStats_Repository *Stats = 0;
	AList_Model *Days = 0;
	AList_Model *Norms = 0;
	AList_Model *Summaries = 0;
	AList_Model *Sessions = 0;
	QString User_Id;
	SProgram Program;
	// Программы может не быть вовсе: тогда дневник показывает личные записи
	// (вес, заметки) за окно по умолчанию, а правила и дни остаются пустыми.
	bool Program_Loaded = false;
	QVector<SProgram_Rule> Loaded_Rules;
	QVector<SDay_Log> Day_Logs;
	// Дни программы: в истории тренировок нужен заголовок дня, а сессия хранит
	// только его идентификатор.
	QVector<SProgram_Day> Loaded_Days;
	// Расписание недели — для сессий без привязки к дню плана: день восстанавливается
	// по дню недели даты тренировки.
	QVector<SWeek_Slot> Loaded_Slots;
	// Замеры и тренировки окна: из них собираются дельты веса, подпись обхватов и
	// список прошедших тренировок — без повторных запросов к БД.
	QVector<SBody_Measurement> Loaded_Measurements;
	QVector<SWorkout_Session> Loaded_Sessions;
	QStringList Measurement_Keys;
	QVariantMap Selection;
	QDate Pinned_Today;
	AError_State Error_State;
};
//-------------------------------------------------------------------------------------------------------------
