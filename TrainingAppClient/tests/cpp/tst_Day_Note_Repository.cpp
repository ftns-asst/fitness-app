#include "Data/Database/Sql_Database.h"
#include "Data/Program/Day_Note_Repository.h"

#include <QTemporaryDir>
#include <QtTest>

using namespace Program_Model;

class ADay_Note_Repository_Test : public QObject
{
	Q_OBJECT

private slots:
	void savesUpdatesAndClearsNote();
	void keepsNotesPerUserAndRange();
	void rejectsInvalidReadRange();

private:
	QString Database_Path();

	QTemporaryDir Directory;
	int Counter = 0;
};
//-------------------------------------------------------------------------------------------------------------
QString ADay_Note_Repository_Test::Database_Path()
{
	++Counter;

	return Directory.filePath(QString("notes_%1.sqlite3").arg(Counter));
}
//-------------------------------------------------------------------------------------------------------------
void ADay_Note_Repository_Test::savesUpdatesAndClearsNote()
{
	AsSql_Database database;
	ADay_Note_Repository repository(&database);
	QVector<SDay_Note> notes;
	SDay_Note note;

	QVERIFY(database.Open(Database_Path()));
	note.User_Id = "u1";
	note.Date = "2026-10-02";
	note.Text = "Первый текст";
	QVERIFY(repository.Save_Note(note));
	note.Text = "Уточнённый текст";
	QVERIFY(repository.Save_Note(note));
	QVERIFY(repository.Load_Range("u1", "2026-10-01", "2026-10-03", notes));
	QCOMPARE(notes.size(), 1);
	QCOMPARE(notes.at(0).Text, QString("Уточнённый текст"));
	note.Text = "   ";
	QVERIFY(repository.Save_Note(note));
	QVERIFY(repository.Load_Range("u1", "2026-10-01", "2026-10-03", notes));
	QVERIFY(notes.isEmpty());
}
//-------------------------------------------------------------------------------------------------------------
void ADay_Note_Repository_Test::keepsNotesPerUserAndRange()
{
	AsSql_Database database;
	ADay_Note_Repository repository(&database);
	QVector<SDay_Note> notes;
	SDay_Note note;

	QVERIFY(database.Open(Database_Path()));
	note.User_Id = "u1";
	note.Date = "2026-10-01";
	note.Text = "A";
	QVERIFY(repository.Save_Note(note));
	note.Date = "2026-10-03";
	note.Text = "B";
	QVERIFY(repository.Save_Note(note));
	note.User_Id = "u2";
	note.Date = "2026-10-02";
	note.Text = "Чужая";
	QVERIFY(repository.Save_Note(note));
	QVERIFY(repository.Load_Range("u1", "2026-10-02", "2026-10-03", notes));
	QCOMPARE(notes.size(), 1);
	QCOMPARE(notes.at(0).Date, QString("2026-10-03"));
}
//-------------------------------------------------------------------------------------------------------------
void ADay_Note_Repository_Test::rejectsInvalidReadRange()
{
	AsSql_Database database;
	ADay_Note_Repository repository(&database);
	QVector<SDay_Note> notes;

	QVERIFY(database.Open(Database_Path()));
	QVERIFY(!repository.Load_Range("", "2026-10-01", "2026-10-03", notes));
	QVERIFY(!repository.Load_Range("u1", "invalid", "2026-10-03", notes));
	QVERIFY(!repository.Load_Range("u1", "2026-10-04", "2026-10-03", notes));
}
//-------------------------------------------------------------------------------------------------------------
QTEST_MAIN(ADay_Note_Repository_Test)

#include "tst_Day_Note_Repository.moc"
