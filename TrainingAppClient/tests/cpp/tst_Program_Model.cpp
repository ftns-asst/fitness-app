#include "Domain/Program_Model.h"

#include <QtTest>

using namespace Program_Model;

class AProgram_Model_Test : public QObject
{
	Q_OBJECT

private slots:
	void enumRoundTrip();
	void enumFromUnknownTextKeepsValue();
	void termComputesEndDate();
	void progressionJsonRoundTrip();
	void progressionJsonRejectsInvalidInput();
	void jsonArrayRoundTrip();
	void enumMaxMarksBoundAndFullRoundTrip();

	// — Категория «Другое» (issue 46) —
	// Текст должен совпадать со значением CHECK в схеме v3, иначе правило
	// запишется в БД и не прочитается обратно.
	void otherCategoryRoundTrip();

	// — Вид правила —
	void ruleKindFollowsTargetPresence();
	void adviceTextMustHaveNonSpaceCharacter();
};

//----------------------------------------------------------------------------
void AProgram_Model_Test::enumRoundTrip()
{
	// Строковые значения обязаны совпадать с CHECK-ограничениями schema
	// v2.
	QCOMPARE(To_String(EProgram_State::Active), QString("active"));
	QCOMPARE(To_String(EProgram_State::Archived), QString("archived"));
	QCOMPARE(To_String(EProgram_Goal::FatLoss), QString("fatLoss"));
	QCOMPARE(To_String(EProgram_Goal::Endurance), QString("endurance"));
	QCOMPARE(To_String(EProgram_Source_Kind::Matched), QString("matched"));
	QCOMPARE(To_String(ESchedule_Kind::Rotation), QString("rotation"));
	QCOMPARE(To_String(ETerm_Mode::OpenEnded), QString("openEnded"));
	QCOMPARE(To_String(ESync_State::Syncing), QString("syncing"));
	QCOMPARE(To_String(EGoal_Metric::RepsAtWeight), QString("repsAtWeight"));
	QCOMPARE(To_String(EMeasurement_Key::Thigh), QString("thigh"));
	QCOMPARE(To_String(ESlot_Kind::Mobility), QString("mobility"));
	QCOMPARE(To_String(EDay_Kind::Cardio), QString("cardio"));
	QCOMPARE(To_String(EWeight_Mode::PercentOf1RM), QString("percentOf1RM"));
	QCOMPARE(To_String(EProgression_Type::DoubleProgression), QString("doubleProgression"));
	QCOMPARE(To_String(ERule_Category::Technique), QString("technique"));
	QCOMPARE(To_String(ERule_Metric::RestHoursBetweenSessions), QString("restHoursBetweenSessions"));
	// Метрики макронутриентов (спринт «КБЖУ в правилах»): правила «белок/углеводы/
	// жиры ≥ N г» создаются из расчёта КБЖУ, и без них в программе нечем закрепить
	// нормы макросов. Значения совпадают с CHECK schema v8.
	QCOMPARE(To_String(ERule_Metric::CarbGrams), QString("carbGrams"));
	QCOMPARE(To_String(ERule_Metric::FatGrams), QString("fatGrams"));
	QCOMPARE(To_String(ERule_Periodicity::PerWorkout), QString("perWorkout"));
	QCOMPARE(To_String(ERule_Applies_To::CustomDays), QString("customDays"));
	QCOMPARE(To_String(ERule_Check_Source::ComputedFromLog), QString("computedFromLog"));
	QCOMPARE(To_String(ERule_Enforcement::Hard), QString("hard"));
	QCOMPARE(To_String(ERule_Display_Unit::GPerKg), QString("gPerKg"));
	QCOMPARE(To_String(ECheckin_State::Partial), QString("partial"));
	QCOMPARE(To_String(ECheckin_Source::Imported), QString("imported"));
	QCOMPARE(To_String(EDeviation::MovedDay), QString("movedDay"));
	QCOMPARE(To_String(EWorkout_State::Cancelled), QString("cancelled"));
	QCOMPARE(To_String(ERevision_Reason::ExercisePlan), QString("exercisePlan"));

	EProgram_State state = EProgram_State::Draft;
	QVERIFY(From_String(QString("paused"), state));
	QCOMPARE(state, EProgram_State::Paused);

	EGoal_Direction direction = EGoal_Direction::Increase;
	QVERIFY(From_String(QString("maintain"), direction));
	QCOMPARE(direction, EGoal_Direction::Maintain);

	ECheckin_Source source = ECheckin_Source::Manual;
	QVERIFY(From_String(QString("computed"), source));
	QCOMPARE(source, ECheckin_Source::Computed);

	// Метрики макронутриентов читаются обратно из строк (правила из расчёта КБЖУ
	// парсятся из кэша/БД теми же строками, что пишет To_String).
	ERule_Metric carb = ERule_Metric::ProteinGrams;
	ERule_Metric fat = ERule_Metric::ProteinGrams;
	QVERIFY(From_String(QString("carbGrams"), carb));
	QCOMPARE(carb, ERule_Metric::CarbGrams);
	QVERIFY(From_String(QString("fatGrams"), fat));
	QCOMPARE(fat, ERule_Metric::FatGrams);

	EGoal_Target_State target_state = EGoal_Target_State::Active;
	QVERIFY(From_String(QString("dropped"), target_state));
	QCOMPARE(target_state, EGoal_Target_State::Dropped);
}
//----------------------------------------------------------------------------
void AProgram_Model_Test::enumFromUnknownTextKeepsValue()
{
	EProgram_State state = EProgram_State::Active;
	ERule_Metric metric = ERule_Metric::ProteinGrams;

	QVERIFY(From_String(QString("wrong"), state) == false);
	QCOMPARE(state, EProgram_State::Active);
	QVERIFY(From_String(QString(""), metric) == false);
	QCOMPARE(metric, ERule_Metric::ProteinGrams);
}
//----------------------------------------------------------------------------
void AProgram_Model_Test::termComputesEndDate()
{
	STerm term;

	// §3.3: endDate = startDate + weeksTotal × 7 − 1 день.
	term.Start_Date = "2026-09-29";
	term.Weeks_Total = 12;
	QCOMPARE(term.Compute_End_Date(), QString("2026-12-21"));

	// Продление — журнал, endDate не двигает.
	term.Extended_Weeks = 2;
	QCOMPARE(term.Compute_End_Date(), QString("2026-12-21"));

	term.Weeks_Total = 1;
	QCOMPARE(term.Compute_End_Date(), QString("2026-10-05"));

	// Открытый срок и битая дата — пустой результат.
	term.Mode = ETerm_Mode::OpenEnded;
	QVERIFY(term.Compute_End_Date().isEmpty());

	term.Mode = ETerm_Mode::FixedWeeks;
	term.Weeks_Total = 4;
	term.Start_Date = "not-a-date";
	QVERIFY(term.Compute_End_Date().isEmpty());
}
//----------------------------------------------------------------------------
void AProgram_Model_Test::progressionJsonRoundTrip()
{
	SProgression_Rule rule;
	SProgression_Rule parsed;
	QString json;

	rule.Type = EProgression_Type::DoubleProgression;
	rule.Increment_Kg = 2.5;
	rule.Has_Increment_Kg = true;
	rule.Trigger_Sets = 4;
	rule.Has_Trigger_Sets = true;
	rule.Trigger_Reps_Min = 10;
	rule.Has_Trigger_Reps_Min = true;
	rule.Max_Rpe = 8.0;
	rule.Has_Max_Rpe = true;
	rule.Deload_Every_Weeks = 4;
	rule.Has_Deload_Every_Weeks = true;

	json = To_Progression_Json(rule);
	QVERIFY(Parse_Progression_Json(json, parsed));
	QCOMPARE(parsed.Type, rule.Type);
	QVERIFY(parsed.Has_Increment_Kg);
	QCOMPARE(parsed.Increment_Kg, 2.5);
	QVERIFY(parsed.Has_Trigger_Sets);
	QCOMPARE(parsed.Trigger_Sets, 4);
	QVERIFY(parsed.Has_Trigger_Reps_Min);
	QCOMPARE(parsed.Trigger_Reps_Min, 10);
	QVERIFY(parsed.Has_Max_Rpe);
	QCOMPARE(parsed.Max_Rpe, 8.0);
	QVERIFY(parsed.Has_Deload_Every_Weeks);
	QCOMPARE(parsed.Deload_Every_Weeks, 4);

	// Минимальное правило: только тип, необязательные поля
	// отсутствуют.
	rule = SProgression_Rule();
	rule.Type = EProgression_Type::Manual;
	json = To_Progression_Json(rule);
	QVERIFY(json.contains("manual"));
	QVERIFY(json.contains("incrementKg") == false);
	QVERIFY(Parse_Progression_Json(json, parsed));
	QCOMPARE(parsed.Type, EProgression_Type::Manual);
	QVERIFY(parsed.Has_Increment_Kg == false);
	QVERIFY(parsed.Has_Deload_Every_Weeks == false);
}
//----------------------------------------------------------------------------
void AProgram_Model_Test::progressionJsonRejectsInvalidInput()
{
	SProgression_Rule keep;

	keep.Type = EProgression_Type::Manual;
	QVERIFY(Parse_Progression_Json(QString("not json"), keep) == false);
	QVERIFY(Parse_Progression_Json(QString("[]"), keep) == false);
	QVERIFY(Parse_Progression_Json(QString("{\"type\":\"bogus\"}"), keep) == false);
	QCOMPARE(keep.Type, EProgression_Type::Manual); // отказ значение не меняет
}
//----------------------------------------------------------------------------
void AProgram_Model_Test::jsonArrayRoundTrip()
{
	QStringList list;
	QStringList parsed;
	QString json;

	list << "legs"
	     << "push"
	     << "core";
	json = To_Json_Array(list);
	QVERIFY(json == QString("[\"legs\",\"push\",\"core\"]"));
	QVERIFY(Parse_Json_Array(json, parsed));
	QCOMPARE(parsed, list);

	json = To_Json_Array(QStringList());
	QVERIFY(json == QString("[]"));
	QVERIFY(Parse_Json_Array(json, parsed));
	QVERIFY(parsed.isEmpty());

	QVERIFY(Parse_Json_Array(QString("{\"a\":1}"), parsed) == false);
	QVERIFY(Parse_Json_Array(QString("broken"), parsed) == false);
}
//----------------------------------------------------------------------------
// Категория «Другое» — то, чем пользователь пишет рекомендацию текстом.
// Значение обязано совпадать со строкой в CHECK-ограничении схемы v3.
void AProgram_Model_Test::otherCategoryRoundTrip()
{
	ERule_Category category = ERule_Category::Other;
	QString text = To_String(ERule_Category::Other);

	QCOMPARE(text, QString("other"));
	QVERIFY(From_String(text, category));
	QCOMPARE(category, ERule_Category::Other);

	// Метрика custom уже была в модели, но не использовалась: с ней
	// рекомендация проходит полный круг и обратно.
	QCOMPARE(To_String(ERule_Metric::Custom), QString("custom"));
	ERule_Metric metric = ERule_Metric::Custom;
	QVERIFY(From_String(QString("custom"), metric));
	QCOMPARE(metric, ERule_Metric::Custom);

	// «Other» не должен попадать в предыдущие категории: иначе фильтр в UI
	// покажет рекомендацию среди питания.
	QVERIFY(To_String(ERule_Category::Other) != To_String(ERule_Category::Nutrition));
	QVERIFY(To_String(ERule_Category::Other) != To_String(ERule_Category::Training));
}
//----------------------------------------------------------------------------
// Вид правила выводится из наличия цели, а не из категории: рекомендацией
// делается именно то правило, у которого нечего измерить. Проверка в БД
// (CHECK в схеме v3) и проверка в репозитории (Has_Advice_Text) в этом
// должны совпадать, иначе правило пройдёт одну и упадёт на другой.
void AProgram_Model_Test::ruleKindFollowsTargetPresence()
{
	SProgram_Rule rule;

	rule.Category = ERule_Category::Nutrition;
	rule.Has_Target_Min = true;
	QCOMPARE(Get_Rule_Kind(rule), ERule_Kind::Measurable);

	rule.Has_Target_Min = false;
	rule.Category = ERule_Category::Other;
	rule.Note = "Пей воду натощак";
	QCOMPARE(Get_Rule_Kind(rule), ERule_Kind::Advice);

	// Граница «Другое» с числом — это измеримое правило: цель задана, значит
	// вердикт есть, и в adherence правило участвует.
	rule.Category = ERule_Category::Other;
	rule.Target_Min = 8;
	rule.Has_Target_Min = true;
	QCOMPARE(Get_Rule_Kind(rule), ERule_Kind::Measurable);

	// max и exact дают измеримость наравне с min.
	rule.Has_Target_Min = false;
	rule.Target_Max = 3000;
	rule.Has_Target_Max = true;
	QCOMPARE(Get_Rule_Kind(rule), ERule_Kind::Measurable);

	rule.Has_Target_Max = false;
	rule.Target_Exact = 0.5;
	rule.Has_Target_Exact = true;
	QCOMPARE(Get_Rule_Kind(rule), ERule_Kind::Measurable);
}
//----------------------------------------------------------------------------
// Текст рекомендации должен быть содержательным. Строка из пробелов формально
// не пуста, и без этой проверки «Другое» превратилось бы в способ завести
// правило вообще без содержания — то самое, ради чего снималась проверка цели.
void AProgram_Model_Test::adviceTextMustHaveNonSpaceCharacter()
{
	SProgram_Rule rule;

	rule.Category = ERule_Category::Other;
	rule.Note = "Пей воду натощак";
	QVERIFY(Has_Advice_Text(rule));

	rule.Note = "   ";
	QVERIFY(Has_Advice_Text(rule) == false);

	rule.Note = "\t\n ";
	QVERIFY(Has_Advice_Text(rule) == false);

	rule.Note = QString();
	QVERIFY(Has_Advice_Text(rule) == false);

	// У измеримого правила текста может не быть вовсе — это не рекомендация,
	// и жаловаться тут не на что.
	rule.Category = ERule_Category::Nutrition;
	rule.Has_Target_Min = true;
	rule.Note = QString();
	QVERIFY(Has_Advice_Text(rule) == false);
}
//----------------------------------------------------------------------------
void AProgram_Model_Test::enumMaxMarksBoundAndFullRoundTrip()
{
	EProgram_State state = EProgram_State::Draft;
	EProgram_State parsed_state = EProgram_State::Archived;
	ERule_Metric metric = ERule_Metric::ProteinGrams;
	ERule_Metric parsed_metric = ERule_Metric::Custom;
	ERule_Display_Unit unit = ERule_Display_Unit::G;
	ERule_Display_Unit parsed_unit = ERule_Display_Unit::Units;
	ESchedule_Kind kind = ESchedule_Kind::Weekday;
	ESchedule_Kind parsed_kind = ESchedule_Kind::Rotation;
	int index = 0;

	// Max — маркер количества: все значения до него проходят
	// раунд-трип.
	for (index = 0; index < (int)EProgram_State::Max; ++index)
	{
		state = (EProgram_State)index;
		parsed_state = EProgram_State::Archived;
		QVERIFY(From_String(To_String(state), parsed_state));
		QCOMPARE(parsed_state, state);
	}

	for (index = 0; index < (int)ERule_Metric::Max; ++index)
	{
		metric = (ERule_Metric)index;
		parsed_metric = ERule_Metric::Custom;
		QVERIFY(From_String(To_String(metric), parsed_metric));
		QCOMPARE(parsed_metric, metric);
	}

	for (index = 0; index < (int)ERule_Display_Unit::Max; ++index)
	{
		unit = (ERule_Display_Unit)index;
		parsed_unit = ERule_Display_Unit::Units;
		QVERIFY(From_String(To_String(unit), parsed_unit));
		QCOMPARE(parsed_unit, unit);
	}

	for (index = 0; index < (int)ESchedule_Kind::Max; ++index)
	{
		kind = (ESchedule_Kind)index;
		parsed_kind = ESchedule_Kind::Rotation;
		QVERIFY(From_String(To_String(kind), parsed_kind));
		QCOMPARE(parsed_kind, kind);
	}

	// Сам маркер строкового представления не имеет.
	QVERIFY(To_String((EProgram_State)EProgram_State::Max).isEmpty());
	QVERIFY(To_String((ERule_Metric)ERule_Metric::Max).isEmpty());
	QVERIFY(To_String((ESlot_Kind)ESlot_Kind::Max).isEmpty());
}
//----------------------------------------------------------------------------

QTEST_MAIN(AProgram_Model_Test)

#include "tst_Program_Model.moc"
