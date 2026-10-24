#pragma once

#include <QString>
#include <QStringList>

//----------------------------------------------------------------------------
// Перечисления модели программы. Строковые представления (To_String /
// From_String) стабильны, ASCII и совпадают со значениями CHECK-ограничений
// schema v2 и ролями mock-контракта (`02-program-model.md` §3, §8).
//----------------------------------------------------------------------------
namespace Program_Model
{
enum class EProgram_Source_Kind : unsigned char
{
	Template,
	Copy,
	Scratch,
	Matched,
	Max
};

enum class EProgram_Goal : unsigned char
{
	Strength,
	Muscle,
	Fitness,
	FatLoss,
	Endurance,
	Max
};

enum class EProgram_Level : unsigned char
{
	Beginner,
	Intermediate,
	Advanced,
	Max
};

enum class ESchedule_Kind : unsigned char
{
	Weekday,
	Rotation,
	Max
};

enum class EProgram_State : unsigned char
{
	Draft,
	Active,
	Paused,
	Completed,
	Cancelled,
	Archived,
	Max
};

enum class ETerm_Mode : unsigned char
{
	FixedWeeks,
	OpenEnded,
	Max
};

enum class ESync_State : unsigned char
{
	Synced,
	Local,
	Syncing,
	Offline,
	Max
};

enum class EGoal_Metric : unsigned char
{
	BodyWeightKg,
	WorkingWeightKg,
	BodyFatPercent,
	MeasurementCm,
	WorkoutsPerWeek,
	TonnagePerWeekKg,
	RepsAtWeight,
	Max
};

enum class EGoal_Unit : unsigned char
{
	Kg,
	Percent,
	Cm,
	Count,
	TimesPerWeek,
	Max
};

enum class EGoal_Direction : unsigned char
{
	Increase,
	Decrease,
	Maintain,
	Max
};

enum class EGoal_Target_State : unsigned char
{
	Active,
	Achieved,
	Dropped,
	Max
};

enum class EMeasurement_Key : unsigned char
{
	Chest,
	Waist,
	Hip,
	Arm,
	Thigh,
	Max
};

enum class ESlot_Kind : unsigned char
{
	Workout,
	Rest,
	Optional,
	Cardio,
	Mobility,
	Max
};

enum class EDay_Kind : unsigned char
{
	Workout,
	Rest,
	Cardio,
	Mobility,
	Max
};

enum class EWeight_Mode : unsigned char
{
	Fixed,
	PercentOf1RM,
	ByFeel,
	FromLastSession,
	BodyWeight,
	Max
};

enum class EProgression_Type : unsigned char
{
	LinearWeight,
	DoubleProgression,
	PercentPerWeek,
	Manual,
	Max
};

enum class ERule_Category : unsigned char
{
	Nutrition,
	Training,
	Recovery,
	Lifestyle,
	Technique,
	// Рекомендация, написанная текстом: измеримой цели у неё нет, и в этом
	// её смысл. Снимает требование цели только вместе с непустым текстом
	// (см. Validate_Rules), поэтому не превращается в способ завести пустое
	// правило. Значение 'other' связано с CHECK в схеме v3.
	Other,
	Max
};

//----------------------------------------------------------------------------
// Вид правила. Определяется наличием цели, а не категорией: рекомендацией
// является то, у чего нечего измерить, поэтому «Другое» с числом остаётся
// измеримым. Проверка обязана совпадать с CHECK в схеме v3 и с Validate_Rules,
// иначе правило пройдёт одну и упадёт на другой.
enum class ERule_Kind : unsigned char
{
	Measurable,
	Advice,
	Max
};

enum class ERule_Metric : unsigned char
{
	ProteinGrams,
	CaloriesKcal,
	WaterMl,
	SleepHours,
	StepCount,
	BodyWeightKg,
	MaxRpe,
	RestHoursBetweenSessions,
	AlcoholUnits,
	Custom,
	Max
};

enum class ERule_Periodicity : unsigned char
{
	PerDay,
	PerWeek,
	PerWorkout,
	Once,
	Max
};

enum class ERule_Applies_To : unsigned char
{
	AllDays,
	TrainingDays,
	RestDays,
	CustomDays,
	Max
};

enum class ERule_Check_Source : unsigned char
{
	SelfReport,
	ComputedFromSets,
	ComputedFromProfile,
	ComputedFromSessions,
	ComputedFromLog,
	Mixed,
	Max
};

enum class ERule_Enforcement : unsigned char
{
	Soft,
	Hard,
	Max
};

enum class ERule_Display_Unit : unsigned char
{
	G,
	GPerKg,
	// Кг добавлены миграцией v4: без них метрика «вес тела» была недоступна в
	// форме своего правила, и «вес не выше 80 кг» не сохранялось бы вовсе.
	Kg,
	Kcal,
	Ml,
	Hours,
	Count,
	Rpe,
	Units,
	Max
};

enum class ECheckin_State : unsigned char
{
	Done,
	Failed,
	Skipped,
	Partial,
	Unknown,
	Max
};

enum class ECheckin_Source : unsigned char
{
	Manual,
	Computed,
	Imported,
	Max
};

enum class EDeviation : unsigned char
{
	AsPlanned,
	Partial,
	Substituted,
	ExtraDay,
	MovedDay,
	Max
};

enum class EWorkout_State : unsigned char
{
	Draft,
	Completed,
	Cancelled,
	Max
};

enum class ERevision_Reason : unsigned char
{
	Goal,
	Term,
	Schedule,
	Days,
	Rules,
	ExercisePlan,
	State,
	Max
};

//----------------------------------------------------------------------------
// Срок программы (§3.3). End_Date всегда производное и не хранится в базе:
// Compute_End_Date() = start_date + weeks_total × 7 − 1 день (openEnded — пусто).
// Extended_Weeks — суммарное продление для журнала, endDate считает только
// Weeks_Total (§3.3).
//----------------------------------------------------------------------------
struct STerm
{
	QString Start_Date;  // YYYY-MM-DD; пустая — не задана
	int Weeks_Total = 0; // 0 = открытый срок «до достижения цели»
	int Extended_Weeks = 0;
	ETerm_Mode Mode = ETerm_Mode::FixedWeeks;

	QString Compute_End_Date() const;
};

//----------------------------------------------------------------------------
// Измеримая цель программы (§3.2). Цель-«упражнение» (RepsAtWeight) хранит
// Target_Weight_Kg и Target_Reps отдельно.
//----------------------------------------------------------------------------
struct SGoal_Target
{
	QString ID;
	QString Program_Id;
	EGoal_Metric Metric = EGoal_Metric::BodyWeightKg;
	QString Exercise_Id; // для WorkingWeightKg / RepsAtWeight; пустая — не задана
	EMeasurement_Key Measurement_Key = EMeasurement_Key::Chest;
	bool Has_Measurement_Key = false;
	EGoal_Unit Unit = EGoal_Unit::Kg;
	double Start_Value = -1;
	bool Has_Start_Value = false;
	double Target_Value = 0;
	double Target_Weight_Kg = -1;
	bool Has_Target_Weight_Kg = false;
	int Target_Reps = -1;
	bool Has_Target_Reps = false;
	EGoal_Direction Direction = EGoal_Direction::Increase;
	QString Target_Date;
	EGoal_Target_State State = EGoal_Target_State::Active;
};

//----------------------------------------------------------------------------
// Слот недельного расписания (§3.4). Для Weekday заполнен Day_Of_Week (ISO
// 1–7), для Rotation — Rotation_Index; 0 означает «не задан».
//----------------------------------------------------------------------------
struct SWeek_Slot
{
	QString ID;
	QString Program_Id;
	int Day_Of_Week = 0;    // ISO-день 1–7; 0 для ротации
	int Rotation_Index = 0; // позиция в цикле; 0 для расписания по дням недели
	QString Program_Day_Id; // пустая — слот отдыха
	ESlot_Kind Kind = ESlot_Kind::Rest;
	bool Is_Anchor = false;
	QString Note;
};

//----------------------------------------------------------------------------
// День плана (§3.4). Focus — список enum-строк (legs/push/pull/core/fullBody).
//----------------------------------------------------------------------------
struct SProgram_Day
{
	QString ID;
	QString Program_Id;
	QString Title;
	EDay_Kind Kind = EDay_Kind::Workout;
	QStringList Focus;
	int Estimated_Minutes = 0;
	int Order_Index = 0;
	QString Source_Day_Id; // снимок plan_days.id шаблона; пустая — день с нуля
	QString Note;
};

//----------------------------------------------------------------------------
// Правило прогрессии веса (§3.5). Сериализуется в program_day_exercises.
// progression_json; необязательные числа — пары «значение + Has_».
//----------------------------------------------------------------------------
struct SProgression_Rule
{
	EProgression_Type Type = EProgression_Type::LinearWeight;
	double Increment_Kg = -1;
	bool Has_Increment_Kg = false;
	int Trigger_Sets = -1;
	bool Has_Trigger_Sets = false;
	int Trigger_Reps_Min = -1;
	bool Has_Trigger_Reps_Min = false;
	double Max_Rpe = -1;
	bool Has_Max_Rpe = false;
	int Deload_Every_Weeks = -1;
	bool Has_Deload_Every_Weeks = false;
};

//----------------------------------------------------------------------------
// Предписание упражнения в дне плана (§3.5).
//----------------------------------------------------------------------------
struct SDay_Exercise
{
	QString ID;
	QString Program_Day_Id;
	QString Exercise_Id;
	int Order_Index = 0;
	int Sets = 1; // рабочие подходы, 1–20
	int Reps_Min = 0;
	int Reps_Max = 0;
	EWeight_Mode Weight_Mode = EWeight_Mode::ByFeel;
	double Target_Weight_Kg = -1;
	bool Has_Target_Weight_Kg = false;
	double Percent_Of_1rm = -1;
	bool Has_Percent_Of_1rm = false;
	double Rpe_Target = -1;
	bool Has_Rpe_Target = false;
	int Rest_Seconds = 0;
	QString Tempo;          // нотация 3-1-1-0; пустая — не задана
	QString Superset_Group; // метка суперсета; пустая — обычная строка
	int Warmup_Sets = 0;
	SProgression_Rule Progression;
	bool Has_Progression = false;
	QStringList Substitution_Ids; // разрешённые замены
	bool Is_Optional = false;
	QString Note;
};

//----------------------------------------------------------------------------
// Правило-цель программы (§3.6): что (метрика), сколько (цель + единица),
// как проверяем (источник и периодичность).
//----------------------------------------------------------------------------
struct SProgram_Rule
{
	QString ID;
	QString Program_Id;
	ERule_Category Category = ERule_Category::Training;
	QString Code; // типовой код каталога правил или custom
	QString Title;
	ERule_Metric Metric = ERule_Metric::Custom;
	ERule_Periodicity Periodicity = ERule_Periodicity::PerDay;
	double Target_Min = -1;
	bool Has_Target_Min = false;
	double Target_Max = -1;
	bool Has_Target_Max = false;
	double Target_Exact = -1;
	bool Has_Target_Exact = false;
	ERule_Display_Unit Target_Unit = ERule_Display_Unit::Count;
	ERule_Applies_To Applies_To = ERule_Applies_To::AllDays;
	QStringList Applies_Days; // для CustomDays
	ERule_Check_Source Check_Source = ERule_Check_Source::SelfReport;
	ERule_Enforcement Enforcement = ERule_Enforcement::Soft;
	QString Active_From; // YYYY-MM-DD; пустая — с начала программы
	QString Active_To;
	ERule_Display_Unit Display_Unit = ERule_Display_Unit::Count;
	QString Note;
};

//----------------------------------------------------------------------------
// Отметка правила за день/неделю (§3.7). Одна отметка на пару ruleId + date.
//----------------------------------------------------------------------------
struct SRule_Checkin
{
	QString ID;
	QString Rule_Id;
	QString Program_Id;
	QString Date; // YYYY-MM-DD (для PerWeek — дата начала недели)
	double Value = -1;
	bool Has_Value = false;
	QString Unit;
	ECheckin_State State = ECheckin_State::Unknown;
	ECheckin_Source Source = ECheckin_Source::Manual;
	QString Session_Id;  // тренировка-источник для ComputedFromSets
	QString Meal_Log_Id; // резерв дневника питания (§3.10)
	QString Note;
	QString Created_At;
	QString Updated_At;
};

//----------------------------------------------------------------------------
// Замер тела (§3.8, инвариант 20 §4): отдельная таблица, одна запись на пару
// (user, date); nullable-поля — пары «значение + Has_».
//----------------------------------------------------------------------------
struct SBody_Measurement
{
	QString ID;
	QString User_Id;
	QString Date; // YYYY-MM-DD
	double Weight_Kg = -1;
	bool Has_Weight_Kg = false;
	double Chest_Cm = -1;
	bool Has_Chest_Cm = false;
	double Waist_Cm = -1;
	bool Has_Waist_Cm = false;
	double Hip_Cm = -1;
	bool Has_Hip_Cm = false;
	double Arm_Cm = -1;
	bool Has_Arm_Cm = false;
	double Thigh_Cm = -1;
	bool Has_Thigh_Cm = false;
};

//----------------------------------------------------------------------------
// Сессия-факт тренировки (§3.8). Program_Id пустая — тренировка вне
// программы. Program_Revision — снимок версии структуры на момент сессии.
//----------------------------------------------------------------------------
struct SWorkout_Session
{
	QString ID;
	QString User_Id;
	QString Program_Id;
	QString Program_Day_Id;
	QString Planned_Date;       // YYYY-MM-DD; пустая — внеплановая
	int Planned_Week_Index = 0; // 0 — не задан
	QString Date;               // фактическая дата выполнения
	QString Started_At;
	QString Completed_At;
	int Duration_Seconds = 0; // 0 — не посчитано
	EWorkout_State State = EWorkout_State::Draft;
	EDeviation Deviation = EDeviation::AsPlanned;
	bool Has_Deviation = false;
	int Exercise_Count = 0;
	int Set_Count = 0;
	double Tonnage_Kg = 0;
	double Avg_Rpe = -1;
	bool Has_Avg_Rpe = false;
	int Program_Revision = 0;         // 0 — сессия вне программы
	QString Correction_Of_Session_Id; // исправляемая сессия (§3.8)
	QString Note;
	ESync_State Sync_State = ESync_State::Local;
};

//----------------------------------------------------------------------------
// Подход-факт (§3.8). Target_Weight_Kg / Target_Reps — снимок рекомендации
// на момент тренировки; правки плана историю не переписывают.
//----------------------------------------------------------------------------
struct SSet_Log
{
	QString ID;
	QString Session_Id;
	QString Day_Exercise_Id; // пустая — подход вне предписания
	QString Exercise_Id;
	int Set_Number = 1;
	bool Is_Warmup = false;
	double Weight_Kg = 0;
	int Reps = 0;
	double Rpe = -1;
	bool Has_Rpe = false;
	double Target_Weight_Kg = -1;
	bool Has_Target_Weight_Kg = false;
	int Target_Reps = -1;
	bool Has_Target_Reps = false;
	bool Done = false;
	QString Note;
};

//----------------------------------------------------------------------------
// Глобальная программа пользователя (§3.1). Экземпляр-снимок шаблона
// каталога или программа, собранная с нуля; Days_Per_Week — производное от
// расписания и не хранится.
//----------------------------------------------------------------------------
struct SProgram
{
	QString ID;
	QString User_Id;
	QString Title;
	EProgram_Source_Kind Source_Kind = EProgram_Source_Kind::Scratch;
	QString Source_Template_Id; // пустая при Scratch
	EProgram_Goal Goal = EProgram_Goal::Strength;
	EProgram_Level Level = EProgram_Level::Beginner;
	ESchedule_Kind Schedule_Kind = ESchedule_Kind::Weekday;
	EProgram_State State = EProgram_State::Draft;
	int Revision = 1;
	STerm Term;
	QString Actual_Start_Date; // YYYY-MM-DD; пустая — не начата
	QString Created_At;
	QString Updated_At;
	ESync_State Sync_State = ESync_State::Local;
	QString Note;
};

//----------------------------------------------------------------------------
// Строка кэша статистики (§5.4). Первичные данные — только факты; здесь
// лежит готовая JSON-нагрузка для UI, привязанная к ревизии структуры
// программы. Owner_Id — programId для `program_stats` и userId для
// `lifetime_stats`; Program_Revision = 0 у общей статистики пользователя.
//----------------------------------------------------------------------------
struct SStats_Cache_Entry
{
	QString Owner_Id;
	QString Computed_At; // ISO 8601
	int Program_Revision = 0;
	QString Payload_Json;
};

//----------------------------------------------------------------------------
// Вид правила. Get_Rule_Kind отвечает на вопрос «есть ли что измерять»
// по наличию цели; Has_Advice_Text — «написано ли содержательное
// содержание». Обе проверки живут здесь, потому что повторяются в трёх слоях
// (домен, репозиторий, CHECK в БД) и обязаны совпадать.
//----------------------------------------------------------------------------
ERule_Kind Get_Rule_Kind(const SProgram_Rule &in_rule);
bool Has_Advice_Text(const SProgram_Rule &in_rule);

//----------------------------------------------------------------------------
// Строковые представления перечислений (schema v2 / mock-контракт).
// From_String возвращает false для неизвестного текста, out_value не меняется.
//----------------------------------------------------------------------------
QString To_String(EProgram_Source_Kind in_value);
QString To_String(EProgram_Goal in_value);
QString To_String(EProgram_Level in_value);
QString To_String(ESchedule_Kind in_value);
QString To_String(EProgram_State in_value);
QString To_String(ETerm_Mode in_value);
QString To_String(ESync_State in_value);
QString To_String(EGoal_Metric in_value);
QString To_String(EGoal_Unit in_value);
QString To_String(EGoal_Direction in_value);
QString To_String(EGoal_Target_State in_value);
QString To_String(EMeasurement_Key in_value);
QString To_String(ESlot_Kind in_value);
QString To_String(EDay_Kind in_value);
QString To_String(EWeight_Mode in_value);
QString To_String(EProgression_Type in_value);
QString To_String(ERule_Category in_value);
QString To_String(ERule_Metric in_value);
QString To_String(ERule_Periodicity in_value);
QString To_String(ERule_Applies_To in_value);
QString To_String(ERule_Check_Source in_value);
QString To_String(ERule_Enforcement in_value);
QString To_String(ERule_Display_Unit in_value);
QString To_String(ECheckin_State in_value);
QString To_String(ECheckin_Source in_value);
QString To_String(EDeviation in_value);
QString To_String(EWorkout_State in_value);
QString To_String(ERevision_Reason in_value);

bool From_String(const QString &in_text, EProgram_Source_Kind &out_value);
bool From_String(const QString &in_text, EProgram_Goal &out_value);
bool From_String(const QString &in_text, EProgram_Level &out_value);
bool From_String(const QString &in_text, ESchedule_Kind &out_value);
bool From_String(const QString &in_text, EProgram_State &out_value);
bool From_String(const QString &in_text, ETerm_Mode &out_value);
bool From_String(const QString &in_text, ESync_State &out_value);
bool From_String(const QString &in_text, EGoal_Metric &out_value);
bool From_String(const QString &in_text, EGoal_Unit &out_value);
bool From_String(const QString &in_text, EGoal_Direction &out_value);
bool From_String(const QString &in_text, EGoal_Target_State &out_value);
bool From_String(const QString &in_text, EMeasurement_Key &out_value);
bool From_String(const QString &in_text, ESlot_Kind &out_value);
bool From_String(const QString &in_text, EDay_Kind &out_value);
bool From_String(const QString &in_text, EWeight_Mode &out_value);
bool From_String(const QString &in_text, EProgression_Type &out_value);
bool From_String(const QString &in_text, ERule_Category &out_value);
bool From_String(const QString &in_text, ERule_Metric &out_value);
bool From_String(const QString &in_text, ERule_Periodicity &out_value);
bool From_String(const QString &in_text, ERule_Applies_To &out_value);
bool From_String(const QString &in_text, ERule_Check_Source &out_value);
bool From_String(const QString &in_text, ERule_Enforcement &out_value);
bool From_String(const QString &in_text, ERule_Display_Unit &out_value);
bool From_String(const QString &in_text, ECheckin_State &out_value);
bool From_String(const QString &in_text, ECheckin_Source &out_value);
bool From_String(const QString &in_text, EDeviation &out_value);
bool From_String(const QString &in_text, EWorkout_State &out_value);
bool From_String(const QString &in_text, ERevision_Reason &out_value);

//----------------------------------------------------------------------------
// Правка структуры программы запрещена в read-only состояниях (§3.1,
// инвариант 24 §4): completed/cancelled/archived. Предикат живёт в модели,
// а не в репозитории, потому что по нему же экран решает, показывать ли
// пункт «Редактировать», — иначе UI и база разошлись бы на одной программе.
//----------------------------------------------------------------------------
bool Program_State_Is_Editable(EProgram_State in_state);

//----------------------------------------------------------------------------
// JSON-сериализация TEXT-полей schema v2 (§8: progression_json,
// substitution_ids, focus, applies_days, snapshot_json).
//----------------------------------------------------------------------------
QString To_Progression_Json(const SProgression_Rule &in_rule);
bool Parse_Progression_Json(const QString &in_json, SProgression_Rule &out_rule);
QString To_Json_Array(const QStringList &in_list);
bool Parse_Json_Array(const QString &in_json, QStringList &out_list);
//----------------------------------------------------------------------------
} // namespace Program_Model
