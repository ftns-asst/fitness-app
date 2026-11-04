#pragma once

#include "Domain/Error_State.h"
#include "Domain/Macro_Calculator.h"
#include "Domain/Program_Model.h"
#include "Presentation/List_Model.h"

#include <QDate>
#include <QLoggingCategory>
#include <QObject>
#include <QString>

using namespace Program_Model;

class AProgram_Repository;
class ARule_Repository;
class AWorkout_Repository;
class AMeasurement_Repository;
class AStats_Repository;
class AUsers_Repository;

//----------------------------------------------------------------------------
// ViewModel главного экрана «Сегодня» (issue 27, §5.1–5.2).
//
// Читает план дня из расписания активной программы: слот по ISO-номеру дня
// недели (1 = понедельник, §3.4), день слота, его упражнения, правила с
// состоянием отметок за сегодня и завершённую тренировку дня.
//
// Отсутствие активной программы — не ошибка, а пустое состояние главного
// экрана: поэтому `Load` возвращает true с `hasProgram = false` и пустым
// `errorKey`, иначе гость видел бы на главной ошибку вместо приглашения
// создать программу (§5.5).
//
// Владение: списки — дети ViewModel (родитель-потомок QObject).
//----------------------------------------------------------------------------
class Avm_Today : public QObject
{
	Q_OBJECT

	Q_PROPERTY(bool hasProgram READ Has_Program NOTIFY changed)
	Q_PROPERTY(QString programId READ programId NOTIFY changed)
	Q_PROPERTY(QString programTitle READ programTitle NOTIFY changed)
	Q_PROPERTY(QString goal READ goal NOTIFY changed)
	Q_PROPERTY(QString today READ today NOTIFY changed)
	// Номер дня недели ISO 1…7: QML переводит его в название сам (§3.4).
	Q_PROPERTY(int weekday READ weekday NOTIFY changed)
	// Вид дня числом EDay_Kind: контракт отдаёт enum числом (§7).
	Q_PROPERTY(int dayKind READ dayKind NOTIFY changed)
	// Вид дня строкой (`workout` / `rest` / `cardio` / `mobility`) — для
	// подписей на экране: таблица перевода живёт рядом с моделью, иначе QML
	// знал бы про модель меньше, чем домен.
	Q_PROPERTY(QString dayKindName READ Day_Kind_Name NOTIFY changed)
	// Ближайший тренировочный день сегодняшней или следующей недели, ISO 1…7.
	// 0 — такого дня в расписании нет. День отдыха отвечает на вопрос «что
	// делать сегодня», и без следующего дня подпись была бы пустой.
	Q_PROPERTY(int nextTrainingWeekday READ Next_Training_Weekday NOTIFY changed)
	Q_PROPERTY(bool isRestDay READ Is_Rest_Day NOTIFY changed)
	Q_PROPERTY(QString dayTitle READ dayTitle NOTIFY changed)
	Q_PROPERTY(int estimatedMinutes READ estimated_minutes NOTIFY changed)
	// Срок программы и текущая неделя: hero-карточка дня показывает «нед. 3 из 8»,
	// и без срока экран рисовал бы «нед. 0» (§5.1).
	Q_PROPERTY(int currentWeek READ Current_Week NOTIFY changed)
	Q_PROPERTY(int weeksTotal READ weeks_total NOTIFY changed)
	Q_PROPERTY(bool sessionCompleted READ Session_Completed NOTIFY changed)
	Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)
	// — Авторасчёт КБЖУ (спринт S7, миграция v6) —
	// Результат расчёта с экрана «Рассчитать КБЖУ». Главная показывает числа, а не
	// кнопку «Рассчитать»: расчёт делает человек по кнопке (§9.2 доклада), но
	// увидеть его он должен там, где живёт — рядом с замером веса, откуда берётся
	// вес для формулы. Пусто — расчёт ещё не делали, и карточка предлагает его
	// сделать, а не рисует нули.
	Q_PROPERTY(bool hasMacroPlan READ Has_Macro_Plan NOTIFY changed)
	Q_PROPERTY(double macroCaloriesKcal READ macro_calories_kcal NOTIFY changed)
	Q_PROPERTY(double macroProteinG READ macro_protein_g NOTIFY changed)
	Q_PROPERTY(double macroFatG READ macro_fat_g NOTIFY changed)
	Q_PROPERTY(double macroCarbsG READ macro_carbs_g NOTIFY changed)
	Q_PROPERTY(QString macroCalculatedAt READ macro_calculated_at NOTIFY changed)
	Q_PROPERTY(AList_Model *exercises READ exercises CONSTANT)
	Q_PROPERTY(AList_Model *rules READ rules CONSTANT)
	// — Ввод значений (спринт S6, issue 51) —
	// Замер сегодняшнего дня. Поля хранятся парами «значение + флаг»: пустой
	// обхват — это «не измерял», а ноль сантиметров — измерение, поэтому
	// одним свойством их не различить (§5.5 «нет данных» ≠ «ноль»).
	Q_PROPERTY(bool hasMeasurement READ Has_Measurement NOTIFY changed)
	Q_PROPERTY(double weightKg READ weightKg NOTIFY changed)
	Q_PROPERTY(double chestCm READ chestCm NOTIFY changed)
	Q_PROPERTY(double waistCm READ waistCm NOTIFY changed)
	Q_PROPERTY(double hipCm READ hipCm NOTIFY changed)
	Q_PROPERTY(double armCm READ armCm NOTIFY changed)
	Q_PROPERTY(double thighCm READ thighCm NOTIFY changed)

public:
	Avm_Today(AProgram_Repository *in_program_repository, ARule_Repository *in_rule_repository,
	          AWorkout_Repository *in_workout_repository, AMeasurement_Repository *in_measurement_repository,
	          AStats_Repository *in_stats_repository, AUsers_Repository *in_users_repository, QObject *in_parent = 0);

	// Обёртки QML-моста: имена в нижнем регистре первой буквы, чтобы разметка не
	// зависела от внутреннего стиля имён C++ (§2 CODE_STYLE).
	Q_INVOKABLE bool reload(const QString &in_user_id);
	// Фиксированная дата для тестов: без неё результат зависел бы от часов.
	Q_INVOKABLE void setToday(const QString &in_date);

	// — Ввод значений (спринт S6, issue 51) —
	// Замер за сегодня. Незаданные поля (in_has_* = false) не пишутся: запись
	// сливается с уже сохранённой, поэтому утренний вес не теряется при
	// вечерней записи обхватов. Повторный ввод за день обновляет запись, а не
	// создаёт вторую (upsert на паре user + date).
	Q_INVOKABLE bool saveMeasurement(double in_weight_kg, bool in_has_weight, double in_chest_cm, bool in_has_chest,
	                                 double in_waist_cm, bool in_has_waist, double in_hip_cm, bool in_has_hip,
	                                 double in_arm_cm, bool in_has_arm, double in_thigh_cm, bool in_has_thigh);

	// Отметка правила за сегодня. Запись идемпотентна по паре (rule, date):
	// повторная отметка тем же состоянием не должна увеличивать счётчик дней
	// (§5.4 — первичные данные только факты).
	Q_INVOKABLE bool saveCheckin(const QString &in_rule_id, const QString &in_state, double in_value,
	                             bool in_has_value);

	// Числовая отметка: «съел 1800 ккал». Состояние выводится из цели правила («не ниже» — done
	// от порога, «не выше» — done до порога, превышение — failed, недобор — partial).
	Q_INVOKABLE bool saveCheckinValue(const QString &in_rule_id, double in_value);
	// «Выполнено» одним касанием: у числового правила значение заполняется целью.
	Q_INVOKABLE bool markCheckinDone(const QString &in_rule_id);
	// Снятие отметки за сегодня: правило возвращается в «нет данных».
	Q_INVOKABLE bool clearCheckin(const QString &in_rule_id);

	bool Load(const QString &in_user_id);
	void Set_Today(const QString &in_date);

	// Замер сегодняшнего дня целиком: используется и формой, и перечитыванием
	// после записи (одним источником, чтобы форма и список разошлись).
	bool Load_Measurement(const QString &in_user_id);

	bool Has_Program() const;
	QString programId() const;
	QString programTitle() const;
	QString goal() const;
	QString today() const;
	int weekday() const;
	int dayKind() const;
	QString Day_Kind_Name() const;
	int Next_Training_Weekday() const;
	bool Is_Rest_Day() const;
	QString dayTitle() const;
	int estimated_minutes() const;
	int Current_Week() const;
	int weeks_total() const;
	bool Session_Completed() const;
	QString errorKey() const;
	// — Авторасчёт КБЖУ (миграция v6) —
	bool Has_Macro_Plan() const;
	double macro_calories_kcal() const;
	double macro_protein_g() const;
	double macro_fat_g() const;
	double macro_carbs_g() const;
	QString macro_calculated_at() const;
	AList_Model *exercises() const;
	AList_Model *rules() const;
	bool Has_Measurement() const;
	double weightKg() const;
	double chestCm() const;
	double waistCm() const;
	double hipCm() const;
	double armCm() const;
	double thighCm() const;

	// Ввод значений (спринт S6, issue 51), предметная область —
	bool Save_Measurement(const QString &in_user_id, double in_weight_kg, bool in_has_weight, double in_chest_cm,
	                      bool in_has_chest, double in_waist_cm, bool in_has_waist, double in_hip_cm, bool in_has_hip,
	                      double in_arm_cm, bool in_has_arm, double in_thigh_cm, bool in_has_thigh);
	bool Save_Checkin(const QString &in_rule_id, const QString &in_state, double in_value, bool in_has_value);
	const SProgram_Rule *Find_Loaded_Rule(const QString &in_rule_id) const;
	// Пользователь, от имени которого идёт ввод: замеры и отметки не зависят
	// от того, есть ли активная программа, поэтому идентификатор хранится
	// отдельно от неё.
	QString Current_User_Id() const;

signals:
	void changed();

private:
	void Clear_Error();
	void Clear_Data();
	// Дата экрана одной функцией: её читают загрузка, неделя и очистка.
	QDate date_of_today() const;
	// Порядковый номер недели срока: 0 — программа ещё не началась или её срок
	// не задан.
	int Compute_Week(const QDate &in_date, const SProgram &in_program) const;
	// Причина отказа нужна экрану: «нет активной программы» и «сбой записи» —
	// разные ситуации, и обе требуют, чтобы экран объяснил, что делать, а не
	// показал прочерк.
	bool Set_Error(const QString &in_error_key);
	// Вид дня строкой и ближайший тренировочный день. Оба считаются из
	// расписания, поэтому экран не ищет их сам (§5.1).
	void Load_Day(const QVector<SWeek_Slot> &in_slots, const QVector<SProgram_Day> &in_days);
	// Ближайший день недели с упражнениями, начиная с сегодняшнего. 0 — такого
	// дня нет: программа состоит из одних дней отдыха, и тогда подпись про
	// следующую тренировку показывать нечего.
	int Find_Next_Training_Weekday(const QVector<SWeek_Slot> &in_slots) const;
	void Load_Day_Exercises(const QString &in_day_id);
	void Load_Rules_For_Today();
	void Load_Session();
	// Результат авторасчёта КБЖУ читается вместе с замером: он не зависит от
	// программы (как и замеры, инвариант 20 §4), поэтому грузится до проверки
	// активной программы — гостю без программы расчёт тоже нужен.
	void Load_Macro_Plan(const QString &in_user_id);
	// Запись замера или отметки делает прежние числа статистики недействительными
	// (§5.4): кэш удаляется из точки записи, иначе экран статистики продолжал бы
	// показывать вес, которого уже нет.
	void Invalidate_Stats();

	// Слот по ISO-номеру дня недели; nullptr — день расписанием не покрыт.
	const SWeek_Slot *Find_Slot(const QVector<SWeek_Slot> &in_slots, int in_weekday) const;
	const SProgram_Day *Find_Day(const QVector<SProgram_Day> &in_days, const QString &in_day_id) const;

	AProgram_Repository *Program_Repository_Instance = 0;
	ARule_Repository *Rule_Repository_Instance = 0;
	AWorkout_Repository *Workout_Repository_Instance = 0;
	AMeasurement_Repository *Measurement_Repository_Instance = 0;
	AStats_Repository *Stats_Repository_Instance = 0;
	AUsers_Repository *Users_Repository_Instance = 0;

	// Замер сегодняшнего дня целиком. Держится структурой, а не набором
	// отдельных полей ViewModel: форма читает и пишет её одним объектом, и
	// раздельные свойства расходились бы с ней при частичном вводе.
	SBody_Measurement Measurement;
	// Результат авторасчёта КБЖУ: числа и дата расчёта. Держится структурой
	// домена, а не набором полей ViewModel: экран показывает числа как есть, и
	// тот же расчёт на экране «Рассчитать КБЖУ» заполняет эту же структуру.
	AsMacro_Calculator::SResult Macro_Result;
	QString Macro_Calculated_At_Text;
	// Идентификатор пользователя, от имени которого шла последняя загрузка.
	// Замеры и отметки не зависят от активной программы (замеры — инвариант
	// 20 §4), поэтому держать их по Program_User_Id нельзя: у гостя без
	// программы записать вес было бы некуда.
	QString Input_User_Id;

	AList_Model *Exercises = 0;
	AList_Model *Rules = 0;
	QVector<SProgram_Rule> Loaded_Rules;

	QString Program_Id_Text;
	QString Program_User_Id;
	QString Program_Title_Text;
	QString Goal_Text;
	QString Today_Text;
	QString Day_Title_Text;
	// Идентификатор дня плана, к которому ведёт слот сегодняшнего дня недели.
	QString Slot_Day_Id_Text;
	QString Error_Key_Text;
	int Weekday_Number = 1;
	int Day_Kind_Value = int(EDay_Kind::Rest);
	int Estimated_Minutes = 0;
	int Current_Week_Value = 0;
	int Weeks_Total_Value = 0;
	int Next_Training_Weekday_Value = 0;
	bool Session_Done = false;
};
//----------------------------------------------------------------------------