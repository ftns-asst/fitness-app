#pragma once

#include "Domain/Error_State.h"
#include "Domain/Program_Model.h"

#include <QObject>
#include <QVector>

//-------------------------------------------------------------------------------------------------------------
// Числа задаёт пользователь. Нулевые обязательные значения не являются нормами по умолчанию.
struct SPSMF_Template_Request
{
	QString User_Id;
	QString Start_Date;
	double Calories_Max = 0;
	double Protein_Min = 0;
	double Carbs_Max = 0;
	double Fat_Max = 0;
	bool Has_Water_Min = false;
	double Water_Min_Ml = 0;
	bool Has_Sleep_Min = false;
	double Sleep_Min_Hours = 0;
	bool Has_Steps_Min = false;
	double Steps_Min = 0;
	Program_Model::SBody_Measurement Initial_Measurement;
};
//-------------------------------------------------------------------------------------------------------------
// Полный новый снимок: интервал замеров не является полем SProgram или STerm.
struct SProgram_Template_Draft
{
	Program_Model::SProgram Program;
	QVector<Program_Model::SGoal_Target> Targets;
	QVector<Program_Model::SProgram_Day> Days;
	QVector<Program_Model::SDay_Exercise> Exercises;
	QVector<Program_Model::SWeek_Slot> Slots;
	QVector<Program_Model::SProgram_Rule> Rules;
	int Measurement_Interval_Days = 3;
	bool Has_Initial_Measurement = false;
	Program_Model::SBody_Measurement Initial_Measurement;
};
//-------------------------------------------------------------------------------------------------------------
// Адаптер существующего хранения программ. false гарантирует отсутствие любых записей этого вызова.
// Все части снимка, начальная ревизия и слияние замера записываются ОДНОЙ транзакцией.
// Перед записью: проверка владельца, ссылок/идентификаторов и метаданных локальных builtin-упражнений.
// Замер сливается по (user, date), незаданные поля сохраняются. Активная программа не меняется.
// Текущие Insert_Program_Full + Replace_Rules нельзя просто вызвать подряд: у них отдельные транзакции.
class AProgram_Template_Repository_Port
{
public:
	virtual ~AProgram_Template_Repository_Port() = default;
	virtual bool Insert_Template_Draft(const QString &in_user_id, const SProgram_Template_Draft &in_draft) = 0;
	virtual QString Get_Last_Error() const = 0;
};
//-------------------------------------------------------------------------------------------------------------
class AProgram_Template_Service : public QObject
{
	Q_OBJECT

public:
	explicit AProgram_Template_Service(AProgram_Template_Repository_Port *in_repository, QObject *in_parent = 0);

	QString Get_Last_Error() const;
	static QStringList Exercise_Ids();
	// in_user_id — доверенный текущий пользователь; User_Id запроса и замера не могут ему противоречить.
	bool Create_PSMF_Draft(const QString &in_user_id, const SPSMF_Template_Request &in_request,
	                       QString &out_program_id);

private:
	bool Validate_Request(const QString &in_user_id, const SPSMF_Template_Request &in_request);
	void Build_Draft(const SPSMF_Template_Request &in_request, SProgram_Template_Draft &out_draft);
	void Add_Exercise(SProgram_Template_Draft &io_draft, int in_day_index, const QString &in_exercise_id);
	void Add_Rule(SProgram_Template_Draft &io_draft, const QString &in_code, const QString &in_title,
	              Program_Model::ERule_Category in_category, Program_Model::ERule_Metric in_metric,
	              Program_Model::ERule_Display_Unit in_unit, double in_value, bool in_is_minimum);
	static QString New_Id();
	static bool Is_Valid_Date(const QString &in_date);
	static bool Is_Nonnegative_Number(double in_value);

	// Не владеет: адаптер живёт дольше сервиса.
	AProgram_Template_Repository_Port *Repository_Instance;
	AError_State Error_State;
};
//-------------------------------------------------------------------------------------------------------------
