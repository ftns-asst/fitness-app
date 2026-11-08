#include "Presentation/List_Model.h"

#include <QSignalSpy>
#include <QtTest>

// Базовый список Presentation: строки — QVariantMap с именованными ролями.
// Проверяем контракт, на который опираются все конкретные модели §7.
class AList_Model_Test : public QObject
{
	Q_OBJECT

private slots:
	void startsEmpty();
	void rowsAndRolesAreExposed();
	void dataReturnsRoleValueByName();
	void unknownRoleReturnsInvalidVariant();
	void setRowsEmitsResetAndReplacesData();
	void outOfRangeIndexIsInvalid();
	void rowsPreservedAreCopied();

private:
	QVector<QVariantMap> Make_Rows();
};

QVector<QVariantMap> AList_Model_Test::Make_Rows()
{
	QVector<QVariantMap> rows;

	rows.append(QVariantMap{{QString("id"), QString("p1")}, {QString("title"), QString("Сила")}});
	rows.append(QVariantMap{{QString("id"), QString("p2")}, {QString("title"), QString("Масса")}});

	return rows;
}
//-------------------------------------------------------------------------------------------------------------
void AList_Model_Test::startsEmpty()
{
	AList_Model model;

	model.Set_Roles(QStringList{"id", "title"});

	QCOMPARE(model.Row_Count(), 0);
}
//-------------------------------------------------------------------------------------------------------------
void AList_Model_Test::rowsAndRolesAreExposed()
{
	AList_Model model;

	model.Set_Roles(QStringList{"id", "title"});
	model.Set_Rows(Make_Rows());

	QCOMPARE(model.Row_Count(), 2);
	QCOMPARE(model.Role_Name(Qt::UserRole + 1), QString("id"));
	QCOMPARE(model.Role_Name(Qt::UserRole + 2), QString("title"));
	const QHash<int, QByteArray> roles = model.roleNames();

	QCOMPARE(roles.value(Qt::UserRole + 1), QByteArray("id"));
	QCOMPARE(roles.value(Qt::UserRole + 2), QByteArray("title"));
	QCOMPARE(roles.size(), 2);
}
//-------------------------------------------------------------------------------------------------------------
void AList_Model_Test::dataReturnsRoleValueByName()
{
	AList_Model model;

	model.Set_Roles(QStringList{"id", "title"});
	model.Set_Rows(Make_Rows());

	QCOMPARE(model.Data(0, Qt::UserRole + 1).toString(), QString("p1"));
	QCOMPARE(model.Data(1, Qt::UserRole + 2).toString(), QString("Масса"));
}
//-------------------------------------------------------------------------------------------------------------
void AList_Model_Test::unknownRoleReturnsInvalidVariant()
{
	AList_Model model;

	model.Set_Roles(QStringList{"id"});
	model.Set_Rows(Make_Rows());

	QVERIFY(!model.Data(0, Qt::UserRole + 9).isValid());
}
//-------------------------------------------------------------------------------------------------------------
void AList_Model_Test::setRowsEmitsResetAndReplacesData()
{
	AList_Model model;
	QSignalSpy spy(&model, &AList_Model::modelReset);

	model.Set_Roles(QStringList{"id", "title"});
	model.Set_Rows(Make_Rows());
	const int resets_after_fill = spy.count();

	model.Set_Rows(QVector<QVariantMap>{QVariantMap{{QString("id"), QString("p9")}}});

	// Set_Roles тоже инвалидирует: смена набора ролей обязана сбросить модель.
	QCOMPARE(resets_after_fill, 2);
	QCOMPARE(spy.count(), 3);
	QCOMPARE(model.Row_Count(), 1);
	QCOMPARE(model.Data(0, Qt::UserRole + 2).toString(), QString());
}
//-------------------------------------------------------------------------------------------------------------
void AList_Model_Test::outOfRangeIndexIsInvalid()
{
	AList_Model model;

	model.Set_Roles(QStringList{"id"});
	model.Set_Rows(Make_Rows());

	QVERIFY(!model.Data(-1, Qt::UserRole + 1).isValid());
	QVERIFY(!model.Data(5, Qt::UserRole + 1).isValid());
}
//-------------------------------------------------------------------------------------------------------------
void AList_Model_Test::rowsPreservedAreCopied()
{
	AList_Model model;
	QVector<QVariantMap> rows = Make_Rows();

	model.Set_Roles(QStringList{"id", "title"});
	model.Set_Rows(rows);

	rows[0][QString("title")] = QString("Изменено вне модели");

	// Модель держит собственную копию: правка исходного контейнера не влияет.
	QCOMPARE(model.Data(0, Qt::UserRole + 2).toString(), QString("Сила"));
}

QTEST_GUILESS_MAIN(AList_Model_Test)
#include "tst_List_Model.moc"