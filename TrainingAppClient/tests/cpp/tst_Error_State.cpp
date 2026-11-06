#include "Domain/Error_State.h"

#include <QtTest>

// Проверка общего хранилища ошибки: очистка в начале вызова, запись с
// логированием и неизменность публичного имени Get_Last_Error().
class AError_State_Test : public QObject
{
	Q_OBJECT

private slots:
	void startsEmpty();
	void setErrorStoresTextAndFails();
	void clearResetsPreviousError();
	void clearBySetterIsIdempotent();

private:
	AError_State state;
};

void AError_State_Test::startsEmpty()
{
	QVERIFY(state.Get_Last_Error().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void AError_State_Test::setErrorStoresTextAndFails()
{
	const bool result = state.Set_Last_Error("Некорректная дата");

	QVERIFY(!result);
	QCOMPARE(state.Get_Last_Error(), QString("Некорректная дата"));
}
//-------------------------------------------------------------------------------------------------------------
void AError_State_Test::clearResetsPreviousError()
{
	state.Set_Last_Error("Прошлая ошибка");

	state.Clear_Last_Error();

	QVERIFY(state.Get_Last_Error().isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void AError_State_Test::clearBySetterIsIdempotent()
{
	state.Set_Last_Error("Первая ошибка");
	state.Set_Last_Error("Вторая ошибка");

	QCOMPARE(state.Get_Last_Error(), QString("Вторая ошибка"));
}

QTEST_GUILESS_MAIN(AError_State_Test)
#include "tst_Error_State.moc"