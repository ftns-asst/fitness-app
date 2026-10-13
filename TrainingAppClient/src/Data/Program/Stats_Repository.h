#pragma once

#include "Data/Program/Program_Repository_Base.h"
#include "Domain/Program_Model.h"

#include <QString>

//----------------------------------------------------------------------------
// Локальный кэш статистики (срез W4, issue 23; §5.4).
//
// Первичные данные — только факты: workouts, sets, rule_checkins, замеры и
// структура программы с журналом ревизий. `program_stats` / `lifetime_stats`
// хранят готовую JSON-нагрузку вместе с меткой пересчёта и ревизией структуры;
// сам пересчёт делает `AsProgress_Calculator` (W7), репозиторий отвечает за
// хранение, проверку актуальности и инвалидацию.
//
// Инвалидация (§5.4) удаляет строку: любая запись факта, правка раздела
// программы, правка правил, завершение программы или новый замер делают
// прежние числа недействительными, а пересчёт обязан дать те же числа, что и
// инкрементальное обновление (тест-инвариант W7).
//----------------------------------------------------------------------------
class AStats_Repository : public AProgram_Repository_Base
{
	Q_OBJECT

public:
	explicit AStats_Repository(AsSql_Database *in_database, QObject *in_parent = 0);

	// Чтение кэша. Отсутствие строки — false, детали — Get_Last_Error().
	bool Load_Program_Stats(const QString &in_program_id, SStats_Cache_Entry &out_entry);
	bool Load_Lifetime_Stats(const QString &in_user_id, SStats_Cache_Entry &out_entry);

	// Актуальность: строка есть и посчитана на указанной ревизии структуры.
	bool Is_Program_Stats_Fresh(const QString &in_program_id, int in_program_revision, bool &out_is_fresh);

	// Запись. Computed_At штампуется репозиторием, нагрузка проверяется как
	// корректный JSON-объект (иначе false).
	bool Save_Program_Stats(const QString &in_program_id, int in_program_revision, const QString &in_payload_json);
	bool Save_Lifetime_Stats(const QString &in_user_id, const QString &in_payload_json);

	// Инвалидация: одна программа, общая статистика, всё по пользователю.
	bool Invalidate_Program_Stats(const QString &in_program_id);
	bool Invalidate_Lifetime_Stats(const QString &in_user_id);
	bool Invalidate_User_Stats(const QString &in_user_id);

private:
	bool Validate_Payload(const QString &in_payload_json);
};
//----------------------------------------------------------------------------
