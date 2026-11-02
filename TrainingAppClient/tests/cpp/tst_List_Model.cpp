#include "Presentation/List_Model.h"

#include <QAbstractItemModelTester>
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
	void unchangedRowsDoNotReset();
	void appendAndUpdatePreserveIndexes();
	void changedIdentityResets();

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

void AList_Model_Test::unchangedRowsDoNotReset()
{
	AList_Model model;
	model.Set_Roles({"id", "title"});
	model.Set_Rows(Make_Rows());
	QPersistentModelIndex anchor(model.index(1));
	QSignalSpy resets(&model, &AList_Model::modelReset);
	QSignalSpy counts(&model, &AList_Model::countChanged);

	model.Set_Rows(Make_Rows());

	QCOMPARE(resets.count(), 0);
	QCOMPARE(counts.count(), 0);
	QVERIFY(anchor.isValid());
	QCOMPARE(anchor.row(), 1);
}

void AList_Model_Test::appendAndUpdatePreserveIndexes()
{
	AList_Model model;
	QAbstractItemModelTester tester(&model, QAbstractItemModelTester::FailureReportingMode::QtTest);
	model.Set_Roles({"id", "title"});
	model.Set_Rows(Make_Rows());
	QPersistentModelIndex anchor(model.index(1));
	QSignalSpy resets(&model, &AList_Model::modelReset);
	QSignalSpy inserts(&model, &AList_Model::rowsInserted);
	QSignalSpy removes(&model, &AList_Model::rowsRemoved);
	QSignalSpy changes(&model, &AList_Model::dataChanged);
	QSignalSpy counts(&model, &AList_Model::countChanged);
	QVector<QVariantMap> page{{{"id", "p3"}, {"title", "Third"}}};
	model.Append_Rows({});
	model.Append_Rows(page);
	page[0]["title"] = "External change";
	QCOMPARE(model.get(2).value("title").toString(), QString("Third"));
	QCOMPARE(inserts.count(), 1);
	QCOMPARE(inserts.at(0).at(1).toInt(), 2);
	QCOMPARE(inserts.at(0).at(2).toInt(), 2);
	QVector<QVariantMap> rows = Make_Rows();
	rows[1]["title"] = "Updated";
	model.Update_Rows(rows, "id");
	QCOMPARE(resets.count(), 0);
	QCOMPARE(changes.count(), 1);
	QCOMPARE(qvariant_cast<QModelIndex>(changes.at(0).at(0)).row(), 1);
	QCOMPARE(removes.count(), 1);
	QCOMPARE(counts.count(), 2);
	QVERIFY(anchor.isValid());
	QCOMPARE(anchor.row(), 1);
	QCOMPARE(anchor.data(Qt::UserRole + 2).toString(), QString("Updated"));
	model.Update_Rows(rows, "id");
	QCOMPARE(changes.count(), 1);
	QCOMPARE(counts.count(), 2);
	model.Update_Rows({}, "id");
	QCOMPARE(model.Row_Count(), 0);
	QVERIFY(!anchor.isValid());
	QCOMPARE(resets.count(), 0);
}
//-------------------------------------------------------------------------------------------------------------
void AList_Model_Test::changedIdentityResets()
{
	AList_Model model;
	model.Set_Roles({"id", "title"});
	model.Set_Rows(Make_Rows());
	QVector<QVariantMap> rows = Make_Rows();
	rows.swapItemsAt(0, 1);
	QSignalSpy resets(&model, &AList_Model::modelReset);
	QSignalSpy counts(&model, &AList_Model::countChanged);
	model.Update_Rows(rows, "id");
	QCOMPARE(resets.count(), 1);
	QCOMPARE(counts.count(), 0);
	QCOMPARE(model.get(0).value("id").toString(), QString("p2"));
}

QTEST_GUILESS_MAIN(AList_Model_Test)
#include "tst_List_Model.moc"
