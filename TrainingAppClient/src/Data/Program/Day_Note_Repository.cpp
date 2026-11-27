#include "Data/Program/Day_Note_Repository.h"

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>

using namespace Program_Model;

ADay_Note_Repository::ADay_Note_Repository(AsSql_Database *in_database, QObject *in_parent)
    : AProgram_Repository_Base(in_database, in_parent)
{
}
//-------------------------------------------------------------------------------------------------------------
bool ADay_Note_Repository::Is_Valid_Date(const QString &in_date) const
{
	const QDate date = QDate::fromString(in_date, Qt::ISODate);

	return in_date.size() == 10 && date.isValid() && date.toString("yyyy-MM-dd") == in_date;
}
//-------------------------------------------------------------------------------------------------------------
bool ADay_Note_Repository::Load_Range(const QString &in_user_id, const QString &in_date_from, const QString &in_date_to,
                                      QVector<SDay_Note> &out_notes)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();
	out_notes.clear();

	if (!Check_Open())
		return false;

	if (in_user_id.isEmpty() || !Is_Valid_Date(in_date_from) || !Is_Valid_Date(in_date_to) || in_date_from > in_date_to)
		return Set_Last_Error("Чтению заметок нужны пользователь и корректный диапазон дат");

	query.prepare(
	    "SELECT user_id, date, note FROM day_notes WHERE user_id = ? AND date >= ? AND date <= ? ORDER BY date");
	query.addBindValue(in_user_id);
	query.addBindValue(in_date_from);
	query.addBindValue(in_date_to);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось прочитать заметки дня: %1").arg(query.lastError().text()));

	while (query.next())
	{
		SDay_Note note;
		note.User_Id = query.value(0).toString();
		note.Date = query.value(1).toString();
		note.Text = query.value(2).toString();
		out_notes.append(note);
	}

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool ADay_Note_Repository::Save_Note(const SDay_Note &in_note)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();

	if (!Check_Open())
		return false;

	if (in_note.User_Id.isEmpty() || !Is_Valid_Date(in_note.Date))
		return Set_Last_Error("Заметке нужны пользователь и корректная дата");

	if (in_note.Text.trimmed().isEmpty())
		query.prepare("DELETE FROM day_notes WHERE user_id = ? AND date = ?");
	else
		query.prepare("INSERT INTO day_notes(user_id, date, note) VALUES(?, ?, ?) ON CONFLICT(user_id, date) DO UPDATE "
		              "SET note = excluded.note");

	query.addBindValue(in_note.User_Id);
	query.addBindValue(in_note.Date);
	if (!in_note.Text.trimmed().isEmpty())
		query.addBindValue(in_note.Text);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось сохранить заметку дня: %1").arg(query.lastError().text()));

	return true;
}
