#pragma once

#include "Domain/Macro_Calculator.h"
#include "Domain/Program_Model.h"
#include "Domain/Rule_Engine.h"
#include "Presentation/List_Model.h"

#include <QHash>
#include <QObject>
#include <QString>

class AProgram_Repository;
class ARule_Repository;
class AUsers_Repository;

using namespace Program_Model;

//-------------------------------------------------------------------------------------------------------------
// ViewModel правил программы и отметок пользователя (срез W10, экран 43,
// §3.6–3.7 `02-program-model.md`).
//
// Отвечает за два действия, которые пользователь делает руками: посмотреть
// правила программы и отметить выполнение. Расчётные правила
// (`computedFromSets` / `computedFromSessions`) проверяют себя сами — их
// состояние за сегодня приходит из Avm_Program_Stats, а здесь они
// показываются как «неизвестно», потому что фактов тренировок у этого
// ViewModel нет (§3.6: отсутствие данных не равно «выполнено»).
//
// Поверх БД не зависит: репозитории скрыты за указателями. Владение —
// модели-списки как дети ViewModel (родитель-потомок QObject).
//
// Регистрация в QML — в qml-модуле приложения (QML_NAMED_ELEMENT), ядро
// не зависит от Qt Qml.
//-------------------------------------------------------------------------------------------------------------
class Avm_Program_Rules : public QObject
{
	Q_OBJECT

	Q_PROPERTY(bool hasProgram READ Has_Program NOTIFY changed)
	Q_PROPERTY(QString programId READ programId NOTIFY changed)
	Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)
	Q_PROPERTY(QString today READ today NOTIFY changed)
	// Границы окна отметок: экран рисует период подписью.
	Q_PROPERTY(int periodDays READ periodDays NOTIFY changed)
	Q_PROPERTY(QString rangeFrom READ rangeFrom NOTIFY changed)
	Q_PROPERTY(QString rangeTo READ rangeTo NOTIFY changed)
	Q_PROPERTY(int checkinCount READ checkinCount NOTIFY changed)
	Q_PROPERTY(double adherencePercent READ adherencePercent NOTIFY changed)
	Q_PROPERTY(bool hasAdherencePercent READ hasAdherencePercent NOTIFY changed)
	// Сохранённый расчёт КБЖУ пользователя: экран правил предлагает кнопку
	// «Добавить нормы из расчёта» только когда расчёт есть, иначе она обещала
	// бы работу, которую нечем наполнить.
	Q_PROPERTY(bool hasMacroResult READ Has_Macro_Result NOTIFY changed)

	// Модели списков объявлены свойствами, а не только методами: иначе QML
	// видел бы их как функции и получал пустую модель (§5 QML-контракт).
	Q_PROPERTY(AList_Model *rules READ rules CONSTANT)
	Q_PROPERTY(AList_Model *checkins READ checkins CONSTANT)

public:
	Avm_Program_Rules(AProgram_Repository *in_program_repository, ARule_Repository *in_rule_repository,
	                  AUsers_Repository *in_users_repository, QObject *in_parent = 0);

	// Перечитывает правила активной программы и её отметки за период.
	// Активной программы может не быть — это не сбой чтения, но UI обязан
	// QML-мост: имена в нижнем регистре первой буквы, чтобы разметка не зависела
	// от внутреннего стиля имён C++ (§2 CODE_STYLE). Подробности — в .cpp.
	Q_INVOKABLE bool reload(const QString &in_user_id);
	Q_INVOKABLE bool setPeriodDays(int in_days);
	Q_INVOKABLE bool saveCheckin(const QString &in_rule_id, const QString &in_state, double in_value, bool in_has_value,
	                             const QString &in_note);
	// Генерация четырёх правил питания (белок, углеводы, жиры, калории) из
	// сохранённого расчёта КБЖУ: цели — «не ниже N г» для макросов и «ровно
	// C ккал» для калорийности. Повторное нажатие обновляет цели по свежему
	// расчёту, а не копит дубликаты.
	Q_INVOKABLE bool generateRulesFromMacro(const QString &in_user_id);

	// Перечитывает правила активной программы и её отметки за период.
	// Активной программы может не быть — это не сбой чтения, но UI обязан
	// отличать состояние «нет программы» от ошибки (errorKey).
	bool Reload(const QString &in_user_id);

	// Период отметок в днях. Допустимы 7, 30 и 90 (§5.1); другое значение
	// отвергается, чтобы экран не показывал бессмысленные окна.
	bool Set_Period_Days(int in_days);

	// Ручная отметка за сегодня: состояние текстом (done/failed/skipped/
	// partial), необязательное значение и заметка. Расчётная отметка
	// пользователем не ставится (§3.7) — репозиторий её тоже не примет.
	bool Save_Checkin(const QString &in_rule_id, const QString &in_state, double in_value, bool in_has_value,
	                  const QString &in_note);

	// Фиксирует «сегодня» (YYYY-MM-DD). По умолчанию — дата устройства;
	// задаётся явно в тестах, чтобы результат не зависел от часов.
	void Set_Today(const QString &in_today);

	// Генерация правил питания из расчёта КБЖУ (предметная область).
	bool Generate_Rules_From_Macro(const QString &in_user_id);

	bool Has_Program() const;
	QString programId() const;
	QString errorKey() const;
	QString today() const;
	int periodDays() const;
	QString rangeFrom() const;
	QString rangeTo() const;
	int checkinCount() const;
	double adherencePercent() const;
	bool hasAdherencePercent() const;
	// Есть ли сохранённый результат расчёта КБЖУ (заполняется при reload).
	bool Has_Macro_Result() const;

	AList_Model *rules() const;
	AList_Model *checkins() const;

signals:
	void changed();

private:
	void Clear_All();
	bool Load_Program_Id(const QString &in_user_id);
	bool Load_Rules();
	bool Load_Checkins();
	// today — фиксированная в тестах дата устройства (§5.5).
	QString Today() const;
	void Apply_Period();
	// Состояние отметок за сегодня по ruleId для роли todayState.
	QHash<QString, QString> Today_States() const;
	// Adherence по вердиктам отметок; «неизвестно» в знаменатель не входит
	// (§5.5) — считает доменный AsRule_Engine.
	void Recalculate_Adherence();
	void Clear_Error();
	void Set_Error(const QString &in_key);

	AProgram_Repository *Program_Repository_Instance;
	ARule_Repository *Rule_Repository_Instance;
	// Кэш параметров и результата КБЖУ живёт в AUsers_Repository (миграции
	// v5–v6): правила питания генерируются из сохранённого расчёта, и без
	// этого указателя экран не смог бы их создать.
	AUsers_Repository *Users_Repository_Instance;

	AList_Model *Rules;
	AList_Model *Checkins;

	QString Program_Id_Text;
	QString Today_Text_Override;
	QString Range_From_Text;
	QString Range_To_Text;
	QString Error_Key_Text;
	QVector<SProgram_Rule> Loaded_Rules;
	QVector<SRule_Checkin> Loaded_Checkins;
	int Period_Days_Number = 7;
	double Adherence_Number = 0;
	bool Has_Adherence = false;
	// Расчёт КБЖУ пользователя прочитан (заполняется в Reload). Кнопка
	// генерации правил доступна только при сохранённом расчёте.
	bool Has_Macro_Result_Value = false;
};
//-------------------------------------------------------------------------------------------------------------
