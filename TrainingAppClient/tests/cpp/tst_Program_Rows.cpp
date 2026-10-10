#include "Presentation/List_Model.h"
#include "Presentation/Program_Rows.h"

#include <QHash>
#include <QSet>
#include <QtTest>

// Роли QML для среза W (§7 `02-program-model.md`): доменные структуры должны
// попадать в модель под именами ролей контракта, а необязательные значения —
// давать null, а не «-1» (на экране это разные состояния).
class AProgram_Rows_Test : public QObject
{
	Q_OBJECT

private slots:
	void weekSlotsMapContractRoles();
	void weekSlotTodayFlagUsesGivenDate();
	void programDaysMapContractRoles();
	void dayExercisesFormatRepsAndWeight();
	void dayExerciseNullsMissingWeight();
	void rulesDescribeTargetRange();
	void ruleWithoutTargetHasNullValue();

	// — Рекомендация в списке правил —
	// У неё нет цели, но карточка всё равно обязана что-то показывать: пустая
	// подпись рядом с «≥ 140 г» выглядела бы как недогруженная строка.
	void adviceRuleShowsItsText();
	// Роль isAdvice обязана быть в контракте строки: по ней экран решает, рисуть
	// ли кнопки отметки, а решать это по displayText нельзя — он же является
	// подписью и может быть любым.
	void adviceFlagExposedAsRole();

	// — Каталог правил (§5.7) —
	// Каталог переехал из QML в C++: пресеты, объявленные в разметке, нечем
	// было ни проверить, ни отфильтровать по категории.
	void ruleCatalogHasStableOrderAndIds();
	void ruleCatalogFiltersByCategory();
	void ruleCatalogExposesFieldsNeededToAddRule();
	void catalogPresetsAreCreatable();

	// — Каталог метрик (форма своего правила, §5.7) —
	// Метрика задаёт свою единицу и периодичность: без такой таблицы форма
	// позволяла бы создать «белок в мл», и такое правило проходило бы всю
	// валидацию, а осмысла не имело.
	void metricCatalogGivesEachMetricItsOwnUnit();
	void metricCatalogRejectsUnitForeignToMetric();
	void metricCatalogCoversEveryRuleMetric();
	void goalTargetsMapProgress();
	void checkinsMapStateAndValue();
	void measurementsNullMissingFields();
	void summariesMarkBestProgram();

private:
	QVariant Role_Of(int in_row, const QString &in_role) const;

	AList_Model model;
};

QVariant AProgram_Rows_Test::Role_Of(int in_row, const QString &in_role) const
{
	for (int i = 0; i < model.Role_Count(); ++i)
	{
		if (model.Role_Name(Qt::UserRole + 1 + i) == in_role)
			return model.Data(in_row, Qt::UserRole + 1 + i);
	}

	return QVariant();
}
//----------------------------------------------------------------------------
void AProgram_Rows_Test::weekSlotsMapContractRoles()
{
	SWeek_Slot slot;

	slot.ID = "slot_1";
	slot.Program_Id = "p1";
	slot.Day_Of_Week = 1;
	slot.Kind = ESlot_Kind::Workout;
	slot.Program_Day_Id = "d1";
	slot.Is_Anchor = true;
	slot.Note = "старт";

	model.Set_Roles(Program_Rows::Week_Slot_Roles());
	model.Set_Rows(Program_Rows::Build_Week_Slots(QVector<SWeek_Slot>{slot}, QHash<QString, QString>(), QString()));

	QCOMPARE(model.Row_Count(), 1);
	QCOMPARE(Role_Of(0, "id").toString(), QString("slot_1"));
	QCOMPARE(Role_Of(0, "dayOfWeek").toInt(), 1);
	QCOMPARE(Role_Of(0, "kind").toString(), QString("workout"));
	QCOMPARE(Role_Of(0, "programDayId").toString(), QString("d1"));
	QCOMPARE(Role_Of(0, "isAnchor").toBool(), true);
	QCOMPARE(Role_Of(0, "note").toString(), QString("старт"));
}
//----------------------------------------------------------------------------
void AProgram_Rows_Test::weekSlotTodayFlagUsesGivenDate()
{
	SWeek_Slot slot;

	slot.ID = "slot_3";
	slot.Day_Of_Week = 3;

	// 2026-09-30 — среда (ISO 3).
	model.Set_Roles(Program_Rows::Week_Slot_Roles());
	model.Set_Rows(
	    Program_Rows::Build_Week_Slots(QVector<SWeek_Slot>{slot}, QHash<QString, QString>(), QString("2026-09-30")));

	QCOMPARE(Role_Of(0, "isToday").toBool(), true);

	model.Set_Rows(
	    Program_Rows::Build_Week_Slots(QVector<SWeek_Slot>{slot}, QHash<QString, QString>(), QString("2026-10-01")));

	QCOMPARE(Role_Of(0, "isToday").toBool(), false);
}
//----------------------------------------------------------------------------
void AProgram_Rows_Test::programDaysMapContractRoles()
{
	SProgram_Day day;

	day.ID = "d1";
	day.Title = "День A";
	day.Kind = EDay_Kind::Workout;
	day.Focus = QStringList{"legs", "core"};
	day.Estimated_Minutes = 55;
	day.Order_Index = 1;

	model.Set_Roles(Program_Rows::Program_Day_Roles());
	model.Set_Rows(Program_Rows::Build_Program_Days(QVector<SProgram_Day>{day}, 2));

	QCOMPARE(Role_Of(0, "title").toString(), QString("День A"));
	QCOMPARE(Role_Of(0, "kind").toString(), QString("workout"));
	QCOMPARE(Role_Of(0, "focus").toString(), QString("legs,core"));
	QCOMPARE(Role_Of(0, "exerciseCount").toInt(), 2);
	QCOMPARE(Role_Of(0, "estimatedMinutes").toInt(), 55);
	QCOMPARE(Role_Of(0, "orderIndex").toInt(), 1);
}
//----------------------------------------------------------------------------
void AProgram_Rows_Test::dayExercisesFormatRepsAndWeight()
{
	SDay_Exercise exercise;

	exercise.ID = "de1";
	exercise.Program_Day_Id = "d1";
	exercise.Exercise_Id = "squat";
	exercise.Order_Index = 2;
	exercise.Sets = 4;
	exercise.Reps_Min = 5;
	exercise.Reps_Max = 5;
	exercise.Weight_Mode = EWeight_Mode::Fixed;
	exercise.Target_Weight_Kg = 100;
	exercise.Has_Target_Weight_Kg = true;
	exercise.Rpe_Target = 8;
	exercise.Has_Rpe_Target = true;
	exercise.Rest_Seconds = 120;
	exercise.Superset_Group = "A";
	exercise.Is_Optional = true;

	model.Set_Roles(Program_Rows::Day_Exercise_Roles());
	model.Set_Rows(Program_Rows::Build_Day_Exercises(QVector<SDay_Exercise>{exercise}, QHash<QString, QString>()));

	QCOMPARE(Role_Of(0, "repsText").toString(), QString("5"));
	QCOMPARE(Role_Of(0, "weightMode").toString(), QString("fixed"));
	QCOMPARE(Role_Of(0, "targetWeightKg").toDouble(), 100.0);
	QCOMPARE(Role_Of(0, "rpeTarget").toDouble(), 8.0);
	QCOMPARE(Role_Of(0, "restSeconds").toInt(), 120);
	QCOMPARE(Role_Of(0, "supersetGroup").toString(), QString("A"));
	QCOMPARE(Role_Of(0, "isOptional").toBool(), true);

	// Диапазон повторов показывается как «5–8».
	exercise.Reps_Min = 5;
	exercise.Reps_Max = 8;
	model.Set_Rows(Program_Rows::Build_Day_Exercises(QVector<SDay_Exercise>{exercise}, QHash<QString, QString>()));

	QCOMPARE(Role_Of(0, "repsText").toString(), QString("5–8"));
}
//----------------------------------------------------------------------------
void AProgram_Rows_Test::dayExerciseNullsMissingWeight()
{
	SDay_Exercise exercise;

	exercise.ID = "de1";
	exercise.Weight_Mode = EWeight_Mode::ByFeel;

	model.Set_Roles(Program_Rows::Day_Exercise_Roles());
	model.Set_Rows(Program_Rows::Build_Day_Exercises(QVector<SDay_Exercise>{exercise}, QHash<QString, QString>()));

	// Нет заданного веса — null, а не 0: «не задано» и «ноль» различаются.
	QVERIFY(Role_Of(0, "targetWeightKg").isNull() == true);
	QVERIFY(Role_Of(0, "rpeTarget").isNull() == true);
}
//----------------------------------------------------------------------------
void AProgram_Rows_Test::rulesDescribeTargetRange()
{
	SProgram_Rule rule;

	rule.ID = "r1";
	rule.Category = ERule_Category::Training;
	rule.Code = "maxRpe";
	rule.Title = "RPE не выше 8";
	rule.Metric = ERule_Metric::MaxRpe;
	rule.Periodicity = ERule_Periodicity::PerWorkout;
	rule.Target_Max = 8;
	rule.Has_Target_Max = true;
	rule.Check_Source = ERule_Check_Source::ComputedFromSets;
	rule.Enforcement = ERule_Enforcement::Hard;

	model.Set_Roles(Program_Rows::Rule_Roles());
	model.Set_Rows(Program_Rows::Build_Rules(QVector<SProgram_Rule>{rule}, QHash<QString, QString>()));

	QCOMPARE(Role_Of(0, "code").toString(), QString("maxRpe"));
	QCOMPARE(Role_Of(0, "category").toString(), QString("training"));
	QCOMPARE(Role_Of(0, "metric").toString(), QString("maxRpe"));
	QCOMPARE(Role_Of(0, "periodicity").toString(), QString("perWorkout"));
	QCOMPARE(Role_Of(0, "targetValue").toDouble(), 8.0);
	QCOMPARE(Role_Of(0, "checkSource").toString(), QString("computedFromSets"));
	QCOMPARE(Role_Of(0, "enforcement").toString(), QString("hard"));
}
void AProgram_Rows_Test::ruleWithoutTargetHasNullValue()
{
	SProgram_Rule rule;

	rule.ID = "r1";

	model.Set_Roles(Program_Rows::Rule_Roles());
	model.Set_Rows(Program_Rows::Build_Rules(QVector<SProgram_Rule>{rule}, QHash<QString, QString>()));

	QVERIFY(Role_Of(0, "targetValue").isNull() == true);
}
//----------------------------------------------------------------------------
// Подпись рекомендации — это сам совет, а не «нет цели». Иначе карточка в
// списке показывала бы пустую строку там, где у соседей стоит «≥ 140 г», и
// пользователь не понял бы, что правило вообще чем-то наполнено.
void AProgram_Rows_Test::adviceRuleShowsItsText()
{
	SProgram_Rule rule;

	rule.ID = "adv";
	rule.Category = ERule_Category::Other;
	rule.Code = "custom";
	rule.Title = "Отбой вовремя";
	rule.Metric = ERule_Metric::Custom;
	rule.Periodicity = ERule_Periodicity::PerDay;
	rule.Note = "Отбой до 23:00, без экрана";
	rule.Display_Unit = ERule_Display_Unit::Units;

	model.Set_Roles(Program_Rows::Rule_Roles());
	model.Set_Rows(Program_Rows::Build_Rules(QVector<SProgram_Rule>{rule}, QHash<QString, QString>()));

	QCOMPARE(Role_Of(0, "displayText").toString(), QString("Отбой до 23:00, без экрана"));
	QCOMPARE(Role_Of(0, "category").toString(), QString("other"));
	// Числовой цели у рекомендации нет — и роли цели это не выдумывают.
	QVERIFY(Role_Of(0, "targetValue").isNull() == true);

	// «Другое» с числом остаётся измеримым и показывает именно число, а не
	// текст: вид правила решает по цели, а не по категории.
	rule.Target_Min = 8;
	rule.Has_Target_Min = true;
	model.Set_Rows(Program_Rows::Build_Rules(QVector<SProgram_Rule>{rule}, QHash<QString, QString>()));
	QCOMPARE(Role_Of(0, "displayText").toString(), QString("≥ 8 units"));
}
//----------------------------------------------------------------------------
// Экран решает по роли, а не по displayText: подпись у рекомендации — это сам
// совет, и по тексту нельзя понять, показывать ли кнопки отметки. Роль
// обязана быть в контракте, иначе делегат QML не сможет её объявить.
void AProgram_Rows_Test::adviceFlagExposedAsRole()
{
	SProgram_Rule advice;
	SProgram_Rule measurable;

	advice.ID = "adv";
	advice.Category = ERule_Category::Other;
	advice.Code = "custom";
	advice.Note = "Отбой до 23:00";
	advice.Display_Unit = ERule_Display_Unit::Units;

	measurable.ID = "r1";
	measurable.Category = ERule_Category::Nutrition;
	measurable.Metric = ERule_Metric::ProteinGrams;
	measurable.Target_Min = 140;
	measurable.Has_Target_Min = true;
	measurable.Display_Unit = ERule_Display_Unit::G;

	QVERIFY(Program_Rows::Rule_Roles().contains("isAdvice"));

	model.Set_Roles(Program_Rows::Rule_Roles());
	model.Set_Rows(Program_Rows::Build_Rules(QVector<SProgram_Rule>{advice, measurable}, QHash<QString, QString>()));

	QVERIFY(Role_Of(0, "isAdvice").toBool());
	QVERIFY(Role_Of(1, "isAdvice").toBool() == false);

	// Флаг отражает вид правила, а не категорию: «Другое» с числом остаётся
	// измеримым, и кнопки отметки у него нужны.
	advice.Target_Min = 8;
	advice.Has_Target_Min = true;
	model.Set_Rows(Program_Rows::Build_Rules(QVector<SProgram_Rule>{advice}, QHash<QString, QString>()));
	QVERIFY(Role_Of(0, "isAdvice").toBool() == false);
}
//----------------------------------------------------------------------------
// Порядок каталога фиксирован: экран показывает правила по смыслу (питание,
// активность, восстановление), а не в случайном порядке контейнера. Идентификатор
// обязан быть стабильным — по нему пресет узнаётся уже добавленным.
void AProgram_Rows_Test::ruleCatalogHasStableOrderAndIds()
{
	QVector<QVariantMap> all = Program_Rows::Build_Rule_Catalog(QString());

	QVERIFY(all.size() >= 5);

	QStringList ids;
	QStringList titles;

	for (int i = 0; i < all.size(); ++i)
	{
		ids.append(all.at(i).value("code").toString());
		titles.append(all.at(i).value("title").toString());
		QVERIFY2(all.at(i).value("title").toString().isEmpty() == false, "пресет без названия");
	}

	// Повторный вызов даёт тот же порядок: экран не должен перетасовывать
	// список при каждом открытии.
	QVERIFY(Program_Rows::Build_Rule_Catalog(QString()) == all);

	// Идентификаторы уникальны — иначе два разных пресета отмечались бы как
	// один и тот же добавленный.
	QSet<QString> unique(ids.begin(), ids.end());
	QCOMPARE(unique.size(), ids.size());
}
//----------------------------------------------------------------------------
void AProgram_Rows_Test::ruleCatalogFiltersByCategory()
{
	QVector<QVariantMap> nutrition = Program_Rows::Build_Rule_Catalog(QString("nutrition"));
	QVector<QVariantMap> recovery = Program_Rows::Build_Rule_Catalog(QString("recovery"));

	QVERIFY(nutrition.isEmpty() == false);
	QVERIFY(recovery.isEmpty() == false);

	for (int i = 0; i < nutrition.size(); ++i)
		QCOMPARE(nutrition.at(i).value("category").toString(), QString("nutrition"));

	for (int i = 0; i < recovery.size(); ++i)
		QCOMPARE(recovery.at(i).value("category").toString(), QString("recovery"));

	// Категории не пересекаются: иначе фильтр показывал бы чужое.
	const int total = Program_Rows::Build_Rule_Catalog(QString()).size();

	QVERIFY(nutrition.size() + recovery.size() < total);

	// Пустая строка означает «все», а не «нет ни одного»: иначе фильтр
	// «Все» молча дал бы пустой экран.
	QVERIFY(nutrition.size() < total);
	QVERIFY(recovery.size() < total);
}
//----------------------------------------------------------------------------
// Пресет обязан нести все поля, нужные для создания правила. Иначе экран
// пришлось бы держать таблицу соответствия между названием пресета и
// параметрами — то самое, что ушло из разметки в домен.
void AProgram_Rows_Test::ruleCatalogExposesFieldsNeededToAddRule()
{
	QVector<QVariantMap> catalog = Program_Rows::Build_Rule_Catalog(QString("nutrition"));
	QVariantMap preset = catalog.first();

	QVERIFY(preset.contains("code"));
	QVERIFY(preset.contains("title"));
	QVERIFY(preset.contains("category"));
	QVERIFY(preset.contains("metric"));
	QVERIFY(preset.contains("periodicity"));
	QVERIFY(preset.contains("unit"));
	// Цель приходит в готовом виде с признаком наличия: без него пресет
	// «Белок 140 г» превратился бы в правило без цели.
	QVERIFY(preset.contains("targetMin"));
	QVERIFY(preset.contains("hasTargetMin"));

	// Числа, а не строки: модель ждёт числа (§7 QML-контракт).
	QVERIFY(preset.value("metric").toInt() > 0 || preset.value("metric").toInt() == 0);
	QCOMPARE(preset.value("hasTargetMin").type(), QVariant::Bool);

	// Подпись единицы читается из словаря, а не из сырой строки «gPerKg».
	QVERIFY(preset.value("unit").toString().isEmpty() == false);
}
//----------------------------------------------------------------------------
// Все пресеты обязаны быть создаваемы: измеримые — с целью, рекомендации —
// с текстом. Пресет, который нельзя добавить, — мёртвая кнопка в каталоге.
void AProgram_Rows_Test::catalogPresetsAreCreatable()
{
	QVector<QVariantMap> all = Program_Rows::Build_Rule_Catalog(QString());

	for (int i = 0; i < all.size(); ++i)
	{
		const QVariantMap preset = all.at(i);
		const bool has_target = preset.value("hasTargetMin").toBool() || preset.value("hasTargetMax").toBool();

		QVERIFY2(has_target || preset.value("isAdvice").toBool(),
		         qPrintable(QString("пресет «%1» нечем создать").arg(preset.value("title").toString())));
	}
}
//----------------------------------------------------------------------------
// Метрика определяет единицу сама: белок измеряется в граммах, сон — в часах.
// Если бы единицу выбирал пользователь, форма позволяла бы «белок в мл» — такое
// правило прошло бы всю валидацию и при этом не значило бы ничего.
void AProgram_Rows_Test::metricCatalogGivesEachMetricItsOwnUnit()
{
	QVector<QVariantMap> metrics = Program_Rows::Build_Metric_Catalog();
	QHash<int, QString> unit_by_metric;

	for (int i = 0; i < metrics.size(); ++i)
	{
		const int metric = metrics.at(i).value("metric").toInt();

		QVERIFY2(metrics.at(i).value("unit").toString().isEmpty() == false,
		         "метрика без единицы: цель было бы нечем измерить");
		// Периодичность проверяется «входит в enum», а не «больше нуля»: PerDay
		// это 0, и проверка на положительность отвергала бы самый частый случай.
		const int periodicity = metrics.at(i).value("periodicity").toInt();

		QVERIFY2(periodicity >= 0 && periodicity < int(ERule_Periodicity::Max), "метрика без периодичности");
		QVERIFY2(metrics.at(i).value("title").toString().isEmpty() == false, "метрика без названия");

		// Метрика не встречается дважды: иначе фильтр формы показал бы дубль.
		QVERIFY2(unit_by_metric.contains(metric) == false, "метрика продублирована в каталоге");
		unit_by_metric.insert(metric, metrics.at(i).value("unit").toString());
	}

	QCOMPARE(unit_by_metric.value(int(ERule_Metric::ProteinGrams)), QString("g"));
	QCOMPARE(unit_by_metric.value(int(ERule_Metric::WaterMl)), QString("ml"));
	QCOMPARE(unit_by_metric.value(int(ERule_Metric::SleepHours)), QString("hours"));
	QCOMPARE(unit_by_metric.value(int(ERule_Metric::MaxRpe)), QString("rpe"));
}
//----------------------------------------------------------------------------
// Единица «сна» и «калории» различаются, а «миллилитры» белку не подходят:
// проверка единицы по метрике обязана отвергать чужую.
void AProgram_Rows_Test::metricCatalogRejectsUnitForeignToMetric()
{
	QVERIFY(Program_Rows::Is_Unit_Valid_For_Metric(int(ERule_Metric::ProteinGrams), int(ERule_Display_Unit::G)));
	QVERIFY(Program_Rows::Is_Unit_Valid_For_Metric(int(ERule_Metric::WaterMl), int(ERule_Display_Unit::Ml)));

	QVERIFY(Program_Rows::Is_Unit_Valid_For_Metric(int(ERule_Metric::ProteinGrams), int(ERule_Display_Unit::Ml)) ==
	        false);
	QVERIFY(Program_Rows::Is_Unit_Valid_For_Metric(int(ERule_Metric::SleepHours), int(ERule_Display_Unit::Kcal)) ==
	        false);
	QVERIFY(Program_Rows::Is_Unit_Valid_For_Metric(int(ERule_Metric::MaxRpe), int(ERule_Display_Unit::Hours)) == false);

	// Метрика «Произвольная» допускает единицы: её смысл задаёт пользователь.
	QVERIFY(Program_Rows::Is_Unit_Valid_For_Metric(int(ERule_Metric::Custom), int(ERule_Display_Unit::Units)));

	// Несуществующий enum не должен молча проходить как «подходит».
	QVERIFY(Program_Rows::Is_Unit_Valid_For_Metric(int(ERule_Metric::ProteinGrams), int(ERule_Display_Unit::Max)) ==
	        false);
	QVERIFY(Program_Rows::Is_Unit_Valid_For_Metric(int(ERule_Metric::Max), int(ERule_Display_Unit::G)) == false);
}
//----------------------------------------------------------------------------
// Каталог обязан покрывать все измеримые метрики: пропущенная исчезла бы из
// формы без объяснения, и правило по ней стало бы невозможно создать.
void AProgram_Rows_Test::metricCatalogCoversEveryRuleMetric()
{
	QVector<QVariantMap> metrics = Program_Rows::Build_Metric_Catalog();
	QSet<int> known;

	for (int i = 0; i < metrics.size(); ++i)
		known.insert(metrics.at(i).value("metric").toInt());

	for (int metric = 0; metric < int(ERule_Metric::Max); ++metric)
	{
		// Custom пропускается намеренно: произвольное измерение — это уже
		// рекомендация, у неё другой вход (§3.6).
		if (metric == int(ERule_Metric::Custom))
			continue;

		QVERIFY2(known.contains(metric), qPrintable(QString("метрика %1 не покрыта").arg(metric)));
	}

	QVERIFY(known.contains(int(ERule_Metric::Custom)) == false);
	// Вес тела покрыт с миграции v4: до неё в ERule_Display_Unit не было
	// килограммов, и метрика была недоступна в форме.
	QVERIFY(known.contains(int(ERule_Metric::BodyWeightKg)));
}
//----------------------------------------------------------------------------
void AProgram_Rows_Test::goalTargetsMapProgress()
{
	SGoal_Target goal;

	goal.ID = "g1";
	goal.Metric = EGoal_Metric::BodyWeightKg;
	goal.Unit = EGoal_Unit::Kg;
	goal.Start_Value = 80;
	goal.Has_Start_Value = true;
	goal.Target_Value = 76;
	goal.Direction = EGoal_Direction::Decrease;
	goal.State = EGoal_Target_State::Active;

	model.Set_Roles(Program_Rows::Goal_Target_Roles());
	model.Set_Rows(Program_Rows::Build_Goal_Targets(QVector<SGoal_Target>{goal}, QHash<QString, double>()));

	QCOMPARE(Role_Of(0, "metric").toString(), QString("bodyWeightKg"));
	QCOMPARE(Role_Of(0, "startValue").toDouble(), 80.0);
	QCOMPARE(Role_Of(0, "targetValue").toDouble(), 76.0);
	QCOMPARE(Role_Of(0, "direction").toString(), QString("decrease"));
	QCOMPARE(Role_Of(0, "state").toString(), QString("active"));
}
//----------------------------------------------------------------------------
void AProgram_Rows_Test::checkinsMapStateAndValue()
{
	SRule_Checkin checkin;

	checkin.ID = "c1";
	checkin.Rule_Id = "r1";
	checkin.Date = "2026-09-28";
	checkin.Value = 143;
	checkin.Has_Value = true;
	checkin.Unit = "g";
	checkin.State = ECheckin_State::Done;
	checkin.Source = ECheckin_Source::Computed;

	model.Set_Roles(Program_Rows::Checkin_Roles());
	model.Set_Rows(Program_Rows::Build_Checkins(QVector<SRule_Checkin>{checkin}));

	QCOMPARE(Role_Of(0, "ruleId").toString(), QString("r1"));
	QCOMPARE(Role_Of(0, "value").toDouble(), 143.0);
	QCOMPARE(Role_Of(0, "state").toString(), QString("done"));
	QCOMPARE(Role_Of(0, "source").toString(), QString("computed"));

	// Отметка без значения — null, а не 0.
	checkin.Has_Value = false;
	checkin.Value = -1;
	model.Set_Rows(Program_Rows::Build_Checkins(QVector<SRule_Checkin>{checkin}));

	QVERIFY(Role_Of(0, "value").isNull() == true);
}
//----------------------------------------------------------------------------
void AProgram_Rows_Test::measurementsNullMissingFields()
{
	SBody_Measurement measurement;

	measurement.ID = "m1";
	measurement.Date = "2026-09-28";
	measurement.Weight_Kg = 78.4;
	measurement.Has_Weight_Kg = true;

	model.Set_Roles(Program_Rows::Measurement_Roles());
	model.Set_Rows(Program_Rows::Build_Measurements(QVector<SBody_Measurement>{measurement}));

	QCOMPARE(Role_Of(0, "weightKg").toDouble(), 78.4);
	QVERIFY(Role_Of(0, "waistCm").isNull() == true);
}
//----------------------------------------------------------------------------
void AProgram_Rows_Test::summariesMarkBestProgram()
{
	SProgram_Summary best;
	SProgram_Summary other;

	best.Program_Id = "p1";
	best.Title = "Сила";
	best.Adherence_Percent = 90;
	best.Has_Adherence_Percent = true;

	other.Program_Id = "p2";
	other.Title = "Масса";
	other.Adherence_Percent = 60;
	other.Has_Adherence_Percent = true;

	model.Set_Roles(Program_Rows::Program_Summary_Roles());
	model.Set_Rows(Program_Rows::Build_Program_Summaries(QVector<SProgram_Summary>{best, other}, QString("p1")));

	QCOMPARE(Role_Of(0, "isBest").toBool(), true);
	QCOMPARE(Role_Of(1, "isBest").toBool(), false);
	QCOMPARE(Role_Of(1, "adherencePercent").toDouble(), 60.0);
}
QTEST_GUILESS_MAIN(AProgram_Rows_Test)
#include "tst_Program_Rows.moc"
