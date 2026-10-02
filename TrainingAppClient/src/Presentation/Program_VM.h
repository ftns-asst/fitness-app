#pragma once

#include "Domain/Program_Model.h"
#include "Presentation/List_Model.h"

#include <QHash>
#include <QObject>
#include <QString>

class AProgram_Repository;
class ARule_Repository;

using namespace Program_Model;

//----------------------------------------------------------------------------
// ViewModel глобальной программы (срез W8, issue 26, §7
// `02-program-model.md`). Доменное имя сущности — `SProgram`; «Моя
// программа» — её название в UI (§1 термины), в коде оно не используется.
//
// Собирает дерево активной программы из репозиториев и отдаёт его QML
// готовыми моделями-списками. Скалярные свойства программы — свойства
// объекта, списки — отдельные модели с именованными ролями.
//
// Поверх БД не зависит: ViewModel не знает SQL, репозитории скрыты за
// указателями, поэтому подставляются фейками в тестах. Обратная ссылка на
// репозиторий нужна только для перечитывания дерева.
//
// Владение: модели-списки — дети ViewModel (родитель-потомок QObject),
// lifetime обеспечивается Qt.
//
// Регистрация в QML: тип объявляется в qml-модуле приложения
// (`QML_NAMED_ELEMENT(Program)`), а не здесь — ядро не зависит от QML.
//------------------------------------------------------------------------------
class Avm_Program : public QObject
{
	Q_OBJECT

	Q_PROPERTY(bool hasProgram READ Has_Program NOTIFY changed)
	Q_PROPERTY(QString programId READ programId NOTIFY changed)
	Q_PROPERTY(QString title READ title NOTIFY changed)
	Q_PROPERTY(QString goal READ goal NOTIFY changed)
	Q_PROPERTY(QString state READ state NOTIFY changed)
	// Программа в read-only состоянии (completed/cancelled/archived, §3.1): её
	// структуру править нельзя, и экран обязан сказать это заранее.
	Q_PROPERTY(bool isReadOnly READ Is_Read_Only NOTIFY changed)
	Q_PROPERTY(int weeksTotal READ weeksTotal NOTIFY changed)
	Q_PROPERTY(int revision READ revision NOTIFY changed)
	Q_PROPERTY(QString startDate READ startDate NOTIFY changed)
	Q_PROPERTY(QString selectedDayId READ selectedDayId NOTIFY changed)
	Q_PROPERTY(QString errorKey READ errorKey NOTIFY changed)

	// Модели списков объявлены свойствами, а не только методами: иначе QML
	// видел бы их как функции и получал пустую модель (§5 QML-контракт).
	Q_PROPERTY(AList_Model *week_slots READ week_slots CONSTANT)
	Q_PROPERTY(AList_Model *program_days READ program_days CONSTANT)
	Q_PROPERTY(AList_Model *day_exercises READ day_exercises CONSTANT)
	Q_PROPERTY(AList_Model *goal_targets READ goal_targets CONSTANT)
	Q_PROPERTY(AList_Model *rules READ rules CONSTANT)

public:
	Avm_Program(AProgram_Repository *in_program_repository, ARule_Repository *in_rule_repository,
	            QObject *in_parent = 0);

	// Перечитывает активную программу пользователя. Отсутствие программы —
	// QML-мост: имена в нижнем регистре первой буквы, чтобы разметка не зависела
	// от внутреннего стиля имён C++ (§2 CODE_STYLE). Подробности — в .cpp.
	Q_INVOKABLE bool reload(const QString &in_user_id, const QString &in_program_id = QString());
	Q_INVOKABLE void selectDay(const QString &in_program_day_id);

	// не сбой чтения, но ошибка для UI: errorKey заполняется, модели пусты.
	// Пустой in_program_id открывает активную программу — как раньше.
	bool Reload(const QString &in_user_id, const QString &in_program_id = QString());

	// Выбор дня плана: перечитывает его предписания. Неизвестный день
	// очищает список, чтобы UI не показывал предыдущие упражнения.
	void Select_Day(const QString &in_program_day_id);

	bool Has_Program() const;
	QString programId() const;
	QString title() const;
	QString goal() const;
	QString state() const;
	bool Is_Read_Only() const;
	int weeksTotal() const;
	int revision() const;
	QString startDate() const;
	QString selectedDayId() const;
	QString errorKey() const;

	AList_Model *week_slots() const;
	AList_Model *program_days() const;
	AList_Model *day_exercises() const;
	AList_Model *goal_targets() const;
	AList_Model *rules() const;

signals:
	void changed();

private:
	void Clear_Tree();
	bool Load_Tree(const QString &in_program_id);
	bool Load_Day_Exercises(const QString &in_program_day_id);
	void Clear_Error();
	// Причина отказа нужна, чтобы отличить «нет активной программы» от
	// «программа не найдена или принадлежит другому пользователю».
	enum EReason
	{
		Reason_Unspecified,

		Reason_Not_Owned
	};

	void Set_Error(EReason in_reason = Reason_Unspecified);

	// today — локальная дата устройства (YYYY-MM-dd), §5.5.
	QString Today_Text() const;

	AProgram_Repository *Program_Repository_Instance;
	ARule_Repository *Rule_Repository_Instance;

	AList_Model *Week_Slots;
	AList_Model *Program_Days;
	AList_Model *Day_Exercises;
	AList_Model *Goal_Targets;
	AList_Model *Rules;

	Program_Model::SProgram Program;
	QHash<QString, QString> Known_Day_Ids;
	QHash<QString, QString> Exercise_Titles;
	QString Selected_Day_Id;
	QString Error_Key_Text;
};
//----------------------------------------------------------------------------
