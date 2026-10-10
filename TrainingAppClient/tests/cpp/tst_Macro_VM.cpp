#include "Data/Auth/Auth_Session.h"
#include "Data/Auth/Token_Store.h"
#include "Data/Database/Sql_Database.h"
#include "Data/Network/Http_Client.h"
#include "Data/Program/Measurement_Repository.h"
#include "Data/Users/Users_Repository.h"
#include "Domain/Macro_Calculator.h"
#include "Presentation/Macro_VM.h"

#include <QTemporaryDir>
#include <QUrl>
#include <QtTest>

//------------------------------------------------------------------------------
// Avm_Macro: экран «Рассчитать КБЖУ» (спринт S7, issue 50) и сохранение
// результата (миграция v6).
//
// Проверяется то, ради чего результат пишется в базу: главная показывает числа
// без вызова домена, а экран расчёта при следующем открытии не выглядит так,
// будто расчёт делают впервые — без этого человек терял бы свои числа после
// перезапуска и считал бы их заново на глаз.
//------------------------------------------------------------------------------
class AMacro_VM_Test : public QObject
{
	Q_OBJECT

private slots:
	void initTestCase();
	void resultIsEmptyBeforeFirstCalculation();
	void calculationPersistsResultForNextOpen();
	void recalculationReplacesPreviousNumbers();

private:
	QTemporaryDir Directory;
	AsSql_Database Database;
	AsHttp_Client HTTP_Client = AsHttp_Client(QUrl());
	AsToken_Store Token_Store = AsToken_Store(&Database);
	AsAuth_Session Auth_Session = AsAuth_Session(&HTTP_Client, &Token_Store);
	AMeasurement_Repository *Measurement_Repository = 0;
	AUsers_Repository *Users_Repository = 0;
	AsMacro_Calculator *Calculator = 0;
};
//------------------------------------------------------------------------------
void AMacro_VM_Test::initTestCase()
{
	QVERIFY(Directory.isValid());
	QVERIFY(Database.Open(Directory.filePath("macro_vm.sqlite3")));
	Measurement_Repository = new AMeasurement_Repository(&Database, this);
	Users_Repository = new AUsers_Repository(&Auth_Session, &Database, this);
	Calculator = new AsMacro_Calculator;
}
//------------------------------------------------------------------------------
// До первого расчёта чисел нет: пустые нули на главной выглядели бы как
// честный результат, и человек не пошёл бы за расчётом.
//------------------------------------------------------------------------------
void AMacro_VM_Test::resultIsEmptyBeforeFirstCalculation()
{
	Avm_Macro vm(Calculator, Measurement_Repository, Users_Repository);

	vm.setToday("2026-10-09");
	QVERIFY(vm.load("macro-vm-user"));
	QVERIFY(vm.Has_Result() == false);
	QVERIFY(vm.calculated_at().isEmpty());
	QCOMPARE(vm.calories_kcal(), 0.0);
}
//------------------------------------------------------------------------------
void AMacro_VM_Test::calculationPersistsResultForNextOpen()
{
	Avm_Macro vm(Calculator, Measurement_Repository, Users_Repository);

	vm.setToday("2026-10-09");
	QVERIFY(vm.load("macro-vm-user"));
	QVERIFY(vm.calculate("macro-vm-user", 82, 180, false, "1994-05-17", "moderate", "maintain"));

	const double calories = vm.calories_kcal();
	const double protein = vm.protein_g();

	QVERIFY(calories > 0);
	QVERIFY(vm.calculated_at().isEmpty() == false);

	// Следующее открытие экрана — те же числа без повторного расчёта.
	QVERIFY(vm.load("macro-vm-user"));
	QVERIFY(vm.Has_Result());
	QCOMPARE(vm.calories_kcal(), calories);
	QCOMPARE(vm.protein_g(), protein);
	QVERIFY(vm.calculated_at().isEmpty() == false);
}
//------------------------------------------------------------------------------
// Пересчёт заменяет прежние числа, а не дополняет их: на главной должно быть
// последнее расчёта, иначе там осталось бы значение, по которому человек уже
// не питается.
//------------------------------------------------------------------------------
void AMacro_VM_Test::recalculationReplacesPreviousNumbers()
{
	Avm_Macro vm(Calculator, Measurement_Repository, Users_Repository);

	vm.setToday("2026-10-09");
	QVERIFY(vm.load("macro-vm-user"));
	QVERIFY(vm.calculate("macro-vm-user", 95, 180, false, "1994-05-17", "moderate", "maintain"));

	const double heavier = vm.calories_kcal();

	QVERIFY(vm.calculate("macro-vm-user", 82, 180, false, "1994-05-17", "moderate", "maintain"));

	const double lighter = vm.calories_kcal();

	QVERIFY(lighter < heavier);

	QVERIFY(vm.load("macro-vm-user"));
	QCOMPARE(vm.calories_kcal(), lighter);
}
//------------------------------------------------------------------------------
QTEST_GUILESS_MAIN(AMacro_VM_Test)
#include "tst_Macro_VM.moc"
