#include "Data/Database/Sql_Database.h"
#include "Data/Program/Measurement_Repository.h"
#include "Data/Program/Program_Repository.h"
#include "Data/Program/Rule_Repository.h"
#include "Data/Program/Workout_Repository.h"
#include "Demo/Demo_Seeder.h"
#include "Domain/Rule_Engine.h"

#include <QDate>
#include <QSqlError>
#include <QSqlQuery>
#include <QTemporaryDir>
#include <QtTest>

using namespace Program_Model;

//-------------------------------------------------------------------------------------------------------------
// Сидер демо-программы: он наполняет базу данными, на которых смотрят экраны
// «Мои программы» и «Статистика». Без него доступны только пустые состояния,
// а «обычные» состояния экранов проверить нечем.
//
// Проверяется не содержимое каждого поля (это деталь фикстуры), а то, что
// данные валидны и читаются обратно теми же репозиториями, что и экран.
//-------------------------------------------------------------------------------------------------------------
class ADemo_Seeder_Test : public QObject
{
	Q_OBJECT

private slots:
	void createsFullWeekWithExercises();
	void createsRulesThatRepositoryReadsBack();
	void createsCompletedWorkoutForStatistics();
	// Отметки и замеры — те два блока, которые без посева остаются пустыми:
	// процент выполнения правил и метрика «Тело, кг».
	void createsCheckinsForRuleWindow();
	void createsMeasurementsForBodyMetric();
	void seedingTwiceKeepsOneProgram();
	void doesNotTouchExistingProgram();
	void refusesToSeedWithoutDatabase();

private:
	QTemporaryDir Directory;
	AsSql_Database Database;
	AProgram_Repository *Programs = 0;
	ARule_Repository *Rules = 0;
	AWorkout_Repository *Workouts = 0;
	AMeasurement_Repository *Measurements = 0;
	bool Open_Database();
	// Общий вызов посева: сигнатура выросла, и каждый тест обязан передавать
	// репозиторий замеров — иначе проверялось бы не то, что запускает
	// приложение.
	bool Seed_Demo(ADemo_Seeder &in_seeder, const QString &in_user_id);
};

//-------------------------------------------------------------------------------------------------------------
bool ADemo_Seeder_Test::Open_Database()
{
	const QString path = Directory.filePath("demo.sqlite3");

	if (!Directory.isValid() || !Database.Open(path))
		return false;

	Programs = new AProgram_Repository(&Database, this);
	Rules = new ARule_Repository(&Database, this);
	Workouts = new AWorkout_Repository(&Database, this);
	Measurements = new AMeasurement_Repository(&Database, this);

	return true;
}
//-------------------------------------------------------------------------------------------------------------
bool ADemo_Seeder_Test::Seed_Demo(ADemo_Seeder &in_seeder, const QString &in_user_id)
{
	return in_seeder.Seed(Programs, Rules, in_user_id, Workouts, Measurements);
}
//-------------------------------------------------------------------------------------------------------------
// Неделя должна быть заполнена: экран обзора рисует семь плиток, и без
// упражнений все они показывают пустое тире — состояние «создано, но не
// настроено», а не «программа в работе».
void ADemo_Seeder_Test::createsFullWeekWithExercises()
{
	ADemo_Seeder seeder;
	QVector<SProgram_Day> days;
	QVector<SWeek_Slot> week_slots;

	QVERIFY(Open_Database());
	QVERIFY2(Seed_Demo(seeder, "u_demo"), qPrintable(seeder.Get_Last_Error()));

	QVERIFY(Programs->Load_Days(seeder.Program_Id(), days));
	QCOMPARE(days.size(), 7);

	// Упражнения читаются по дням: это единственный публичный путь, каким их
	// читает экран дня, и он же проверяет, что сидер не рассчитывает на
	// внутренний метод репозитория.
	int total_exercises = 0;

	for (int day_index = 0; day_index < days.size(); ++day_index)
	{
		QVector<SDay_Exercise> day_exercises;

		QVERIFY(Programs->Load_Day_Exercises(days.at(day_index).ID, day_exercises));
		total_exercises += day_exercises.size();
	}

	QVERIFY2(total_exercises > 0, "в демо-программе нет ни одного упражнения");

	// Упражнение есть не в каждом дне: день отдыха по определению пуст.
	QVERIFY(Programs->Load_Schedule(seeder.Program_Id(), week_slots));
	QCOMPARE(week_slots.size(), 7);
}
//-------------------------------------------------------------------------------------------------------------
// Правила читаются обратно тем же репозиторием, что и экран «Правила». Если
// сидер запишет правило в обход ARule_Repository, экран покажет пустоту при
// полной базе — ровно тот дефект, который сидер и должен исключить.
void ADemo_Seeder_Test::createsRulesThatRepositoryReadsBack()
{
	ADemo_Seeder seeder;
	QVector<SProgram_Rule> rules;

	QVERIFY(Open_Database());
	QVERIFY2(Seed_Demo(seeder, "u_demo"), qPrintable(seeder.Get_Last_Error()));

	QVERIFY(Rules->Load_Rules(seeder.Program_Id(), rules));
	QVERIFY2(!rules.isEmpty(), "демо-программа без правил: экран правил пуст");

	for (int i = 0; i < rules.size(); ++i)
	{
		QVERIFY2(!rules.at(i).Title.isEmpty(), "правило без названия");
		// Измеримое правило обязано иметь цель: без неё оно не отметится.
		QVERIFY2(Get_Rule_Kind(rules.at(i)) == ERule_Kind::Advice || rules.at(i).Has_Target_Min ||
		             rules.at(i).Has_Target_Max || rules.at(i).Has_Target_Exact,
		         "правило без цели и без текста");
	}
}
//-------------------------------------------------------------------------------------------------------------
// Статистика показывает «Пока нет данных», пока нет завершённых тренировок.
// Без них половина экрана недостижима.
void ADemo_Seeder_Test::createsCompletedWorkoutForStatistics()
{
	ADemo_Seeder seeder;
	QVector<SWorkout_Session> sessions;

	QVERIFY(Open_Database());
	QVERIFY2(Seed_Demo(seeder, "u_demo"), qPrintable(seeder.Get_Last_Error()));

	// Пустой диапазон дат означает «все сессии» — так задумано в
	// Load_Sessions, и сидер не должен полагаться на конкретные даты.
	QVERIFY(Workouts->Load_Sessions("u_demo", seeder.Program_Id(), QString(), QString(), sessions));
	QVERIFY2(!sessions.isEmpty(), "нет завершённых тренировок: статистика пуста");

	bool has_completed = false;

	for (int i = 0; i < sessions.size(); ++i)
	{
		if (sessions.at(i).State == EWorkout_State::Completed)
			has_completed = true;
	}

	QVERIFY(has_completed);
}
//-------------------------------------------------------------------------------------------------------------
// Экран «Правила» без отметок показывает прочерк вместо процента и подпись
// «Отметок за это окно нет». Проверяется не число отметок, а то, что окно
// экрана (неделя, §5.1) после посева непусто и что отметки принадлежат
// правилам программы: отметка чужой программы попала бы в другую выборку.
void ADemo_Seeder_Test::createsCheckinsForRuleWindow()
{
	ADemo_Seeder seeder;
	QVector<SRule_Checkin> checkins;
	const QString today = QDate::currentDate().toString("yyyy-MM-dd");
	const QString week_ago = QDate::currentDate().addDays(-6).toString("yyyy-MM-dd");

	QVERIFY(Open_Database());
	QVERIFY2(Seed_Demo(seeder, "u_demo"), qPrintable(seeder.Get_Last_Error()));

	// Окно ровно то, которое запрашивает экран по умолчанию.
	QVERIFY(Rules->Load_Program_Checkins_Range(seeder.Program_Id(), week_ago, today, checkins));
	QVERIFY2(!checkins.isEmpty(), "нет отметок правил: процент выполнения не показывается");

	for (int i = 0; i < checkins.size(); ++i)
	{
		QCOMPARE(checkins.at(i).Program_Id, seeder.Program_Id());
		QVERIFY2(!checkins.at(i).Rule_Id.isEmpty(), "отметка без правила");
		QVERIFY2(!checkins.at(i).Unit.isEmpty(), "отметка без единицы измерения");
	}

	// Рекомендация текстом не отмечается: её отметка не измеряет ничего и не
	// должна попадать в процент выполнения (§3.6).
	QVector<SProgram_Rule> rules;
	QVERIFY(Rules->Load_Rules(seeder.Program_Id(), rules));

	for (int rule_index = 0; rule_index < rules.size(); ++rule_index)
	{
		if (Get_Rule_Kind(rules.at(rule_index)) != ERule_Kind::Advice)
			continue;

		for (int i = 0; i < checkins.size(); ++i)
			QVERIFY(checkins.at(i).Rule_Id != rules.at(rule_index).ID);
	}

	// Отметка измеримого правила несёт вердикт: с состоянием «неизвестно»
	// заполненная отметка выглядит как «Нет отметки», а процент выполнения не
	// показывается вовсе — в метрику входят только вердикты (§5.5).
	for (int i = 0; i < checkins.size(); ++i)
		QVERIFY2(checkins.at(i).State != ECheckin_State::Unknown, "отметка без вердикта");

	AsRule_Engine engine;
	SRule_Stats overall;

	QVERIFY2(engine.Compute_Overall_Stats(rules, checkins, overall), qPrintable(engine.Get_Last_Error()));
	QVERIFY2(overall.Has_Adherence_Percent, "ожидаемые отметки не дают процента выполнения");

	// Оба вердикта присутствуют: иначе экран проверяется только на «выполнено».
	QVERIFY(overall.Done_Count > 0);
	QVERIFY(overall.Failed_Count > 0);
}
//-------------------------------------------------------------------------------------------------------------
// Метрика «Тело, кг» без замеров остаётся прочерком. Проверяется, что вес есть
// и что история непуста: калькулятор считает дельту по первым и последним
// замерам окна, и одна запись дала бы нулевую дельту вместо её вида.
void ADemo_Seeder_Test::createsMeasurementsForBodyMetric()
{
	ADemo_Seeder seeder;
	QVector<SBody_Measurement> measurements;
	const QString today = QDate::currentDate().toString("yyyy-MM-dd");
	// Границы обязательны: Load_Range в отличие от Load_Sessions не трактует
	// пустую дату как «вся история», и пустые границы вернули бы пустой список.
	const QString history_start = QDate::currentDate().addDays(-30).toString("yyyy-MM-dd");

	QVERIFY(Open_Database());
	QVERIFY2(Seed_Demo(seeder, "u_demo"), qPrintable(seeder.Get_Last_Error()));

	QVERIFY(Measurements->Load_Range("u_demo", history_start, today, measurements));
	QVERIFY2(measurements.size() > 1, "замеров меньше двух: дельта веса не считается");

	int with_weight = 0;

	for (int i = 0; i < measurements.size(); ++i)
	{
		if (!measurements.at(i).Has_Weight_Kg)
			continue;

		++with_weight;
		QVERIFY2(measurements.at(i).Weight_Kg > 0, "неправдоподобный вес в демо-данных");
	}

	QVERIFY2(with_weight > 1, "вес записан один раз: дельта не появится");

	// Последний замер несёт обхваты: иначе блок изменения замеров пуст.
	QVERIFY2(measurements.last().Date == today, "последний замер не за сегодня");
	QVERIFY2(measurements.last().Has_Chest_Cm, "нет обхватов в последнем замере");
}
//-------------------------------------------------------------------------------------------------------------
// Повторный посев не плодит программы: экран запускается много раз за сеанс,
// и каждый запуск добавлял бы «Моя программа» в каталог.
void ADemo_Seeder_Test::seedingTwiceKeepsOneProgram()
{
	ADemo_Seeder seeder;
	QVector<SProgram> programs;

	QVERIFY(Open_Database());
	QVERIFY(Seed_Demo(seeder, "u_demo"));
	QVERIFY(Seed_Demo(seeder, "u_demo"));

	QVERIFY(Programs->Load_Programs("u_demo", programs));
	QCOMPARE(programs.size(), 1);
}
//-------------------------------------------------------------------------------------------------------------
// Чужая программа не должна наполняться демо-упражнениями. Индекс
// programs_one_active_per_user допускает одну активную программу, поэтому
// посев поверх неё упал бы с UNIQUE; сидер обязан пропустить посев и не
// превратить чужую программу в демонстрационную.
void ADemo_Seeder_Test::doesNotTouchExistingProgram()
{
	ADemo_Seeder seeder;
	SProgram existing;
	QVector<SProgram_Day> days;

	QVERIFY(Open_Database());

	// Свой идентификатор пользователя: база у теста одна на все случаи, и
	// программа, посеянная предыдущим тестом, иначе попала бы в выборку.
	existing.ID = "p_existing";
	existing.User_Id = "u_existing";
	existing.Title = "Моя программа";
	existing.State = EProgram_State::Draft;
	existing.Revision = 1;

	QVERIFY(Programs->Insert_Program_Full(existing, QVector<SGoal_Target>(), QVector<SProgram_Day>(),
	                                      QVector<SDay_Exercise>(), QVector<SWeek_Slot>()));

	// Посев не падает и не добавляет программ.
	QVERIFY2(Seed_Demo(seeder, "u_existing"), qPrintable(seeder.Get_Last_Error()));

	QVector<SProgram> programs;

	QVERIFY(Programs->Load_Programs("u_existing", programs));
	QCOMPARE(programs.size(), 1);
	QCOMPARE(programs.at(0).ID, QString("p_existing"));

	QVERIFY(Programs->Load_Days("p_existing", days));
	QVERIFY2(days.isEmpty(), "демо-данные попали в чужую программу");
}
//-------------------------------------------------------------------------------------------------------------
// Без открытой базы сидер обязан отказать, а не упасть: он вызывается из
// App_Context на старте, где отказ должен быть внятной ошибкой.
void ADemo_Seeder_Test::refusesToSeedWithoutDatabase()
{
	ADemo_Seeder seeder;

	QVERIFY(!seeder.Seed(0, 0, "u_demo", 0, 0));
	QVERIFY(!seeder.Get_Last_Error().isEmpty());
}

QTEST_GUILESS_MAIN(ADemo_Seeder_Test)
#include "tst_Demo_Seeder.moc"