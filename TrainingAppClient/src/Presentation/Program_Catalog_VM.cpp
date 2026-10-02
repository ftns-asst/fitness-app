#include "Presentation/Program_Catalog_VM.h"

#include "Data/Program/Program_Repository.h"
#include "Presentation/Program_Rows.h"

#include <QLoggingCategory>

Q_LOGGING_CATEGORY(PROGRAM_CATALOG_VM_Log, "trainingapp.program.catalog")

//----------------------------------------------------------------------------
Avm_Program_Catalog::Avm_Program_Catalog(AProgram_Repository *in_program_repository, QObject *in_parent)
    : QObject(in_parent), Program_Repository_Instance(in_program_repository), Active_List(new AList_Model(this)),
      Draft_List(new AList_Model(this)), Finished_List(new AList_Model(this))
{
	const QStringList roles = Program_Rows::Catalog_Program_Roles();

	// Роли задаются до строк: AList_Model хранит соответствие «имя ↔ номер» и
	// перевыставляет его при смене набора, поэтому порядок важен.
	Active_List->Set_Roles(roles);
	Draft_List->Set_Roles(roles);
	Finished_List->Set_Roles(roles);
}
// — QML-мост (camelCase) —
// Обёртка отделена от метода предметной области: контракт экрана не должен
// повторять внутренний стиль имён (§2 CODE_STYLE).
bool Avm_Program_Catalog::reload(const QString &in_user_id)
{
	return Reload(in_user_id);
}
//----------------------------------------------------------------------------
bool Avm_Program_Catalog::Reload(const QString &in_user_id)
{
	QVector<SProgram> programs;

	Clear_Error();
	Active_List->Set_Rows(QVector<QVariantMap>());
	Draft_List->Set_Rows(QVector<QVariantMap>());
	Finished_List->Set_Rows(QVector<QVariantMap>());

	// Без хранилища каталог не читается вовсе. Показывать пустоту при сбое
	// нельзя: экран стёр бы настоящие программы молча, без признака ошибки.
	if (Program_Repository_Instance == 0)
	{
		Set_Error();

		return false;
	}

	if (Program_Repository_Instance->Load_Programs(in_user_id, programs) == false)
	{
		Set_Error();

		return false;
	}

	Fill_Groups(programs);

	emit changed();

	return true;
}
//----------------------------------------------------------------------------
void Avm_Program_Catalog::Fill_Groups(const QVector<SProgram> &in_programs)
{
	QVector<SProgram> active;
	QVector<SProgram> draft;
	QVector<SProgram> finished;

	// Порядок внутри группы сохраняется: Load_Programs уже отсортировал по
	// убыванию updated_at, а перестановка строк сделала бы каталог
	// непредсказуемым при каждом открытии.
	for (int i = 0; i < in_programs.size(); ++i)
	{
		switch (in_programs.at(i).State)
		{
		case EProgram_State::Active:
			active.append(in_programs.at(i));
			break;

		// Пауза стоит рядом с черновиком: это программа, к которой вернутся, а
		// не программа в истории.
		case EProgram_State::Draft:
		case EProgram_State::Paused:
			draft.append(in_programs.at(i));
			break;

		default:
			finished.append(in_programs.at(i));
			break;
		}
	}

	Active_List->Set_Rows(Program_Rows::Build_Catalog_Programs(active));
	Draft_List->Set_Rows(Program_Rows::Build_Catalog_Programs(draft));
	Finished_List->Set_Rows(Program_Rows::Build_Catalog_Programs(finished));
}
//----------------------------------------------------------------------------
bool Avm_Program_Catalog::Has_Programs() const
{
	return Active_List->Row_Count() + Draft_List->Row_Count() + Finished_List->Row_Count() > 0;
}
//----------------------------------------------------------------------------
QString Avm_Program_Catalog::Error_Key() const
{
	return Error_Key_Text;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program_Catalog::active_list() const
{
	return Active_List;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program_Catalog::draft_list() const
{
	return Draft_List;
}
//----------------------------------------------------------------------------
AList_Model *Avm_Program_Catalog::finished_list() const
{
	return Finished_List;
}
//----------------------------------------------------------------------------
void Avm_Program_Catalog::Clear_Error()
{
	Error_Key_Text.clear();
}
//----------------------------------------------------------------------------
bool Avm_Program_Catalog::Set_Error()
{
	Error_Key_Text = "undefined_error";

	qCInfo(PROGRAM_CATALOG_VM_Log).noquote() << "catalog load failed" << Error_Key_Text;

	return false;
}
//----------------------------------------------------------------------------