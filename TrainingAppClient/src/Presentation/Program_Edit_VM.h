#pragma once

#include "Domain/Error_State.h"
#include "Domain/Program_Model.h"
#include "Domain/Program_Service.h"
#include "Presentation/List_Model.h"

#include <QObject>
#include <QString>
#include <QStringList>
#include <QVector>

class AProgram_Repository;
class ARule_Repository;

using namespace Program_Model;

//----------------------------------------------------------------------------
// ViewModel создания программы (срез W11, экран 42).
//
// Экран правил «каждый день недели настраивается отдельно» (§3.4) — это и есть
// то, что ViewModel собирает: семь дней, у каждого свой набор упражнений, плюс
// цели и правила. Пользователю не нужно знать про ревизии, идентификаторы и
// то, что программа сначала создаётся черновиком: всё это внутри.
//
// Черновик программы (созданная, но не активированная) — нормальное
// состояние (§3.1), поэтому активация вынесена отдельным флагом Create().
//
// Поверх БД не зависит: хранилище скрыто за указателями, ViewModel не знает
// SQL и не создаёт идентификаторы сам — это делает сервис.
// Роли дней редактора: день недели, вид и число упражнений. Отдельный набор от
// ролей дней программы — редактор показывает неделю до создания.
QStringList Edit_Day_Roles();
//----------------------------------------------------------------------------
class Avm_Program_Edit : public QObject
{
	Q_OBJECT

	Q_PROPERTY(bool canCreate READ canCreate NOTIFY changed)
	Q_PROPERTY(bool created READ created NOTIFY changed)
	// Сохранение включает программу (§2.3: активная у пользователя одна),
	// поэтому прежняя активная уходит в паузу. Признак нужен экрану: об этой
	// смене состояния надо сказать пользователю, иначе программа «пропадёт»
	// из активных молча.
	Q_PROPERTY(bool previousProgramPaused READ Previous_Program_Paused NOTIFY changed)
	// Черновик существует сразу после reset(), а программа — только после
	// create(). Экран редактирования обязан показывать семь дней недели и
	// введённые значения по черновику, иначе до создания программы экран пуст.
	Q_PROPERTY(bool hasDraft READ Has_Draft NOTIFY changed)
	// Автосохранение мутаций в БД со статусом Draft. Выключено по умолчанию,
	// чтобы тесты оставались детерминированными: экраны редактора включают
	// его при открытии. Отзыв: «добавил упражнение, нажал назад — ничего не
	// сохранилось»: с включённым автосохранением каждое действие дня уже в БД.
	Q_PROPERTY(bool autoSaveDraft READ autoSaveEnabled WRITE setAutoSaveEnabled NOTIFY changed)
	Q_PROPERTY(QString programId READ programId NOTIFY changed)
	Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)
	// Текст ошибки хранилища: ключ говорит «что не так» словами продукта, а
	// подробность — почему именно отказал SQLite/сервис. Без неё отказ создания
	// выглядел как «кнопка ничего не делает»: create() возвращал false без ключа.
	Q_PROPERTY(QString errorDetail READ errorDetail NOTIFY changed)
	Q_PROPERTY(QString title READ title NOTIFY changed)
	Q_PROPERTY(int weeks READ weeks NOTIFY changed)
	Q_PROPERTY(QString startDate READ startDate NOTIFY changed)
	// Производная дата окончания (§3.3): старт + срок − 1 день. Считает домен, а
	// не экран: та же формула уже участвует в статистике, и вторая её копия в
	// QML разошлась бы с доменом при первом же изменении правила. Пусто, пока
	// нет даты старта или срока — тогда окончание не определено, и подсказке
	// нечего показывать.
	Q_PROPERTY(QString plannedEndDate READ plannedEndDate NOTIFY changed)
	Q_PROPERTY(int trainingDays READ trainingDays NOTIFY changed)
	// Выбранная цель и уровень нужны экрану, чтобы показать уже сделанный
	// выбор: без них нельзя подсветить активный вариант, и пользователь не
	// понимает, что выбор сохранился.
	Q_PROPERTY(int goal READ Goal_Value NOTIFY changed)
	Q_PROPERTY(int level READ Level_Value NOTIFY changed)
	// День недели, который открыт на экране настройки дня (ISO 1–7).
	Q_PROPERTY(int selectedWeekday READ Selected_Weekday NOTIFY changed)
	// Модель дней недели. Без этого свойства QML видел бы program_days как
	// метод и получал пустую модель, то есть экран оставался бы без семи дней.
	Q_PROPERTY(AList_Model *program_days READ program_days CONSTANT)

	// Модели списков объявлены свойствами, а не только методами: иначе QML
	// видел бы их как функции и получал пустую модель (§5 QML-контракт).
	Q_PROPERTY(AList_Model *goal_targets READ goal_targets CONSTANT)
	Q_PROPERTY(AList_Model *rules READ rules CONSTANT)
	// — Экран выбора правил (§5.7) —
	// Каталог пресетов и каталог метрик приходят из Program_Rows: объявленные
	// в разметке пресеты нельзя было ни отфильтровать, ни сверить с
	// добавленными правилами, а у метрик ещё и не было бы своих единиц.
	Q_PROPERTY(AList_Model *catalog READ catalog CONSTANT)
	Q_PROPERTY(AList_Model *metrics READ metrics CONSTANT)
	// Упражнения выбранного дня. Экран дня показывает их отдельной страницей,
	// поэтому модель одна и переключается вместе с selectedWeekday.
	Q_PROPERTY(AList_Model *day_exercises READ day_exercises CONSTANT)

public:
	Avm_Program_Edit(AProgram_Repository *in_program_repository, ARule_Repository *in_rule_repository,
	                 AsProgram_Service *in_service, QObject *in_parent = 0);

	// QML-мост. Имена в нижнем регистре первой буквы: из C++ они выглядели бы
	// как set_Weeks, то есть внутренний стиль утекал бы в контракт экрана
	// (§2 CODE_STYLE). Каждая обёртка — одна строка вызова.
	Q_INVOKABLE bool reset(const QString &in_user_id);
	// Продолжает прерванное создание: если в БД остался черновик (состояние
	// Draft) — открывает его, иначе даёт чистый редактор. Без него повторное
	// открытие «Новой программы» стирало бы набранные дни.
	Q_INVOKABLE bool resumeDraft(const QString &in_user_id);
	// Удаление брошенного черновика: сохранённая работа стирается из БД,
	// редактор остаётся открытым с чистой формой. После него список программ
	// не копит отказ от halfway-созданий.
	Q_INVOKABLE bool deleteDraft();
	// Немедленная запись черновика в хранилище. Мутации пишут сами, когда
	// включён autoSaveDraft; отдельный вызов нужен экрану для явного
	// «сейчас сохранить» без мутации.
	Q_INVOKABLE bool saveDraft();
	// Открывает существующую программу в редакторе (срез S2). Без неё
	// «Настроить» из каталога открывало бы пустой редактор создания.
	Q_INVOKABLE bool load(const QString &in_user_id, const QString &in_program_id);
	Q_INVOKABLE void setTitle(const QString &in_title);
	Q_INVOKABLE void setGoal(int in_goal);
	Q_INVOKABLE void setLevel(int in_level);
	Q_INVOKABLE void setStartDate(const QString &in_date);
	Q_INVOKABLE void setWeeks(int in_weeks);
	Q_INVOKABLE bool setDayKind(int in_weekday, int in_kind);
	Q_INVOKABLE bool addDayExercise(int in_weekday, const QString &in_exercise_id, int in_sets, int in_reps_min,
	                                int in_reps_max, double in_target_weight_kg, bool in_has_target_weight);
	Q_INVOKABLE bool setTarget(int in_metric, double in_start_value, bool in_has_start_value, double in_target_value,
	                           int in_direction);
	Q_INVOKABLE bool addRule(const QString &in_code, const QString &in_title, int in_metric, double in_target_min,
	                         bool in_has_target_min, double in_target_max, bool in_has_target_max);
	Q_INVOKABLE bool create(bool in_activate);

	// — Экран дня (отдельная страница) —
	// Модель упражнений общая на все дни, поэтому выбор дня обязан её
	// перезаполнять, иначе экран покажет упражнения чужого дня.
	Q_INVOKABLE int selectDay(int in_weekday);
	// Удаление упражнения из дня: ошибочно добавленное иначе осталось бы в
	// программе навсегда.
	Q_INVOKABLE bool removeDayExercise(int in_weekday, int in_index);
	// Копирование упражнений из дня в день: семь дней настраиваются вручную,
	// а разница между ними обычно мала.
	Q_INVOKABLE bool copyDay(int in_from_weekday, int in_to_weekday);

	// — Цели и правила: создание и удаление —
	// Правило создаётся с полным набором полей: категория, периодичность,
	// единица, дни применения, жёсткость. Короткий Add_Rule жёстко ставил
	// Nutrition/в день/граммы, из-за чего сон и шаги выглядели как «граммы».
	Q_INVOKABLE bool addRuleDetailed(const QString &in_code, const QString &in_title, int in_category, int in_metric,
	                                 int in_periodicity, double in_target_min, bool in_has_target_min,
	                                 double in_target_max, bool in_has_target_max, int in_unit, int in_applies_to,
	                                 int in_enforcement);
	Q_INVOKABLE bool removeRule(int in_index);
	Q_INVOKABLE bool removeTarget(int in_index);

	// Рекомендация текстом: категория «Другое», цель не требуется. Отдельный
	// вход, а не флаг у Add_Rule_Detailed, потому что для неё меняется не
	// значение поля, а сам смысл правила (07-programs-ui-spec.md §5.7).
	Q_INVOKABLE bool addAdvice(const QString &in_title, const QString &in_text);

	// — Экран выбора правил (§5.7) —
	// Каталог приходит из Program_Rows: пресеты, объявленные в разметке
	// редактора, нельзя было ни отфильтровать, ни сверить с добавленными.
	Q_INVOKABLE bool setCatalogFilter(const QString &in_category);
	Q_INVOKABLE bool addCatalogPreset(const QString &in_code);
	// Своё правило: метрика задаёт периодичность, единицу пользователь выбирает
	// из тех, что осмысленны для этой метрики, а модель сверяет. Цель приходит
	// одной из форм (min/max/exact), какая допустима — говорит каталог метрик.
	Q_INVOKABLE bool addCustomRule(const QString &in_title, int in_metric, int in_unit, double in_min, bool in_has_min,
	                               double in_max, bool in_has_max, double in_exact, bool in_has_exact);

	// Доменные операции объявлены отдельно от QML-обёрток: тесты и будущие
	// сценарии вызывают предметную область напрямую, и тогда имена методов не
	// зависят от того, как их видит QML.
	int Select_Day(int in_weekday);
	bool Remove_Day_Exercise(int in_weekday, int in_index);
	bool Copy_Day(int in_from_weekday, int in_to_weekday);
	bool Add_Rule_Detailed(const QString &in_code, const QString &in_title, int in_category, int in_metric,
	                       int in_periodicity, double in_target_min, bool in_has_target_min, double in_target_max,
	                       bool in_has_target_max, int in_unit, int in_applies_to, int in_enforcement);
	bool Remove_Rule(int in_index);
	bool Remove_Target(int in_index);
	// Текстовая рекомендация: правило без цели, но с содержательным текстом.
	// Именно это снимает требование цели — «Другое» в макете и §3.6.
	bool Add_Advice_Rule(const QString &in_title, const QString &in_text);

	// Фильтр каталога типовых правил (пустая строка — все категории).
	bool Set_Catalog_Filter(const QString &in_category);
	// Добавление пресета по коду. Повторное добавление уже имеющегося кода
	// отвергается: вторая такая же проверка ничего не меняет.
	bool Add_Catalog_Preset(const QString &in_code);
	// Произвольное измеримое правило по метрике из каталога.
	bool Add_Custom_Rule(const QString &in_title, int in_metric, int in_unit, double in_min, bool in_has_min,
	                     double in_max, bool in_has_max, double in_exact, bool in_has_exact);

	// Очищает редактор и запоминает владельца программы.
	bool Reset(const QString &in_user_id);

	bool Load(const QString &in_user_id, const QString &in_program_id);

	void Set_Title(const QString &in_title);
	void Set_Goal(int in_goal);
	void Set_Level(int in_level);
	void Set_Start_Date(const QString &in_date);
	void Set_Weeks(int in_weeks);

	// День недели 1…7 (ISO). Вид дня: EDay_Kind как int — QML сопоставляет
	// строки, числовые значения типов в QML не принимаются (§7).
	bool Set_Day_Kind(int in_weekday, int in_kind);

	// Упражнение в день. Пустой идентификатор упражнения отвергается: день без
	// упражнения — это день отдыха, и он задаётся видом дня.
	bool Add_Day_Exercise(int in_weekday, const QString &in_exercise_id, int in_sets, int in_reps_min, int in_reps_max,
	                      double in_target_weight_kg, bool in_has_target_weight);

	bool Set_Target(int in_metric, double in_start_value, bool in_has_start_value, double in_target_value,
	                int in_direction);
	bool Add_Rule(const QString &in_code, const QString &in_title, int in_metric, double in_target_min,
	              bool in_has_target_min, double in_target_max, bool in_has_target_max);

	// Создаёт программу. in_activate сразу переводит её в активную; без флага
	// остаётся черновиком, который можно доделать позже.
	bool Create(bool in_activate);

	// Черновик появляется после reset() и не зависит от того, сохранена ли уже
	// программа: без него экран до создания программы остаётся пустым.
	bool Has_Draft() const
	{
		return !Draft.User_Id.isEmpty();
	}

	bool canCreate() const;
	bool created() const;
	QString programId() const;
	// Истина, если последнее сохранение поставило на паузу прежнюю активную
	// программу пользователя (§2.3). Экран говорит об этом тостом.
	bool Previous_Program_Paused() const;
	QString errorKey() const;
	QString errorDetail() const
	{
		return Error_Detail_Text;
	}
	QString title() const;
	int weeks() const;
	QString startDate() const;
	// Дата окончания — производная от даты старта и срока, поэтому отдельного
	// сеттера у неё нет: экран двигает срок, дата едет за ним.
	QString plannedEndDate() const;
	int trainingDays() const;
	// Геттеры для экрана: значение выбора, а не сам enum (§7 — в QML числа).
	int Goal_Value() const;
	int Level_Value() const;
	int Selected_Weekday() const;

	bool autoSaveEnabled() const;
	void setAutoSaveEnabled(bool in_enabled);

	AList_Model *program_days() const;
	AList_Model *goal_targets() const;
	AList_Model *rules() const;
	AList_Model *catalog() const;
	AList_Model *metrics() const;
	AList_Model *day_exercises() const;

signals:
	void changed();

private:
	bool Set_Error(const QString &in_key);
	// Отказ записи с технической причиной: ключ «save_failed» для экрана плюс
	// текст репозитория/сервиса для errorDetail. Молчаливый false приводил к
	// тому, что создание падало без единой строки на экране.
	bool Fail_Save(const QString &in_detail);
	void Clear_Error();
	// Правила набраны в редакторе до создания, поэтому после появления
	// идентификатора программы им проставляется он.
	bool Link_Drafts_To_Program();
	// Запись черновика в БД со статусом Draft (см. Persist_Draft в .cpp).
	bool Persist_Draft();
	// Поиск прерванного черновика пользователя и его загрузка в редактор.
	bool Resume_Draft(const QString &in_user_id);
	// Стирание сохранённого черновика из БД и очистка формы редактора.
	bool Delete_Draft();
	int Count_Day_Exercises(int in_weekday) const;
	// Цель измеримая, если для неё обязательны цели программы (§4 инвариант 8).
	static bool Is_Measurable_Goal(EProgram_Goal in_goal);
	// Идентификаторы выдаёт сервис при создании; редактору нужны лишь
	// собственные для дней и упражнений, поэтому они генерируются здесь.
	QString New_Id() const;
	// Расписание строится из видов дней: рабочий день даёт слот-тренировку,
	// день отдыха — слот отдыха (§3.4).
	QVector<SWeek_Slot> Build_Schedule() const;
	// Состав дня для Create_From_Scratch: все семь дней, даже дни отдыха —
	// без них слот отдыха не на что сослаться (инвариант 4).
	bool Publish_Days();
	void Refresh_Day_Model();
	// Перезаполнение моделей целей, правил и упражнений дня. Вынесено отдельно,
	// потому что добавление и удаление меняют одни и те же списки.
	void Refresh_Target_Model();
	void Refresh_Rule_Model();
	void Refresh_Catalog_Model();
	void Refresh_Exercise_Model();
	// Индексы 0-based, как у QML-модели: удаление по строке экрана не должно
	// отличаться от удаления по строке таблицы.
	bool Remove_By_Index(QVector<SGoal_Target> &io_targets, int in_index);
	bool Remove_By_Index(QVector<SProgram_Rule> &io_rules, int in_index);
	// Идентификатор дня недели (ISO 1–7) для внутреннего дня. Пустая строка,
	// если день не задан.
	QString Day_Id_For(int in_weekday) const;
	// Порядковые номера упражнений дня пересчитываются после удаления и
	// копирования, иначе экран покажет разрывы в нумерации упражнений.
	void Reindex_Day_Exercises(const QString &in_day_id);

	AProgram_Repository *Program_Repository_Instance;
	ARule_Repository *Rule_Repository_Instance;
	AsProgram_Service *Service_Instance;

	AList_Model *Program_Days;
	AList_Model *Goal_Targets;
	AList_Model *Rules;
	AList_Model *Catalog;
	AList_Model *Metrics;
	// Фильтр каталога: пустая строка — все категории (§5.7).
	QString Catalog_Category;
	AList_Model *Day_Exercises;

	// Открытый на экране день (ISO 1–7). Хранится в ViewModel, а не в QML,
	// потому что модель упражнений принадлежит ему.
	int Selected_Day = 1;

	// Автосохранение мутаций в БД (черновик). По умолчанию выключено: тесты
	// проверяют «отказ не пишет в базу» на точных числах, и фоновая запись их
	// сломала бы. Экраны редактора включают флаг при открытии.
	bool Auto_Save_Enabled = false;

	SProgram Draft;
	QVector<SGoal_Target> Draft_Targets;
	QVector<SProgram_Day> Draft_Days;
	QVector<SDay_Exercise> Draft_Exercises;
	QVector<SProgram_Rule> Draft_Rules;
	QString Created_Program_Id;
	// Программа, приостановленная последним сохранением (пусто, если
	// пользователь был без активной программы).
	QString Paused_Previous_Program_Id;
	QString Error_Key_Text;
	QString Error_Detail_Text;
};
//----------------------------------------------------------------------------
