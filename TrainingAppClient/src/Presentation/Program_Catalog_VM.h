#pragma once

#include "Domain/Program_Model.h"
#include "Presentation/List_Model.h"

#include <QObject>
#include <QString>
#include <QVector>

class AProgram_Repository;

using namespace Program_Model;

//----------------------------------------------------------------------------
// ViewModel каталога программ (срез S1, issue 47).
//
// Отдельный объект, а не режим Avm_Program: тот читает дерево ОДНОЙ активной
// программы, а каталогу нужен список всех программ пользователя. Смешивать их
// в одном объекте означало бы держать два набора данных и два разных состояния
// пустоты («нет активной» против «нет ни одной»), которые на экране выглядят
// по-разному.
//
// Группы отдаются тремя отдельными моделями, а не одной общей с фильтром в
// QML: AList_Model отдаёт роли только по целочисленному номеру, поэтому
// отбор строк на стороне QML означал бы вызовы data() в обход нормального
// пути и молчал бы при изменении набора ролей. Три модели также снимают
// дублирование «счётчик группы + длина списка» — два источника одного числа.
//
// Модели объявлены свойствами, а не только методами: иначе QML увидел бы их как
// функции и получил пустые списки (§5 QML-контракт).
//
// Поверх БД не зависит: хранилище скрыто за указателем, поэтому в тестах
// подставляется временная база или nullptr.
//----------------------------------------------------------------------------
class Avm_Program_Catalog : public QObject
{
	Q_OBJECT

	Q_PROPERTY(bool hasPrograms READ Has_Programs NOTIFY changed)
	Q_PROPERTY(QString errorKey READ Error_Key NOTIFY changed)
	Q_PROPERTY(AList_Model *active_list READ active_list CONSTANT)
	Q_PROPERTY(AList_Model *draft_list READ draft_list CONSTANT)
	Q_PROPERTY(AList_Model *finished_list READ finished_list CONSTANT)

public:
	explicit Avm_Program_Catalog(AProgram_Repository *in_program_repository, QObject *in_parent = 0);

	// QML-мост. Имена в нижнем регистре первой буквы: внутренний стиль имён
	// C++ не должен утекать в контракт экрана (§2 CODE_STYLE).
	Q_INVOKABLE bool reload(const QString &in_user_id);

	// Пустой каталог — законное состояние, а не ошибка чтения: пользователь
	// ещё ничего не создавал. Ошибка возникает только при сбое хранилища.
	bool Reload(const QString &in_user_id);

	bool Has_Programs() const;
	QString Error_Key() const;
	AList_Model *active_list() const;
	AList_Model *draft_list() const;
	AList_Model *finished_list() const;

signals:
	void changed();

private:
	bool Set_Error();
	void Clear_Error();
	// Раскладывает программы по группам каталога: активная, черновик с
	// паузой и всё завершённое. Раскладка здесь, а не в QML, потому что группа —
	// это свойство данных, а не вёрстки.
	void Fill_Groups(const QVector<SProgram> &in_programs);

	AProgram_Repository *Program_Repository_Instance;
	AList_Model *Active_List;
	AList_Model *Draft_List;
	AList_Model *Finished_List;
	QString Error_Key_Text;
};
//----------------------------------------------------------------------------