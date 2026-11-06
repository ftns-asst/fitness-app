#include "Data/Program/Program_Repository_Base.h"

#include <QDateTime>
#include <QLoggingCategory>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

Q_LOGGING_CATEGORY(PROGRAM_Repository_Log, "trainingapp.program.repository")

//-------------------------------------------------------------------------------------------------------------
// AProgram_Repository_Base
//-------------------------------------------------------------------------------------------------------------
AProgram_Repository_Base::AProgram_Repository_Base(AsSql_Database *in_database, QObject *in_parent)
    : QObject(in_parent), Database_Instance(in_database), Error_State(&PROGRAM_Repository_Log())
{
}
//-------------------------------------------------------------------------------------------------------------
QString AProgram_Repository_Base::Get_Last_Error() const
{
	return Error_State.Get_Last_Error();
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository_Base::Set_Last_Error(const QString &in_error)
{
	return Error_State.Set_Last_Error(in_error);
}
//-------------------------------------------------------------------------------------------------------------
void AProgram_Repository_Base::Clear_Last_Error()
{
	Error_State.Clear_Last_Error();
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository_Base::Check_Open()
{
	if (Database_Instance != 0 && Database_Instance->Is_Open())
		return true;

	return Set_Last_Error("SQLite не открыта");
}
//-------------------------------------------------------------------------------------------------------------
QSqlDatabase AProgram_Repository_Base::Connection() const
{
	if (Database_Instance != 0)
		return Database_Instance->Sql_Connection();

	return QSqlDatabase();
}
//-------------------------------------------------------------------------------------------------------------
QVariant AProgram_Repository_Base::Text_Column(const QString &in_value)
{
	return in_value.isEmpty() ? QVariant() : QVariant(in_value);
}
//-------------------------------------------------------------------------------------------------------------
QVariant AProgram_Repository_Base::Number_Column(double in_value, bool in_has)
{
	return in_has ? QVariant(in_value) : QVariant();
}
//-------------------------------------------------------------------------------------------------------------
QVariant AProgram_Repository_Base::Int_Column(int in_value, bool in_has)
{
	return in_has ? QVariant(in_value) : QVariant();
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository_Base::Is_Read_Only_State(EProgram_State in_state)
{
	// Предикат взят из модели, а не продублирован здесь: по нему же экран
	// решает, можно ли править программу, и две копии условия рано или поздно
	// разошлись бы.
	return !Program_Model::Program_State_Is_Editable(in_state);
}
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository_Base::Write_Revision_Row(const QString &in_program_id, int in_revision,
                                                  ERevision_Reason in_reason, const QString &in_summary,
                                                  const QString &in_snapshot_json)
{
	QSqlQuery query(Connection());

	query.prepare("INSERT INTO program_revisions(id, program_id, revision, changed_at, reason, summary, snapshot_json) "
	              "VALUES(?, ?, ?, ?, ?, ?, ?)");
	query.addBindValue(QUuid::createUuid().toString(QUuid::WithoutBraces));
	query.addBindValue(in_program_id);
	query.addBindValue(in_revision);
	query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
	query.addBindValue(To_String(in_reason));
	query.addBindValue(in_summary);
	query.addBindValue(Text_Column(in_snapshot_json));

	if (query.exec())
		return true;

	return Set_Last_Error(QString("Не удалось записать ревизию: %1").arg(query.lastError().text()));
}
//-------------------------------------------------------------------------------------------------------------
// Оптимистичная блокировка: ревизия увеличивается только с прежним значением;
// параллельная правка отклоняется, а не перезаписывается молча.
//-------------------------------------------------------------------------------------------------------------
bool AProgram_Repository_Base::Bump_Revision_Internal(const QString &in_program_id, int in_old_revision)
{
	QSqlQuery query(Connection());

	query.prepare("UPDATE programs SET revision = revision + 1, updated_at = ? WHERE id = ? AND revision = ?");
	query.addBindValue(QDateTime::currentDateTimeUtc().toString(Qt::ISODate));
	query.addBindValue(in_program_id);
	query.addBindValue(in_old_revision);

	if (!query.exec())
		return Set_Last_Error(QString("Не удалось увеличить ревизию: %1").arg(query.lastError().text()));

	if (query.numRowsAffected() != 1)
		return Set_Last_Error("Ревизия программы изменилась параллельно, правка отклонена");

	return true;
}
//-------------------------------------------------------------------------------------------------------------