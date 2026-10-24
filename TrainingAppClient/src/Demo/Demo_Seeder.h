#pragma once

#include "Domain/Error_State.h"
#include "Domain/Program_Model.h"

#include <QObject>
#include <QString>

using namespace Program_Model;

class AProgram_Repository;
class ARule_Repository;
class AWorkout_Repository;
class AMeasurement_Repository;

//----------------------------------------------------------------------------
// Сидер демо-программы: наполняет базу данными, на которых смотрят экраны
// «Мои программы», «Обзор», «Правила» и «Статистика».
//
// Зачем он нужен. Без него доступны только пустые состояния экранов: неделя
// из семи тире, «Пока нет данных», «Программ пока нет». Состояния «обычное»
// из спецификации §6 оказываются недостижимыми — их нечем проверять, и
// дефекты в них не ловятся.
//
// Отдельный класс, а не флаг в App_Context: сидер пишет данные, и его
// случайный вызов в релизной сборке наполнил бы базу пользователя мусором.
// Вызов только из кода, который явно передаёт признак «это демо» (Main.qml по
// аргументу запуска), и никогда сам по себе.
//
// Данные проходят через репозитории и доменную валидацию, а не через прямой
// SQL: иначе сидер мог бы записать то, что экраны потом не прочитают, и
// расхождение осталось бы незамеченным — ровно тот случай, ради которого
// сидер и написан.
class ADemo_Seeder : public QObject
{
	Q_OBJECT

public:
	explicit ADemo_Seeder(QObject *in_parent = 0);

	QString Get_Last_Error() const;
	// Идентификатор созданной программы; пуст, пока Seed не вызывался или
	// отказал. Нужен экрану для проверки, что сидер отработал.
	QString Program_Id() const;

	// Создаёт демо-программу пользователя in_user_id вместе с завершёнными
	// тренировками, отметками правил и замерами тела. Повторный вызов ничего
	// не меняет и возвращает true: база не должна накапливать программы при
	// каждом запуске приложения.
	bool Seed(AProgram_Repository *in_programs, ARule_Repository *in_rules, const QString &in_user_id,
	          AWorkout_Repository *in_workouts, AMeasurement_Repository *in_measurements);

private:
	bool Set_Last_Error(const QString &in_error);
	void Clear_Last_Error();

	// Шаги посева. Порядок обязателен: программа → структура → правила →
	// тренировки → отметки → замеры, потому что каждая следующая вставка
	// ссылается на предыдущую: отметка — на правило и тренировку.
	bool Seed_Program(AProgram_Repository *in_programs, const QString &in_user_id);
	bool Seed_Structure(AProgram_Repository *in_programs);
	bool Seed_Rules(ARule_Repository *in_rules);
	bool Seed_Workouts(AWorkout_Repository *in_workouts, const QString &in_user_id);
	// Отметки заполняют процент выполнения правил: без них экран показывает
	// «—» и подпись «Отметок за это окно нет».
	bool Seed_Checkins(ARule_Repository *in_rules);
	// Замеры заполняют метрику «Тело, кг» на экране статистики.
	bool Seed_Measurements(AMeasurement_Repository *in_measurements, const QString &in_user_id);

	// Есть ли у пользователя программа: над существующей посев не выполняется.
	bool Has_Any_Program(AProgram_Repository *in_programs, const QString &in_user_id);
	// Даты демо-тренировок отсчитываются от сегодняшней, а не от фиксированной
	// строки: иначе через несколько недель вся статистика оказалась бы в
	// прошлом и экран показывал пустоту.
	static QString Recent_Date(int in_days_ago);
	static QString Recent_Stamp(const QString &in_date, int in_hour);

	AError_State Error_State;
	QString Program_Id_Text;
};
//----------------------------------------------------------------------------