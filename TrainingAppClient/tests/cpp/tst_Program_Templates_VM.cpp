#include "Presentation/Program_Templates_VM.h"

#include <QSignalSpy>
#include <QtTest>

//-------------------------------------------------------------------------------------------------------------
class ATemplates_VM_Test_Store : public AProgram_Template_Repository_Port
{
public:
	bool Insert_Template_Draft(const QString &, const SProgram_Template_Draft &in_draft) override
	{
		++Calls;
		Draft = in_draft;
		return !Fail;
	}
	QString Get_Last_Error() const override
	{
		return QString("disk_full");
	}
	SProgram_Template_Draft Draft;
	int Calls = 0;
	bool Fail = false;
};
//-------------------------------------------------------------------------------------------------------------
class AProgram_Templates_VM_Test : public QObject
{
	Q_OBJECT

private slots:
	void createsDraftAndEmitsOnlyAfterPersistence();
	void invalidMapsDoNotReachPersistence();
	void failureClearsPreviousSuccess();
	void noServiceFailsClearly();

private:
	QVariantMap Preset();
};
//-------------------------------------------------------------------------------------------------------------
QVariantMap AProgram_Templates_VM_Test::Preset()
{
	QVariantMap preset;
	preset.insert("caloriesMax", 1300.0);
	preset.insert("proteinMin", 180.0);
	preset.insert("carbsMax", 25.0);
	preset.insert("fatMax", 25.0);
	return preset;
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Templates_VM_Test::createsDraftAndEmitsOnlyAfterPersistence()
{
	ATemplates_VM_Test_Store store;
	AProgram_Template_Service service(&store);
	Avm_Program_Templates vm(&service);
	QSignalSpy created(&vm, &Avm_Program_Templates::templateCreated);
	QVariantMap measurements;
	measurements.insert("weightKg", 89.2);
	measurements.insert("neckCm", 39.0);
	QVERIFY(vm.createDraft("owner", "2026-11-08", Preset(), measurements));
	QCOMPARE(created.count(), 1);
	QCOMPARE(created.first().first().toString(), vm.programId());
	QVERIFY(vm.errorKey().isEmpty());
	QVERIFY(store.Draft.Initial_Measurement.Has_Weight_Kg);
	QVERIFY(store.Draft.Initial_Measurement.Has_Neck_Cm);
	QVERIFY(!store.Draft.Initial_Measurement.Has_Waist_Cm);
	QCOMPARE(store.Draft.Program.State, Program_Model::EProgram_State::Draft);
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Templates_VM_Test::invalidMapsDoNotReachPersistence()
{
	ATemplates_VM_Test_Store store;
	AProgram_Template_Service service(&store);
	Avm_Program_Templates vm(&service);
	QVariantMap preset = Preset();
	QVariantMap measurements;
	preset.remove("proteinMin");
	QVERIFY(!vm.createDraft("owner", "2026-11-08", preset, measurements));
	preset = Preset();
	preset.insert("proteinMin", true);
	QVERIFY(!vm.createDraft("owner", "2026-11-08", preset, measurements));
	preset = Preset();
	measurements.insert("userId", "foreign");
	QVERIFY(!vm.createDraft("owner", "2026-11-08", preset, measurements));
	measurements.clear();
	measurements.insert("weightKg", "abc");
	QVERIFY(!vm.createDraft("owner", "2026-11-08", preset, measurements));
	QCOMPARE(store.Calls, 0);
	QCOMPARE(vm.errorKey(), QString("invalid_input"));
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Templates_VM_Test::failureClearsPreviousSuccess()
{
	ATemplates_VM_Test_Store store;
	AProgram_Template_Service service(&store);
	Avm_Program_Templates vm(&service);
	QSignalSpy created(&vm, &Avm_Program_Templates::templateCreated);
	QVERIFY(vm.createDraft("owner", "2026-11-08", Preset(), QVariantMap()));
	store.Fail = true;
	QVERIFY(!vm.createDraft("owner", "2026-11-08", Preset(), QVariantMap()));
	QVERIFY(vm.programId().isEmpty());
	QCOMPARE(created.count(), 1);
	QCOMPARE(vm.errorKey(), QString("create_failed"));
	QCOMPARE(vm.errorDetail(), QString("disk_full"));
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Templates_VM_Test::noServiceFailsClearly()
{
	Avm_Program_Templates vm(0);
	QVERIFY(!vm.createDraft("owner", "2026-11-08", Preset(), QVariantMap()));
	QCOMPARE(vm.errorKey(), QString("create_failed"));
}
//-------------------------------------------------------------------------------------------------------------
QTEST_GUILESS_MAIN(AProgram_Templates_VM_Test)
#include "tst_Program_Templates_VM.moc"
