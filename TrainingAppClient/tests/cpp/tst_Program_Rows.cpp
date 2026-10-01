#include "Presentation/List_Model.h"
#include "Presentation/Program_Rows.h"

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
