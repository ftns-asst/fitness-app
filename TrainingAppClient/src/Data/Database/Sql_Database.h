#pragma once

#include <QObject>
#include <QSqlDatabase>
#include <QString>

//----------------------------------------------------------------------------
// Локальная SQLite-база приложения. Схема v1 хранит только клиентские данные:
// auth_tokens, кэш users/user_profiles, локальные параметры профиля и outbox.
// Серверный pass_hash никогда не является частью клиентской схемы.
//----------------------------------------------------------------------------
class AsSql_Database : public QObject
{
	Q_OBJECT
	Q_PROPERTY(bool open READ Is_Open CONSTANT)
	Q_PROPERTY(QString path READ Get_Database_Path CONSTANT)
	Q_PROPERTY(int schemaVersion READ Get_Schema_Version CONSTANT)

public:
	explicit AsSql_Database(QObject *in_parent = 0);
	~AsSql_Database() override;

	bool Open(const QString &in_path = QString());
	void Close();
	bool Migrate();
	bool Is_Open() const;
	QString Get_Database_Path() const;
	int Get_Schema_Version() const;
	QString Get_Last_Error() const;

	// Соединение для Data-сервисов (token store, репозитории). Неактивно,
	// пока база не открыта; владельцем соединения остаётся AsSql_Database.
	QSqlDatabase Sql_Connection() const;

	static QString Get_Default_Database_Path();
	static int Get_Current_Schema_Version();

private:
	bool Ensure_Directory(const QString &in_path);
	bool Set_Last_Error(const QString &in_error);
	bool Execute_SQL(const QString &in_sql);
	bool Create_Schema_V1();
	bool Create_Schema_V2();
	// v3 — рекомендации текстом (issue 46): program_rules получает категорию
	// 'other' и перестаёт требовать измеримую цель, если задан текст. SQLite не
	// умеет менять CHECK, поэтому таблица пересоздаётся, а данные переносятся
	// копированием — терять накопленные правила нельзя.
	bool Migrate_Schema_V2_To_V3();
	// v4 — килограммы в перечне единиц правил.
	bool Migrate_Schema_V3_To_V4();
	// v5 — параметры авторасчёта КБЖУ (спринт S7, issue 50): дата рождения и
	// уровень активности в profile_local.
	//
	// Миграция на ADD COLUMN, а не пересоздание таблицы: CHECK у новых столбцов
	// не меняет старый, а накопленный локальный профиль (уровень, цель, частота)
	// пришлось бы переносить копированием ради двух колонок — лишний риск на
	// данных пользователя. SQLite складывает такие столбцы в конец таблицы, и
	// upsert по имени столбцов от этого не страдает.
	bool Migrate_Schema_V4_To_V5();
	// v6 — результат авторасчёта КБЖУ (спринт S7): числа расчёта в profile_local.
	// Без них главная не знает, что расчёт уже делали, и после перезапуска
	// предлагала бы «Рассчитать» снова, а человек терял бы свои числа.
	bool Migrate_Schema_V5_To_V6();
	// v7 — кэш каталога упражнений и справочников (спринт U2, issue 30):
	// exercises_cache изоморфен ответу GET /exercises (списки значений —
	// через запятую: значения справочников — латиница без запятых),
	// muscle_groups_ref и equipments_ref — справочники для фильтров.
	bool Migrate_Schema_V6_To_V7();
	bool Migrate_Schema_V7_To_V8();
	// Столбец profile_local добавляется один раз: повторный ALTER падает, а
	// версия схемы могла не записаться при сбое в середине миграции.
	bool Add_Column_If_Missing(const QString &in_table, const QString &in_column, const QString &in_definition);
	// DDL program_rules вынесен в метод: смена CHECK требует пересоздания
	// таблицы, и две копии определения разошлись бы при правке.
	bool Create_Program_Rules_Table(const QString &in_name);
	// rule_checkins ссылается на program_rules через FK, поэтому создаётся
	// после неё отдельным вызовом — порядок не должен зависеть от позиции
	// строки в списке statements.
	bool Create_Rule_Checkins_Table();
	// Базовый каталог упражнений (ex_squat/ex_bench/ex_row/ex_deadlift): без
	// строк в exercises не сохраняется ни одно предписание дня — проверка FK
	// program_day_exercises.exercise_id → exercises.id отвергает вставку.
	// Запуск при каждом открытии безопасен: сначала чтение, запись только если
	// каталог неполон (INSERT OR IGNORE), версия схемы не меняется, ручной
	// каталог пользователя не трогается.
	bool Seed_Builtin_Exercises();
	bool Read_Schema_Version(int &out_version);
	bool Write_Schema_Version(int in_version);
	QString Connection_Name() const;

	QSqlDatabase *Database_Handle = 0;
	QString Database_File_Path;
	QString Connection_Name_Text;
	QString Last_Error_Text;
	int Current_Version = 0;
};
//----------------------------------------------------------------------------
