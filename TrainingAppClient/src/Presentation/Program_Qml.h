#pragma once

#include "Presentation/Macro_VM.h"
#include "Presentation/Program_Catalog_VM.h"
#include "Presentation/Program_Rules_VM.h"
#include "Presentation/Program_Stats_VM.h"
#include "Presentation/Program_VM.h"

#include <QObject>
#include <QQmlEngine>

//-------------------------------------------------------------------------------------------------------------
// Регистрация Avm_Program в QML-модуле приложения.
//
// Тип объявляется здесь, а не в самом ViewModel: TrainingAppCore не должен
// зависеть от Qt Qml (иначе юнит-тесты домена тянут QML-стек). Слой-QML
// содержит только макросы экспорта — вся логика остаётся в Presentation.
// В QML доступен как `Program`, создаётся composition root, поэтому
// QML_UNCREATABLE: экраны получают готовый экземпляр, а не создают свой.
//-------------------------------------------------------------------------------------------------------------
class AProgram_Qml : public Avm_Program
{
	Q_OBJECT
	QML_NAMED_ELEMENT(Program)
	QML_UNCREATABLE("Экземпляр создаётся в composition root")
};

//-------------------------------------------------------------------------------------------------------------
// Экспорт Avm_Program_Stats под именем ProgramStats — те же правила, что и
// для Program: тип принадлежит qml-модулю, а не ядру.
//-------------------------------------------------------------------------------------------------------------
class AProgram_Stats_Qml : public Avm_Program_Stats
{
	Q_OBJECT
	QML_NAMED_ELEMENT(ProgramStats)
	QML_UNCREATABLE("Экземпляр создаётся в composition root")
};
//-------------------------------------------------------------------------------------------------------------
// Экспорт Avm_Program_Rules под именем ProgramRules — те же правила, что и
// для Program и ProgramStats.
//-------------------------------------------------------------------------------------------------------------
class AProgram_Rules_Qml : public Avm_Program_Rules
{
	Q_OBJECT
	QML_NAMED_ELEMENT(ProgramRules)
	QML_UNCREATABLE("Экземпляр создаётся в composition root")
};
//-------------------------------------------------------------------------------------------------------------

//-------------------------------------------------------------------------------------------------------------
// Экспорт Avm_Program_Catalog под именем ProgramCatalog — те же правила, что у
// остальных ViewModel программы: создаёт экземпляр composition root, а не сам
// QML-модуль.
//-------------------------------------------------------------------------------------------------------------
class AProgram_Catalog_Qml : public Avm_Program_Catalog
{
	Q_OBJECT
	QML_NAMED_ELEMENT(ProgramCatalog)
	QML_UNCREATABLE("Экземпляр создаётся в composition root")
};

//-------------------------------------------------------------------------------------------------------------
// Экспорт Avm_Macro под именем Macro — те же правила, что у Program и Today:
// тип принадлежит qml-модулю, а экземпляр создаёт composition root.
//-------------------------------------------------------------------------------------------------------------
class AMacro_Qml : public Avm_Macro
{
	Q_OBJECT
	QML_NAMED_ELEMENT(Macro)
	QML_UNCREATABLE("Экземпляр создаётся в composition root")
};
