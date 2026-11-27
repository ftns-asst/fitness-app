#include "Presentation/Program_Templates_VM.h"

#include "Data/Exercises/Exercises_Repository.h"
#include <QtMath>

Q_LOGGING_CATEGORY(PROGRAM_Templates_VM_Log, "trainingapp.program.templates.vm")

//-------------------------------------------------------------------------------------------------------------
Avm_Program_Templates::Avm_Program_Templates(AProgram_Template_Service *in_service, QObject *in_parent)
    : QObject(in_parent), Service_Instance(in_service), Error_State(&PROGRAM_Templates_VM_Log())
{
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Program_Templates::createDraft(const QString &in_user_id, const QString &in_start_date,
                                        const QVariantMap &in_preset, const QVariantMap &in_measurements)
{
	return Create_Draft(in_user_id, in_start_date, in_preset, in_measurements);
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Program_Templates::Create_Draft(const QString &in_user_id, const QString &in_start_date,
                                         const QVariantMap &in_preset, const QVariantMap &in_measurements)
{
	SPSMF_Template_Request request;
	Error_State.Clear_Last_Error();
	Error_Key_Text.clear();
	Created_Program_Id.clear();
	if (Service_Instance == 0)
		return Fail("create_failed", "Сервис шаблонов не подключён");
	if (!Parse_Request(in_user_id, in_start_date, in_preset, in_measurements, request))
		return Fail("invalid_input", "Проверьте числовые значения и названия полей");
	if (!Service_Instance->Create_PSMF_Draft(in_user_id, request, Created_Program_Id))
		return Fail("create_failed", Service_Instance->Get_Last_Error());

	emit changed();
	emit templateCreated(Created_Program_Id);
	return true;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Program_Templates::programId() const
{
	return Created_Program_Id;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Program_Templates::errorKey() const
{
	return Error_Key_Text;
}
//-------------------------------------------------------------------------------------------------------------
QString Avm_Program_Templates::errorDetail() const
{
	return Error_State.Get_Last_Error();
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Program_Templates::Parse_Request(const QString &in_user_id, const QString &in_start_date,
                                          const QVariantMap &in_preset, const QVariantMap &in_measurements,
                                          SPSMF_Template_Request &out_request)
{
	const QStringList preset_keys = QStringList() << "caloriesMax" << "proteinMin" << "carbsMax" << "fatMax"
	                                              << "waterMinMl" << "sleepMinHours" << "stepsMin";
	QStringList measurement_keys;
	double value = 0;

	measurement_keys << "weightKg";
	for (int i = 0; i < (int)Program_Model::EMeasurement_Key::Max; ++i)
		measurement_keys << Program_Model::To_String((Program_Model::EMeasurement_Key)i) + "Cm";
	for (const QString &key : in_preset.keys())
	{
		if (!preset_keys.contains(key) || !Read_Number(in_preset, key, value))
			return false;
	}
	for (const QString &key : in_measurements.keys())
	{
		if (!measurement_keys.contains(key) || !Read_Number(in_measurements, key, value))
			return false;
	}

	out_request.User_Id = in_user_id;
	out_request.Start_Date = in_start_date;
	if (!Read_Number(in_preset, "caloriesMax", out_request.Calories_Max) ||
	    !Read_Number(in_preset, "proteinMin", out_request.Protein_Min) ||
	    !Read_Number(in_preset, "carbsMax", out_request.Carbs_Max) ||
	    !Read_Number(in_preset, "fatMax", out_request.Fat_Max))
		return false;
	out_request.Has_Water_Min = in_preset.contains("waterMinMl");
	if (out_request.Has_Water_Min)
		Read_Number(in_preset, "waterMinMl", out_request.Water_Min_Ml);
	out_request.Has_Sleep_Min = in_preset.contains("sleepMinHours");
	if (out_request.Has_Sleep_Min)
		Read_Number(in_preset, "sleepMinHours", out_request.Sleep_Min_Hours);
	out_request.Has_Steps_Min = in_preset.contains("stepsMin");
	if (out_request.Has_Steps_Min)
		Read_Number(in_preset, "stepsMin", out_request.Steps_Min);
	out_request.Initial_Measurement.Has_Weight_Kg = in_measurements.contains("weightKg");
	if (out_request.Initial_Measurement.Has_Weight_Kg)
		Read_Number(in_measurements, "weightKg", out_request.Initial_Measurement.Weight_Kg);
	for (int i = 0; i < (int)Program_Model::EMeasurement_Key::Max; ++i)
	{
		QString key = Program_Model::To_String((Program_Model::EMeasurement_Key)i) + "Cm";
		if (in_measurements.contains(key))
		{
			Read_Number(in_measurements, key, value);
			Program_Model::Set_Measurement_Value(out_request.Initial_Measurement, (Program_Model::EMeasurement_Key)i,
			                                     value);
		}
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Program_Templates::Fail(const QString &in_key, const QString &in_detail)
{
	Error_Key_Text = in_key;
	Error_State.Set_Last_Error(in_detail);
	emit changed();
	return false;
}
//-------------------------------------------------------------------------------------------------------------
bool Avm_Program_Templates::Read_Number(const QVariantMap &in_values, const QString &in_key, double &out_value)
{
	QVariant value = in_values.value(in_key);
	bool is_valid = false;
	if (!value.isValid() || value.isNull() || value.metaType().id() == QMetaType::Bool ||
	    (value.metaType().id() != QMetaType::QString && !value.metaType().flags().testFlag(QMetaType::IsEnumeration) &&
	     value.metaType().id() != QMetaType::Int && value.metaType().id() != QMetaType::UInt &&
	     value.metaType().id() != QMetaType::LongLong && value.metaType().id() != QMetaType::ULongLong &&
	     value.metaType().id() != QMetaType::Double && value.metaType().id() != QMetaType::Float))
		return false;
	if (value.metaType().flags().testFlag(QMetaType::IsEnumeration))
		return false;
	out_value = value.toDouble(&is_valid);
	return is_valid && qIsFinite(out_value);
}
//-------------------------------------------------------------------------------------------------------------

QStringList Avm_Program_Templates::measurementKeys() const
{
	QStringList muscles;
	const QStringList ids = AProgram_Template_Service::Exercise_Ids();
	for (const SExercises_Item &exercise : AExercises_Repository::Base_Exercises())
		if (ids.contains(exercise.ID))
			muscles.append(exercise.Muscle_Groups);
	return Program_Model::Measurement_Keys_For_Muscles(muscles);
}
//-------------------------------------------------------------------------------------------------------------
