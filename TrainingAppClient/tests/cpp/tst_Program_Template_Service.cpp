#include "Data/Database/Sql_Database.h"
#include "Data/Program/Measurement_Repository.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Domain/Program_Template_Service.h"
#include "Presentation/Program_Edit_VM.h"
#include <QSqlQuery>

#include <QtTest>

#include <limits>

//-------------------------------------------------------------------------------------------------------------
class ATemplate_Test_Store : public AProgram_Template_Repository_Port
{
public:
	bool Insert_Template_Draft(const QString &in_user_id, const SProgram_Template_Draft &in_draft) override
	{
		++Calls;
		if (Fail || in_draft.Program.User_Id != in_user_id)
			return false;
		Drafts.append(in_draft);
		return true;
	}
	QString Get_Last_Error() const override
	{
		return QString("atomic_insert_failed");
	}
	QVector<SProgram_Template_Draft> Drafts;
	int Calls = 0;
	bool Fail = false;
};
//-------------------------------------------------------------------------------------------------------------
class AProgram_Template_Service_Test : public QObject
{
	Q_OBJECT

private slots:
	void createsIndependentDraftWithUserRulesAndRotation();
	void rejectsInvalidInputBeforePersistence();
	void rejectsForeignMeasurements();
	void acceptsPartialMeasurementsAndDoesNotInventTargets();
	void persistenceFailureDoesNotPublishAnId();
	void missingStoreIsAnError();
	void realStorageAndEditorPreserveRotation();
	void storageFailureRollsBackWholeTemplate();

private:
	SPSMF_Template_Request Request();
};
//-------------------------------------------------------------------------------------------------------------
SPSMF_Template_Request AProgram_Template_Service_Test::Request()
{
	SPSMF_Template_Request request;
	request.User_Id = "owner";
	request.Start_Date = "2026-11-08";
	request.Calories_Max = 1450;
	request.Protein_Min = 170;
	request.Carbs_Max = 30;
	request.Fat_Max = 28;
	return request;
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Template_Service_Test::createsIndependentDraftWithUserRulesAndRotation()
{
	ATemplate_Test_Store store;
	AProgram_Template_Service service(&store);
	SPSMF_Template_Request request = Request();
	QString first_id;
	QString second_id;
	request.Has_Water_Min = true;
	request.Water_Min_Ml = 2100;
	request.Has_Sleep_Min = true;
	request.Sleep_Min_Hours = 7.5;
	request.Has_Steps_Min = true;
	request.Steps_Min = 7000;
	QVERIFY(service.Create_PSMF_Draft("owner", request, first_id));
	request.Protein_Min = 190;
	QVERIFY(service.Create_PSMF_Draft("owner", request, second_id));
	QVERIFY(first_id != second_id);
	const SProgram_Template_Draft &first = store.Drafts.first();
	const SProgram_Template_Draft &second = store.Drafts.last();
	QCOMPARE(first.Program.State, Program_Model::EProgram_State::Draft);
	QCOMPARE(first.Program.Source_Kind, Program_Model::EProgram_Source_Kind::Template);
	QCOMPARE(first.Program.Source_Template_Id, QString("psmf-4-weeks-v1"));
	QCOMPARE(first.Program.Term.Start_Date, request.Start_Date);
	QCOMPARE(first.Program.Term.Weeks_Total, 4);
	QCOMPARE(first.Program.Term.Compute_End_Date(), QString("2026-12-05"));
	QVERIFY(first.Program.Actual_Start_Date.isEmpty());
	QCOMPARE(first.Program.Rep_Tolerance, 2);
	QCOMPARE(first.Measurement_Interval_Days, 3);
	QCOMPARE(first.Days.size(), 2);
	QCOMPARE(first.Slots.size(), 4);
	QCOMPARE(first.Rules.size(), 7);
	QCOMPARE(first.Rules.at(0).Target_Max, 1450.0);
	QCOMPARE(first.Rules.at(1).Target_Min, 170.0);
	QCOMPARE(second.Rules.at(1).Target_Min, 190.0);
	QCOMPARE(first.Rules.at(4).Target_Min, 2100.0);
	QCOMPARE(first.Rules.at(5).Target_Min, 7.5);
	QCOMPARE(first.Rules.at(6).Target_Min, 7000.0);
	QVERIFY(first.Days.first().ID != second.Days.first().ID);
	QVERIFY(first.Exercises.first().ID != second.Exercises.first().ID);
	QVERIFY(first.Rules.first().ID != second.Rules.first().ID);
	QCOMPARE(first.Exercises.last().Exercise_Id, QString("builtin-crunch"));
	for (int i = 0; i < first.Slots.size(); ++i)
	{
		QCOMPARE(first.Slots.at(i).Rotation_Index, i + 1);
		QCOMPARE(first.Slots.at(i).Kind,
		         i % 2 == 0 ? Program_Model::ESlot_Kind::Workout : Program_Model::ESlot_Kind::Rest);
		if (i % 2 != 0)
			QVERIFY(first.Slots.at(i).Program_Day_Id.isEmpty());
	}
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Template_Service_Test::rejectsInvalidInputBeforePersistence()
{
	ATemplate_Test_Store store;
	AProgram_Template_Service service(&store);
	SPSMF_Template_Request request = Request();
	QString id = "stale";
	request.Start_Date = "2026-02-30";
	QVERIFY(!service.Create_PSMF_Draft("owner", request, id));
	QVERIFY(id.isEmpty());
	request = Request();
	request.Calories_Max = std::numeric_limits<double>::quiet_NaN();
	QVERIFY(!service.Create_PSMF_Draft("owner", request, id));
	request = Request();
	request.Carbs_Max = -1;
	QVERIFY(!service.Create_PSMF_Draft("owner", request, id));
	request = Request();
	request.Has_Sleep_Min = true;
	request.Sleep_Min_Hours = 25;
	QVERIFY(!service.Create_PSMF_Draft("owner", request, id));
	request = Request();
	request.Has_Steps_Min = true;
	request.Steps_Min = 2.5;
	QVERIFY(!service.Create_PSMF_Draft("owner", request, id));
	request = Request();
	QVERIFY(!service.Create_PSMF_Draft("foreign", request, id));
	QVERIFY(!service.Create_PSMF_Draft("", request, id));
	QCOMPARE(store.Calls, 0);
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Template_Service_Test::rejectsForeignMeasurements()
{
	ATemplate_Test_Store store;
	AProgram_Template_Service service(&store);
	SPSMF_Template_Request request = Request();
	QString id;
	request.Initial_Measurement.User_Id = "foreign";
	QVERIFY(!service.Create_PSMF_Draft("owner", request, id));
	request.Initial_Measurement.User_Id = "owner";
	request.Initial_Measurement.Date = "2026-11-07";
	QVERIFY(!service.Create_PSMF_Draft("owner", request, id));
	request.Initial_Measurement.Date.clear();
	request.Initial_Measurement.ID = "existing-measurement";
	QVERIFY(!service.Create_PSMF_Draft("owner", request, id));
	QCOMPARE(store.Calls, 0);
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Template_Service_Test::acceptsPartialMeasurementsAndDoesNotInventTargets()
{
	ATemplate_Test_Store store;
	AProgram_Template_Service service(&store);
	SPSMF_Template_Request request = Request();
	QString id;
	request.Initial_Measurement.Has_Waist_Cm = true;
	request.Initial_Measurement.Waist_Cm = 92.5;
	QVERIFY(service.Create_PSMF_Draft("owner", request, id));
	QVERIFY(store.Drafts.first().Has_Initial_Measurement);
	QCOMPARE(store.Drafts.first().Initial_Measurement.Date, request.Start_Date);
	QCOMPARE(store.Drafts.first().Initial_Measurement.User_Id, request.User_Id);
	QVERIFY(!store.Drafts.first().Initial_Measurement.Has_Weight_Kg);
	QVERIFY(store.Drafts.first().Targets.isEmpty());
	QCOMPARE(store.Drafts.first().Rules.size(), 4);
	request.Initial_Measurement.Waist_Cm = 0;
	QVERIFY(!service.Create_PSMF_Draft("owner", request, id));
	QCOMPARE(store.Calls, 1);
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Template_Service_Test::persistenceFailureDoesNotPublishAnId()
{
	ATemplate_Test_Store store;
	AProgram_Template_Service service(&store);
	QString id = "stale";
	store.Fail = true;
	QVERIFY(!service.Create_PSMF_Draft("owner", Request(), id));
	QVERIFY(id.isEmpty());
	QCOMPARE(service.Get_Last_Error(), QString("atomic_insert_failed"));
	QVERIFY(store.Drafts.isEmpty());
	store.Fail = false;
	QVERIFY(service.Create_PSMF_Draft("owner", Request(), id));
	QVERIFY(service.Get_Last_Error().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Template_Service_Test::missingStoreIsAnError()
{
	AProgram_Template_Service service(0);
	QString id;
	QVERIFY(!service.Create_PSMF_Draft("owner", Request(), id));
	QVERIFY(!service.Get_Last_Error().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Template_Service_Test::realStorageAndEditorPreserveRotation()
{
	AsSql_Database database;
	QVERIFY(database.Open(":memory:"));
	AProgram_Repository programs(&database);
	ARule_Repository rules(&database);
	AMeasurement_Repository measurements(&database);
	AProgram_Template_Service service(&programs);
	SPSMF_Template_Request request = Request();
	request.Initial_Measurement.Has_Weight_Kg = true;
	request.Initial_Measurement.Weight_Kg = 90;
	QString id;
	QVERIFY2(service.Create_PSMF_Draft("owner", request, id), qPrintable(service.Get_Last_Error()));
	SProgram program;
	QVERIFY(programs.Load_Program(id, program));
	QCOMPARE(program.Rep_Tolerance, 2);
	QCOMPARE(program.State, Program_Model::EProgram_State::Draft);
	QVector<Program_Model::SProgram_Rule> saved_rules;
	QVERIFY(rules.Load_Rules(id, saved_rules));
	QCOMPARE(saved_rules.size(), 4);
	AsProgram_Service program_service(&programs);
	Avm_Program_Edit editor(&programs, &rules, &program_service, 0);
	QVERIFY(editor.Load("owner", id));
	QVERIFY(editor.Is_Rotation());
	QCOMPARE(editor.program_days()->Row_Count(), 4);
	editor.setAutoSaveEnabled(true);
	QVERIFY(editor.setRepTolerance(3));
	QVERIFY(editor.saveDraft());
	QVector<Program_Model::SWeek_Slot> schedule;
	QVERIFY(programs.Load_Schedule(id, schedule));
	QCOMPARE(schedule.size(), 4);
	QCOMPARE(schedule.at(0).Kind, Program_Model::ESlot_Kind::Workout);
	QCOMPARE(schedule.at(1).Kind, Program_Model::ESlot_Kind::Rest);
	QCOMPARE(schedule.at(2).Kind, Program_Model::ESlot_Kind::Workout);
	QVERIFY(programs.Load_Program(id, program));
	QCOMPARE(program.Rep_Tolerance, 3);
	Program_Model::SBody_Measurement measurement;
	QVERIFY(measurements.Find_Measurement("owner", request.Start_Date, measurement));
	QCOMPARE(measurement.Weight_Kg, 90.0);
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Template_Service_Test::storageFailureRollsBackWholeTemplate()
{
	AsSql_Database database;
	QVERIFY(database.Open(":memory:"));
	AProgram_Repository programs(&database);
	AProgram_Template_Service service(&programs);
	QSqlQuery query(database.Sql_Connection());
	QVERIFY(query.exec(
	    "CREATE TRIGGER fail_template BEFORE INSERT ON program_rules BEGIN SELECT RAISE(ABORT,'disk_full'); END"));
	QString id;
	QVERIFY(!service.Create_PSMF_Draft("owner", Request(), id));
	QVERIFY(id.isEmpty());
	QVERIFY(query.exec("SELECT COUNT(*) FROM programs"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 0);
	QVERIFY(query.exec("SELECT COUNT(*) FROM program_days"));
	QVERIFY(query.next());
	QCOMPARE(query.value(0).toInt(), 0);
}
//-------------------------------------------------------------------------------------------------------------
QTEST_GUILESS_MAIN(AProgram_Template_Service_Test)
#include "tst_Program_Template_Service.moc"
