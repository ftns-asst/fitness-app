#pragma once

#include "Data/Database/Sql_Database.h"
#include "Domain/Error_State.h"
#include "Domain/Program_Model.h"

#include <QObject>
#include <QSqlDatabase>
#include <QString>
#include <QVariant>

using namespace Program_Model;

//----------------------------------------------------------------------------
// Общий каркас SQL-репозиториев модели программы (срез W, issue 23):
// AProgram_Repository, ARule_Repository, AMeasurement_Repository,
// AStats_Repository. Единая точка для того, что раньше дублировалось
// в каждом репозитории:
//
// - Connection() — null-безопасный доступ к соединению: nullptr отдаёт
//   невалидный QSqlDatabase, а Check_Open() превращает это в ошибку
//   «SQLite не открыта» вместо разыменования нуля в начале метода;
// - Set_Last_Error/Get_Last_Error — запись и логирование ошибок в одной
//   общей категории (trainingapp.program.repository);
// - Text_Column/Number_Column/Int_Column — значения → SQL NULL для
//   TEXT/REAL/INTEGER-колонок schema v2;
// - Write_Revision_Row/Bump_Revision_Internal — журнал program_revisions
//   (§3.9) и оптимистическая блокировка ревизии: единственная реализация
//   инварианта «каждая правка увеличивает revision и оставляет след»,
//   вместо двух копий в Program_Repository и Rule_Repository;
// - Is_Read_Only_State — инвариант 24 §4 (completed/cancelled/archived
//   — read-only по структуре), вместо двух копий проверки.
//----------------------------------------------------------------------------
class AProgram_Repository_Base : public QObject
{
	Q_OBJECT

public:
	QString Get_Last_Error() const;

protected:
	explicit AProgram_Repository_Base(AsSql_Database *in_database, QObject *in_parent = 0);

	bool Check_Open();

	// Хранилище ошибки (рефакторинг ошибок): состояние и логирование в
	// PROGRAM_Repository_Log живут в AError_State.
	bool Set_Last_Error(const QString &in_error);
	void Clear_Last_Error();

	// Null-безопасное соединение: невалидный QSqlDatabase при закрытой
	// базе или nullptr — дальнейшие запросы откатятся через Check_Open().
	QSqlDatabase Connection() const;

	// Пустая строка → SQL NULL (даты и необязательные ссылки).
	static QVariant Text_Column(const QString &in_value);
	static QVariant Number_Column(double in_value, bool in_has);
	static QVariant Int_Column(int in_value, bool in_has);

	// Инвариант 24 §4: правки структуры запрещены.
	static bool Is_Read_Only_State(Program_Model::EProgram_State in_state);

	// Журнал program_revisions (§3.9) и инкремент ревизии с оптимистичной
	// проверкой: параллельная правка отклоняется, а не теряется молча.
	bool Write_Revision_Row(const QString &in_program_id, int in_revision, Program_Model::ERevision_Reason in_reason,
	                        const QString &in_summary, const QString &in_snapshot_json);
	bool Bump_Revision_Internal(const QString &in_program_id, int in_old_revision);

	AsSql_Database *Database_Instance;
	AError_State Error_State;
};
//----------------------------------------------------------------------------