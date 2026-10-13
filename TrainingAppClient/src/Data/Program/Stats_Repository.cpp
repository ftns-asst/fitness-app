#include "Data/Program/Stats_Repository.h"

#include <QDateTime>
#include <QJsonDocument>
#include <QJsonParseError>
#include <QSqlError>
#include <QSqlQuery>
//----------------------------------------------------------------------------
static QString Now_Stamp()
{
	return QDateTime::currentDateTimeUtc().toString(Qt::ISODate);
}
//----------------------------------------------------------------------------
// AStats_Repository
//----------------------------------------------------------------------------
AStats_Repository::AStats_Repository(AsSql_Database *in_database, QObject *in_parent)
    : AProgram_Repository_Base(in_database, in_parent)
{
}
//----------------------------------------------------------------------------
// Нагрузка кэша — компактный JSON-объект (метрики §5.1/§5.3). Проверяем на
// записи, чтобы битый JSON не всплывал ошибкой на экране статистики.
//----------------------------------------------------------------------------
bool AStats_Repository::Validate_Payload(const QString &in_payload_json)
{
	QJsonParseError parse_error;

	if (in_payload_json.isEmpty())
		return false;

	QJsonDocument document = QJsonDocument::fromJson(in_payload_json.toUtf8(), &parse_error);

	if (parse_error.error != QJsonParseError::NoError)
		return false;

	return document.isObject();
}
//----------------------------------------------------------------------------
bool AStats_Repository::Load_Program_Stats(const QString &in_program_id, SStats_Cache_Entry &out_entry)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();
	out_entry = SStats_Cache_Entry();

	if (Check_Open() == false)
		return false;

	query.prepare("SELECT program_id, computed_at, program_revision, payload_json FROM program_stats "
	              "WHERE program_id = ?");
	query.addBindValue(in_program_id);

	if (query.exec() == false)
		return Set_Last_Error(QString("Не удалось прочитать кэш статистики: %1").arg(query.lastError().text()));

	if (query.next() == false)
		return Set_Last_Error(QString("Кэш статистики программы %1 не найден").arg(in_program_id));

	out_entry.Owner_Id = query.value(0).toString();
	out_entry.Computed_At = query.value(1).toString();
	out_entry.Program_Revision = query.value(2).toInt();
	out_entry.Payload_Json = query.value(3).toString();

	return true;
}
//----------------------------------------------------------------------------
bool AStats_Repository::Load_Lifetime_Stats(const QString &in_user_id, SStats_Cache_Entry &out_entry)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();
	out_entry = SStats_Cache_Entry();

	if (Check_Open() == false)
		return false;

	query.prepare("SELECT user_id, computed_at, payload_json FROM lifetime_stats WHERE user_id = ?");
	query.addBindValue(in_user_id);

	if (query.exec() == false)
		return Set_Last_Error(QString("Не удалось прочитать общую статистику: %1").arg(query.lastError().text()));

	if (query.next() == false)
		return Set_Last_Error(QString("Общая статистика пользователя %1 не найдена").arg(in_user_id));

	out_entry.Owner_Id = query.value(0).toString();
	out_entry.Computed_At = query.value(1).toString();
	out_entry.Program_Revision = 0;
	out_entry.Payload_Json = query.value(2).toString();

	return true;
}
//----------------------------------------------------------------------------
bool AStats_Repository::Is_Program_Stats_Fresh(const QString &in_program_id, int in_program_revision,
                                               bool &out_is_fresh)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();
	out_is_fresh = false;

	if (Check_Open() == false)
		return false;

	query.prepare("SELECT program_revision FROM program_stats WHERE program_id = ?");
	query.addBindValue(in_program_id);

	if (query.exec() == false)
		return Set_Last_Error(QString("Не удалось проверить кэш статистики: %1").arg(query.lastError().text()));

	if (query.next() == false)
		return true;

	out_is_fresh = (query.value(0).toInt() == in_program_revision);

	return true;
}
//----------------------------------------------------------------------------
bool AStats_Repository::Save_Program_Stats(const QString &in_program_id, int in_program_revision,
                                           const QString &in_payload_json)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();

	if (Check_Open() == false)
		return false;

	if (in_program_id.isEmpty())
		return Set_Last_Error("Кэшу статистики нужен programId");

	if (in_program_revision < 1)
		return Set_Last_Error("Кэш статистики привязан к ревизии структуры программы (revision >= 1)");

	if (Validate_Payload(in_payload_json) == false)
		return Set_Last_Error("Нагрузка кэша статистики должна быть JSON-объектом");

	query.prepare("INSERT INTO program_stats(program_id, computed_at, program_revision, payload_json) "
	              "VALUES(?, ?, ?, ?) "
	              "ON CONFLICT(program_id) DO UPDATE SET computed_at = excluded.computed_at, "
	              "program_revision = excluded.program_revision, payload_json = excluded.payload_json");
	query.addBindValue(in_program_id);
	query.addBindValue(Now_Stamp());
	query.addBindValue(in_program_revision);
	query.addBindValue(in_payload_json);

	if (query.exec())
		return true;

	return Set_Last_Error(QString("Не удалось сохранить кэш статистики: %1").arg(query.lastError().text()));
}
//----------------------------------------------------------------------------
bool AStats_Repository::Save_Lifetime_Stats(const QString &in_user_id, const QString &in_payload_json)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();

	if (Check_Open() == false)
		return false;

	if (in_user_id.isEmpty())
		return Set_Last_Error("Общей статистике нужен userId");

	if (Validate_Payload(in_payload_json) == false)
		return Set_Last_Error("Нагрузка общей статистики должна быть JSON-объектом");

	query.prepare("INSERT INTO lifetime_stats(user_id, computed_at, payload_json) VALUES(?, ?, ?) "
	              "ON CONFLICT(user_id) DO UPDATE SET computed_at = excluded.computed_at, "
	              "payload_json = excluded.payload_json");
	query.addBindValue(in_user_id);
	query.addBindValue(Now_Stamp());
	query.addBindValue(in_payload_json);

	if (query.exec())
		return true;

	return Set_Last_Error(QString("Не удалось сохранить общую статистику: %1").arg(query.lastError().text()));
}
//----------------------------------------------------------------------------
bool AStats_Repository::Invalidate_Program_Stats(const QString &in_program_id)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();

	if (Check_Open() == false)
		return false;

	query.prepare("DELETE FROM program_stats WHERE program_id = ?");
	query.addBindValue(in_program_id);

	if (query.exec())
		return true;

	return Set_Last_Error(QString("Не удалось сбросить кэш статистики: %1").arg(query.lastError().text()));
}
//----------------------------------------------------------------------------
bool AStats_Repository::Invalidate_Lifetime_Stats(const QString &in_user_id)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();

	if (Check_Open() == false)
		return false;

	query.prepare("DELETE FROM lifetime_stats WHERE user_id = ?");
	query.addBindValue(in_user_id);

	if (query.exec())
		return true;

	return Set_Last_Error(QString("Не удалось сбросить общую статистику: %1").arg(query.lastError().text()));
}
//----------------------------------------------------------------------------
// Запись факта или завершение программы касаются и программы, и общей
// статистики, поэтому сброс идёт одной транзакцией.
//----------------------------------------------------------------------------
bool AStats_Repository::Invalidate_User_Stats(const QString &in_user_id)
{
	QSqlQuery query(Connection());

	Clear_Last_Error();

	if (Check_Open() == false)
		return false;

	if (Connection().transaction() == false)
		return Set_Last_Error("Не удалось начать транзакцию");

	query.prepare("DELETE FROM program_stats WHERE program_id IN (SELECT id FROM programs WHERE user_id = ?)");
	query.addBindValue(in_user_id);

	if (query.exec() == false)
	{
		Connection().rollback();

		return Set_Last_Error(QString("Не удалось сбросить кэш статистики программ: %1").arg(query.lastError().text()));
	}

	query.prepare("DELETE FROM lifetime_stats WHERE user_id = ?");
	query.addBindValue(in_user_id);

	if (query.exec() == false)
	{
		Connection().rollback();

		return Set_Last_Error(QString("Не удалось сбросить общую статистику: %1").arg(query.lastError().text()));
	}

	if (Connection().commit() == false)
		return Set_Last_Error("Не удалось зафиксировать сброс статистики");

	return true;
}
//----------------------------------------------------------------------------
